#include "hcsr04.h"

#include <stdbool.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "hcsr04";

struct hcsr04_t {
    gpio_num_t trig;
    gpio_num_t echo;
    uint32_t timeout_ms;
    float air_temp_c;
    SemaphoreHandle_t lock;       // one ping at a time
    SemaphoreHandle_t done;       // given by the ISR on the falling edge
    volatile bool armed;
    volatile int64_t rise_us;
    volatile int64_t fall_us;
};

static void echo_isr(void *arg)
{
    struct hcsr04_t *dev = arg;
    if (!dev->armed) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (gpio_get_level(dev->echo)) {
        dev->rise_us = now;
    } else if (dev->rise_us) {
        dev->fall_us = now;
        dev->armed = false;
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(dev->done, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}

esp_err_t hcsr04_create(const hcsr04_config_t *config, hcsr04_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    esp_err_t ret = ESP_OK;
    struct hcsr04_t *dev = calloc(1, sizeof(*dev));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "out of memory");
    dev->trig = config->trig_gpio;
    dev->echo = config->echo_gpio;
    dev->timeout_ms = config->timeout_ms ? config->timeout_ms : 30;
    dev->air_temp_c = config->air_temp_c;
    dev->lock = xSemaphoreCreateMutex();
    dev->done = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(dev->lock && dev->done, ESP_ERR_NO_MEM, err, TAG, "out of memory");

    gpio_config_t trig_cfg = {
        .pin_bit_mask = 1ULL << dev->trig,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&trig_cfg), err, TAG, "trig config failed");
    gpio_set_level(dev->trig, 0);

    gpio_config_t echo_cfg = {
        .pin_bit_mask = 1ULL << dev->echo,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // keeps the line low if ECHO is unplugged
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&echo_cfg), err, TAG, "echo config failed");

    // Shared with other drivers; already installed is fine
    ret = gpio_install_isr_service(0);
    ESP_GOTO_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, ret, err, TAG, "ISR service failed");
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(dev->echo, echo_isr, dev), err, TAG, "ISR add failed");

    *ret_handle = dev;
    return ESP_OK;

err:
    if (dev->lock) {
        vSemaphoreDelete(dev->lock);
    }
    if (dev->done) {
        vSemaphoreDelete(dev->done);
    }
    free(dev);
    return ret;
}

esp_err_t hcsr04_delete(hcsr04_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    gpio_isr_handler_remove(handle->echo);
    gpio_reset_pin(handle->echo);
    gpio_reset_pin(handle->trig);
    vSemaphoreDelete(handle->lock);
    vSemaphoreDelete(handle->done);
    free(handle);
    return ESP_OK;
}

esp_err_t hcsr04_measure_us(hcsr04_handle_t handle, uint32_t *echo_us)
{
    ESP_RETURN_ON_FALSE(handle && echo_us, ESP_ERR_INVALID_ARG, TAG, "null argument");
    xSemaphoreTake(handle->lock, portMAX_DELAY);

    xSemaphoreTake(handle->done, 0);  // drop a late echo from an earlier ping
    handle->rise_us = 0;
    handle->fall_us = 0;
    handle->armed = true;

    gpio_set_level(handle->trig, 1);
    esp_rom_delay_us(10);
    gpio_set_level(handle->trig, 0);

    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(handle->done, pdMS_TO_TICKS(handle->timeout_ms) + 1) == pdTRUE) {
        *echo_us = (uint32_t)(handle->fall_us - handle->rise_us);
    } else {
        handle->armed = false;
        ret = ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(handle->lock);
    return ret;
}

esp_err_t hcsr04_measure_cm(hcsr04_handle_t handle, float *distance_cm)
{
    ESP_RETURN_ON_FALSE(handle && distance_cm, ESP_ERR_INVALID_ARG, TAG, "null argument");
    uint32_t us;
    esp_err_t ret = hcsr04_measure_us(handle, &us);
    if (ret != ESP_OK) {
        return ret;
    }
    float speed_cm_per_us = (331.3f + 0.606f * handle->air_temp_c) / 10000.0f;
    *distance_cm = us * speed_cm_per_us / 2.0f;  // there and back
    return ESP_OK;
}

void hcsr04_set_air_temp(hcsr04_handle_t handle, float celsius)
{
    if (handle) {
        handle->air_temp_c = celsius;
    }
}
