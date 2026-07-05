#pragma once

// Selects the active board's pin header based on the Kconfig board choice
// (main/Kconfig.projbuild). Each board header defines the same set of
// CAM_PIN_* macros.

#if CONFIG_BOARD_FREENOVE_S3
#include "freenove_s3.h"
#elif CONFIG_BOARD_XIAO_S3_SENSE
#include "xiao_s3_sense.h"
#elif CONFIG_BOARD_ESP32_S3_EYE
#include "esp32_s3_eye.h"
#else
#error "No board selected - run `idf.py menuconfig` under PrintSpy Cam Configuration"
#endif
