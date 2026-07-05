#include "camera.h"

#include "boards/board.h"
#include "esp_log.h"
#include "settings.h"

static const char *TAG = "printspy_camera";

// ponytail: NVS resolution/quality default to 0 when never set. 0 happens
// to be a valid framesize_t (FRAMESIZE_96X96), so it can't double as
// "unset" - treat 0 as unset and fall back to these defaults.
//
// HD default: PSRAM DMA is off (see sdkconfig.defaults.esp32s3 - broken
// for JPEG, espressif/esp32-camera#775), so bigger frames cost real,
// physical time to scan out of the sensor and to ship over wifi - no
// firmware config makes that free. Resolution and quality are both
// live-adjustable from the admin UI (settings_get/post_handler in
// http_server.c) so that trade is a dial, not a hardcoded default.
#define DEFAULT_FRAMESIZE FRAMESIZE_HD
#define DEFAULT_JPEG_QUALITY 12

esp_err_t printspy_camera_init(void) {
  uint8_t stored_resolution = printspy_nvs_get_resolution();
  uint8_t stored_quality = printspy_nvs_get_quality();

  camera_config_t config = {
      .pin_pwdn = CAM_PIN_PWDN,
      .pin_reset = CAM_PIN_RESET,
      .pin_xclk = CAM_PIN_XCLK,
      .pin_sccb_sda = CAM_PIN_SIOD,
      .pin_sccb_scl = CAM_PIN_SIOC,
      .pin_d7 = CAM_PIN_D7,
      .pin_d6 = CAM_PIN_D6,
      .pin_d5 = CAM_PIN_D5,
      .pin_d4 = CAM_PIN_D4,
      .pin_d3 = CAM_PIN_D3,
      .pin_d2 = CAM_PIN_D2,
      .pin_d1 = CAM_PIN_D1,
      .pin_d0 = CAM_PIN_D0,
      .pin_vsync = CAM_PIN_VSYNC,
      .pin_href = CAM_PIN_HREF,
      .pin_pclk = CAM_PIN_PCLK,
      .xclk_freq_hz = 20000000,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,
      .pixel_format = PIXFORMAT_JPEG,
      .frame_size = stored_resolution ? stored_resolution : DEFAULT_FRAMESIZE,
      .jpeg_quality = stored_quality ? stored_quality : DEFAULT_JPEG_QUALITY,
      .fb_count = 2,
      .fb_location = CAMERA_FB_IN_PSRAM,
      // LATEST, not WHEN_EMPTY: if a reader (the MJPEG stream loop) falls
      // even slightly behind the sensor, WHEN_EMPTY serves whatever's
      // already queued - a stale frame - instead of dropping it for the
      // current one. Pure added lag, unrelated to capture/encode cost.
      .grab_mode = CAMERA_GRAB_LATEST,
  };

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    return err;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_hmirror(sensor, printspy_nvs_get_hmirror());
    sensor->set_vflip(sensor, printspy_nvs_get_vflip());
    sensor->set_brightness(sensor, printspy_nvs_get_cam_brightness());
    sensor->set_contrast(sensor, printspy_nvs_get_cam_contrast());
    sensor->set_saturation(sensor, printspy_nvs_get_cam_saturation());
  }

  ESP_LOGI(TAG, "camera initialized (frame_size=%d, quality=%d)",
           config.frame_size, config.jpeg_quality);
  return ESP_OK;
}

camera_fb_t *printspy_camera_capture(void) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "esp_camera_fb_get returned NULL");
  }
  return fb;
}
