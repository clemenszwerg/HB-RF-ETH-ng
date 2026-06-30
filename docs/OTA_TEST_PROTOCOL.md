# OTA & Update-Search Test Protocol

A hands-on checklist to verify the **update search** and **OTA update** paths on
real HB-RF-ETH-ng hardware. The firmware logic is covered by code review, but the
on-device TLS handshake against GitHub's CDN and the heap behaviour during an OTA
can only be proven on a physical device — that is what this protocol is for.

> Recommended test vehicle: flash a device with **v2.2.0-Beta.13**, then use this
> protocol to update it to **v2.2.0-Beta.14**. That exercises the full
> search → download → flash → reboot chain end to end.

## Prerequisites

- A device reachable on the LAN with a known IP and admin password.
- The device has working DNS + outbound HTTPS (needed to reach `api.github.com`
  and `objects.githubusercontent.com`).
- Python 3 on the test host (for `test_ota_function.py`).
- A serial console attached is helpful but optional (`./idf.py monitor`) to watch
  the `UpdateCheck` / OTA log lines.

---

## Part 1 — Update search (`/api/check_update`)

Goal: confirm the device fetches the GitHub Releases API, parses it, and reports
the correct latest version for both channels.

### 1a. Stable channel

1. In the WebUI, make sure the **beta channel** toggle is **off**.
2. Open **Firmware Update** and press **Check for update**.
3. Expected: `latestVersion` resolves to the newest **stable** release
   (e.g. `2.1.10`), `isPrerelease=false`, and `error` is `null`.

### 1b. Beta channel

1. Enable the **beta channel** toggle and press **Check for update** again.
2. Expected: `latestVersion` resolves to the highest pre-release
   (e.g. `2.2.0-Beta.14`), `isPrerelease=true`, a non-empty `downloadUrl`
   ending in `firmware_<version>.bin`, and `updateAvailable=true` when the
   running firmware is older.

### 1c. Raw API spot-check (optional)

```bash
# Trigger a fresh fetch and inspect the JSON the device returns.
TOKEN=$(curl -s -X POST http://<device-ip>/login.json \
  -H 'Content-Type: application/json' \
  -d '{"password":"<admin-password>"}' | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])')

curl -s -X POST http://<device-ip>/api/check_update \
  -H "Authorization: Token $TOKEN" | python3 -m json.tool
```

Check that `latestVersion`, `downloadUrl`, `betaChannel` and `fetchInProgress`
look sane and that the call returns within ~25 s even under contention.

### Search — pass criteria

- [ ] Stable channel reports the latest non-prerelease version.
- [ ] Beta channel reports the highest pre-release (semver-correct, not just the
      first list entry).
- [ ] `downloadUrl` points at a `firmware_*.bin` asset that actually exists.
- [ ] A rate-limited / offline device surfaces a clear `error` string instead of
      silently claiming "up to date".

---

## Part 2 — OTA update from URL (`/api/ota_url` + `/api/ota_status`)

`test_ota_function.py` drives the URL-based OTA path the WebUI uses: it logs in,
POSTs the firmware URL, then polls `/api/ota_status` until `success` or `failed`.

```bash
python3 test_ota_function.py <device-ip> <admin-password>
# defaults to the v2.2.0-Beta.14 release asset; override with:
python3 test_ota_function.py <device-ip> <admin-password> \
  --url https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.2.0-Beta.14/firmware_2.2.0-Beta.14.bin
```

Expected console output (abridged):

```
Authenticated. Token: ...
Triggering OTA with URL: https://github.com/.../firmware_2.2.0-Beta.14.bin
OTA Triggered: OTA update started
Status: downloading, Progress: 0%
Status: downloading, Progress: 37%
Status: downloading, Progress: 100%
Status: success, Progress: 100%
OTA Update Successful! Device is restarting...
```

The device reboots automatically on success. After it comes back, confirm the new
version on the **System Info** page or via `/sysinfo.json`.

### OTA — pass criteria

- [ ] TLS handshake against the GitHub CDN succeeds (no `ESP_ERR_*` /
      `MBEDTLS_ERR_*` in the serial log) — this is the historically fragile step.
- [ ] Progress advances monotonically 0 → 100%.
- [ ] `esp_https_ota_is_complete_data_received` passes (no "download incomplete").
- [ ] Device reboots and reports the **new** version after restart.
- [ ] A bad URL / wrong host is rejected with a clear `failed` state and an error
      string, and the device stays on the old firmware.

---

## Part 3 — Negative & robustness checks (optional)

- [ ] Trigger a second OTA while one is running → rejected with
      `"OTA update already in progress"` (no partition corruption).
- [ ] Point `--url` at a non-`.bin` / HTML page → OTA fails cleanly, old firmware
      keeps running.
- [ ] Pull power mid-download → device boots the previous (still-valid) partition
      after restart (A/B rollback safety).

---

## Notes

- The integrated "update now" button in the WebUI uses the same `/api/ota_url`
  download path with the `downloadUrl` returned by `/api/check_update`, so Part 1
  + Part 2 together cover the one-click online-update flow.
- Both the search and the OTA download serialize on `g_net_fetch_mutex`, so a
  background update-check and a manual OTA cannot run their TLS handshakes at the
  same time and exhaust the heap.
