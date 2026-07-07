# Activation Server Integration TODO

## Goal

Move from purely offline supporter keys to optional online activation with
device binding, while keeping the current cosmetic/supporter-only spirit.

## Decisions Needed

- Device identity:
  - Use ESP base MAC, Ethernet MAC, chip ID, or a generated install UUID.
  - Recommendation: generated install UUID stored in NVS plus hardware MAC in
    the activation payload. This avoids privacy issues and survives normal
    reboots, but can be reset intentionally.
- Key format:
  - Keep current legacy keys for existing installs.
  - Add a new server-issued activation token or activation receipt for bound
    devices.
  - Do not put PayPal or buyer data on the device.
- Offline behavior:
  - Decide whether an already activated device may stay active offline.
  - Recommendation: cache a signed activation receipt with expiry in NVS and
    refresh when online.
- Privacy:
  - Hash device IDs on the server with `ACTIVATION_SECRET`.
  - Never store raw device IDs unless debugging is explicitly enabled.
- Abuse handling:
  - Keep existing CRL support for revoked keys.
  - Add admin revoke endpoint/export for `revoked_keys.json`.

## Firmware Work

- Add a stable device identifier helper.
- Add an activation client:
  - `POST /activate` with key + device ID.
  - Store server response in NVS.
  - Show states: active, expired, revoked, already bound, offline cached.
- Add timeout/backoff:
  - Never block boot.
  - Retry in background, similar to update checks.
- Add TLS trust handling:
  - Use normal HTTPS validation.
  - Optionally pin server host/public key later.
- Keep current offline validation as fallback for legacy keys.

## WebUI Work

- In `Einstellungen -> Lizenz`, add activation status text:
  - "Aktiviert auf diesem Gerät"
  - "Key ist bereits an ein anderes Gerät gebunden"
  - "Server nicht erreichbar, letzter Status wird verwendet"
- Add a manual "Aktivierung prüfen" button.
- Add clear renewal/expired messaging.
- Make sure no full key is shown again after storing, only masked display.

## Server Work

- Replace prototype HTTP server with either:
  - keep Python stdlib for tiny deployment, or
  - FastAPI/uvicorn if we want OpenAPI, validation and cleaner middleware.
- Add migrations.
- Add structured logging.
- Add rate limiting for `/activate`.
- Add admin UI or CLI for:
  - list licenses
  - revoke license
  - resend email
  - export CRL hashes
  - edit buyer email/name
- Add backups:
  - daily SQLite dump
  - encrypted offsite backup

## PayPal Flow

- Create PayPal product/button/checkout.
- Use PayPal webhooks:
  - Verify webhook signature with PayPal before fulfillment.
  - On completed payment, issue a license.
  - Send email automatically.
- Store:
  - PayPal order/capture ID
  - payer email
  - amount/currency
  - issued license ID
- Keep it small:
  - No shop system at first.
  - One "Supporter 1 year" product is enough.
  - Optional manual admin issue stays available.

## Email Flow

- Keep SMTP configurable by env vars.
- Use bilingual templates or buyer language.
- Include:
  - key
  - expiry date
  - activation path: `Einstellungen -> Lizenz`
  - "do not share" note
  - support/contact line

## Key Tool Integration

- Option A: keep desktop key tool for manual/offline issuing.
- Option B: move issuing into activation server admin endpoints.
- Recommendation:
  - Keep the desktop tool for private/manual emergency use.
  - Use the server for paid/automated issuing.
  - Share one key-generation module/test vector between both so formats do not drift.

## Deployment Sketch

- Linux VPS.
- Reverse proxy with Caddy or nginx.
- systemd service running:
  - `python3 /opt/hb-rf-eth-ng-activation/server.py`
- SQLite database in `/var/lib/hb-rf-eth-ng-activation/activation.sqlite3`.
- Secrets in `/etc/hb-rf-eth-ng-activation/env`.
- HTTPS required before exposing activation or PayPal webhook endpoints.

