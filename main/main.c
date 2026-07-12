#include "camera.h"
#include "boards/board.h"
#include "chip_temp.h"
#include "esp_log.h"
#include "log.h"
#include "nvs_flash.h"
#include "settings.h"
#include "version.h"
#include "wifi.h"

#ifdef CAM_FLASH_LED_PIN
#include "driver/gpio.h"
#endif

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
  ESP_ERROR_CHECK(printspy_chip_temp_init());

#ifdef CAM_FLASH_LED_PIN
  // These flash LEDs are too dim to be useful even at full drive (2K
  // resistor + 3.3V rail leaves almost no headroom for a white LED's
  // forward voltage) - forced hard off here rather than left floating
  // (which is what caused two otherwise-identical boards to idle at
  // different brightness in the first place) or exposed as a control.
  gpio_set_direction(CAM_FLASH_LED_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(CAM_FLASH_LED_PIN, 0);
#endif

  ESP_ERROR_CHECK(printspy_camera_init());

  // WiFi runs as its own task - AP fallback mode blocks forever until the
  // device reboots, so it can't live on app_main's own stack/task.
  xTaskCreateStatic(wifi_task_run, "wifi_task", WIFI_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, wifiTaskStack, &wifiTaskBuffer);
}
