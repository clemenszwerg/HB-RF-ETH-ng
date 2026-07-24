#!/usr/bin/env python3
"""Update the independent HB-RF-ETH-ng WebUI release metadata.

Usage:
    python update_webui_version.py <webui-version> \
        [--min-firmware <firmware-version>] [--api-version <integer>]
"""

import argparse
import json
import re
from pathlib import Path

SEMVER = re.compile(r"^\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?$")


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def save_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def semver_key(value: str) -> tuple[int, int, int, int, str, int]:
    match = re.fullmatch(
        r"(\d+)\.(\d+)\.(\d+)(?:-([A-Za-z]+)\.(\d+))?", value
    )
    if not match:
        raise SystemExit(f"Invalid semantic version: {value}")
    major, minor, patch, label, number = match.groups()
    return (
        int(major),
        int(minor),
        int(patch),
        1 if label is None else 0,
        (label or "").lower(),
        int(number or 0),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument("--min-firmware")
    parser.add_argument("--api-version", type=int)
    args = parser.parse_args()

    version = args.version.removeprefix("webui-v").removeprefix("v")
    if not SEMVER.fullmatch(version):
        raise SystemExit(f"Invalid WebUI version: {version}")

    package_path = Path("webui/package.json")
    lock_path = Path("webui/package-lock.json")
    compatibility_path = Path("webui/compatibility.json")
    firmware_contract_path = Path("main/webui_api_contract.json")

    package = load_json(package_path)
    package["version"] = version

    lock = load_json(lock_path)
    lock["version"] = version
    lock.setdefault("packages", {}).setdefault("", {})["version"] = version

    compatibility = load_json(compatibility_path)
    if args.min_firmware:
        minimum = args.min_firmware.removeprefix("v")
        if not SEMVER.fullmatch(minimum):
            raise SystemExit(f"Invalid minimum firmware version: {minimum}")
        compatibility["minFirmwareVersion"] = minimum
    if args.api_version is not None:
        if args.api_version < 1:
            raise SystemExit("API version must be at least 1")
        compatibility["apiVersion"] = args.api_version

    firmware_contract = load_json(firmware_contract_path)
    supported_api = firmware_contract.get("supportedApiVersion")
    if compatibility.get("apiVersion") != supported_api:
        raise SystemExit(
            f"WebUI requires API {compatibility.get('apiVersion')}, but "
            f"firmware implements API {supported_api}. Update the firmware "
            "contract and implementation first."
        )

    current_firmware = Path("version.txt").read_text(encoding="utf-8").strip()
    if semver_key(current_firmware) < semver_key(
        str(compatibility.get("minFirmwareVersion", ""))
    ):
        raise SystemExit(
            f"WebUI requires firmware >="
            f"{compatibility['minFirmwareVersion']}, but the current firmware "
            f"is {current_firmware}."
        )

    # Write only after the complete compatibility contract has passed so a
    # rejected release request cannot leave partially updated metadata behind.
    save_json(package_path, package)
    save_json(lock_path, lock)
    save_json(compatibility_path, compatibility)

    print(
        f"WebUI updated to {version}; requires firmware "
        f">={compatibility['minFirmwareVersion']} and API "
        f"{compatibility['apiVersion']}"
    )


if __name__ == "__main__":
    main()
