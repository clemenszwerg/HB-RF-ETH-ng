/*
 *  updatecheck.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "updatecheck.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "string.h"
#include "reset_info.h"
#include "system_reset.h"
#include "semver.h"
#include "ota_config.h"
#include "monitoring.h"
#include "esp_heap_caps.h"

static const char *TAG = "UpdateCheck";

// GitHub repository for HB-RF-ETH-ng. Single source of truth for version,
// release notes and firmware binary.
static const char *GITHUB_REPO = "Xerolux/HB-RF-ETH-ng";

// Cap for the heap buffer used to receive the GitHub releases JSON.
//
// The buffer is allocated lazily: only after the TLS handshake completes and
// only when response body data actually starts arriving (see _gh_event_handler),
// and it is freed again the moment parsing finishes. It therefore never
// overlaps with another TLS handshake's memory use.
//
// The stable channel uses /releases/latest (a single release object, ~8-12 KB);
// the beta channel uses /releases?per_page=1 (~13 KB). 24 KB comfortably holds
// either response with headroom for growing release notes.
//
// Parsing uses a zero-allocation string parser (_parseGitHubReleasesString)
// instead of cJSON: cJSON needs 3-5x the JSON size in heap for its internal
// parse tree, which - combined with this buffer and the still-live TLS context
// - exhausted the WROOM-32's ~90 KB free heap and crashed the update check with
// an out-of-memory watchdog reset. The string parser adds ~0 bytes of heap.
static const size_t GH_RESPONSE_CAP = 24 * 1024;

// esp_https_ota in IDF 6.x no longer exposes ESP_ERR_HTTPS_OTA_INCOMPLETE; use
// a private application code to report a download that ended prematurely.
#define OTA_ERR_DOWNLOAD_INCOMPLETE 0x10001

void _update_check_task_func(void *parameter)
{
  ((UpdateCheck *)parameter)->_taskFunc();
}

// ---- Testable helpers ------------------------------------------------------

void buildReleasesApiUrl(bool beta, char* out, size_t outLen)
{
    // "/releases/latest" excludes prereleases; "/releases" lists every
    // non-draft release including prereleases, newest first.
    if (beta) {
        // per_page=1: each release JSON is ~13 KB; cJSON needs 3-5× RAM
        // for its internal parse tree. With per_page=2 the 27 KB response
        // exhausted the heap (64 KB buffer + ~60 KB cJSON tree > 94 KB heap).
        // per_page=1 keeps the response at ~14 KB, leaving heap for cJSON.
        // GitHub's /releases endpoint generally returns newest first; the
        // rare out-of-order case is covered by the fallback string parser.
        snprintf(out, outLen,
                 "https://api.github.com/repos/%s/releases?per_page=1",
                 GITHUB_REPO);
    } else {
        snprintf(out, outLen,
                 "https://api.github.com/repos/%s/releases/latest",
                 GITHUB_REPO);
    }
}

void normalizeTag(const char* tag, char* out, size_t outLen)
{
    if (outLen == 0) return;
    out[0] = 0;
    if (!tag) return;

    const char* p = tag;
    // Strip a single leading v/V (GitHub tags are typically "v2.1.11").
    if ((p[0] == 'v' || p[0] == 'V') &&
        p[1] >= '0' && p[1] <= '9') {
        p++;
    }
    // snprintf always null-terminates; strncpy triggers -Wstringop-truncation
    // when the compiler deduces source and destination have the same length.
    snprintf(out, outLen, "%s", p);
}

// ---- HTTP response accumulator --------------------------------------------

struct GhResponse {
    char *buf;        // heap-allocated lazily (GH_RESPONSE_CAP + terminator)
    size_t len;
    size_t cap;
    int httpStatus;
    bool allocFailed; // true if the lazy buffer allocation could not be served
    bool truncated;   // true when the response exceeded cap
};

static esp_err_t _gh_event_handler(esp_http_client_event_t *evt)
{
    GhResponse *r = (GhResponse *)evt->user_data;
    if (!r) return ESP_OK;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (r->httpStatus == 0) {
            r->httpStatus = esp_http_client_get_status_code(evt->client);
        }
        // Only buffer successful responses. A 403 (rate limit) or 404 (no
        // releases yet) body is small and not worth parsing.
        if (r->httpStatus == 200) {
            // Allocate the (large) response buffer lazily, only once body data
            // is actually arriving. By this point the TLS handshake is already
            // complete, so we never hold ~48 KB of heap across the handshake -
            // that starved the PSA-crypto signature verification on the
            // WROOM-32 (no PSRAM) and aborted the handshake with
            // PSA_ERROR_INSUFFICIENT_MEMORY (-0x008D / -141).
            if (!r->buf && !r->allocFailed) {
                r->buf = (char *)malloc(r->cap + 1);
                if (!r->buf) {
                    r->allocFailed = true;
                    ESP_LOGE(TAG, "Failed to allocate %u bytes for GitHub response",
                             (unsigned)r->cap);
                    // Abort the transfer immediately: there is no point
                    // decrypting and downloading the rest of a response we
                    // cannot store. _doFetch reports the error via allocFailed.
                    return ESP_ERR_NO_MEM;
                }
            }
            if (r->buf) {
                size_t copy = evt->data_len;
                if (r->len + copy > r->cap) {
                    copy = (r->len < r->cap) ? (r->cap - r->len) : 0;
                    r->truncated = true;
                }
                if (copy > 0) {
                    memcpy(r->buf + r->len, evt->data, copy);
                    r->len += copy;
                }
                if (r->truncated) {
                    // Never accept a release parsed from an incomplete JSON
                    // document. Abort early and report a deterministic error.
                    return ESP_ERR_INVALID_SIZE;
                }
            }
        }
    }
    return ESP_OK;
}

// Zero-allocation parser for the GitHub Releases JSON response.
//
// Replaces cJSON, which needed 3-5x the JSON size in heap for its parse tree
// and - combined with the response buffer and the still-live TLS context -
// exhausted the WROOM-32's ~90 KB free heap, crashing the update check with an
// out-of-memory watchdog reset. This parser allocates nothing on the heap.
//
// Handles both response shapes:
//   - /releases/latest  -> a single release object
//   - /releases?per_page=N -> an array of release objects
// The list endpoint is deliberately requested with per_page=1, so parsing the
// first non-draft release is sufficient and avoids large temporary objects on
// the task stack. Extracts tag_name (->version), published_at,
// prerelease, draft (skipped), the firmware_*.bin browser_download_url and the
// release-notes body. releaseUrl is constructed from the repo + tag because the
// "html_url" field is interleaved with author/uploader objects and cannot be
// reliably matched to a release by string scanning alone.
static bool _parseGitHubReleasesString(const char *buf, ReleaseInfo *out)
{
    if (!buf || !out) return false;

    // Parse directly into the caller-owned result. ReleaseInfo contains a 4 KB
    // notes buffer; keeping both "current" and "best" copies here consumed
    // nearly 10 KB of stack on top of refresh()'s result object and overflowed
    // the 12 KB UpdateCheck task at its first run, 30 seconds after boot.
    memset(out, 0, sizeof(*out));
    const char *p = buf;

    while ((p = strstr(p, "\"tag_name\":")) != NULL) {
        // --- version (strip a single leading v/V) ---
        const char *v = p + strlen("\"tag_name\":");
        while (*v == ' ' || *v == ':') v++;
        if (*v == '"') v++;
        const char *tagEnd = strchr(v, '"');
        if (!tagEnd) break;
        {
            const char *vp = v;
            if ((vp[0] == 'v' || vp[0] == 'V') && vp[1] >= '0' && vp[1] <= '9') vp++;
            size_t tl = (size_t)(tagEnd - vp);
            if (tl >= sizeof(out->version)) tl = sizeof(out->version) - 1;
            memcpy(out->version, vp, tl);
            out->version[tl] = 0;
        }

        // Remaining fields of this release object live up to the next tag_name
        // (or end of buffer).
        const char *scan = tagEnd + 1;
        const char *objEnd = strstr(scan, "\"tag_name\":");
        if (!objEnd) objEnd = scan + strlen(scan);

        // --- draft (drafts are skipped) ---
        bool isDraft = false;
        const char *dr = strstr(scan, "\"draft\":");
        if (dr && dr < objEnd) {
            dr += strlen("\"draft\":");
            while (*dr == ' ' || *dr == ':') dr++;
            isDraft = (strncmp(dr, "true", 4) == 0);
        }
        if (isDraft) {
            memset(out, 0, sizeof(*out));
            p = objEnd;
            continue;
        }

        // --- prerelease ---
        const char *pr = strstr(scan, "\"prerelease\":");
        if (pr && pr < objEnd) {
            pr += strlen("\"prerelease\":");
            while (*pr == ' ' || *pr == ':') pr++;
            out->isPrerelease = (strncmp(pr, "true", 4) == 0);
        }

        // --- published_at ---
        const char *pa = strstr(scan, "\"published_at\":");
        if (pa && pa < objEnd) {
            pa += strlen("\"published_at\":");
            while (*pa == ' ' || *pa == ':') pa++;
            if (*pa == '"') pa++;
            const char *pe = strchr(pa, '"');
            if (pe) {
                size_t pl = (size_t)(pe - pa);
                if (pl >= sizeof(out->publishedAt)) pl = sizeof(out->publishedAt) - 1;
                memcpy(out->publishedAt, pa, pl);
                out->publishedAt[pl] = 0;
            }
        }

        // --- firmware_*.bin browser_download_url ---
        const char *bdu = strstr(scan, "\"browser_download_url\":");
        while (bdu && bdu < objEnd) {
            const char *bv = bdu + strlen("\"browser_download_url\":");
            while (*bv == ' ' || *bv == ':') bv++;
            if (*bv == '"') bv++;
            const char *be = strchr(bv, '"');
            if (!be) break;
            size_t bl = (size_t)(be - bv);
            // extract the filename portion (after the last '/')
            const char *fn = bv;
            for (const char *c = bv; c < be; c++) {
                if (*c == '/') fn = c + 1;
            }
            size_t fnLen = (size_t)(be - fn);
            if (!isDraft &&
                fnLen > 9 && strncmp(fn, "firmware_", 9) == 0 &&
                fnLen > 4 && strncmp(be - 4, ".bin", 4) == 0 &&
                bl < sizeof(out->downloadUrl)) {
                memcpy(out->downloadUrl, bv, bl);
                out->downloadUrl[bl] = 0;
            }
            bdu = strstr(be, "\"browser_download_url\":");
        }

        // --- body (release notes), JSON-unescaped, capped to the buffer ---
        const char *bt = strstr(scan, "\"body\":");
        if (bt && bt < objEnd) {
            bt += strlen("\"body\":");
            while (*bt == ' ' || *bt == ':') bt++;
            if (*bt == '"') bt++;
            const char *be = bt;
            while (*be) {
                if (*be == '\\' && be[1]) { be += 2; continue; }
                if (*be == '"') break;
                be++;
            }
            size_t cap = sizeof(out->body) - 1;
            size_t bi = 0;
            const char *src = bt;
            while (src < be && bi < cap) {
                if (*src == '\\' && src + 1 < be) {
                    char c = src[1];
                    if (c == 'n') out->body[bi++] = '\n';
                    else if (c == 't') out->body[bi++] = '\t';
                    else if (c != 'r') out->body[bi++] = c;  // drop \r
                    src += 2;
                } else {
                    out->body[bi++] = *src++;
                }
            }
            out->body[bi] = 0;
        }

        if (out->version[0]) {
            // releaseUrl: reconstruct from repo + tag (reliable and avoids the
            // html_url ordering issue with nested author/uploader objects).
            snprintf(out->releaseUrl, sizeof(out->releaseUrl),
                     "https://github.com/%s/releases/tag/v%s",
                     GITHUB_REPO, out->version);
            out->valid = true;
            return true;
        }

        memset(out, 0, sizeof(*out));
        p = objEnd;
    }
    return false;
}

// ---- UpdateCheck -----------------------------------------------------------

UpdateCheck::UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED)
    : _sysInfo(sysInfo), _statusLED(statusLED), _settings(settings)
{
    _stateMutex = xSemaphoreCreateMutex();
    _fetchLock = xSemaphoreCreateMutex();
    if (!_stateMutex || !_fetchLock) {
        ESP_LOGE(TAG, "Failed to allocate UpdateCheck synchronization objects");
    }
}

void UpdateCheck::start()
{
    if (_tHandle) return;
    // ReleaseInfo body is 4 KB – the struct exceeds 5 KB on the stack when
    // copied by getReleaseInfo() / refresh(). 12 KB gives the task comfortable
    // headroom and avoids stack-overflow → watchdog-reset loops.
    if (xTaskCreate(_update_check_task_func, "UpdateCheck", 12288,
                    this, 3, &_tHandle) != pdPASS) {
        _tHandle = NULL;
        ESP_LOGE(TAG, "Failed to create UpdateCheck task");
    }
}

void UpdateCheck::stop()
{
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
}

const char *UpdateCheck::getLatestVersion()
{
    return _latestVersion;
}

ReleaseInfo UpdateCheck::getReleaseInfo()
{
    ReleaseInfo snap;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        snap = _release;
        xSemaphoreGive(_stateMutex);
    } else {
        memset(&snap, 0, sizeof(snap));
        strncpy(snap.version, "n/a", sizeof(snap.version) - 1);
    }
    return snap;
}

VersionSnapshot UpdateCheck::getVersionSnapshot()
{
    VersionSnapshot s;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        s.valid = _release.valid;
        // snprintf (not strncpy) to avoid -Wstringop-truncation when source
        // and destination have the same size.
        snprintf(s.version, sizeof(s.version), "%s", _release.version);
        s.isPrerelease = _release.isPrerelease;
        snprintf(s.error, sizeof(s.error), "%s", _release.error);
        xSemaphoreGive(_stateMutex);
    }
    return s;
}

bool UpdateCheck::refresh()
{
    // Refuse to stack fetches - a single GitHub request can take several
    // seconds (DNS + TLS handshake + download) and the background timer
    // and a manual "Check now" can otherwise race.
    if (!_fetchLock || xSemaphoreTake(_fetchLock, 0) != pdTRUE) {
        ESP_LOGD(TAG, "refresh: another fetch is already in progress");
        return false;
    }

    // Reflect "checking" state for MQTT / WebUI. Only flip the state if no
    // OTA is currently running so we never clobber OTA_DOWNLOADING etc.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        if (_otaState == OTA_STATE_IDLE) {
            _setOtaStateLocked(OTA_STATE_CHECKING);
        }
        xSemaphoreGive(_stateMutex);
    }

    ReleaseInfo fresh = {};
    bool ok = _doFetch(&fresh);
    int64_t now = esp_timer_get_time() / 1000;

    // Publish atomically. On failure we keep the last-known-good snapshot
    // (version, URLs, body) so the WebUI continues to show useful data,
    // and just surface the error + retry timestamp.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        if (ok) {
            fresh.fetchedAtMs = now;
            _release = fresh;
            strncpy(_latestVersion, fresh.version, sizeof(_latestVersion) - 1);
            _latestVersion[sizeof(_latestVersion) - 1] = 0;
        } else {
            _release.fetchedAtMs = now;
            if (fresh.error[0]) {
                strncpy(_release.error, fresh.error, sizeof(_release.error) - 1);
                _release.error[sizeof(_release.error) - 1] = 0;
            }
            // If we've never had a valid snapshot, keep the "n/a" default
            // so legacy callers (sysinfo, MQTT) report the same.
            if (!_release.valid) {
                strncpy(_latestVersion, "n/a", sizeof(_latestVersion) - 1);
                _latestVersion[sizeof(_latestVersion) - 1] = 0;
            }
        }
        // Clear CHECKING state unless an OTA ran in parallel (very unlikely
        // but defensive). Reset back to IDLE so MQTT can show "idle".
        if (_otaState == OTA_STATE_CHECKING) {
            _setOtaStateLocked(OTA_STATE_IDLE);
        }
        xSemaphoreGive(_stateMutex);
    }

    xSemaphoreGive(_fetchLock);
    return ok;
}

bool UpdateCheck::isFetchInProgress()
{
    if (!_fetchLock) return false;
    // xSemaphoreGetMutexHolder returns the owning task handle or NULL. We
    // don't care who owns it - any non-NULL value means "busy".
    return xSemaphoreGetMutexHolder(_fetchLock) != NULL;
}

void UpdateCheck::_setOtaStateLocked(ota_state_t state)
{
    _otaState = state;
    if (state == OTA_STATE_IDLE) {
        _otaProgress = -1;
        _otaErrorText[0] = '\0';
        _otaErrorCode = 0;
    }
}

void UpdateCheck::_setOtaProgressLocked(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    _otaProgress = percent;
}

void UpdateCheck::_setOtaErrorLocked(int code, const char* text)
{
    _otaErrorCode = code;
    if (text) {
        strncpy(_otaErrorText, text, sizeof(_otaErrorText) - 1);
        _otaErrorText[sizeof(_otaErrorText) - 1] = '\0';
    } else {
        _otaErrorText[0] = '\0';
    }
}

OtaSnapshot UpdateCheck::getOtaState()
{
    OtaSnapshot snap;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        snap.state = _otaState;
        snap.progress_pct = _otaProgress;
        snap.error_code = _otaErrorCode;
        // snprintf (not strncpy) to avoid -Wstringop-truncation: both buffers
        // are 64 bytes and _otaErrorText is always null-terminated.
        snprintf(snap.error_text, sizeof(snap.error_text), "%s", _otaErrorText);
        xSemaphoreGive(_stateMutex);
    } else {
        snap.state = OTA_STATE_IDLE;
        snap.progress_pct = -1;
    }
    return snap;
}

bool UpdateCheck::tryBeginOtaOperation()
{
    bool expected = false;
    return _otaInProgress.compare_exchange_strong(expected, true);
}

void UpdateCheck::finishOtaOperation()
{
    _otaInProgress.store(false);
}

bool UpdateCheck::_doFetch(ReleaseInfo *out)
{
    bool beta = _settings ? _settings->getBetaChannel() : false;
    char url[128];
    buildReleasesApiUrl(beta, url, sizeof(url));

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Fetching %s release info from GitHub (beta: %d, heap free: %u KB)",
             beta ? "latest (incl. pre-release)" : "stable", beta ? 1 : 0,
             (unsigned)(free_heap / 1024));

    // Serialize with the changelog proxy — two TLS handshakes at once exhaust
    // the heap on the ESP32. 15 s timeout: GitHub should never take that long
    // for a single release.
    bool net_locked = false;
    if (g_net_fetch_mutex) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) != pdTRUE) {
            snprintf(out->error, sizeof(out->error), "network busy");
            ESP_LOGW(TAG, "GitHub fetch skipped: another HTTPS operation is still active");
            return false;
        }
        net_locked = true;
    }

    // Declared here so goto cleanup never skips an initialisation (C++).
    esp_http_client_handle_t client = NULL;
    esp_err_t err = ESP_FAIL;
    int status = 0;
    size_t bodyLen = 0;
    bool parsedOk = false;

    // These are declared before the first goto so they're never skipped.
    GhResponse resp = {};
    esp_http_client_config_t config = {};

    // The response buffer is allocated lazily in _gh_event_handler once body
    // data starts arriving, so its 24 KB is not held during the TLS handshake.
    resp.cap = GH_RESPONSE_CAP;

    configure_ota_http_client(config, url);
    config.timeout_ms = 10000;
    config.buffer_size = 2048;
    config.event_handler = _gh_event_handler;
    config.user_data = &resp;

    client = esp_http_client_init(&config);
    if (!client) {
        snprintf(out->error, sizeof(out->error), "HTTP client init failed");
        ESP_LOGE(TAG, "Failed to init HTTP client");
        goto cleanup;
    }

    // GitHub requires a User-Agent and accepts a vendor media type for JSON.
    esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");
    // Prevent GitHub from gzip-compressing the response: ESP-IDF's HTTP client
    // does not automatically decompress Content-Encoding, so the parser would
    // receive binary data and fail. Asking for identity ensures plain JSON.
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    err = esp_http_client_perform(client);
    status = resp.httpStatus;
    bodyLen = resp.len;

    // Free the HTTP client (and its TLS context) BEFORE parsing the response.
    // The TLS session + http-client buffers hold several KB; releasing them
    // here roughly doubles the heap available to the parser and eliminates the
    // out-of-memory crash that aborted the update check on the WROOM-32 (no
    // PSRAM). resp.buf is a separate malloc and survives the cleanup.
    esp_http_client_cleanup(client);
    client = NULL;

    parsedOk = false;
    if (resp.allocFailed) {
        snprintf(out->error, sizeof(out->error), "out of memory");
        ESP_LOGE(TAG, "GitHub response buffer allocation failed");
    } else if (resp.truncated) {
        snprintf(out->error, sizeof(out->error), "GitHub response too large");
        ESP_LOGE(TAG, "GitHub response exceeded %u-byte limit", (unsigned)resp.cap);
    } else if (err == ESP_OK && status == 200 && bodyLen > 0 && resp.buf) {
        // Null-terminate before parsing.
        resp.buf[bodyLen] = 0;

        // Diagnostic preview of the first bytes (handy for spotting gzip,
        // HTML captchas or rate-limit responses).
        {
            size_t dumpLen = bodyLen < 96 ? bodyLen : 96;
            char hex[192];
            size_t pos = 0;
            for (size_t i = 0; i < dumpLen && pos < sizeof(hex) - 4; i++) {
                unsigned char c = (unsigned char)resp.buf[i];
                if (c >= 0x20 && c <= 0x7E) {
                    hex[pos++] = (char)c;
                } else if (c == 0x0A) {
                    hex[pos++] = '\\'; hex[pos++] = 'n';
                } else if (c == 0x0D) {
                    /* skip CR */
                } else {
                    hex[pos++] = '.';
                }
            }
            hex[pos] = 0;
            ESP_LOGI(TAG, "Response preview (%u/%u bytes): [%s]",
                     (unsigned)dumpLen, (unsigned)bodyLen, hex);
        }

        size_t heapBeforeParse = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Parsing GitHub response (heap free %u KB)...",
                 (unsigned)(heapBeforeParse / 1024));

        // Zero-allocation string parser - no cJSON heap overhead. Handles both
        // /releases/latest (single object) and /releases?per_page=N (array).
        parsedOk = _parseGitHubReleasesString(resp.buf, out);

        ESP_LOGI(TAG, "Parse %s (heap free %u KB)",
                 parsedOk ? "ok" : "failed",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

        if (!parsedOk) {
            snprintf(out->error, sizeof(out->error), "could not parse release JSON");
            ESP_LOGE(TAG, "GitHub response parse failed");
        }
    } else if (err == ESP_OK && status == 404) {
        snprintf(out->error, sizeof(out->error),
                 "no stable release available yet");
        ESP_LOGW(TAG, "GitHub returned 404 - no stable release published");
    } else if (err == ESP_OK && status == 403) {
        snprintf(out->error, sizeof(out->error),
                 "GitHub API rate limit exceeded");
        ESP_LOGW(TAG, "GitHub API rate limited (HTTP 403)");
    } else {
        snprintf(out->error, sizeof(out->error),
                 "HTTP %d (%s)", status, esp_err_to_name(err));
        ESP_LOGE(TAG, "GitHub fetch failed: HTTP %d (%s)", status, esp_err_to_name(err));
    }

cleanup:
    free(resp.buf);   // free(NULL) is safe when no body ever arrived
    if (net_locked) xSemaphoreGive(g_net_fetch_mutex);

    size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Fetch complete: heap free %u KB (delta %+d KB)",
             (unsigned)(heap_after / 1024),
             (int)((int)heap_after - (int)free_heap) / 1024);

    if (parsedOk) {
        ESP_LOGI(TAG, "Latest %s release: %s%s (asset: %s)",
                 beta ? "any" : "stable",
                 out->version,
                 out->isPrerelease ? " [prerelease]" : "",
                 out->downloadUrl[0] ? out->downloadUrl : "(none)");
    } else {
        ESP_LOGW(TAG, "GitHub release fetch failed: %s", out->error);
    }

    return parsedOk;
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(pdMS_TO_TICKS(30000));

  for (;;)
  {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Stack high water: %u B, heap free: %u KB",
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)(free_heap / 1024));
    ESP_LOGI(TAG, "Checking for firmware updates...");
    ESP_LOGI(TAG, "Current version: %s", _sysInfo->getCurrentVersion());

    // refresh() is non-blocking: it returns false when a fetch is already in
    // progress (coalesced) - that is NOT an error. Use info.valid as the
    // authoritative signal so we don't log a bogus error during the boot race.
    refresh();
    VersionSnapshot info = getVersionSnapshot();

    if (info.valid)
    {
      ESP_LOGI(TAG, "Latest available version: %s", info.version);

      if (compareVersions(_sysInfo->getCurrentVersion(), info.version) < 0)
      {
        ESP_LOGW(TAG, "An updated firmware with version %s is available!", info.version);
        _statusLED->setState(LED::getProgram(LED_PROG_UPDATE_AVAILABLE));
      }
      else if (compareVersions(_sysInfo->getCurrentVersion(), info.version) > 0)
      {
        ESP_LOGI(TAG, "Running version (%s) is newer than available version (%s)",
                 _sysInfo->getCurrentVersion(), info.version);
      }
      else
      {
        ESP_LOGI(TAG, "Firmware is up to date (version %s)", info.version);
      }
    }
    else if (info.error[0])
    {
      ESP_LOGE(TAG, "Failed to determine latest version: %s", info.error);
    }
    else
    {
      // Cache empty and no error yet - a fetch is in progress or has not run.
      ESP_LOGI(TAG, "Release info not available yet (fetch in progress).");
    }

    // 24h, split into 1h chunks: pdMS_TO_TICKS((TickType_t)ms * configTICK_RATE_HZ)
    // overflows 32-bit TickType_t arithmetic for a 24h millisecond value
    // (86,400,000 ms * 100 Hz > UINT32_MAX), which silently wrapped this
    // delay down to ~500 s - hammering the GitHub API every ~8 minutes
    // instead of once a day. 1h chunks stay well within range.
    for (int hour = 0; hour < 24; hour++)
    {
      vTaskDelay(pdMS_TO_TICKS(60 * 60000));
    }
  }

  vTaskDelete(NULL);
}

void UpdateCheck::performOnlineUpdate()
{
    if (!tryBeginOtaOperation()) {
        ESP_LOGW(TAG, "OTA update rejected: another OTA operation is active");
        return;
    }

    // Only the URL/version are needed here. Avoid copying the 4 KB release
    // notes field onto the relatively small OTA worker stack.
    char downloadUrl[sizeof(_release.downloadUrl)] = {0};
    char version[sizeof(_release.version)] = "n/a";
    bool releaseValid = false;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        releaseValid = _release.valid;
        snprintf(downloadUrl, sizeof(downloadUrl), "%s", _release.downloadUrl);
        snprintf(version, sizeof(version), "%s", _release.version);
        xSemaphoreGive(_stateMutex);
    }

    if (!releaseValid || downloadUrl[0] == 0) {
        ESP_LOGE(TAG, "No firmware asset URL available (latest: %s)",
                 releaseValid ? version : "n/a");
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ESP_ERR_NOT_FOUND, "no firmware asset URL");
            xSemaphoreGive(_stateMutex);
        }
        finishOtaOperation();
        return;
    }

    ESP_LOGI(TAG, "Starting OTA update from %s", downloadUrl);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    // Mark "starting" so MQTT / WebUI can show the new state immediately.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_STARTING);
        _setOtaProgressLocked(0);
        xSemaphoreGive(_stateMutex);
    }

    esp_http_client_config_t config = {};
    configure_ota_http_client(config, downloadUrl);
    config.timeout_ms = 60000;
    config.buffer_size = 4096;

    bool net_locked = false;
    if (g_net_fetch_mutex) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(30000)) != pdTRUE) {
            ESP_LOGE(TAG, "OTA update could not start: HTTPS subsystem busy");
            if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
                _setOtaStateLocked(OTA_STATE_FAILED);
                _setOtaErrorLocked(ESP_ERR_TIMEOUT, "HTTPS subsystem busy");
                xSemaphoreGive(_stateMutex);
            }
            finishOtaOperation();
            return;
        }
        net_locked = true;
    }

    auto releaseOperation = [&]() {
        if (net_locked) {
            xSemaphoreGive(g_net_fetch_mutex);
            net_locked = false;
        }
        finishOtaOperation();
    };

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    // Use the advanced esp_https_ota API so we can report real progress to
    // MQTT / WebUI. The simple esp_https_ota(&ota_config) call is a thin
    // wrapper around these steps but hides all intermediate state.
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ret, esp_err_to_name(ret));
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        releaseOperation();
        return;
    }

    int image_size = esp_https_ota_get_image_size(ota_handle);
    ESP_LOGI(TAG, "OTA image size: %d bytes", image_size);

    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_DOWNLOADING);
        xSemaphoreGive(_stateMutex);
    }

    // Loop until the full image has been streamed into the OTA partition.
    // perform() pulls one buffer chunk at a time and writes it to flash.
    int otaChunk = 0;
    while (true) {
        ret = esp_https_ota_perform(ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        if (image_size > 0) {
            int read = esp_https_ota_get_image_len_read(ota_handle);
            int pct = (int)((uint64_t)read * 100 / (uint64_t)image_size);
            if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
                _setOtaProgressLocked(pct);
                xSemaphoreGive(_stateMutex);
            }
        }
        // Yield periodically so the idle task (and thus the task watchdog) and
        // other FreeRTOS tasks (MQTT, httpd) get CPU time during the long
        // download. perform() blocks on socket reads, but a defensive yield
        // keeps the watchdog happy even on slow networks.
        if ((++otaChunk & 0x07) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    bool complete = esp_https_ota_is_complete_data_received(ota_handle);
    if (ret != ESP_OK || !complete) {
        ESP_LOGE(TAG, "OTA perform failed: ret=%s complete=%d",
                 esp_err_to_name(ret), complete ? 1 : 0);
        const char* err_text = (ret == ESP_OK) ? "download incomplete" : esp_err_to_name(ret);
        int err_code = (ret == ESP_OK) ? OTA_ERR_DOWNLOAD_INCOMPLETE : ret;
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(err_code, err_text);
            xSemaphoreGive(_stateMutex);
        }
        esp_https_ota_abort(ota_handle);
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        releaseOperation();
        return;
    }

    // All bytes streamed - now validate and switch the boot partition.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_FLASHING);
        _setOtaProgressLocked(100);
        xSemaphoreGive(_stateMutex);
    }

    ret = esp_https_ota_finish(ota_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful, restarting...");
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_SUCCESS);
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
        releaseOperation();
        full_system_restart();
    } else {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ret, esp_err_to_name(ret));
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        releaseOperation();
    }
}
