// LM35 analog temperature sensor: 10 mV per degree C.
// Power it from 5 V (it needs at least 4 V); its output stays well under 3.3 V.
#pragma once

#include "esp_err.h"
#include "analog_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lm35_t *lm35_handle_t;

typedef struct {
    int gpio;
    int samples;         // readings averaged per call; the ESP32 ADC is noisy
    float offset_c;      // added to every reading, to correct against a known thermometer
    adc_atten_t atten;   // ADC_ATTEN_DB_2_5 covers 0..~1.25 V (up to about 125 C)
} lm35_config_t;

#define LM35_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                 \
    .samples = 32,                 \
    .offset_c = 0.0f,              \
    .atten = ADC_ATTEN_DB_2_5,     \
}

esp_err_t lm35_create(const lm35_config_t *config, lm35_handle_t *ret_handle);
esp_err_t lm35_delete(lm35_handle_t handle);

esp_err_t lm35_read_celsius(lm35_handle_t handle, float *celsius);
esp_err_t lm35_read_mv(lm35_handle_t handle, int *mv);

#ifdef __cplusplus
}
#endif
