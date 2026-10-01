#include "lm35.h"

#include <stdlib.h>
#include "esp_check.h"

static const char *TAG = "lm35";

struct lm35_t {
    analog_in_handle_t adc;
    float offset_c;
};

esp_err_t lm35_create(const lm35_config_t *config, lm35_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    struct lm35_t *dev = calloc(1, sizeof(*dev));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "out of memory");

    analog_in_config_t adc_cfg = {
        .gpio = config->gpio,
        .atten = config->atten,
        .samples = config->samples,
    };
    esp_err_t err = analog_in_create(&adc_cfg, &dev->adc);
    if (err != ESP_OK) {
        free(dev);
        return err;
    }
    dev->offset_c = config->offset_c;
    *ret_handle = dev;
    return ESP_OK;
}

esp_err_t lm35_delete(lm35_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    analog_in_delete(handle->adc);
    free(handle);
    return ESP_OK;
}

esp_err_t lm35_read_mv(lm35_handle_t handle, int *mv)
{
    ESP_RETURN_ON_FALSE(handle && mv, ESP_ERR_INVALID_ARG, TAG, "null argument");
    return analog_in_read_mv(handle->adc, mv);
}

esp_err_t lm35_read_celsius(lm35_handle_t handle, float *celsius)
{
    ESP_RETURN_ON_FALSE(handle && celsius, ESP_ERR_INVALID_ARG, TAG, "null argument");
    int mv;
    ESP_RETURN_ON_ERROR(analog_in_read_mv(handle->adc, &mv), TAG, "read failed");
    *celsius = mv / 10.0f + handle->offset_c;
    return ESP_OK;
}
