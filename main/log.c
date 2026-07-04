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

// Strips ANSI SGR color escapes (esp_log's default format wraps every line
// in "\033[0;3Xm...\033[0m") and trailing CR/LF - a browser <pre>/textarea
// would otherwise show the raw escape bytes as garbage.
static void clean_line(char *dst, size_t dst_size, const char *src) {
  size_t di = 0;
  for (size_t si = 0; src[si] != '\0' && di + 1 < dst_size; si++) {
    if (src[si] == '\033' && src[si + 1] == '[') {
      si += 2;
      while (src[si] != '\0' && src[si] != 'm') {
        si++;
      }
      continue; // si now on 'm' (or end of string), loop's si++ advances past it
    }
    dst[di++] = src[si];
  }
  dst[di] = '\0';
  while (di > 0 && (dst[di - 1] == '\r' || dst[di - 1] == '\n')) {
    dst[--di] = '\0';
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
