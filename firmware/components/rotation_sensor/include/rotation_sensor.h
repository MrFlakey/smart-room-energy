// Rotation sensor (potentiometer knob). Power it from 3V3.
// Set .invert = true if turning the knob clockwise makes the reading go down.
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "analog_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rotation_sensor_t *rotation_sensor_handle_t;

typedef struct {
    int gpio;
    int samples;        // readings averaged per call
    int min_mv;         // voltage that reads as 0 %
    int max_mv;         // voltage that reads as 100 %
    bool invert;        // true flips the scale (100 % at min_mv)
} rotation_sensor_config_t;

// Powered from 3V3 with 12 dB attenuation, the ESP32 reads about 0..3100 mV
#define ROTATION_SENSOR_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                 \
    .samples = 16,                 \
    .min_mv = 0,                   \
    .max_mv = 3100,                \
    .invert = false,               \
}

esp_err_t rotation_sensor_create(const rotation_sensor_config_t *config, rotation_sensor_handle_t *ret_handle);
esp_err_t rotation_sensor_delete(rotation_sensor_handle_t handle);

esp_err_t rotation_sensor_read_raw(rotation_sensor_handle_t handle, int *raw);   // 0..4095
esp_err_t rotation_sensor_read_mv(rotation_sensor_handle_t handle, int *mv);
esp_err_t rotation_sensor_read_percent(rotation_sensor_handle_t handle, float *percent);  // 0..100, clamped

#ifdef __cplusplus
}
#endif
