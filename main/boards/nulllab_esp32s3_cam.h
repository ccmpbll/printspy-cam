#pragma once

// nulllaborg/esp32s3-cam (github.com/nulllaborg/esp32s3-cam) - pins
// verified against the board's schematic (esp32s3_cam_sch.pdf, U3/U7),
// not the README/AI-summarized pin tables, which disagree with each
// other and with the real board. XCLK/D7 ride the XTAL_32K_P/N
// dual-function pins - board has no 32kHz crystal fitted, so GPIO47/48
// are free for camera use. Sold sensor-agnostic (OV2640 or OV3660);
// esp32-camera auto-detects the sensor chip, no header changes needed
// either way.

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1  // CAM_RST hardwired pulled high via R11, no GPIO
#define CAM_PIN_XCLK 47
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 48
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
