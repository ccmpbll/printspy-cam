#include "http_server.h"

#include "esp_log.h"

static const char *TAG = "printspy_http";

// TODO: real implementation - esp_http_server with /snapshot, /stream
// (multipart/x-mixed-replace), /, /api/status, /api/settings, /api/wifi,
// /api/ota handlers. See notes/printspy-cam.md "MJPEG streaming
// implementation" for the frame loop shape.
esp_err_t printspy_http_server_start(void) {
  ESP_LOGI(TAG, "http_server_start stub - not yet implemented");
  return ESP_OK;
}
