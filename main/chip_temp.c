#include "chip_temp.h"

#include "driver/temperature_sensor.h"
#include "esp_log.h"

static const char *TAG = "printspy_chip_temp";
static temperature_sensor_handle_t tsens = NULL;

esp_err_t printspy_chip_temp_init(void) {
  // 10-80C covers idle through genuinely-concerning-hot; the driver picks
  // the best internal sub-range to fit it.
  temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
  esp_err_t err = temperature_sensor_install(&config, &tsens);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "temperature_sensor_install failed: %s", esp_err_to_name(err));
    return err;
  }
  return temperature_sensor_enable(tsens);
}

float printspy_chip_temp_read(void) {
  if (!tsens) {
    return 0.0f;
  }
  float celsius = 0.0f;
  temperature_sensor_get_celsius(tsens, &celsius);
  return celsius;
}
