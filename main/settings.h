#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t printspy_nvs_init(void);

const char *printspy_nvs_get_hostname(void);
esp_err_t printspy_nvs_set_hostname(const char *hostname, size_t size);

const char *printspy_nvs_get_wifi_ssid(void);
esp_err_t printspy_nvs_set_wifi_ssid(const char *ssid, size_t size);

const char *printspy_nvs_get_wifi_pass(void);
esp_err_t printspy_nvs_set_wifi_pass(const char *pass, size_t size);

uint8_t printspy_nvs_get_resolution(void);
esp_err_t printspy_nvs_set_resolution(uint8_t framesize);

uint8_t printspy_nvs_get_quality(void);
esp_err_t printspy_nvs_set_quality(uint8_t quality);

uint8_t printspy_nvs_get_led_brightness(void);
esp_err_t printspy_nvs_set_led_brightness(uint8_t brightness);

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
