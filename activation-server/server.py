#!/usr/bin/env python3
"""
Small HB-RF-ETH-ng supporter activation server prototype.

This is intentionally not wired into the firmware or WebUI yet. It provides:

- SQLite-backed license records
- current-style supporter key generation for email delivery
- first-activation device binding
- SMTP email sending
- a PayPal webhook placeholder that stores events for later verification logic

Run:
  ADMIN_TOKEN=change-me ACTIVATION_SECRET=change-me python3 server.py
"""

from __future__ import annotations

import argparse
import base64
import csv
import hashlib
import hmac
import io
import json
import os
import secrets
import smtplib
import sqlite3
import sys
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from email.message import EmailMessage
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
EPOCH = date(2025, 1, 1)
DEFAULT_DB = Path(__file__).with_name("activation.sqlite3")


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def today_utc() -> date:
    return datetime.now(timezone.utc).date()


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def base32_encode(raw: bytes) -> str:
    bits = 0
    nbits = 0
    out: list[str] = []
    for byte in raw:
        bits = (bits << 8) | byte
        nbits += 8
        while nbits >= 5:
            nbits -= 5
            out.append(ALPHABET[(bits >> nbits) & 0x1F])
    if nbits > 0:
        out.append(ALPHABET[(bits << (5 - nbits)) & 0x1F])
    return "".join(out)


def day_offset(target: date) -> int:
    return (target - EPOCH).days


def format_key(raw16: str) -> str:
    return f"{raw16[0:4]}-{raw16[4:8]}-{raw16[8:12]}-{raw16[12:16]}"


def generate_legacy_supporter_key(expires_on: date) -> str:
    expiry_day = day_offset(expires_on)
    if not 0 <= expiry_day <= 0xFFFF:
        raise ValueError("expiry date outside key range")
    body = bytes([(expiry_day >> 8) & 0xFF, expiry_day & 0xFF]) + secrets.token_bytes(6)
    crc = crc16_ccitt(body)
    raw = bytes([(crc >> 8) & 0xFF, crc & 0xFF]) + body
    return format_key(base32_encode(raw))


def normalize_key(value: str) -> str:
    return "".join(ch for ch in value.upper() if ch in ALPHABET)


def hash_value(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def hmac_digest(secret: str, value: str) -> str:
    return hmac.new(secret.encode("utf-8"), value.encode("utf-8"), hashlib.sha256).hexdigest()


def load_json_body(handler: BaseHTTPRequestHandler) -> dict[str, Any]:
    length = int(handler.headers.get("Content-Length", "0") or "0")
    if length <= 0:
        return {}
    raw = handler.rfile.read(length)
    return json.loads(raw.decode("utf-8"))


def send_json(handler: BaseHTTPRequestHandler, status: int, payload: dict[str, Any]) -> None:
    body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Cache-Control", "no-store")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    handler.wfile.write(body)


def require_admin(handler: BaseHTTPRequestHandler, admin_token: str) -> bool:
    token = handler.headers.get("Authorization", "")
    expected = f"Bearer {admin_token}"
    if not admin_token or not hmac.compare_digest(token, expected):
        send_json(handler, HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "unauthorized"})
        return False
    return True


SCHEMA = """
PRAGMA journal_mode=WAL;

CREATE TABLE IF NOT EXISTS licenses (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  license_key TEXT NOT NULL UNIQUE,
  key_hash TEXT NOT NULL UNIQUE,
  buyer_name TEXT NOT NULL DEFAULT '',
  buyer_email TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'issued',
  issued_at TEXT NOT NULL,
  expires_on TEXT NOT NULL,
  days INTEGER NOT NULL,
  payment_provider TEXT NOT NULL DEFAULT '',
  payment_id TEXT NOT NULL DEFAULT '',
  device_hash TEXT NOT NULL DEFAULT '',
  activated_at TEXT NOT NULL DEFAULT '',
  revoked_at TEXT NOT NULL DEFAULT '',
  note TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS activations (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  license_id INTEGER NOT NULL,
  device_hash TEXT NOT NULL,
  activated_at TEXT NOT NULL,
  remote_addr TEXT NOT NULL DEFAULT '',
  user_agent TEXT NOT NULL DEFAULT '',
  FOREIGN KEY (license_id) REFERENCES licenses(id)
);

CREATE TABLE IF NOT EXISTS webhook_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  provider TEXT NOT NULL,
  event_id TEXT NOT NULL DEFAULT '',
  received_at TEXT NOT NULL,
  verified INTEGER NOT NULL DEFAULT 0,
  payload TEXT NOT NULL
);
"""


@dataclass
class Config:
    db: Path
    host: str
    port: int
    admin_token: str
    activation_secret: str
    public_base_url: str
    smtp_host: str
    smtp_port: int
    smtp_user: str
    smtp_password: str
    smtp_from: str
    dry_run_email: bool


class Store:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as db:
            db.executescript(SCHEMA)

    def connect(self) -> sqlite3.Connection:
        db = sqlite3.connect(self.path)
        db.row_factory = sqlite3.Row
        return db

    def issue_license(
        self,
        *,
        buyer_name: str,
        buyer_email: str,
        days: int,
        payment_provider: str = "",
        payment_id: str = "",
        note: str = "",
    ) -> sqlite3.Row:
        expires_on = today_utc() + timedelta(days=days)
        for _ in range(20):
            key = generate_legacy_supporter_key(expires_on)
            try:
                with self.connect() as db:
                    cur = db.execute(
                        """
                        INSERT INTO licenses
                          (license_key, key_hash, buyer_name, buyer_email, issued_at,
                           expires_on, days, payment_provider, payment_id, note)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            key,
                            hash_value(normalize_key(key)),
                            buyer_name,
                            buyer_email,
                            utc_now(),
                            expires_on.isoformat(),
                            days,
                            payment_provider,
                            payment_id,
                            note,
                        ),
                    )
                    return db.execute("SELECT * FROM licenses WHERE id = ?", (cur.lastrowid,)).fetchone()
            except sqlite3.IntegrityError:
                continue
        raise RuntimeError("could not generate unique license key")

    def find_by_key(self, license_key: str) -> sqlite3.Row | None:
        key_hash = hash_value(normalize_key(license_key))
        with self.connect() as db:
            return db.execute("SELECT * FROM licenses WHERE key_hash = ?", (key_hash,)).fetchone()

    def activate(
        self,
        *,
        license_key: str,
        device_id: str,
        activation_secret: str,
        remote_addr: str,
        user_agent: str,
    ) -> tuple[str, sqlite3.Row | None]:
        row = self.find_by_key(license_key)
        if row is None:
            return "not_found", None
        if row["status"] == "revoked" or row["revoked_at"]:
            return "revoked", row
        if date.fromisoformat(row["expires_on"]) < today_utc():
            return "expired", row

        device_hash = hmac_digest(activation_secret, device_id.strip())
        with self.connect() as db:
            current = db.execute("SELECT * FROM licenses WHERE id = ?", (row["id"],)).fetchone()
            if current["device_hash"] and current["device_hash"] != device_hash:
                return "already_bound", current
            if not current["device_hash"]:
                now = utc_now()
                db.execute(
                    "UPDATE licenses SET device_hash = ?, activated_at = ?, status = 'active' WHERE id = ?",
                    (device_hash, now, current["id"]),
                )
                db.execute(
                    """
                    INSERT INTO activations (license_id, device_hash, activated_at, remote_addr, user_agent)
                    VALUES (?, ?, ?, ?, ?)
                    """,
                    (current["id"], device_hash, now, remote_addr, user_agent),
                )
            return "active", db.execute("SELECT * FROM licenses WHERE id = ?", (row["id"],)).fetchone()

    def revoke(self, license_key: str, note: str = "") -> sqlite3.Row | None:
        row = self.find_by_key(license_key)
        if row is None:
            return None
        with self.connect() as db:
            db.execute(
                "UPDATE licenses SET status = 'revoked', revoked_at = ?, note = ? WHERE id = ?",
                (utc_now(), note or row["note"], row["id"]),
            )
            return db.execute("SELECT * FROM licenses WHERE id = ?", (row["id"],)).fetchone()

    def list_licenses(self, limit: int = 100) -> list[sqlite3.Row]:
        with self.connect() as db:
            return list(db.execute("SELECT * FROM licenses ORDER BY id DESC LIMIT ?", (limit,)))

    def store_webhook(self, provider: str, payload: dict[str, Any], verified: bool = False) -> None:
        event_id = str(payload.get("id") or payload.get("event_id") or "")
        with self.connect() as db:
            db.execute(
                """
                INSERT INTO webhook_events (provider, event_id, received_at, verified, payload)
                VALUES (?, ?, ?, ?, ?)
                """,
                (provider, event_id, utc_now(), 1 if verified else 0, json.dumps(payload, ensure_ascii=False)),
            )


def license_public(row: sqlite3.Row, include_key: bool = False) -> dict[str, Any]:
    data = {
        "id": row["id"],
        "buyerName": row["buyer_name"],
        "buyerEmail": row["buyer_email"],
        "status": row["status"],
        "issuedAt": row["issued_at"],
        "expiresOn": row["expires_on"],
        "days": row["days"],
        "activated": bool(row["device_hash"]),
        "activatedAt": row["activated_at"],
        "revokedAt": row["revoked_at"],
        "paymentProvider": row["payment_provider"],
        "paymentId": row["payment_id"],
        "note": row["note"],
    }
    if include_key:
        data["licenseKey"] = row["license_key"]
    return data


def email_body(row: sqlite3.Row) -> str:
    name = row["buyer_name"] or "Supporter"
    return (
        f"Hallo {name},\n\n"
        "vielen Dank fuer deine Unterstuetzung!\n\n"
        "Dein persoenlicher HB-RF-ETH-ng Supporter-Key:\n\n"
        f"    {row['license_key']}\n\n"
        f"Gueltig bis: {row['expires_on']}\n\n"
        "Aktivierung aktuell in der WebUI:\n"
        "  Einstellungen -> Lizenz -> Supporter-Key -> Aktivieren\n\n"
        "Bitte gib den Key nicht weiter. Geteilte Keys koennen widerrufen werden.\n\n"
        "Basti / Xerolux\n"
    )


def send_license_email(config: Config, row: sqlite3.Row) -> dict[str, Any]:
    if not row["buyer_email"]:
        return {"sent": False, "reason": "missing buyerEmail"}
    if config.dry_run_email or not config.smtp_host:
        return {"sent": False, "dryRun": True, "body": email_body(row)}

    msg = EmailMessage()
    msg["From"] = config.smtp_from or config.smtp_user
    msg["To"] = row["buyer_email"]
    msg["Subject"] = "Dein HB-RF-ETH-ng Supporter-Key"
    msg.set_content(email_body(row))

    with smtplib.SMTP(config.smtp_host, config.smtp_port, timeout=20) as smtp:
        smtp.starttls()
        if config.smtp_user:
            smtp.login(config.smtp_user, config.smtp_password)
        smtp.send_message(msg)
    return {"sent": True}


class ActivationHandler(BaseHTTPRequestHandler):
    server_version = "HB-RF-ETH-ng-Activation/0.1"

    @property
    def app(self) -> "ActivationHTTPServer":
        return self.server  # type: ignore[return-value]

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def do_GET(self) -> None:
        if self.path == "/health":
            send_json(self, HTTPStatus.OK, {"ok": True, "time": utc_now()})
            return
        if self.path.startswith("/admin/licenses"):
            if not require_admin(self, self.app.config.admin_token):
                return
            rows = self.app.store.list_licenses()
            send_json(self, HTTPStatus.OK, {"ok": True, "licenses": [license_public(r) for r in rows]})
            return
        send_json(self, HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})

    def do_POST(self) -> None:
        try:
            payload = load_json_body(self)
        except Exception:
            send_json(self, HTTPStatus.BAD_REQUEST, {"ok": False, "error": "invalid_json"})
            return

        if self.path == "/admin/licenses":
            if not require_admin(self, self.app.config.admin_token):
                return
            try:
                days = int(payload.get("days") or 365)
                if days < 1:
                    raise ValueError("days must be positive")
                row = self.app.store.issue_license(
                    buyer_name=str(payload.get("buyerName") or ""),
                    buyer_email=str(payload.get("buyerEmail") or ""),
                    days=days,
                    payment_provider=str(payload.get("paymentProvider") or ""),
                    payment_id=str(payload.get("paymentId") or ""),
                    note=str(payload.get("note") or ""),
                )
                mail = send_license_email(self.app.config, row)
                send_json(self, HTTPStatus.CREATED, {"ok": True, "license": license_public(row, include_key=True), "email": mail})
            except Exception as exc:
                send_json(self, HTTPStatus.BAD_REQUEST, {"ok": False, "error": str(exc)})
            return

        if self.path == "/admin/revoke":
            if not require_admin(self, self.app.config.admin_token):
                return
            row = self.app.store.revoke(str(payload.get("licenseKey") or ""), str(payload.get("note") or ""))
            if row is None:
                send_json(self, HTTPStatus.NOT_FOUND, {"ok": False, "error": "license_not_found"})
            else:
                send_json(self, HTTPStatus.OK, {"ok": True, "license": license_public(row)})
            return

        if self.path == "/activate":
            license_key = str(payload.get("licenseKey") or "")
            device_id = str(payload.get("deviceId") or "")
            if not license_key or not device_id:
                send_json(self, HTTPStatus.BAD_REQUEST, {"ok": False, "error": "licenseKey_and_deviceId_required"})
                return
            status, row = self.app.store.activate(
                license_key=license_key,
                device_id=device_id,
                activation_secret=self.app.config.activation_secret,
                remote_addr=self.client_address[0],
                user_agent=self.headers.get("User-Agent", ""),
            )
            if row is None:
                send_json(self, HTTPStatus.NOT_FOUND, {"ok": False, "status": status})
            else:
                send_json(self, HTTPStatus.OK, {"ok": status == "active", "status": status, "license": license_public(row)})
            return

        if self.path == "/webhooks/paypal":
            # Prototype only: production must verify the PayPal webhook
            # signature against PayPal before fulfilling an order.
            self.app.store.store_webhook("paypal", payload, verified=False)
            send_json(self, HTTPStatus.ACCEPTED, {"ok": True, "stored": True, "verified": False})
            return

        send_json(self, HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})


class ActivationHTTPServer(ThreadingHTTPServer):
    def __init__(self, address: tuple[str, int], config: Config) -> None:
        super().__init__(address, ActivationHandler)
        self.config = config
        self.store = Store(config.db)


def load_config(argv: list[str] | None = None) -> Config:
    parser = argparse.ArgumentParser(description="HB-RF-ETH-ng activation server prototype")
    parser.add_argument("--host", default=os.getenv("HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("PORT", "8088")))
    parser.add_argument("--db", type=Path, default=Path(os.getenv("DB_PATH", str(DEFAULT_DB))))
    args = parser.parse_args(argv)

    admin_token = os.getenv("ADMIN_TOKEN", "")
    activation_secret = os.getenv("ACTIVATION_SECRET", "")
    if not admin_token:
        raise SystemExit("ADMIN_TOKEN is required")
    if not activation_secret:
        raise SystemExit("ACTIVATION_SECRET is required")

    return Config(
        db=args.db,
        host=args.host,
        port=args.port,
        admin_token=admin_token,
        activation_secret=activation_secret,
        public_base_url=os.getenv("PUBLIC_BASE_URL", ""),
        smtp_host=os.getenv("SMTP_HOST", ""),
        smtp_port=int(os.getenv("SMTP_PORT", "587")),
        smtp_user=os.getenv("SMTP_USER", ""),
        smtp_password=os.getenv("SMTP_PASSWORD", ""),
        smtp_from=os.getenv("SMTP_FROM", os.getenv("SMTP_USER", "")),
        dry_run_email=os.getenv("DRY_RUN_EMAIL", "1") != "0",
    )


def main(argv: list[str] | None = None) -> int:
    config = load_config(argv)
    httpd = ActivationHTTPServer((config.host, config.port), config)
    print(f"activation server listening on http://{config.host}:{config.port}")
    print(f"database: {config.db}")
    if config.dry_run_email:
        print("email: dry-run mode")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
