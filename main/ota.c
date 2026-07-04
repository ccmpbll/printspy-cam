#include "ota.h"

#include "esp_log.h"

static const char *TAG = "printspy_ota";

// TODO: real implementation - port from bitclock-redux main/libs/ota.c
esp_err_t printspy_ota_write_chunk(const uint8_t *data, size_t len) {
  ESP_LOGI(TAG, "ota_write_chunk stub - not yet implemented (%d bytes)",
           (int)len);
  return ESP_OK;
}

esp_err_t printspy_ota_finish(void) {
  ESP_LOGI(TAG, "ota_finish stub - not yet implemented");
  return ESP_OK;
}
