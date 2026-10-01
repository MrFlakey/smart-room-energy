#include "analog_in.h"

#include <stdlib.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "analog_in";

#define MAX_UNITS 2

// One oneshot unit handle per ADC unit, shared by every input on that unit
static adc_oneshot_unit_handle_t s_units[MAX_UNITS];
static int s_unit_refs[MAX_UNITS];

struct analog_in_t {
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t atten;
    int samples;
    adc_cali_handle_t cali;  // NULL when the chip has no calibration data
};

static esp_err_t unit_acquire(adc_unit_t unit, adc_oneshot_unit_handle_t *out)
{
    int i = (int)unit;
    ESP_RETURN_ON_FALSE(i >= 0 && i < MAX_UNITS, ESP_ERR_INVALID_ARG, TAG, "bad ADC unit");
    if (s_unit_refs[i] == 0) {
        adc_oneshot_unit_init_cfg_t cfg = {
            .unit_id = unit,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&cfg, &s_units[i]), TAG, "ADC unit init failed");
    }
    s_unit_refs[i]++;
    *out = s_units[i];
    return ESP_OK;
}

static void unit_release(adc_unit_t unit)
{
    int i = (int)unit;
    if (--s_unit_refs[i] == 0) {
        adc_oneshot_del_unit(s_units[i]);
        s_units[i] = NULL;
    }
}

static adc_cali_handle_t cali_create(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t err = ESP_ERR_NOT_SUPPORTED;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&cfg, &handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    (void)channel;
    adc_cali_line_fitting_config_t cfg = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_line_fitting(&cfg, &handle);
#endif
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no ADC calibration (%s), millivolts are estimated", esp_err_to_name(err));
        return NULL;
    }
    return handle;
}

static void cali_delete(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_delete_scheme_curve_fitting(handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_delete_scheme_line_fitting(handle);
#endif
}

esp_err_t analog_in_create(const analog_in_config_t *config, analog_in_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");

    adc_unit_t unit;
    adc_channel_t channel;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(config->gpio, &unit, &channel), TAG,
                        "GPIO%d is not an ADC pin", config->gpio);
    if (unit != ADC_UNIT_1) {
        ESP_LOGW(TAG, "GPIO%d is on ADC2, which does not work while Wi-Fi is on", config->gpio);
    }

    struct analog_in_t *in = calloc(1, sizeof(*in));
    ESP_RETURN_ON_FALSE(in, ESP_ERR_NO_MEM, TAG, "out of memory");
    in->unit = unit;
    in->channel = channel;
    in->atten = config->atten;
    in->samples = config->samples > 0 ? config->samples : 1;

    adc_oneshot_unit_handle_t unit_handle;
    esp_err_t err = unit_acquire(unit, &unit_handle);
    if (err != ESP_OK) {
        free(in);
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = config->atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(unit_handle, channel, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "channel config failed: %s", esp_err_to_name(err));
        unit_release(unit);
        free(in);
        return err;
    }

    in->cali = cali_create(unit, channel, config->atten);
    *ret_handle = in;
    return ESP_OK;
}

esp_err_t analog_in_delete(analog_in_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    if (handle->cali) {
        cali_delete(handle->cali);
    }
    unit_release(handle->unit);
    free(handle);
    return ESP_OK;
}

esp_err_t analog_in_read_raw(analog_in_handle_t handle, int *raw)
{
    ESP_RETURN_ON_FALSE(handle && raw, ESP_ERR_INVALID_ARG, TAG, "null argument");
    adc_oneshot_unit_handle_t unit_handle = s_units[handle->unit];
    int sum = 0;
    for (int i = 0; i < handle->samples; i++) {
        int value;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(unit_handle, handle->channel, &value), TAG, "read failed");
        sum += value;
    }
    *raw = sum / handle->samples;
    return ESP_OK;
}

// Rough full-scale voltage per attenuation, used only without calibration
static int full_scale_mv(adc_atten_t atten)
{
    switch (atten) {
    case ADC_ATTEN_DB_0:   return 950;
    case ADC_ATTEN_DB_2_5: return 1250;
    case ADC_ATTEN_DB_6:   return 1750;
    default:               return 3100;
    }
}

esp_err_t analog_in_read_mv(analog_in_handle_t handle, int *mv)
{
    ESP_RETURN_ON_FALSE(handle && mv, ESP_ERR_INVALID_ARG, TAG, "null argument");
    int raw;
    ESP_RETURN_ON_ERROR(analog_in_read_raw(handle, &raw), TAG, "read failed");
    if (handle->cali) {
        return adc_cali_raw_to_voltage(handle->cali, raw, mv);
    }
    *mv = raw * full_scale_mv(handle->atten) / 4095;
    return ESP_OK;
}

bool analog_in_is_calibrated(analog_in_handle_t handle)
{
    return handle && handle->cali;
}
