#include "camera.h"

#include "esp_log.h"

static const char *TAG = "printspy_camera";

// TODO: real implementation - build camera_config_t from boards/board.h
// CAM_PIN_* macros, esp_camera_init(), wire NVS resolution/quality settings.
esp_err_t printspy_camera_init(void) {
  ESP_LOGI(TAG, "camera_init stub - not yet implemented");
  return ESP_OK;
}

camera_fb_t *printspy_camera_capture(void) {
  ESP_LOGW(TAG, "camera_capture stub - not yet implemented");
  return NULL;
}
