// PIR motion sensor (HC-SR501 style, 3.3 V output, safe to wire directly).
// An interrupt tracks the output, so you can ask both "is there motion now?"
// and "how long since the last motion?" without polling fast.
//
// After power-up the sensor needs up to a minute to settle and may trigger
// falsely in that time.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pir_t *pir_handle_t;

typedef struct {
    int gpio;
    bool active_low;   // false for nearly every module (output goes high on motion)
    bool pull_down;    // enable the internal pull-down, handy for open-drain modules
} pir_config_t;

#define PIR_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                \
    .active_low = false,          \
    .pull_down = true,            \
}

esp_err_t pir_create(const pir_config_t *config, pir_handle_t *ret_handle);
esp_err_t pir_delete(pir_handle_t handle);

bool pir_is_motion(pir_handle_t handle);

// Microseconds since boot (esp_timer clock) when motion was last seen,
// or 0 if never. While motion is ongoing this is the current time.
int64_t pir_last_motion_us(pir_handle_t handle);

// Seconds since motion was last seen; 0 while motion is ongoing, -1 if never
float pir_seconds_since_motion(pir_handle_t handle);

#ifdef __cplusplus
}
#endif
