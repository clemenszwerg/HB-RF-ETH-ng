#pragma once

#include <ctype.h>
#include <stddef.h>

#include "semver.h"

enum WebUICompatibilityResult
{
    WEBUI_COMPATIBILITY_COMPATIBLE = 0,
    WEBUI_COMPATIBILITY_INVALID_METADATA,
    WEBUI_COMPATIBILITY_API_MISMATCH,
    WEBUI_COMPATIBILITY_FIRMWARE_TOO_OLD,
};

/**
 * Validate the semantic-version subset used by the firmware and WebUI
 * release pipelines: major.minor.patch with an optional dot-separated
 * pre-release suffix.
 */
static inline bool webui_is_valid_semver(const char *version)
{
    if (!version || !version[0]) return false;

    const char *cursor = version;
    for (int component = 0; component < 3; ++component)
    {
        if (!isdigit(static_cast<unsigned char>(*cursor))) return false;
        while (isdigit(static_cast<unsigned char>(*cursor))) ++cursor;
        if (component < 2)
        {
            if (*cursor != '.') return false;
            ++cursor;
        }
    }

    if (*cursor == '\0') return true;
    if (*cursor != '-') return false;
    ++cursor;
    if (!*cursor) return false;

    bool segment_has_character = false;
    for (; *cursor; ++cursor)
    {
        const unsigned char value = static_cast<unsigned char>(*cursor);
        if (*cursor == '.')
        {
            if (!segment_has_character) return false;
            segment_has_character = false;
            continue;
        }
        if (!isalnum(value) && *cursor != '-') return false;
        segment_has_character = true;
    }
    return segment_has_character;
}

/**
 * Evaluate an external WebUI image against the API implemented by the running
 * firmware. API versions are exact contracts; minFirmwareVersion is a lower
 * bound for compatible additions within the same API version.
 */
static inline WebUICompatibilityResult webui_check_compatibility(
    int image_api_version,
    const char *minimum_firmware_version,
    int supported_api_version,
    const char *current_firmware_version)
{
    if (image_api_version < 1 || supported_api_version < 1 ||
        !webui_is_valid_semver(minimum_firmware_version) ||
        !webui_is_valid_semver(current_firmware_version))
    {
        return WEBUI_COMPATIBILITY_INVALID_METADATA;
    }
    if (image_api_version != supported_api_version)
    {
        return WEBUI_COMPATIBILITY_API_MISMATCH;
    }
    if (compareVersions(current_firmware_version, minimum_firmware_version) < 0)
    {
        return WEBUI_COMPATIBILITY_FIRMWARE_TOO_OLD;
    }
    return WEBUI_COMPATIBILITY_COMPATIBLE;
}

static inline const char *webui_compatibility_status_name(
    WebUICompatibilityResult result)
{
    switch (result)
    {
        case WEBUI_COMPATIBILITY_COMPATIBLE:
            return "compatible";
        case WEBUI_COMPATIBILITY_API_MISMATCH:
            return "api_mismatch";
        case WEBUI_COMPATIBILITY_FIRMWARE_TOO_OLD:
            return "firmware_too_old";
        case WEBUI_COMPATIBILITY_INVALID_METADATA:
        default:
            return "invalid_metadata";
    }
}
