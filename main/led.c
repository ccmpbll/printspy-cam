#include "led.h"

#include "boards/board.h"
#include "settings.h"

#ifdef CAM_FLASH_LED_PIN
#include "driver/ledc.h"

// Separate timer/channel from camera.c's XCLK (LEDC_TIMER_0/CHANNEL_0) -
// they'd otherwise silently fight over the same peripheral slot.
#define LED_LEDC_MODE LEDC_LOW_SPEED_MODE
#define LED_LEDC_TIMER LEDC_TIMER_1
#define LED_LEDC_CHANNEL LEDC_CHANNEL_1
#define LED_DUTY_RES LEDC_TIMER_8_BIT

static void apply_duty(uint8_t percent) {
  uint32_t duty = (uint32_t)percent * 255 / 100;
  ledc_set_duty(LED_LEDC_MODE, LED_LEDC_CHANNEL, duty);
  ledc_update_duty(LED_LEDC_MODE, LED_LEDC_CHANNEL);
}
#endif

esp_err_t printspy_led_init(void) {
#ifdef CAM_FLASH_LED_PIN
  ledc_timer_config_t timer_conf = {
      .speed_mode = LED_LEDC_MODE,
      .duty_resolution = LED_DUTY_RES,
      .timer_num = LED_LEDC_TIMER,
      .freq_hz = 5000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  esp_err_t err = ledc_timer_config(&timer_conf);
  if (err != ESP_OK) {
    return err;
  }

  ledc_channel_config_t chan_conf = {
      .gpio_num = CAM_FLASH_LED_PIN,
      .speed_mode = LED_LEDC_MODE,
      .channel = LED_LEDC_CHANNEL,
      .timer_sel = LED_LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  err = ledc_channel_config(&chan_conf);
  if (err != ESP_OK) {
    return err;
  }

  // Apply the persisted value (default off) instead of leaving the pin to
  // float - an unconfigured GPIO3 is exactly why two otherwise-identical
  // boards showed different idle LED brightness out of the box.
  apply_duty(printspy_nvs_get_led_brightness());
#endif
  return ESP_OK;
}

esp_err_t printspy_led_set_brightness(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  esp_err_t err = printspy_nvs_set_led_brightness(percent);
  if (err != ESP_OK) {
    return err;
  }
#ifdef CAM_FLASH_LED_PIN
  apply_duty(percent);
#endif
  return ESP_OK;
}

uint8_t printspy_led_get_brightness(void) {
  return printspy_nvs_get_led_brightness();
}
