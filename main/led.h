#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
  PRINTSPY_LED_WIFI_AP_MODE,   // slow blue blink - waiting for setup
  PRINTSPY_LED_WIFI_CONNECTING, // fast amber blink - joining a network
  PRINTSPY_LED_WIFI_CONNECTED, // brief green flash, then off
} printspy_led_wifi_state_t;

esp_err_t printspy_led_init(void);

// brightness: 0-255. Manual override (e.g. for use as a camera flash) -
// takes effect immediately, independent of the WiFi status pattern below.
esp_err_t printspy_led_set_brightness(uint8_t brightness);

// Drives the onboard status LED through a state-specific blink pattern
// via a periodic timer. Boards without an addressable LED
// (LED_IS_NEOPIXEL == 0) don't yet implement this - see led.c.
esp_err_t printspy_led_set_wifi_state(printspy_led_wifi_state_t state);
