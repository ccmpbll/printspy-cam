#pragma once

#include "esp_err.h"

// AP-mode WiFi provisioning (softAP + setup.html config page at 192.168.4.1).
// TODO: port from bitclock-redux (main/tasks/wifi_ap.c).

esp_err_t printspy_wifi_ap_start(void);
esp_err_t printspy_wifi_ap_stop(void);
