#include "camera.h"
#include "esp_log.h"
#include "led.h"
#include "log.h"
#include "nvs_flash.h"
#include "settings.h"
#include "version.h"
#include "wifi.h"

static const char *TAG = "printspy_cam";

void app_main(void) {
  // First, so the live log console can show everything from boot onward.
  printspy_log_init();

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

  // WiFi runs as its own task - AP fallback mode blocks forever until the
  // device reboots, so it can't live on app_main's own stack/task.
  xTaskCreateStatic(wifi_task_run, "wifi_task", WIFI_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, wifiTaskStack, &wifiTaskBuffer);
}
