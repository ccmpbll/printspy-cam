#include "camera.h"
#include "esp_log.h"
#include "http_server.h"
#include "led.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "version.h"
#include "wifi.h"
#include "wifi_ap.h"

static const char *TAG = "printspy_cam";

void app_main(void) {
  ESP_LOGI(TAG, "PrintSpy Cam %s starting", PRINTSPY_CAM_VERSION);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(printspy_nvs_init());

  ESP_ERROR_CHECK(printspy_led_init());
  ESP_ERROR_CHECK(printspy_camera_init());

  if (printspy_wifi_has_credentials()) {
    ESP_ERROR_CHECK(printspy_wifi_init_sta());
  } else {
    ESP_LOGI(TAG, "no WiFi credentials stored, starting setup AP");
    ESP_ERROR_CHECK(printspy_wifi_ap_start());
  }

  ESP_ERROR_CHECK(printspy_http_server_start());
}
