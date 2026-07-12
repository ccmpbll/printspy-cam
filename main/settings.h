#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t printspy_nvs_init(void);

uint8_t printspy_nvs_get_resolution(void);
esp_err_t printspy_nvs_set_resolution(uint8_t framesize);

uint8_t printspy_nvs_get_quality(void);
esp_err_t printspy_nvs_set_quality(uint8_t quality);

// Camera image controls. hmirror/vflip default to false (0); together
// they give a 180-degree rotation for boards mounted upside down.
// brightness/contrast/saturation are sensor_t's native -2..2 range.
bool printspy_nvs_get_hmirror(void);
esp_err_t printspy_nvs_set_hmirror(bool enable);

bool printspy_nvs_get_vflip(void);
esp_err_t printspy_nvs_set_vflip(bool enable);

int8_t printspy_nvs_get_cam_brightness(void);
esp_err_t printspy_nvs_set_cam_brightness(int8_t level);

int8_t printspy_nvs_get_cam_contrast(void);
esp_err_t printspy_nvs_set_cam_contrast(int8_t level);

int8_t printspy_nvs_get_cam_saturation(void);
esp_err_t printspy_nvs_set_cam_saturation(int8_t level);

// 0, 90, or 270 degrees - true rotation via software (see camera.c),
// separate from hmirror/vflip. 180 stays free via those two instead.
uint16_t printspy_nvs_get_rotation(void);
esp_err_t printspy_nvs_set_rotation(uint16_t degrees);
