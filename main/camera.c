#include "camera.h"

#include "boards/board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "img_converters.h"
#include "settings.h"
#include <stdlib.h>

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
  // jpg2rgb565() (used by rotate_frame() below) decodes into little-endian
  // RGB565, but fmt2jpg()'s encoder defaults to assuming big-endian input
  // (rgb565_big_endian=true in to_jpg.cpp) - global setting, set once here
  // rather than per-call. Left mismatched, every rotated frame comes out
  // with scrambled colors (channels reading the wrong byte) despite the
  // rotation geometry itself being correct - confirmed on real hardware.
  jpgSetRgb565BE(false);

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
      // Dropped from 20MHz: less aggressive DVP bus timing gives more
      // margin against wifi-coincident capture corruption/timeouts.
      .xclk_freq_hz = 10000000,
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

// Neither OV2640 nor OV3660 can rotate on readout - only mirror/flip in
// hardware (see set_hmirror/set_vflip above, which cover 180 for free).
// True 90/270 needs the frame decoded, transposed, and re-encoded as
// JPEG - real CPU cost per frame and a second full-frame PSRAM buffer on
// top of the decoded one (worst case ~7.7MB combined at UXGA on an 8MB
// PSRAM chip, hence the OOM fallback below rather than a hard failure).
static bool rotate_frame(camera_fb_t *fb, uint16_t degrees, uint8_t quality,
                          uint8_t **out_buf, size_t *out_len) {
  size_t rgb_len = (size_t)fb->width * fb->height * 2; // RGB565
  uint16_t *src = heap_caps_malloc(rgb_len, MALLOC_CAP_SPIRAM);
  if (!src) {
    ESP_LOGW(TAG, "rotate: no PSRAM for %zu-byte decode buffer", rgb_len);
    return false;
  }
  if (!jpg2rgb565(fb->buf, fb->len, (uint8_t *)src, JPG_SCALE_NONE)) {
    ESP_LOGW(TAG, "rotate: JPEG decode failed");
    free(src);
    return false;
  }

  uint16_t *dst = heap_caps_malloc(rgb_len, MALLOC_CAP_SPIRAM);
  if (!dst) {
    ESP_LOGW(TAG, "rotate: no PSRAM for %zu-byte rotate buffer", rgb_len);
    free(src);
    return false;
  }

  uint16_t src_w = fb->width, src_h = fb->height;
  uint16_t dst_w = src_h, dst_h = src_w;
  for (uint16_t y = 0; y < src_h; y++) {
    for (uint16_t x = 0; x < src_w; x++) {
      uint16_t dst_x, dst_y;
      if (degrees == 90) {
        dst_x = src_h - 1 - y;
        dst_y = x;
      } else { // 270
        dst_x = y;
        dst_y = src_w - 1 - x;
      }
      dst[(size_t)dst_y * dst_w + dst_x] = src[(size_t)y * src_w + x];
    }
  }
  free(src);

  bool ok = fmt2jpg((uint8_t *)dst, rgb_len, dst_w, dst_h, PIXFORMAT_RGB565,
                     quality, out_buf, out_len);
  free(dst);
  if (!ok) {
    ESP_LOGW(TAG, "rotate: re-encode failed");
  }
  return ok;
}

printspy_frame_t printspy_camera_capture(void) {
  printspy_frame_t frame = {0};
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "esp_camera_fb_get returned NULL");
    return frame;
  }

  uint16_t rotation = printspy_nvs_get_rotation();
  if (rotation == 90 || rotation == 270) {
    sensor_t *sensor = esp_camera_sensor_get();
    uint8_t quality = sensor ? sensor->status.quality : DEFAULT_JPEG_QUALITY;
    uint8_t *rotated_buf;
    size_t rotated_len;
    if (rotate_frame(fb, rotation, quality, &rotated_buf, &rotated_len)) {
      esp_camera_fb_return(fb);
      frame.buf = rotated_buf;
      frame.len = rotated_len;
      frame.fb = NULL;
      return frame;
    }
    // Decode/encode/OOM failure - fall through and serve the original,
    // unrotated frame rather than dropping it entirely.
  }

  frame.buf = fb->buf;
  frame.len = fb->len;
  frame.fb = fb;
  return frame;
}

void printspy_camera_release_frame(printspy_frame_t *frame) {
  if (frame->fb) {
    esp_camera_fb_return(frame->fb);
  } else if (frame->buf) {
    free((void *)frame->buf);
  }
  frame->buf = NULL;
  frame->fb = NULL;
}
