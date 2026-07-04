#include "wifi.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "http_server.h"
#include "wifi_ap.h"

static const char *TAG = "wifi";

#define WIFI_MAX_BOOT_RETRIES 10

EventGroupHandle_t wifi_event_group_handle;
static StaticEventGroup_t wifi_event_group;

StaticTask_t wifiTaskBuffer;
StackType_t wifiTaskStack[WIFI_STACK_SIZE];

SemaphoreHandle_t wifi_req_semaphore;
static StaticSemaphore_t wifi_req_semaphore_mutex_buffer;

static bool wifi_started = false;
static bool wifi_has_ip_val = false;
static bool wifi_ever_had_ip = false;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  // Avoid any esp_wifi_*() calls directly here - done in the WiFi task,
  // which claims wifi_req_semaphore, to avoid races with other tasks.
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      xEventGroupSetBits(wifi_event_group_handle, WIFI_READY_TO_CONNECT_EVENT);
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
      xEventGroupSetBits(wifi_event_group_handle, WIFI_READY_TO_CONNECT_EVENT);
      break;
    default:
      break;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      wifi_has_ip_val = true;
      wifi_ever_had_ip = true;
      ESP_LOGI(TAG, "Connected with IP Address:" IPSTR,
               IP2STR(&event->ip_info.ip));
      // Idempotent - only starts the HTTP server on first IP.
      printspy_http_server_start();
      break;
    }
    case IP_EVENT_STA_LOST_IP:
      wifi_has_ip_val = false;
      break;
    default:
      break;
    }
  }
}

static void wifi_driver_init(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_event_group_handle = xEventGroupCreateStatic(&wifi_event_group);

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_req_semaphore =
      xSemaphoreCreateMutexStatic(&wifi_req_semaphore_mutex_buffer);
}

bool printspy_wifi_is_started(void) { return wifi_started; }

bool printspy_wifi_has_ip(void) { return wifi_has_ip_val; }

bool printspy_wifi_has_credentials(void) {
  wifi_config_t cfg = {0};
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) {
    return false;
  }
  return cfg.sta.ssid[0] != '\0';
}

static void enter_ap_mode(bool is_fallback) {
  ESP_LOGI(TAG, "Entering AP provisioning mode (fallback=%d)", is_fallback);
  EventBits_t bits = WIFI_AP_MODE_ACTIVE_EVENT;
  if (is_fallback) {
    bits |= WIFI_AP_FALLBACK_EVENT;
  }
  xEventGroupSetBits(wifi_event_group_handle, bits);
  printspy_wifi_ap_start(is_fallback);
  // Block here - AP mode only ends via reboot (triggered inside the setup
  // page's POST handler), so this task effectively sleeps until restart.
  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

void wifi_task_run(void *pvParameters) {
  wifi_driver_init();

  // If no credentials are saved, go straight to AP mode for first-time setup.
  if (!printspy_wifi_has_credentials()) {
    ESP_LOGI(TAG, "No WiFi credentials found, starting AP setup");
    enter_ap_mode(false);
    return; // unreachable - enter_ap_mode blocks until reboot
  }

  // Credentials exist: start STA and attempt to connect.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_started = true;

  EventBits_t wifi_event_bits = 0;
  int boot_retry_count = 0;

  while (1) {
    if (!xSemaphoreTake(wifi_req_semaphore, portMAX_DELAY)) {
      continue;
    }

    if (wifi_event_bits & WIFI_READY_TO_CONNECT_EVENT) {
      if (!wifi_ever_had_ip) {
        // Still in the boot-time connection window - enforce retry limit.
        if (boot_retry_count >= WIFI_MAX_BOOT_RETRIES) {
          xSemaphoreGive(wifi_req_semaphore);
          ESP_LOGW(TAG, "Exhausted %d boot retries, entering AP mode",
                   WIFI_MAX_BOOT_RETRIES);
          enter_ap_mode(true);
          return; // unreachable
        }
        boot_retry_count++;
        ESP_LOGI(TAG, "STA connect attempt %d/%d", boot_retry_count,
                 WIFI_MAX_BOOT_RETRIES);
      }
      esp_wifi_connect();
    }

    xSemaphoreGive(wifi_req_semaphore);

    wifi_event_bits = xEventGroupWaitBits(wifi_event_group_handle,
                                          WIFI_READY_TO_CONNECT_EVENT,
                                          true,  // clear on exit
                                          false, // wait for all
                                          portMAX_DELAY);
  }
}
