#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// TODO: port from bitclock-redux main/libs/ota.c
esp_err_t printspy_ota_write_chunk(const uint8_t *data, size_t len);
esp_err_t printspy_ota_finish(void);
