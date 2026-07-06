/*
 *  supporter_crl.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */

#pragma once

#include <stdbool.h>

// Revocation list (CRL) for supporter keys. Lets the maintainer invalidate a
// key before its embedded expiry — e.g. after a refund or when a key was
// shared publicly. The list is fetched from the repo's revoked_keys.json
// (which contains ONLY 128-bit SHA-256 fingerprints of the keys, never the
// keys themselves, so publishing it reveals nothing) and cached locally so
// revocation also works offline and right after boot.

// Loads the last cached revocation list from NVS and starts a background
// refresh task (~6 h interval). Call once at boot.
void supporter_crl_init(void);

// Re-fetches revoked_keys.json from GitHub and refreshes the in-RAM + NVS
// cache. Network call — serialised on g_net_fetch_mutex like every other
// outbound TLS fetch. Returns true on success. Safe to call manually too.
bool supporter_crl_refresh(void);

// True when the given key's fingerprint is on the cached revocation list.
// Pure RAM check, no network: SHA-256(normalised key)[:16 bytes] is compared
// against the cached 128-bit fingerprints. Fast — mbedTLS uses the ESP32's
// hardware SHA accelerator.
bool supporter_crl_is_revoked(const char *key);
