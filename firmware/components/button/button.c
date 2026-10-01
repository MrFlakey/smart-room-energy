#include "button.h"

#include <stdatomic.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"

static const char *TAG = "button";

#define SAMPLE_MS 5

struct button_t {
    gpio_num_t gpio;
    bool active_low;
    uint32_t debounce_ticks;
    uint32_t long_press_ticks;
    button_cb_t callback;
    void *user_ctx;
    esp_timer_handle_t timer;

    bool raw_last;
    uint32_t stable_ticks;   // how long raw_last has held
    atomic_bool pressed;     // debounced state
    atomic_bool latched;     // for button_was_pressed
    uint32_t held_ticks;
    bool long_sent;
};

static void emit(struct button_t *b, button_event_t event)
{
    if (b->callback) {
        b->callback(b, event, b->user_ctx);
    }
}

static void sample(void *arg)
{
    struct button_t *b = arg;
    bool raw = gpio_get_level(b->gpio) != b->active_low;

    if (raw != b->raw_last) {
        b->raw_last = raw;
        b->stable_ticks = 0;
    } else if (b->stable_ticks < b->debounce_ticks) {
        b->stable_ticks++;
    }

    bool pressed = atomic_load(&b->pressed);
    if (b->stable_ticks >= b->debounce_ticks && raw != pressed) {
        atomic_store(&b->pressed, raw);
        if (raw) {
            b->held_ticks = 0;
            b->long_sent = false;
            atomic_store(&b->latched, true);
            emit(b, BUTTON_EVENT_PRESSED);
        } else {
            emit(b, BUTTON_EVENT_RELEASED);
        }
        return;
    }

    if (pressed && b->long_press_ticks && !b->long_sent && ++b->held_ticks >= b->long_press_ticks) {
        b->long_sent = true;
        emit(b, BUTTON_EVENT_LONG_PRESS);
    }
}

esp_err_t button_create(const button_config_t *config, button_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    esp_err_t ret = ESP_OK;
    struct button_t *b = calloc(1, sizeof(*b));
    ESP_RETURN_ON_FALSE(b, ESP_ERR_NO_MEM, TAG, "out of memory");
    b->gpio = config->gpio;
    b->active_low = config->active_low;
    b->debounce_ticks = (config->debounce_ms + SAMPLE_MS - 1) / SAMPLE_MS;
    b->long_press_ticks = config->long_press_ms / SAMPLE_MS;
    b->callback = config->callback;
    b->user_ctx = config->user_ctx;

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << b->gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = config->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&io), err, TAG, "gpio config failed");

    // Start from the current level so a button held at boot doesn't fire
    b->raw_last = gpio_get_level(b->gpio) != b->active_low;
    b->stable_ticks = b->debounce_ticks;
    atomic_init(&b->pressed, b->raw_last);
    atomic_init(&b->latched, false);
    b->long_sent = true;

    esp_timer_create_args_t args = {
        .callback = sample,
        .arg = b,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "button",
    };
    ESP_GOTO_ON_ERROR(esp_timer_create(&args, &b->timer), err, TAG, "timer create failed");
    ESP_GOTO_ON_ERROR(esp_timer_start_periodic(b->timer, SAMPLE_MS * 1000), err, TAG, "timer start failed");

    *ret_handle = b;
    return ESP_OK;

err:
    if (b->timer) {
        esp_timer_delete(b->timer);
    }
    free(b);
    return ret;
}

esp_err_t button_delete(button_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    esp_timer_stop(handle->timer);
    esp_timer_delete(handle->timer);
    gpio_reset_pin(handle->gpio);
    free(handle);
    return ESP_OK;
}

bool button_is_pressed(button_handle_t handle)
{
    return handle && atomic_load(&handle->pressed);
}

bool button_was_pressed(button_handle_t handle)
{
    return handle && atomic_exchange(&handle->latched, false);
}

int button_get_gpio(button_handle_t handle)
{
    return handle ? handle->gpio : -1;
}
