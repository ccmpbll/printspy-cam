#include "led.h"

#include "boards/board.h"
#include "esp_log.h"

static const char *TAG = "printspy_led";

// TODO: real implementation - LEDC PWM for plain LEDs, RMT/led_strip driver
// for the Freenove board's onboard WS2812B (LED_IS_NEOPIXEL).
esp_err_t printspy_led_init(void) {
  ESP_LOGI(TAG, "led_init stub - not yet implemented (gpio %d, neopixel %d)",
           LED_GPIO_NUM, LED_IS_NEOPIXEL);
  return ESP_OK;
}

esp_err_t printspy_led_set_brightness(uint8_t brightness) {
  ESP_LOGI(TAG, "led_set_brightness stub - not yet implemented (%d)",
           brightness);
  return ESP_OK;
}
