#include "wifi.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "printspy_wifi";

// TODO: real implementation - port from bitclock-redux main/tasks/wifi.c
esp_err_t printspy_wifi_init_sta(void) {
  ESP_LOGI(TAG, "wifi_init_sta stub - not yet implemented");
  return ESP_OK;
}

bool printspy_wifi_has_ip(void) { return false; }

bool printspy_wifi_has_credentials(void) {
  return printspy_nvs_get_wifi_ssid() != NULL;
}
