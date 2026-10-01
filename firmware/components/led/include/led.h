// LED with dimming, driven by the ESP32's LEDC PWM peripheral.
// Wire: GPIO -> 220 ohm -> LED long leg, short leg -> GND.
//
// Each LED needs its own LEDC channel. LEDs can share a LEDC timer as long as
// they use the same frequency; the fan uses a different timer by default.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct led_t *led_handle_t;

typedef struct {
    int gpio;
    ledc_timer_t timer;
    ledc_channel_t channel;
    uint32_t freq_hz;
    bool active_low;   // true if the LED is wired from 3V3 to the pin (pin low = on)
} led_config_t;

#define LED_DEFAULT_CONFIG(pin, ch) { \
    .gpio = (pin),                    \
    .timer = LEDC_TIMER_0,            \
    .channel = (ch),                  \
    .freq_hz = 5000,                  \
    .active_low = false,              \
}

esp_err_t led_create(const led_config_t *config, led_handle_t *ret_handle);
esp_err_t led_delete(led_handle_t handle);

esp_err_t led_set_brightness(led_handle_t handle, uint8_t level);  // 0 = off, 255 = full
uint8_t led_get_brightness(led_handle_t handle);
esp_err_t led_on(led_handle_t handle);       // full brightness
esp_err_t led_off(led_handle_t handle);
esp_err_t led_toggle(led_handle_t handle);   // off <-> full
bool led_is_on(led_handle_t handle);

#ifdef __cplusplus
}
#endif
