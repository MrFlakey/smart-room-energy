#include "led.h"

#include <stdlib.h>
#include "esp_check.h"

static const char *TAG = "led";

#define LED_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define LED_RESOLUTION  LEDC_TIMER_10_BIT
#define LED_DUTY_MAX    (1 << 10)  // LEDC treats 2^bits as fully on

struct led_t {
    ledc_channel_t channel;
    int gpio;
    uint8_t level;
};

esp_err_t led_create(const led_config_t *config, led_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LED_SPEED_MODE,
        .duty_resolution = LED_RESOLUTION,
        .timer_num = config->timer,
        .freq_hz = config->freq_hz ? config->freq_hz : 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "timer config failed");

    ledc_channel_config_t ch_cfg = {
        .gpio_num = config->gpio,
        .speed_mode = LED_SPEED_MODE,
        .channel = config->channel,
        .timer_sel = config->timer,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = config->active_low,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "channel config failed");

    struct led_t *led = calloc(1, sizeof(*led));
    ESP_RETURN_ON_FALSE(led, ESP_ERR_NO_MEM, TAG, "out of memory");
    led->channel = config->channel;
    led->gpio = config->gpio;
    *ret_handle = led;
    return ESP_OK;
}

esp_err_t led_delete(led_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    ledc_set_duty(LED_SPEED_MODE, handle->channel, 0);
    ledc_update_duty(LED_SPEED_MODE, handle->channel);
    free(handle);
    return ESP_OK;
}

esp_err_t led_set_brightness(led_handle_t handle, uint8_t level)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    uint32_t duty = (uint32_t)level * LED_DUTY_MAX / 255;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LED_SPEED_MODE, handle->channel, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LED_SPEED_MODE, handle->channel), TAG, "update duty failed");
    handle->level = level;
    return ESP_OK;
}

uint8_t led_get_brightness(led_handle_t handle)
{
    return handle ? handle->level : 0;
}

esp_err_t led_on(led_handle_t handle)
{
    return led_set_brightness(handle, 255);
}

esp_err_t led_off(led_handle_t handle)
{
    return led_set_brightness(handle, 0);
}

esp_err_t led_toggle(led_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    return led_set_brightness(handle, handle->level ? 0 : 255);
}

bool led_is_on(led_handle_t handle)
{
    return handle && handle->level > 0;
}
