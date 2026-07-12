#include "settings.h"

#include <nvs.h>

static const char *NVS_NAMESPACE = "printspycam";
static const char *NVS_ID_RESOLUTION = "resolution";
static const char *NVS_ID_QUALITY = "quality";
static const char *NVS_ID_HMIRROR = "hmirror";
static const char *NVS_ID_VFLIP = "vflip";
static const char *NVS_ID_CAM_BRIGHTNESS = "cam_bright";
static const char *NVS_ID_CAM_CONTRAST = "cam_contrast";
static const char *NVS_ID_CAM_SATURATION = "cam_sat";
static const char *NVS_ID_ROTATION = "rotation";

static uint8_t resolution_val = 0;
static uint8_t quality_val = 0;
static uint8_t hmirror_val = 0;
static uint8_t vflip_val = 0;
static int8_t cam_brightness_val = 0;
static int8_t cam_contrast_val = 0;
static int8_t cam_saturation_val = 0;
static uint16_t rotation_val = 0;

#define LOAD_NVS_SCALAR(nvs_get_fn, key, dest)                              \
  do {                                                                       \
    err = nvs_get_fn(handle, key, &(dest));                                \
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {                    \
      return err;                                                           \
    }                                                                        \
  } while (0)

#define SCALAR_SETTER(nvs_set_fn, key, dest, val)                           \
  nvs_handle_t handle;                                                      \
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);         \
  if (err != ESP_OK)                                                        \
    return err;                                                             \
  err = nvs_set_fn(handle, key, val);                                      \
  nvs_close(handle);                                                        \
  if (err == ESP_OK)                                                        \
    dest = val;                                                             \
  return err;

esp_err_t printspy_nvs_init(void) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  LOAD_NVS_SCALAR(nvs_get_u8, NVS_ID_RESOLUTION, resolution_val);
  LOAD_NVS_SCALAR(nvs_get_u8, NVS_ID_QUALITY, quality_val);
  LOAD_NVS_SCALAR(nvs_get_u8, NVS_ID_HMIRROR, hmirror_val);
  LOAD_NVS_SCALAR(nvs_get_u8, NVS_ID_VFLIP, vflip_val);
  LOAD_NVS_SCALAR(nvs_get_i8, NVS_ID_CAM_BRIGHTNESS, cam_brightness_val);
  LOAD_NVS_SCALAR(nvs_get_i8, NVS_ID_CAM_CONTRAST, cam_contrast_val);
  LOAD_NVS_SCALAR(nvs_get_i8, NVS_ID_CAM_SATURATION, cam_saturation_val);
  LOAD_NVS_SCALAR(nvs_get_u16, NVS_ID_ROTATION, rotation_val);

  nvs_close(handle);

  return ESP_OK;
}

uint8_t printspy_nvs_get_resolution(void) { return resolution_val; }
esp_err_t printspy_nvs_set_resolution(uint8_t framesize) {
  SCALAR_SETTER(nvs_set_u8, NVS_ID_RESOLUTION, resolution_val, framesize)
}

uint8_t printspy_nvs_get_quality(void) { return quality_val; }
esp_err_t printspy_nvs_set_quality(uint8_t quality) {
  SCALAR_SETTER(nvs_set_u8, NVS_ID_QUALITY, quality_val, quality)
}

bool printspy_nvs_get_hmirror(void) { return hmirror_val != 0; }
esp_err_t printspy_nvs_set_hmirror(bool enable) {
  uint8_t v = enable ? 1 : 0;
  SCALAR_SETTER(nvs_set_u8, NVS_ID_HMIRROR, hmirror_val, v)
}

bool printspy_nvs_get_vflip(void) { return vflip_val != 0; }
esp_err_t printspy_nvs_set_vflip(bool enable) {
  uint8_t v = enable ? 1 : 0;
  SCALAR_SETTER(nvs_set_u8, NVS_ID_VFLIP, vflip_val, v)
}

int8_t printspy_nvs_get_cam_brightness(void) { return cam_brightness_val; }
esp_err_t printspy_nvs_set_cam_brightness(int8_t level) {
  SCALAR_SETTER(nvs_set_i8, NVS_ID_CAM_BRIGHTNESS, cam_brightness_val, level)
}

int8_t printspy_nvs_get_cam_contrast(void) { return cam_contrast_val; }
esp_err_t printspy_nvs_set_cam_contrast(int8_t level) {
  SCALAR_SETTER(nvs_set_i8, NVS_ID_CAM_CONTRAST, cam_contrast_val, level)
}

int8_t printspy_nvs_get_cam_saturation(void) { return cam_saturation_val; }
esp_err_t printspy_nvs_set_cam_saturation(int8_t level) {
  SCALAR_SETTER(nvs_set_i8, NVS_ID_CAM_SATURATION, cam_saturation_val, level)
}

// 0, 90, or 270 - software rotation is the only way to get a true 90/270
// out of these sensors (no hardware support for it), applied in
// camera.c on top of whatever esp32-camera capture normally returns.
// 180 stays free via hmirror+vflip above, not routed through this.
uint16_t printspy_nvs_get_rotation(void) { return rotation_val; }
esp_err_t printspy_nvs_set_rotation(uint16_t degrees) {
  SCALAR_SETTER(nvs_set_u16, NVS_ID_ROTATION, rotation_val, degrees)
}
