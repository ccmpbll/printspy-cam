#pragma once

// Seeed XIAO ESP32-S3 Sense (OV2640).
// TODO: verify against Seeed's official pinout docs / sample sketches
// before relying on these - not yet confirmed on real hardware.

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 10
#define CAM_PIN_SIOD 40
#define CAM_PIN_SIOC 39

#define CAM_PIN_D7 48
#define CAM_PIN_D6 11
#define CAM_PIN_D5 12
#define CAM_PIN_D4 14
#define CAM_PIN_D3 16
#define CAM_PIN_D2 18
#define CAM_PIN_D1 17
#define CAM_PIN_D0 15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF 47
#define CAM_PIN_PCLK 13

#define LED_GPIO_NUM 21 // onboard user LED, not a flash LED - no dedicated flash on this board
#define LED_IS_NEOPIXEL 0

#define BOARD_HAS_PSRAM 1
