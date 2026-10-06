#include "log.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_LINE_COUNT 200
#define LOG_LINE_MAX 160

typedef struct {
  char text[LOG_LINE_MAX];
  uint64_t seq; // 0 means never written (slot not yet used)
} log_line_t;

static log_line_t g_lines[LOG_LINE_COUNT];
static uint64_t g_next_seq = 1;
static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t g_orig_vprintf = NULL;

// Strips ANSI color escapes (ESC [ ... final-byte) and the trailing CR/LF
// esp_log always appends. IDF builds the color codes into the vprintf format
// string when CONFIG_LOG_COLORS=y (the IDF default), so strip them here
// regardless of config rather than relying on it being off.
static void clean_line(char *dst, size_t dst_size, const char *src) {
  size_t len = 0;
  for (size_t i = 0; src[i] && len < dst_size - 1; i++) {
    if (src[i] == '\033' && src[i + 1] == '[') {
      i += 2;
      while (src[i] && !(src[i] >= '@' && src[i] <= '~')) {
        i++;
      }
      if (!src[i]) {
        break;
      }
      continue;
    }
    dst[len++] = src[i];
  }
  dst[len] = '\0';
  while (len > 0 && (dst[len - 1] == '\r' || dst[len - 1] == '\n')) {
    dst[--len] = '\0';
  }
}

static int printspy_vprintf(const char *fmt, va_list args) {
  char raw[LOG_LINE_MAX];
  va_list args_copy;
  va_copy(args_copy, args);
  vsnprintf(raw, sizeof(raw), fmt, args_copy);
  va_end(args_copy);

  portENTER_CRITICAL(&g_lock);
  log_line_t *slot = &g_lines[g_next_seq % LOG_LINE_COUNT];
  clean_line(slot->text, sizeof(slot->text), raw);
  slot->seq = g_next_seq;
  g_next_seq++;
  portEXIT_CRITICAL(&g_lock);

  if (g_orig_vprintf) {
    return g_orig_vprintf(fmt, args);
  }
  return 0;
}

esp_err_t printspy_log_init(void) {
  g_orig_vprintf = esp_log_set_vprintf(printspy_vprintf);
  return ESP_OK;
}

size_t printspy_log_read(uint64_t *cursor, char *buf, size_t buf_size) {
  if (buf_size == 0) {
    return 0;
  }

  size_t written = 0;
  buf[0] = '\0';

  portENTER_CRITICAL(&g_lock);
  uint64_t oldest_available =
      (g_next_seq > LOG_LINE_COUNT) ? (g_next_seq - LOG_LINE_COUNT) : 1;
  uint64_t seq = (*cursor < oldest_available) ? oldest_available : *cursor;

  while (seq < g_next_seq) {
    log_line_t *slot = &g_lines[seq % LOG_LINE_COUNT];
    if (slot->seq != seq) {
      seq++; // overwritten mid-read by a concurrent writer - skip, not fatal
      continue;
    }
    size_t line_len = strlen(slot->text);
    if (written + line_len + 2 > buf_size) { // +1 newline, +1 null terminator
      break;
    }
    memcpy(buf + written, slot->text, line_len);
    written += line_len;
    buf[written++] = '\n';
    seq++;
  }
  buf[written] = '\0';
  *cursor = seq;
  portEXIT_CRITICAL(&g_lock);

  return written;
}
