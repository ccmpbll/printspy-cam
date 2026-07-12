#pragma once

#include "esp_err.h"

// ESP32-S3's built-in on-die temperature sensor - no extra hardware, this
// board has no dedicated temp sensor chip (checked the schematic).

esp_err_t printspy_chip_temp_init(void);

// Celsius. Returns 0.0f if the sensor failed to install/enable.
float printspy_chip_temp_read(void);
