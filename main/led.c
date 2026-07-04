#include "led.h"

#include "boards/board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include <stdbool.h>

static const char *TAG = "printspy_led";

static led_strip_handle_t strip = NULL;
static esp_timer_handle_t blink_timer = NULL;
static printspy_led_wifi_state_t wifi_state = PRINTSPY_LED_WIFI_AP_MODE;
static bool blink_on = false;

#define BLINK_INTERVAL_AP_MODE_US (500 * 1000)
#define BLINK_INTERVAL_CONNECTING_US (150 * 1000)

static void apply_color(uint8_t r, uint8_t g, uint8_t b) {
  if (!strip) {
    return;
  }
  led_strip_set_pixel(strip, 0, r, g, b);
  led_strip_refresh(strip);
}

static void blink_tick(void *arg) {
  switch (wifi_state) {
  case PRINTSPY_LED_WIFI_AP_MODE:
    blink_on = !blink_on;
    apply_color(0, 0, blink_on ? 40 : 0);
    break;
  case PRINTSPY_LED_WIFI_CONNECTING:
    blink_on = !blink_on;
    apply_color(blink_on ? 40 : 0, blink_on ? 25 : 0, 0);
    break;
  case PRINTSPY_LED_WIFI_CONNECTED:
    // Fires once via esp_timer_start_once (see printspy_led_set_wifi_state)
    // - turns the brief connect flash back off without blocking the
    // caller (which runs on the shared system event-loop task).
    apply_color(0, 0, 0);
    break;
  }
}

esp_err_t printspy_led_init(void) {
#if LED_IS_NEOPIXEL
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_GPIO_NUM,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags.invert_out = false,
  };
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .flags.with_dma = false,
  };
  esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
    return err;
  }
  led_strip_clear(strip);

  const esp_timer_create_args_t timer_args = {
      .callback = &blink_tick,
      .name = "printspy_led_blink",
  };
  err = esp_timer_create(&timer_args, &blink_timer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
    return err;
  }
  esp_timer_start_periodic(blink_timer, BLINK_INTERVAL_AP_MODE_US);

  ESP_LOGI(TAG, "LED initialized (gpio %d, NeoPixel)", LED_GPIO_NUM);
  return ESP_OK;
#else
  // TODO: plain PWM LED via LEDC for boards without an addressable LED -
  // no such board has real hardware to test against yet (see
  // notes/printspy-cam.md board table).
  ESP_LOGW(TAG, "LED control not implemented for this board (no NeoPixel)");
  return ESP_OK;
#endif
}

esp_err_t printspy_led_set_brightness(uint8_t brightness) {
  if (brightness > 0) {
    apply_color(brightness, brightness, brightness);
  } else if (strip) {
    led_strip_clear(strip);
  }
  return ESP_OK;
}

esp_err_t printspy_led_set_wifi_state(printspy_led_wifi_state_t state) {
  wifi_state = state;
  blink_on = false;

  if (state == PRINTSPY_LED_WIFI_CONNECTED) {
    if (blink_timer) {
      esp_timer_stop(blink_timer); // stop while running is a no-op error if already stopped, ignored
      apply_color(0, 60, 0);       // brief green flash
      esp_timer_start_once(blink_timer, 400 * 1000); // blink_tick fires once, turns it back off
    }
    return ESP_OK;
  }

  if (!blink_timer) {
    return ESP_OK; // no hardware (non-NeoPixel board) - state tracked but nothing to drive
  }

  esp_timer_stop(blink_timer);
  uint64_t interval = (state == PRINTSPY_LED_WIFI_CONNECTING)
                          ? BLINK_INTERVAL_CONNECTING_US
                          : BLINK_INTERVAL_AP_MODE_US;
  esp_timer_start_periodic(blink_timer, interval);
  return ESP_OK;
}
