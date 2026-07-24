# Firmware and WebUI release versioning

HB-RF-ETH-ng uses two independent semantic-version release lines.

| Component | Version source | Tag example | Release asset |
|---|---|---|---|
| Firmware | `version.txt` | `v2.2.5-Beta.2` | `firmware_2.2.5-Beta.2.bin` |
| New Design WebUI | `webui/package.json` | `webui-v1.0.0-Beta.1` | `webui_1.0.0-Beta.1.bin` |

A firmware version change must not modify the WebUI version. A WebUI version
change must not modify `version.txt` or advertise a new firmware update.

## Full release

Use `.github/workflows/release.yml` when firmware changed or when publishing the
first migration-capable release.

The release contains:

- firmware image
- current compatible WebUI image
- bootloader and partition table
- SHA-256 checksums
- separate firmware and WebUI version files
- WebUI compatibility metadata

The repository's `latest.json` or `beta.json` firmware fields and `webui` block
are updated together. A stable full release may become GitHub's Latest release.

The WebUI image keeps its own version. Reusing an unchanged WebUI in a newer
firmware release does not create a fake WebUI version bump.

## WebUI-only release

Use `.github/workflows/release-webui.yml` for changes limited to Vue, CSS,
translations, themes, layout, or other browser-side behavior that uses the
existing firmware API.

The workflow performs a complete ESP-IDF build as a compatibility and size
check, but publishes only:

- `webui_<version>.bin`
- SHA-256 checksum
- WebUI version
- compatibility metadata

The tag is `webui-v<version>`. The release is never marked as GitHub's Latest
firmware release. Existing firmware fields in `latest.json` or `beta.json` are
preserved at the logical JSON-field level; only the `webui` block is replaced.

## Channels

- Stable full release: updates `latest.json` and `beta.json`.
- Beta full release: updates `beta.json` only.
- Stable WebUI-only release: replaces only the `webui` block in both manifests.
- Beta WebUI-only release: replaces only the `webui` block in `beta.json`.

## Compatibility contract

Compatibility has two independent, authoritative sources:

- `main/webui_api_contract.json` defines `supportedApiVersion`, the exact WebUI
  API implemented by the firmware.
- `webui/compatibility.json` defines the WebUI's required `apiVersion` and
  `minFirmwareVersion`.

The contract is satisfied only when both conditions are true:

1. `apiVersion == supportedApiVersion` (exact match).
2. Running firmware `>= minFirmwareVersion` (semantic-version comparison,
   including prerelease identifiers).

`apiVersion` and `minFirmwareVersion` are embedded into
`webui-manifest.json` inside `spiffs.bin` and published in the update manifest.
The browser sends the release contract as preflight headers before a normal
upload, allowing the ESP32 to reject an incompatible release without erasing
the currently installed WebUI. The internal image manifest remains the
authoritative value and is checked again after writing.

At every boot and after every WebUI upload, the firmware validates image size,
SHA-256 when supplied, product, design, image format, required assets, API
version and minimum firmware. An incompatible external WebUI is never served.
The firmware uses its embedded WebUI fallback and the fallback displays a
persistent compatibility warning with a link to the WebUI repair page.

The manual upload remains an expert/recovery path. Use only the image and
compatibility metadata from the matching GitHub release. A file that cannot be
matched to the currently advertised release has no preflight metadata, so the
ESP32 can validate its internal contract only after writing it. An incompatible
manual image is invalidated and the embedded fallback stays active.

### Mandatory change rules

- Increment both the firmware's `supportedApiVersion` and the WebUI's
  `apiVersion` for every incompatible REST, JSON, authentication, routing or
  behavioral contract change. Ship such a change as a full firmware release.
- Compatible API additions keep the current API number. Raise
  `minFirmwareVersion` if the WebUI begins to depend on that addition.
- A WebUI-only release must never change `apiVersion` unless support for that
  API already exists in the released/current firmware line.
- Never remove `apiVersion` or `minFirmwareVersion` from the standalone image
  manifest or update manifests.
- Never weaken the firmware-side boot and upload checks to a browser-only
  warning. The backend is the security and recovery boundary.

The WebUI packaging script, firmware CMake configuration and release workflows
fail when the two API contracts disagree or the current firmware is older than
the declared minimum. This is a release gate, not advisory documentation.

## Initial independent versions

- Firmware line: `2.2.x`
- New Design WebUI line: `1.0.0-Beta.1`
- WebUI API: `1`
