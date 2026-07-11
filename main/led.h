#pragma once

#include "esp_err.h"
#include <stdint.h>

// Onboard flash LED, if the selected board has one (CAM_FLASH_LED_PIN in
// its board header) - PWM brightness via LEDC, not the RMT/WS2812B status
// LED removed earlier (that was a different, addressable-RGB indicator).
// No-op on boards without CAM_FLASH_LED_PIN defined.

esp_err_t printspy_led_init(void);

// 0-100, 0 = off. Persists to NVS and, if the board has the LED, applies
// it immediately.
esp_err_t printspy_led_set_brightness(uint8_t percent);
uint8_t printspy_led_get_brightness(void);
