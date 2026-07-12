#pragma once

#include "esp_camera.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t printspy_camera_init(void);

// Wraps both the raw sensor path and the software-rotated path (see
// camera.c) behind one release call, since they free differently: a raw
// sensor frame goes back to esp32-camera's pool, a rotated one is a plain
// malloc'd buffer. `fb` non-NULL means the former.
typedef struct {
  const uint8_t *buf;
  size_t len;
  camera_fb_t *fb;
} printspy_frame_t;

// frame.buf is NULL on failure. Always release via
// printspy_camera_release_frame(), regardless of which path served it.
printspy_frame_t printspy_camera_capture(void);
void printspy_camera_release_frame(printspy_frame_t *frame);
