#include "fan.h"

#include <stdlib.h>
#include "esp_check.h"

static const char *TAG = "fan";

#define FAN_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define FAN_RESOLUTION  LEDC_TIMER_10_BIT
#define FAN_DUTY_MAX    (1 << 10)  // LEDC treats 2^bits as fully on

struct fan_t {
    ledc_channel_t channel;
    uint8_t min_speed;
    uint8_t speed;
};

esp_err_t fan_create(const fan_config_t *config, fan_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");

    ledc_timer_config_t timer_cfg = {
        .speed_mode = FAN_SPEED_MODE,
        .duty_resolution = FAN_RESOLUTION,
        .timer_num = config->timer,
        .freq_hz = config->freq_hz ? config->freq_hz : 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "timer config failed");

    ledc_channel_config_t ch_cfg = {
        .gpio_num = config->gpio,
        .speed_mode = FAN_SPEED_MODE,
        .channel = config->channel,
        .timer_sel = config->timer,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = config->invert,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "channel config failed");

    struct fan_t *fan = calloc(1, sizeof(*fan));
    ESP_RETURN_ON_FALSE(fan, ESP_ERR_NO_MEM, TAG, "out of memory");
    fan->channel = config->channel;
    fan->min_speed = config->min_speed;
    *ret_handle = fan;
    return ESP_OK;
}

esp_err_t fan_delete(fan_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    fan_off(handle);
    free(handle);
    return ESP_OK;
}

esp_err_t fan_set_speed(fan_handle_t handle, uint8_t speed)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    if (speed > 0 && speed < handle->min_speed) {
        speed = handle->min_speed;
    }
    uint32_t duty = (uint32_t)speed * FAN_DUTY_MAX / 255;
    ESP_RETURN_ON_ERROR(ledc_set_duty(FAN_SPEED_MODE, handle->channel, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(FAN_SPEED_MODE, handle->channel), TAG, "update duty failed");
    handle->speed = speed;
    return ESP_OK;
}

esp_err_t fan_set_percent(fan_handle_t handle, uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    return fan_set_speed(handle, (uint8_t)((percent * 255 + 50) / 100));
}

uint8_t fan_get_speed(fan_handle_t handle)
{
    return handle ? handle->speed : 0;
}

esp_err_t fan_on(fan_handle_t handle)
{
    return fan_set_speed(handle, 255);
}

esp_err_t fan_off(fan_handle_t handle)
{
    return fan_set_speed(handle, 0);
}

bool fan_is_on(fan_handle_t handle)
{
    return handle && handle->speed > 0;
}
