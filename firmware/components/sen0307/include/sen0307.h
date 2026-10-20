// DFRobot SEN0307 (Gravity URM09, analog): ultrasonic distance sensor with a
// voltage output. Datasheet: distance (cm) = Vout (mV) * 520 / Vsupply (mV),
// for 2 to 500 cm, updated up to 30 times a second. The module does its own
// temperature compensation.
//
// Power it from 3V3 so its output can never exceed what the ESP32 pin takes.
// The ESP32 ADC can't read the bottom ~140 mV, so anything closer than about
// 20 cm reads as about 20 cm.
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "analog_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sen0307_t *sen0307_handle_t;

typedef struct {
    int gpio;            // ADC1 pin; on GPIO32/33 sen0307_check_connected also works
    int samples;         // readings averaged per call
    int supply_mv;       // the sensor's supply voltage; the output scales with it
    float range_cm;      // distance at full output (520 in the datasheet formula)
} sen0307_config_t;

#define SEN0307_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                    \
    .samples = 16,                    \
    .supply_mv = 3300,                \
    .range_cm = 520.0f,               \
}

esp_err_t sen0307_create(const sen0307_config_t *config, sen0307_handle_t *ret_handle);
esp_err_t sen0307_delete(sen0307_handle_t handle);

esp_err_t sen0307_read_mv(sen0307_handle_t handle, int *mv);
esp_err_t sen0307_read_cm(sen0307_handle_t handle, float *distance_cm);

// Tells a connected sensor from a loose signal wire, using the pin's internal
// pulls (see analog_in_check_driven). ESP_ERR_NOT_SUPPORTED on GPIO34..39.
esp_err_t sen0307_check_connected(sen0307_handle_t handle, bool *connected);

#ifdef __cplusplus
}
#endif
