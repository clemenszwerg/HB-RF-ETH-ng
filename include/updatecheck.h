/*
 *  updatecheck.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sysinfo.h"
#include "led.h"
#include "settings.h"

// Snapshot of the latest release known to the firmware.
// All fields are safe to copy by value.
struct ReleaseInfo {
    bool valid;                 // true once a successful fetch has populated the fields
    char version[32];           // normalized, e.g. "2.1.11" or "2.2.0-beta.1"
    char downloadUrl[256];      // GitHub asset browser_download_url of firmware_*.bin
    char releaseUrl[256];       // html_url of the release (view on GitHub)
    char publishedAt[32];       // ISO timestamp from GitHub
    bool isPrerelease;          // matches GitHub "prerelease" flag
    char body[4096];            // release notes markdown (truncated if too long)
    int64_t fetchedAtMs;        // epoch millis of the last successful fetch
    char error[128];            // human-readable last error (empty when valid)
};

class UpdateCheck
{
private:
    SysInfo* _sysInfo;
    LED *_statusLED;
    Settings* _settings;
    TaskHandle_t _tHandle = NULL;

    // Serializes writes/reads of the cached _release snapshot.
    SemaphoreHandle_t _stateMutex = NULL;
    // Held while a network fetch is in progress; prevents concurrent fetches
    // from the background task and from manual "Check now" requests.
    SemaphoreHandle_t _fetchLock = NULL;

    ReleaseInfo _release{};
    // Mirror of _release.version for the legacy const char* accessor.
    char _latestVersion[32] = "n/a";

    bool _doFetch(ReleaseInfo* out);

public:
    UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED);
    void start();
    void stop();

    // Perform a synchronous fetch from the GitHub Releases API. Returns true
    // on success, false on network/parse failure or if another fetch is in
    // progress. Respects the configured beta channel setting.
    bool refresh();

    // True while a network fetch is in progress (started by the background
    // task or by refresh()). Used by the HTTP layer to await concurrent
    // "Check now" requests without spawning redundant fetches.
    bool isFetchInProgress();

    // Returns a thread-safe snapshot of the currently cached release info.
    ReleaseInfo getReleaseInfo();

    // Triggers OTA from the cached downloadUrl (the GitHub asset URL).
    void performOnlineUpdate();

    // Legacy accessor: returns a pointer to the cached version string
    // ("n/a" if never fetched). Valid until the next refresh().
    const char* getLatestVersion();

    void _taskFunc();
};

// ---- Testable helpers (no ESP-IDF dependencies) ---------------------------

// Build the GitHub Releases API URL for the given channel.
//   beta=false -> ".../releases/latest"
//   beta=true  -> ".../releases"
void buildReleasesApiUrl(bool beta, char* out, size_t outLen);

// Normalize a Git tag (strip leading 'v'/'V', copy into out).
// "v2.1.11"        -> "2.1.11"
// "V2.2.0-beta.1"  -> "2.2.0-beta.1"
void normalizeTag(const char* tag, char* out, size_t outLen);
