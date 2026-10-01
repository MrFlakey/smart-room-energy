// Debounced pushbutton. Each button is sampled every 5 ms by an esp_timer,
// so contact bounce never reaches your code.
//
// Two ways to use it:
//  - poll: button_is_pressed() for the current state, button_was_pressed() to
//    catch each press exactly once (handy in a loop that runs every 100 ms);
//  - callback: set .callback and get PRESSED / RELEASED / LONG_PRESS events.
//    The callback runs in the esp_timer task: keep it short and never block
//    in it (set a flag, give a semaphore, or post to a queue).
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct button_t *button_handle_t;

typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS,   // once per hold, after long_press_ms
} button_event_t;

typedef void (*button_cb_t)(button_handle_t button, button_event_t event, void *user_ctx);

typedef struct {
    int gpio;
    bool active_low;          // true: button connects the pin to GND (internal pull-up used)
    uint32_t debounce_ms;
    uint32_t long_press_ms;   // 0 disables LONG_PRESS
    button_cb_t callback;     // optional
    void *user_ctx;           // passed to the callback
} button_config_t;

#define BUTTON_DEFAULT_CONFIG(pin) { \
    .gpio = (pin),                   \
    .active_low = true,              \
    .debounce_ms = 30,               \
    .long_press_ms = 1000,           \
    .callback = NULL,                \
    .user_ctx = NULL,                \
}

esp_err_t button_create(const button_config_t *config, button_handle_t *ret_handle);
esp_err_t button_delete(button_handle_t handle);

bool button_is_pressed(button_handle_t handle);

// True once for each press since the last call
bool button_was_pressed(button_handle_t handle);

int button_get_gpio(button_handle_t handle);

#ifdef __cplusplus
}
#endif
