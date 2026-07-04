#pragma once

// Freenove ESP32-S3 CAM (OV2640). Pin mapping confirmed against
// prusa3d/Prusa-Firmware-ESP32-Cam's module_ESP32-S3_Wroom_Freenove.h -
// same physical board, pins are hardware-fixed regardless of firmware.

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 18
#define CAM_PIN_D6 17
#define CAM_PIN_D5 16
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

#define LED_GPIO_NUM 48 // onboard WS2812B, not a plain PWM LED - led.c must special-case this board
#define LED_IS_NEOPIXEL 1

#define BOARD_HAS_PSRAM 1
