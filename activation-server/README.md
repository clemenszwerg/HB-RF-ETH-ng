# HB-RF-ETH-ng Activation Server Prototype

This is a small Linux-friendly prototype for supporter-key issuing and future
device-bound activation. It is not wired into the firmware or WebUI yet.

## What It Does

- Stores licenses in SQLite.
- Issues current firmware-compatible supporter keys.
- Sends supporter-key emails via SMTP, or dry-runs the email body.
- Binds a key to the first `deviceId` that activates it.
- Rejects later activation attempts from a different `deviceId`.
- Stores PayPal webhook payloads for later verified fulfillment logic.

The current firmware still validates keys offline. This server is a prototype
for the next step: online activation and device binding.

## Run Locally

```bash
cd activation-server
ADMIN_TOKEN=change-me \
ACTIVATION_SECRET=change-me-too \
python3 server.py --host 127.0.0.1 --port 8088
```

Health check:

```bash
curl http://127.0.0.1:8088/health
```

## Issue A License

```bash
curl -X POST http://127.0.0.1:8088/admin/licenses \
  -H 'Authorization: Bearer change-me' \
  -H 'Content-Type: application/json' \
  -d '{
    "buyerName": "Max",
    "buyerEmail": "max@example.org",
    "days": 365,
    "paymentProvider": "manual",
    "paymentId": "test-001"
  }'
```

By default, email is dry-run mode. Set `DRY_RUN_EMAIL=0` and configure SMTP to
send for real:

```bash
SMTP_HOST=smtp.example.org
SMTP_PORT=587
SMTP_USER=mail@example.org
SMTP_PASSWORD=secret
SMTP_FROM=mail@example.org
```

## Activate A License

```bash
curl -X POST http://127.0.0.1:8088/activate \
  -H 'Content-Type: application/json' \
  -d '{
    "licenseKey": "XXXX-XXXX-XXXX-XXXX",
    "deviceId": "esp32-unique-device-id"
  }'
```

First activation binds the license to that device. A second activation from the
same device is accepted. A different device returns `already_bound`.

## PayPal Webhook Placeholder

```bash
curl -X POST http://127.0.0.1:8088/webhooks/paypal \
  -H 'Content-Type: application/json' \
  -d '{"id":"demo-event","event_type":"CHECKOUT.ORDER.APPROVED"}'
```

This endpoint only stores webhook payloads for now. Production must verify
PayPal signatures before issuing a key.

## Environment

- `ADMIN_TOKEN`: bearer token for admin endpoints.
- `ACTIVATION_SECRET`: HMAC secret used to hash device IDs.
- `DB_PATH`: SQLite path, default `activation.sqlite3`.
- `HOST`: default `127.0.0.1`.
- `PORT`: default `8088`.
- `DRY_RUN_EMAIL`: default `1`; set `0` to send SMTP mail.
- `SMTP_HOST`, `SMTP_PORT`, `SMTP_USER`, `SMTP_PASSWORD`, `SMTP_FROM`.

