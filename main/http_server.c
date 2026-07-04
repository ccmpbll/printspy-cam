#include "http_server.h"

#include "camera.h"
#include "cJSON.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "version.h"
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
// rate, no artificial delay needed.
static esp_err_t stream_handler(httpd_req_t *req) {
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

static esp_err_t status_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "version", PRINTSPY_CAM_VERSION);
  cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);

  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip_info;
  if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    cJSON_AddStringToObject(root, "ip", ip_str);
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

esp_err_t printspy_http_server_start(void) {
  if (server) {
    return ESP_OK; // already running - IP_EVENT_STA_GOT_IP can refire on reconnect
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192; // multipart streaming needs more than the 4096 default

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

  httpd_register_uri_handler(server, &root_uri);
  httpd_register_uri_handler(server, &snapshot_uri);
  httpd_register_uri_handler(server, &stream_uri);
  httpd_register_uri_handler(server, &status_uri);

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}
