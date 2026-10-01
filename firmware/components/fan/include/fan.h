// DC fan module with a built-in switch (3 pins: VCC, GND, signal).
// The signal pin gets PWM from the LEDC peripheral to set the speed.
// Power the module from 5 V (through the wattmeter if you want it metered).
//
// If the fan only switches on and off without changing speed, the module
// doesn't support PWM; fan_on/fan_off still work.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fan_t *fan_handle_t;

typedef struct {
    int gpio;
    ledc_timer_t timer;      // keep it different from the LEDs' timer if the frequency differs
    ledc_channel_t channel;
    uint32_t freq_hz;        // 1 kHz suits most transistor modules; try 20000+ if it whines
    bool invert;             // true if the module runs the fan when the signal is LOW
    uint8_t min_speed;       // speeds 1..min_speed are raised to this, so the fan doesn't stall
} fan_config_t;

#define FAN_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                \
    .timer = LEDC_TIMER_1,        \
    .channel = LEDC_CHANNEL_2,    \
    .freq_hz = 1000,              \
    .invert = false,              \
    .min_speed = 0,               \
}

esp_err_t fan_create(const fan_config_t *config, fan_handle_t *ret_handle);
esp_err_t fan_delete(fan_handle_t handle);

esp_err_t fan_set_speed(fan_handle_t handle, uint8_t speed);       // 0 = off, 255 = full
esp_err_t fan_set_percent(fan_handle_t handle, uint8_t percent);   // 0..100
uint8_t fan_get_speed(fan_handle_t handle);                        // 0..255, after min_speed
esp_err_t fan_on(fan_handle_t handle);
esp_err_t fan_off(fan_handle_t handle);
bool fan_is_on(fan_handle_t handle);

#ifdef __cplusplus
}
#endif
