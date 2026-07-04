#pragma once

#include "esp_err.h"

// Serves /snapshot, /stream, /, /api/status, /api/settings, /api/wifi,
// /api/ota. See notes/printspy-cam.md "HTTP endpoints" for the full list.
esp_err_t printspy_http_server_start(void);
