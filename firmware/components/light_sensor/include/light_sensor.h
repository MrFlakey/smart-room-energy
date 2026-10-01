// Analog ambient light sensor (photoresistor or phototransistor module).
// Power it from 3V3 so its output can never exceed what the ESP32 pin takes.
// Most modules give a higher voltage in brighter light; if yours reads high in
// the dark, set .invert = true.
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "analog_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct light_sensor_t *light_sensor_handle_t;

typedef struct {
    int gpio;
    int samples;        // readings averaged per call
    int min_mv;         // voltage that reads as 0 %
    int max_mv;         // voltage that reads as 100 %
    bool invert;        // true flips the scale (100 % at min_mv)
} light_sensor_config_t;

// Powered from 3V3 with 12 dB attenuation, the ESP32 reads about 0..3100 mV
#define LIGHT_SENSOR_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                 \
    .samples = 16,                 \
    .min_mv = 0,                   \
    .max_mv = 3100,                \
    .invert = false,               \
}

esp_err_t light_sensor_create(const light_sensor_config_t *config, light_sensor_handle_t *ret_handle);
esp_err_t light_sensor_delete(light_sensor_handle_t handle);

esp_err_t light_sensor_read_raw(light_sensor_handle_t handle, int *raw);   // 0..4095
esp_err_t light_sensor_read_mv(light_sensor_handle_t handle, int *mv);
esp_err_t light_sensor_read_percent(light_sensor_handle_t handle, float *percent);  // 0..100, clamped

#ifdef __cplusplus
}
#endif
