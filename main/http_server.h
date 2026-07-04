#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Serves /, /snapshot, /stream, /api/status. Idempotent - safe to call
// again on WiFi reconnect (only starts the server once).
//
// /api/settings, /api/wifi, /api/ota (config changes, not yet
// implemented) are still TODO - see notes/printspy-cam.md "HTTP
// endpoints" for the full planned list.
esp_err_t printspy_http_server_start(void);
