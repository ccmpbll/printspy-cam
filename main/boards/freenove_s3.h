#pragma once

// Freenove ESP32-S3 CAM (actual sensor on hand: OV3660, not OV2640 -
// esp32-camera auto-detects the sensor chip so this doesn't matter).
// Pin mapping cross-checked against Espressif's own official
// BOARD_ESP32S3_WROOM pinout (esp32-camera's
// examples/camera_example/main/camera_pinout.h) - identical XCLK/SIOD/
// SIOC/VSYNC/HREF/PCLK confirm it's the same board family. D5/D7 were
// transposed here in an earlier transcription from
// prusa3d/Prusa-Firmware-ESP32-Cam's pin header - real hardware showed
// constant "NO-SOI - JPEG start marker missing" (two data-bus lines
// crossed scrambles every captured byte) until this was caught.

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
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
