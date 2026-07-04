#include "settings.h"

#include <nvs.h>
#include <stdlib.h>
#include <string.h>

static const char *NVS_NAMESPACE = "printspycam";
static const char *NVS_ID_HOSTNAME = "hostname";
static const char *NVS_ID_WIFI_SSID = "wifi_ssid";
static const char *NVS_ID_WIFI_PASS = "wifi_pass";
static const char *NVS_ID_RESOLUTION = "resolution";
static const char *NVS_ID_QUALITY = "quality";
static const char *NVS_ID_LED_BRIGHTNESS = "led";

static const char *hostname_str = NULL;
static const char *wifi_ssid_str = NULL;
static const char *wifi_pass_str = NULL;
static uint8_t resolution_val = 0;
static uint8_t quality_val = 0;
static uint8_t led_brightness_val = 0;

// Ported from bitclock-redux: a loaded string must be null-terminated
// printable ASCII or it's dropped, so a corrupted blob (dangling-pointer
// write, pre-upgrade firmware) can't break JSON serialization downstream.
#define LOAD_NVS_STR(key, dest)                                              \
  do {                                                                       \
    size_t _sz = 0;                                                         \
    err = nvs_get_blob(handle, key, NULL, &_sz);                            \
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {                    \
      return err;                                                           \
    }                                                                        \
    if (_sz > 0) {                                                          \
      char *_buf = malloc(_sz);                                            \
      if (!_buf) {                                                          \
        nvs_close(handle);                                                  \
        return ESP_ERR_NO_MEM;                                              \
      }                                                                      \
      err = nvs_get_blob(handle, key, _buf, &_sz);                         \
      if (err != ESP_OK) {                                                  \
        free(_buf);                                                         \
        return err;                                                         \
      }                                                                      \
      bool _v = (_sz > 0 && _buf[_sz - 1] == '\0');                        \
      for (size_t _i = 0; _v && _i < _sz - 1; _i++) {                      \
        unsigned char _c = (unsigned char)_buf[_i];                        \
        if (_c < 0x20 || _c > 0x7E)                                        \
          _v = false;                                                       \
      }                                                                      \
      if (_v) {                                                             \
        dest = _buf;                                                        \
      } else {                                                              \
        free(_buf);                                                         \
      }                                                                      \
    }                                                                        \
  } while (0)

#define BLOB_SETTER(key, dest)                                               \
  nvs_handle_t handle;                                                      \
  esp_err_t err;                                                            \
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);                   \
  if (err != ESP_OK)                                                        \
    return err;                                                             \
  err = nvs_set_blob(handle, key, val, size);                              \
  nvs_close(handle);                                                        \
  if (err != ESP_OK)                                                        \
    return err;                                                             \
  char *copy = malloc(size);                                                \
  if (copy) {                                                               \
    memcpy(copy, val, size);                                                \
    free((void *)dest);                                                     \
    dest = copy;                                                            \
  }                                                                          \
  return ESP_OK;

esp_err_t printspy_nvs_init(void) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  LOAD_NVS_STR(NVS_ID_HOSTNAME, hostname_str);
  LOAD_NVS_STR(NVS_ID_WIFI_SSID, wifi_ssid_str);
  LOAD_NVS_STR(NVS_ID_WIFI_PASS, wifi_pass_str);

  err = nvs_get_u8(handle, NVS_ID_RESOLUTION, &resolution_val);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  err = nvs_get_u8(handle, NVS_ID_QUALITY, &quality_val);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  err = nvs_get_u8(handle, NVS_ID_LED_BRIGHTNESS, &led_brightness_val);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  nvs_close(handle);

  return ESP_OK;
}

const char *printspy_nvs_get_hostname(void) { return hostname_str; }
esp_err_t printspy_nvs_set_hostname(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_HOSTNAME, hostname_str)
}

const char *printspy_nvs_get_wifi_ssid(void) { return wifi_ssid_str; }
esp_err_t printspy_nvs_set_wifi_ssid(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_WIFI_SSID, wifi_ssid_str)
}

const char *printspy_nvs_get_wifi_pass(void) { return wifi_pass_str; }
esp_err_t printspy_nvs_set_wifi_pass(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_WIFI_PASS, wifi_pass_str)
}

uint8_t printspy_nvs_get_resolution(void) { return resolution_val; }
esp_err_t printspy_nvs_set_resolution(uint8_t framesize) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return err;
  err = nvs_set_u8(handle, NVS_ID_RESOLUTION, framesize);
  nvs_close(handle);
  if (err == ESP_OK)
    resolution_val = framesize;
  return err;
}

uint8_t printspy_nvs_get_quality(void) { return quality_val; }
esp_err_t printspy_nvs_set_quality(uint8_t quality) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return err;
  err = nvs_set_u8(handle, NVS_ID_QUALITY, quality);
  nvs_close(handle);
  if (err == ESP_OK)
    quality_val = quality;
  return err;
}

uint8_t printspy_nvs_get_led_brightness(void) { return led_brightness_val; }
esp_err_t printspy_nvs_set_led_brightness(uint8_t brightness) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return err;
  err = nvs_set_u8(handle, NVS_ID_LED_BRIGHTNESS, brightness);
  nvs_close(handle);
  if (err == ESP_OK)
    led_brightness_val = brightness;
  return err;
}
