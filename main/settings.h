#pragma once

#include "esp_err.h"
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
