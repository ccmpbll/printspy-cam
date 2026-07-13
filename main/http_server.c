#include "http_server.h"

#include "camera.h"
#include "cJSON.h"
#include "chip_temp.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "log.h"
#include "ota.h"
#include "settings.h"
#include "version.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "printspy_http";

static httpd_handle_t server = NULL;

extern const uint8_t admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t admin_html_end[] asm("_binary_admin_html_end");

#define PART_BOUNDARY "printspyframeboundary"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Long-running handlers (MJPEG stream, SSE log console) block whichever
// task runs them until the client disconnects. esp_http_server services
// all connections from a single task by default, so without this, one
// open /stream would stall every other request (admin page, /snapshot,
// /api/status, a second /stream viewer) until it closed. This is
// Espressif's own documented pattern for supporting concurrent
// long-running handlers - see
// examples/protocols/http_server/async_handlers in esp-idf.
typedef struct {
  QueueHandle_t queue;
  SemaphoreHandle_t free_slots;
} async_pool_t;

typedef struct {
  httpd_req_t *req;
  esp_err_t (*handler)(httpd_req_t *req);
} async_job_t;

static void async_worker_task(void *arg) {
  async_pool_t *pool = (async_pool_t *)arg;
  while (true) {
    xSemaphoreGive(pool->free_slots);
    async_job_t job;
    if (xQueueReceive(pool->queue, &job, portMAX_DELAY)) {
      job.handler(job.req);
      httpd_req_async_handler_complete(job.req);
    }
  }
}

static esp_err_t async_pool_init(async_pool_t *pool, int size,
                                 const char *name_prefix) {
  pool->free_slots = xSemaphoreCreateCounting(size, 0);
  pool->queue = xQueueCreate(size, sizeof(async_job_t));
  if (!pool->free_slots || !pool->queue) {
    return ESP_ERR_NO_MEM;
  }
  for (int i = 0; i < size; i++) {
    char task_name[20];
    snprintf(task_name, sizeof(task_name), "%s_%d", name_prefix, i);
    xTaskCreate(async_worker_task, task_name, 4096, pool,
               tskIDLE_PRIORITY + 1, NULL);
  }
  return ESP_OK;
}

static esp_err_t async_pool_dispatch(async_pool_t *pool, httpd_req_t *req,
                                     esp_err_t (*handler)(httpd_req_t *)) {
  httpd_req_t *copy = NULL;
  esp_err_t err = httpd_req_async_handler_begin(req, &copy);
  if (err != ESP_OK) {
    return err;
  }

  if (xSemaphoreTake(pool->free_slots, 0) != pdTRUE) {
    httpd_req_async_handler_complete(copy);
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_sendstr(req, "Too many concurrent connections");
    return ESP_OK;
  }

  async_job_t job = {.req = copy, .handler = handler};
  if (xQueueSend(pool->queue, &job, 0) != pdTRUE) {
    httpd_req_async_handler_complete(copy);
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_sendstr(req, "Too many concurrent connections");
    return ESP_OK;
  }
  return ESP_OK;
}

#define STREAM_WORKER_COUNT 2 // user-chosen cap on concurrent /stream viewers
#define LOG_WORKER_COUNT 1
static async_pool_t stream_pool;
static async_pool_t log_pool;

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)admin_html_start,
                         HTTPD_RESP_USE_STRLEN);
}

static esp_err_t snapshot_handler(httpd_req_t *req) {
  camera_fb_t *fb = printspy_camera_capture();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// Grabs frames back-to-back until the client disconnects - grab_mode
// CAMERA_GRAB_WHEN_EMPTY already paces this to the sensor's actual frame
// rate, no artificial delay needed. Runs on an async worker (see above).
static esp_err_t stream_async_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  while (true) {
    camera_fb_t *fb = printspy_camera_capture();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }

    char part_buf[64];
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  return async_pool_dispatch(&stream_pool, req, stream_async_handler);
}

// Polls the ring buffer every 300ms and pushes new lines to the browser as
// Server-Sent Events. Runs on an async worker (see above).
static esp_err_t logs_async_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, "text/event-stream");
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  uint64_t cursor = 0;
  char buf[1024];
  while (true) {
    size_t len = printspy_log_read(&cursor, buf, sizeof(buf));
    if (len > 0) {
      char *line = buf;
      while (line < buf + len) {
        char *nl = strchr(line, '\n');
        if (!nl) {
          break;
        }
        *nl = '\0';
        char frame[192];
        int flen = snprintf(frame, sizeof(frame), "data: %s\n\n", line);
        res = httpd_resp_send_chunk(req, frame, flen);
        if (res != ESP_OK) {
          return res;
        }
        line = nl + 1;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

static esp_err_t logs_handler(httpd_req_t *req) {
  return async_pool_dispatch(&log_pool, req, logs_async_handler);
}

static esp_err_t status_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "version", PRINTSPY_CAM_VERSION);
  cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);
  cJSON_AddNumberToObject(root, "chip_temp_c", printspy_chip_temp_read());

  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip_info;
  if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    cJSON_AddStringToObject(root, "ip", ip_str);
  }

  wifi_config_t wifi_cfg = {0};
  if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
    cJSON_AddStringToObject(root, "wifi_ssid", (const char *)wifi_cfg.sta.ssid);
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    cJSON_AddNumberToObject(root, "frame_size", sensor->status.framesize);
    cJSON_AddNumberToObject(root, "jpeg_quality", sensor->status.quality);
  }

  char *json = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  esp_err_t res = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
  free(json);
  cJSON_Delete(root);
  return res;
}

// Reads the full request body into a heap buffer. Caller must free().
// Only used for small JSON bodies (WiFi creds, settings) - not suitable
// for the multi-hundred-KB OTA upload, which streams instead.
static esp_err_t read_body(httpd_req_t *req, char **out) {
  if (req->content_len == 0 || req->content_len > 4096) {
    return ESP_ERR_INVALID_SIZE;
  }
  char *buf = malloc(req->content_len + 1);
  if (!buf) {
    return ESP_ERR_NO_MEM;
  }
  size_t received = 0;
  while (received < req->content_len) {
    int r = httpd_req_recv(req, buf + received, req->content_len - received);
    if (r <= 0) {
      free(buf);
      return ESP_FAIL;
    }
    received += r;
  }
  buf[received] = '\0';
  *out = buf;
  return ESP_OK;
}

static esp_err_t wifi_post_handler(httpd_req_t *req) {
  char *body = NULL;
  if (read_body(req, &body) != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  cJSON *json = cJSON_Parse(body);
  free(body);
  if (!json) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
  cJSON *password_item = cJSON_GetObjectItem(json, "password");
  if (!cJSON_IsString(ssid_item) || strlen(ssid_item->valuestring) == 0) {
    cJSON_Delete(json);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Missing SSID");
    return ESP_FAIL;
  }

  wifi_config_t wifi_cfg = {0};
  strncpy((char *)wifi_cfg.sta.ssid, ssid_item->valuestring,
          sizeof(wifi_cfg.sta.ssid) - 1);
  if (cJSON_IsString(password_item)) {
    strncpy((char *)wifi_cfg.sta.password, password_item->valuestring,
            sizeof(wifi_cfg.sta.password) - 1);
  }
  cJSON_Delete(json);

  // esp_wifi_set_config persists to NVS flash storage on its own.
  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");

  ESP_LOGI(TAG, "New WiFi credentials saved via web UI, restarting");
  vTaskDelay(pdMS_TO_TICKS(500)); // let the response flush
  // Reboot (rather than hot-swapping the STA config) so the fresh boot
  // gets the finite-retry-then-AP-fallback safety net for whatever
  // credentials were just entered, same as the AP setup flow.
  esp_restart();
  return ESP_OK; // unreachable
}

static esp_err_t settings_get_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "hmirror", printspy_nvs_get_hmirror());
  cJSON_AddBoolToObject(root, "vflip", printspy_nvs_get_vflip());
  cJSON_AddNumberToObject(root, "brightness", printspy_nvs_get_cam_brightness());
  cJSON_AddNumberToObject(root, "contrast", printspy_nvs_get_cam_contrast());
  cJSON_AddNumberToObject(root, "saturation", printspy_nvs_get_cam_saturation());

  // Live values from the sensor, not raw NVS - NVS reads back 0 ("unset")
  // until the user actually changes these once, but the sensor always
  // knows what it's really running at.
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    cJSON_AddNumberToObject(root, "resolution", sensor->status.framesize);
    cJSON_AddNumberToObject(root, "quality", sensor->status.quality);
  }

  char *json = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  esp_err_t res = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
  free(json);
  cJSON_Delete(root);
  return res;
}

static esp_err_t settings_post_handler(httpd_req_t *req) {
  char *body = NULL;
  if (read_body(req, &body) != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  cJSON *json = cJSON_Parse(body);
  free(body);
  if (!json) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Invalid JSON");
    return ESP_FAIL;
  }

  sensor_t *sensor = esp_camera_sensor_get();

  cJSON *item = cJSON_GetObjectItem(json, "hmirror");
  if (cJSON_IsBool(item)) {
    bool val = cJSON_IsTrue(item);
    printspy_nvs_set_hmirror(val);
    if (sensor) sensor->set_hmirror(sensor, val);
  }

  item = cJSON_GetObjectItem(json, "vflip");
  if (cJSON_IsBool(item)) {
    bool val = cJSON_IsTrue(item);
    printspy_nvs_set_vflip(val);
    if (sensor) sensor->set_vflip(sensor, val);
  }

  item = cJSON_GetObjectItem(json, "brightness");
  if (cJSON_IsNumber(item)) {
    int8_t val = (int8_t)item->valueint;
    printspy_nvs_set_cam_brightness(val);
    if (sensor) sensor->set_brightness(sensor, val);
  }

  item = cJSON_GetObjectItem(json, "contrast");
  if (cJSON_IsNumber(item)) {
    int8_t val = (int8_t)item->valueint;
    printspy_nvs_set_cam_contrast(val);
    if (sensor) sensor->set_contrast(sensor, val);
  }

  item = cJSON_GetObjectItem(json, "saturation");
  if (cJSON_IsNumber(item)) {
    int8_t val = (int8_t)item->valueint;
    printspy_nvs_set_cam_saturation(val);
    if (sensor) sensor->set_saturation(sensor, val);
  }

  // FRAMESIZE_96X96 (0) .. FRAMESIZE_UXGA (15) - the full range esp32-camera
  // defines for OV3660/OV2640. Anything outside that can't be a real
  // framesize_t the sensor supports.
  item = cJSON_GetObjectItem(json, "resolution");
  if (cJSON_IsNumber(item) && item->valueint >= 0 && item->valueint <= 15) {
    uint8_t val = (uint8_t)item->valueint;
    printspy_nvs_set_resolution(val);
    if (sensor) sensor->set_framesize(sensor, (framesize_t)val);
  }

  item = cJSON_GetObjectItem(json, "quality");
  if (cJSON_IsNumber(item) && item->valueint >= 4 && item->valueint <= 63) {
    uint8_t val = (uint8_t)item->valueint;
    printspy_nvs_set_quality(val);
    if (sensor) sensor->set_quality(sensor, val);
  }

  cJSON_Delete(json);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"ok\":true}");
  return ESP_OK;
}

// Firmware image streamed straight through to the OTA partition as it
// arrives - not buffered in RAM, images can be a few hundred KB to a few
// MB.
static esp_err_t ota_post_handler(httpd_req_t *req) {
  if (req->content_len == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Missing body");
    return ESP_FAIL;
  }

  if (printspy_ota_begin() != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char buf[4096];
  size_t remaining = req->content_len;
  while (remaining > 0) {
    size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
    int received = httpd_req_recv(req, buf, to_read);
    if (received <= 0) {
      printspy_ota_abort();
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (printspy_ota_write_chunk((uint8_t *)buf, received) != ESP_OK) {
      printspy_ota_abort();
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_sendstr(req, "Firmware write failed");
      return ESP_FAIL;
    }
    remaining -= received;
  }

  if (printspy_ota_finish() != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Firmware image invalid");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");

  ESP_LOGI(TAG, "OTA update applied via web UI, restarting");
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  return ESP_OK; // unreachable
}

esp_err_t printspy_http_server_start(void) {
  if (server) {
    return ESP_OK; // already running - IP_EVENT_STA_GOT_IP can refire on reconnect
  }

  if (async_pool_init(&stream_pool, STREAM_WORKER_COUNT, "stream_worker") !=
          ESP_OK ||
      async_pool_init(&log_pool, LOG_WORKER_COUNT, "log_worker") != ESP_OK) {
    ESP_LOGE(TAG, "failed to init async worker pools");
    return ESP_ERR_NO_MEM;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192; // multipart streaming needs more than the 4096 default
  // Default max_uri_handlers is 8 - we register 9 (/, /snapshot, /stream,
  // /api/status, /api/logs, /api/wifi, /api/settings x2, /api/ota).
  // Past the cap, httpd_register_uri_handler silently drops the excess.
  config.max_uri_handlers = 9;
  // Enough for stream + log workers plus a few quick synchronous requests
  // (admin page, /snapshot, /api/status) to stay responsive at the same time.
  config.max_open_sockets = STREAM_WORKER_COUNT + LOG_WORKER_COUNT + 4;
  config.lru_purge_enable = true;
  // Without TCP keepalive, a stale /api/logs connection (phone sleeps,
  // wifi drops) never gets detected as dead - it just sits there holding
  // the only log worker slot forever, so the next reconnect gets a 503
  // that EventSource treats as permanent (no auto-retry on non-2xx).
  config.keep_alive_enable = true;
  config.keep_alive_idle = 5;
  config.keep_alive_interval = 5;
  config.keep_alive_count = 3;

  esp_err_t err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }

  httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
  httpd_uri_t snapshot_uri = {
      .uri = "/snapshot", .method = HTTP_GET, .handler = snapshot_handler};
  httpd_uri_t stream_uri = {
      .uri = "/stream", .method = HTTP_GET, .handler = stream_handler};
  httpd_uri_t status_uri = {
      .uri = "/api/status", .method = HTTP_GET, .handler = status_handler};
  httpd_uri_t logs_uri = {
      .uri = "/api/logs", .method = HTTP_GET, .handler = logs_handler};
  httpd_uri_t wifi_uri = {
      .uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post_handler};
  httpd_uri_t settings_get_uri = {
      .uri = "/api/settings", .method = HTTP_GET, .handler = settings_get_handler};
  httpd_uri_t settings_post_uri = {.uri = "/api/settings",
                                   .method = HTTP_POST,
                                   .handler = settings_post_handler};
  httpd_uri_t ota_uri = {
      .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler};

  httpd_register_uri_handler(server, &root_uri);
  httpd_register_uri_handler(server, &snapshot_uri);
  httpd_register_uri_handler(server, &stream_uri);
  httpd_register_uri_handler(server, &status_uri);
  httpd_register_uri_handler(server, &logs_uri);
  httpd_register_uri_handler(server, &wifi_uri);
  httpd_register_uri_handler(server, &settings_get_uri);
  httpd_register_uri_handler(server, &settings_post_uri);
  httpd_register_uri_handler(server, &ota_uri);

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}
