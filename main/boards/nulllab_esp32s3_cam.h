#pragma once

// nulllaborg/esp32s3-cam (github.com/nulllaborg/esp32s3-cam) - pins
// verified against the vendor's own README pin table AND their shipped
// MicroPython driver's default pin config (both agree, independently).
// An earlier pass here misread the schematic's XTAL_32K_P/N chip labels
// as GPIO47/48 - real hardware is GPIO15/16 (matches Freenove's pin
// layout exactly). That bug meant XCLK never reached the sensor, so it
// never woke up its SCCB logic - looked identical to a dead board/sensor
// across two separate units until this was caught. Sold sensor-agnostic
// (OV2640 or OV3660); esp32-camera auto-detects the sensor chip, no
// header changes needed either way.

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1  // CAM_RST hardwired pulled high via R11, no GPIO
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

// Onboard white flash LEDs (pair, driven together through Q1/Q2 per
// schematic). Left unconfigured, this pin floats - explains why two
// otherwise-identical boards showed different idle LED brightness
// out of the box. led.c drives it via LEDC PWM and sets it to the
// persisted (default off) brightness at boot instead of leaving it
// floating.
#define CAM_FLASH_LED_PIN 3
