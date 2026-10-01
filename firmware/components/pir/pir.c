#include "pir.h"

#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"

static const char *TAG = "pir";

struct pir_t {
    gpio_num_t gpio;
    bool active_low;
    volatile int64_t last_edge_us;  // time of the last change to "no motion"
    volatile bool seen;
};

static bool level_is_motion(struct pir_t *dev)
{
    return gpio_get_level(dev->gpio) != dev->active_low;
}

static void pir_isr(void *arg)
{
    struct pir_t *dev = arg;
    dev->seen = true;
    dev->last_edge_us = esp_timer_get_time();
}

esp_err_t pir_create(const pir_config_t *config, pir_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    esp_err_t ret = ESP_OK;
    struct pir_t *dev = calloc(1, sizeof(*dev));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "out of memory");
    dev->gpio = config->gpio;
    dev->active_low = config->active_low;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << dev->gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = config->pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&cfg), err, TAG, "gpio config failed");

    ret = gpio_install_isr_service(0);
    ESP_GOTO_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, ret, err, TAG, "ISR service failed");
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(dev->gpio, pir_isr, dev), err, TAG, "ISR add failed");

    if (level_is_motion(dev)) {
        dev->seen = true;
    }
    *ret_handle = dev;
    return ESP_OK;

err:
    free(dev);
    return ret;
}

esp_err_t pir_delete(pir_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    gpio_isr_handler_remove(handle->gpio);
    gpio_reset_pin(handle->gpio);
    free(handle);
    return ESP_OK;
}

bool pir_is_motion(pir_handle_t handle)
{
    return handle && level_is_motion(handle);
}

int64_t pir_last_motion_us(pir_handle_t handle)
{
    if (!handle || !handle->seen) {
        return 0;
    }
    if (level_is_motion(handle)) {
        return esp_timer_get_time();
    }
    return handle->last_edge_us;
}

float pir_seconds_since_motion(pir_handle_t handle)
{
    int64_t last = pir_last_motion_us(handle);
    if (last == 0) {
        return -1.0f;
    }
    return (esp_timer_get_time() - last) / 1e6f;
}
