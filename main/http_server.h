#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Serves /, /snapshot, /stream, /api/status, /api/logs (SSE), /api/wifi
// (POST), /api/settings (GET/POST), /api/ota (POST). Idempotent - safe to
// call again on WiFi reconnect (only starts the server once).
//
// /stream and /api/logs each run on their own small async worker pool
// (STREAM_WORKER_COUNT / LOG_WORKER_COUNT in http_server.c) rather than
// directly on the httpd task, since esp_http_server services all
// connections from one task by default - a blocking handler like these
// would otherwise stall every other request until the client disconnected.
esp_err_t printspy_http_server_start(void);
