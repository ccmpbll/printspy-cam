#pragma once

#include "esp_camera.h"
#include "esp_err.h"

esp_err_t printspy_camera_init(void);

// Caller must call esp_camera_fb_return() on the returned buffer.
camera_fb_t *printspy_camera_capture(void);
