#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ping the target host once.
// target: IP address or hostname
// timeout_ms: ping timeout in milliseconds
// returns: latency in ms, or -1 if unreachable/error.
int ping_service_ping(const char* target, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
