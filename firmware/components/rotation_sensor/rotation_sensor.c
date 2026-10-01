#include "rotation_sensor.h"

#include <stdlib.h>
#include "esp_check.h"

static const char *TAG = "rotation_sensor";

struct rotation_sensor_t {
    analog_in_handle_t adc;
    int min_mv;
    int max_mv;
    bool invert;
};

esp_err_t rotation_sensor_create(const rotation_sensor_config_t *config, rotation_sensor_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    ESP_RETURN_ON_FALSE(config->max_mv > config->min_mv, ESP_ERR_INVALID_ARG, TAG, "max_mv must be above min_mv");
    struct rotation_sensor_t *dev = calloc(1, sizeof(*dev));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "out of memory");

    analog_in_config_t adc_cfg = {
        .gpio = config->gpio,
        .atten = ADC_ATTEN_DB_12,
        .samples = config->samples,
    };
    esp_err_t err = analog_in_create(&adc_cfg, &dev->adc);
    if (err != ESP_OK) {
        free(dev);
        return err;
    }
    dev->min_mv = config->min_mv;
    dev->max_mv = config->max_mv;
    dev->invert = config->invert;
    *ret_handle = dev;
    return ESP_OK;
}

esp_err_t rotation_sensor_delete(rotation_sensor_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    analog_in_delete(handle->adc);
    free(handle);
    return ESP_OK;
}

esp_err_t rotation_sensor_read_raw(rotation_sensor_handle_t handle, int *raw)
{
    ESP_RETURN_ON_FALSE(handle && raw, ESP_ERR_INVALID_ARG, TAG, "null argument");
    return analog_in_read_raw(handle->adc, raw);
}

esp_err_t rotation_sensor_read_mv(rotation_sensor_handle_t handle, int *mv)
{
    ESP_RETURN_ON_FALSE(handle && mv, ESP_ERR_INVALID_ARG, TAG, "null argument");
    return analog_in_read_mv(handle->adc, mv);
}

esp_err_t rotation_sensor_read_percent(rotation_sensor_handle_t handle, float *percent)
{
    ESP_RETURN_ON_FALSE(handle && percent, ESP_ERR_INVALID_ARG, TAG, "null argument");
    int mv;
    ESP_RETURN_ON_ERROR(analog_in_read_mv(handle->adc, &mv), TAG, "read failed");
    float p = 100.0f * (mv - handle->min_mv) / (handle->max_mv - handle->min_mv);
    if (p < 0.0f) {
        p = 0.0f;
    } else if (p > 100.0f) {
        p = 100.0f;
    }
    *percent = handle->invert ? 100.0f - p : p;
    return ESP_OK;
}
