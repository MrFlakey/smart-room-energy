// Calibrated analog input on one GPIO, built on the ESP-IDF ADC oneshot driver.
// Several inputs on the same ADC unit share one unit handle, so the LM35, light
// and rotation drivers can all run on ADC1 together.
//
// Create and delete inputs from one task (normally at startup); reads are safe
// from any task.
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct analog_in_t *analog_in_handle_t;

typedef struct {
    int gpio;            // must be an ADC-capable pin (ADC1 recommended)
    adc_atten_t atten;   // input range; ADC_ATTEN_DB_12 covers about 0..3.1 V
    int samples;         // readings averaged per call (1 = no averaging)
} analog_in_config_t;

#define ANALOG_IN_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                      \
    .atten = ADC_ATTEN_DB_12,           \
    .samples = 16,                      \
}

esp_err_t analog_in_create(const analog_in_config_t *config, analog_in_handle_t *ret_handle);
esp_err_t analog_in_delete(analog_in_handle_t handle);

// Averaged raw reading, 0..4095 on the ESP32
esp_err_t analog_in_read_raw(analog_in_handle_t handle, int *raw);

// Averaged reading in millivolts. Uses the chip's factory calibration when
// available, otherwise a linear estimate (see analog_in_is_calibrated).
esp_err_t analog_in_read_mv(analog_in_handle_t handle, int *mv);

bool analog_in_is_calibrated(analog_in_handle_t handle);

#ifdef __cplusplus
}
#endif
