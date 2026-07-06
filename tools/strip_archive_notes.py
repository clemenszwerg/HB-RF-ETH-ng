#!/usr/bin/env python3
"""One-off: strip the heavy inline `notes` field from every archive.json
entry, keeping (or generating) a short `notesExcerpt`. The full release notes
are now loaded on-demand from GitHub when the user clicks — see the firmware
page's changelog button.

Run from the repo root:  python tools/strip_archive_notes.py
"""
import json, re, sys, pathlib

ARCHIVE = pathlib.Path("archive.json")
EXCERPT_LIMIT = 300

def excerpt(text, limit=EXCERPT_LIMIT):
    if not text:
        return ""
    flat = re.sub(r"\s+", " ", text).strip()
    if len(flat) <= limit:
        return flat
    return flat[:limit].rstrip() + "…"

def main():
    raw = ARCHIVE.read_text(encoding="utf-8")
    data = json.loads(raw)
    releases = data.get("releases", [])
    stripped = 0
    generated = 0
    for r in releases:
        notes = r.get("notes")
        excerpt_val = r.get("notesExcerpt")
        if notes:
            stripped += 1
            if not excerpt_val:
                r["notesExcerpt"] = excerpt(notes)
                generated += 1
            del r["notes"]
        elif not excerpt_val and r.get("body"):
            r["notesExcerpt"] = excerpt(r.get("body"))
            generated += 1
        r.pop("body", None)
    data["updatedAt"] = data.get("updatedAt")
    out = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    ARCHIVE.write_text(out, encoding="utf-8")
    print(f"stripped notes from {stripped} entries, generated {generated} excerpts")
    print(f"new size: {len(out.encode('utf-8'))} bytes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
