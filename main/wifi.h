#pragma once

#include "esp_err.h"
#include <stdbool.h>

// STA-mode WiFi connection lifecycle. AP-mode provisioning lives in wifi_ap.h.
// TODO: port state machine from bitclock-redux (main/tasks/wifi.c) - STA
// connect with 10-retry AP fallback, silent unlimited retry on a
// mid-session disconnect once we've ever had an IP.

esp_err_t printspy_wifi_init_sta(void);
bool printspy_wifi_has_ip(void);
bool printspy_wifi_has_credentials(void);
