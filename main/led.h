#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t printspy_led_init(void);

// brightness: 0-255. Handles the Freenove board's WS2812B NeoPixel
// (LED_IS_NEOPIXEL) vs a plain PWM-driven LED on other boards.
esp_err_t printspy_led_set_brightness(uint8_t brightness);
