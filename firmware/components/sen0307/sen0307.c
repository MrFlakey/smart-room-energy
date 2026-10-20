#include "sen0307.h"

#include <stdlib.h>
#include "esp_check.h"

static const char *TAG = "sen0307";

struct sen0307_t {
    analog_in_handle_t adc;
    int supply_mv;
    float range_cm;
};

esp_err_t sen0307_create(const sen0307_config_t *config, sen0307_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    ESP_RETURN_ON_FALSE(config->supply_mv > 0 && config->range_cm > 0, ESP_ERR_INVALID_ARG, TAG,
                        "supply_mv and range_cm must be above 0");
    struct sen0307_t *dev = calloc(1, sizeof(*dev));
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
    dev->supply_mv = config->supply_mv;
    dev->range_cm = config->range_cm;
    *ret_handle = dev;
    return ESP_OK;
}

esp_err_t sen0307_delete(sen0307_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    analog_in_delete(handle->adc);
    free(handle);
    return ESP_OK;
}

esp_err_t sen0307_read_mv(sen0307_handle_t handle, int *mv)
{
    ESP_RETURN_ON_FALSE(handle && mv, ESP_ERR_INVALID_ARG, TAG, "null argument");
    return analog_in_read_mv(handle->adc, mv);
}

esp_err_t sen0307_read_cm(sen0307_handle_t handle, float *distance_cm)
{
    ESP_RETURN_ON_FALSE(handle && distance_cm, ESP_ERR_INVALID_ARG, TAG, "null argument");
    int mv;
    ESP_RETURN_ON_ERROR(analog_in_read_mv(handle->adc, &mv), TAG, "read failed");
    *distance_cm = (float)mv * handle->range_cm / handle->supply_mv;
    return ESP_OK;
}

esp_err_t sen0307_check_connected(sen0307_handle_t handle, bool *connected)
{
    ESP_RETURN_ON_FALSE(handle && connected, ESP_ERR_INVALID_ARG, TAG, "null argument");
    return analog_in_check_driven(handle->adc, connected);
}
