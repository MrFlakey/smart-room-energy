// HC-SR04 style ultrasonic distance sensor.
// The echo pulse is timed with a GPIO interrupt and esp_timer (1 us resolution),
// so a measurement blocks the calling task only until the echo returns.
//
// Power the sensor from 5 V and put ECHO through a 1k/2k divider: it outputs 5 V.
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hcsr04_t *hcsr04_handle_t;

typedef struct {
    int trig_gpio;
    int echo_gpio;
    uint32_t timeout_ms;   // give up waiting for the echo after this; 30 ms is about 5 m
    float air_temp_c;      // used for the speed of sound; change later with hcsr04_set_air_temp
} hcsr04_config_t;

#define HCSR04_DEFAULT_CONFIG(trig, echo) { \
    .trig_gpio = (trig),                    \
    .echo_gpio = (echo),                    \
    .timeout_ms = 30,                       \
    .air_temp_c = 20.0f,                    \
}

esp_err_t hcsr04_create(const hcsr04_config_t *config, hcsr04_handle_t *ret_handle);
esp_err_t hcsr04_delete(hcsr04_handle_t handle);

// One ping. Returns ESP_ERR_TIMEOUT when nothing is in range (or the wiring is off).
// Leave at least 60 ms between pings so old echoes die out.
esp_err_t hcsr04_measure_us(hcsr04_handle_t handle, uint32_t *echo_us);
esp_err_t hcsr04_measure_cm(hcsr04_handle_t handle, float *distance_cm);

// Speed of sound changes about 0.17 % per degree; feed it the LM35 reading if you like
void hcsr04_set_air_temp(hcsr04_handle_t handle, float celsius);

#ifdef __cplusplus
}
#endif
