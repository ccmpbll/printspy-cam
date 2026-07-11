#include "camera.h"

#include "boards/board.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"

static const char *TAG = "printspy_camera";

// Temporary: read SDA/SCL as plain GPIO inputs (internal pull-up, no I2C
// peripheral involved at all) - distinguishes "properly idle-high, nothing
// answering" from "something's actively holding the bus low" (stuck sensor,
// short, wrong pin).
static void probe_raw_gpio_levels(void) {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << CAM_PIN_SIOD) | (1ULL << CAM_PIN_SIOC),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&io_conf);
  ESP_LOGI(TAG, "raw idle levels: SDA(gpio%d)=%d SCL(gpio%d)=%d "
                "(1=high/idle as expected, 0=held low)",
           CAM_PIN_SIOD, gpio_get_level(CAM_PIN_SIOD), CAM_PIN_SIOC,
           gpio_get_level(CAM_PIN_SIOC));
}

// Temporary: raw I2C scan on the SCCB pins, bypassing esp32-camera's own
// driver entirely - answers "does ANYTHING ack on this bus at all" before
// blaming sensor-ID recognition. Installs/uninstalls its own driver each
// call so repeated calls (at different soak delays) don't conflict.
static int scan_sccb_bus(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = CAM_PIN_SIOD,
      .scl_io_num = CAM_PIN_SIOC,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 100000,
  };
  i2c_param_config(I2C_NUM_0, &conf);
  i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

  int found = 0;
  for (uint8_t addr = 0x03; addr < 0x78; addr++) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "  ACK at 0x%02x", addr);
      found++;
    }
  }

  i2c_driver_delete(I2C_NUM_0);
  return found;
}

// Temporary: repeat the scan at increasing soak delays after boot - tests
// whether the camera rail/reset just needs more time to settle before
// anything on the bus will ACK, since CAM_RST has no GPIO wired to it at
// all (pure R11 pull-up per schematic) and firmware can't drive it directly.
static void scan_sccb_bus_with_soak(void) {
  const int delays_ms[] = {0, 200, 500, 1000, 2000};
  for (size_t i = 0; i < sizeof(delays_ms) / sizeof(delays_ms[0]); i++) {
    if (delays_ms[i] > 0) {
      vTaskDelay(pdMS_TO_TICKS(delays_ms[i]));
    }
    ESP_LOGI(TAG, "SCCB scan at +%dms since boot...", delays_ms[i]);
    int found = scan_sccb_bus();
    ESP_LOGI(TAG, "  -> %d device(s) acked", found);
  }
}

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
  // Bump the driver's own probe logging so a failed sensor detect shows the
  // actual SCCB bytes read instead of just "not supported" - needs
  // CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y too (idf.py menuconfig -> Component
  // config -> Log output -> Maximum Log Verbosity), these calls are no-ops
  // below whatever level was compiled in.
  esp_log_level_set("camera", ESP_LOG_DEBUG);
  esp_log_level_set("sccb", ESP_LOG_DEBUG);
  esp_log_level_set("cam_hal", ESP_LOG_DEBUG);

  probe_raw_gpio_levels();
  scan_sccb_bus_with_soak();

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

camera_fb_t *printspy_camera_capture(void) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "esp_camera_fb_get returned NULL");
  }
  return fb;
}
