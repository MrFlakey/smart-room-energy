#include "sen0291.h"

#include <stdbool.h>
#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "sen0291";

#define REG_CONFIG      0x00
#define REG_SHUNT_V     0x01  // signed, 10 uV per bit
#define REG_BUS_V       0x02  // bits 15..3, 4 mV per bit

// 32 V bus range, +-320 mV shunt range, 12-bit with 16-sample averaging on
// both channels (about 8.5 ms per update), continuous conversion
#define CONFIG_VALUE    0x3E67

#define TIMEOUT_MS      50

struct sen0291_t {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    bool owns_bus;
    uint32_t shunt_milliohm;
    float current_gain;
};

static esp_err_t write_reg(sen0291_handle_t h, uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = { reg, value >> 8, value & 0xFF };
    return i2c_master_transmit(h->dev, buf, sizeof(buf), TIMEOUT_MS);
}

static esp_err_t read_reg(sen0291_handle_t h, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(h->dev, &reg, 1, buf, 2, TIMEOUT_MS),
                        TAG, "read 0x%02x failed", reg);
    *value = (uint16_t)buf[0] << 8 | buf[1];
    return ESP_OK;
}

esp_err_t sen0291_create(const sen0291_config_t *config, sen0291_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "null argument");
    ESP_RETURN_ON_FALSE(config->shunt_milliohm > 0, ESP_ERR_INVALID_ARG, TAG, "shunt must be > 0");
    esp_err_t ret = ESP_OK;
    struct sen0291_t *h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "out of memory");
    h->shunt_milliohm = config->shunt_milliohm;
    h->current_gain = config->current_gain > 0 ? config->current_gain : 1.0f;

    if (config->bus) {
        h->bus = config->bus;
    } else {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = config->i2c_port,
            .sda_io_num = config->sda_gpio,
            .scl_io_num = config->scl_gpio,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,  // the board has its own; these just help
        };
        ESP_GOTO_ON_ERROR(i2c_new_master_bus(&bus_cfg, &h->bus), err, TAG, "I2C bus init failed");
        h->owns_bus = true;
    }

    ret = i2c_master_probe(h->bus, config->address, TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "no device at 0x%02x: check SDA/SCL, 3V3, GND and the address switches",
                 config->address);
        ret = ESP_ERR_NOT_FOUND;
        goto err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->address,
        .scl_speed_hz = config->scl_speed_hz ? config->scl_speed_hz : 100000,
    };
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(h->bus, &dev_cfg, &h->dev), err, TAG, "add device failed");
    ESP_GOTO_ON_ERROR(write_reg(h, REG_CONFIG, CONFIG_VALUE), err, TAG, "config write failed");

    *ret_handle = h;
    return ESP_OK;

err:
    if (h->dev) {
        i2c_master_bus_rm_device(h->dev);
    }
    if (h->owns_bus) {
        i2c_del_master_bus(h->bus);
    }
    free(h);
    return ret;
}

esp_err_t sen0291_delete(sen0291_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "null handle");
    i2c_master_bus_rm_device(handle->dev);
    if (handle->owns_bus) {
        i2c_del_master_bus(handle->bus);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t sen0291_read_bus_voltage(sen0291_handle_t handle, float *volts)
{
    ESP_RETURN_ON_FALSE(handle && volts, ESP_ERR_INVALID_ARG, TAG, "null argument");
    uint16_t raw;
    ESP_RETURN_ON_ERROR(read_reg(handle, REG_BUS_V, &raw), TAG, "bus voltage read failed");
    *volts = (raw >> 3) * 0.004f;
    return ESP_OK;
}

static esp_err_t read_shunt_mv(sen0291_handle_t handle, float *mv)
{
    uint16_t raw;
    ESP_RETURN_ON_ERROR(read_reg(handle, REG_SHUNT_V, &raw), TAG, "shunt read failed");
    *mv = (int16_t)raw * 0.01f;
    return ESP_OK;
}

esp_err_t sen0291_read_current(sen0291_handle_t handle, float *milliamps)
{
    ESP_RETURN_ON_FALSE(handle && milliamps, ESP_ERR_INVALID_ARG, TAG, "null argument");
    float shunt_mv;
    ESP_RETURN_ON_ERROR(read_shunt_mv(handle, &shunt_mv), TAG, "current read failed");
    *milliamps = shunt_mv * 1000.0f / handle->shunt_milliohm * handle->current_gain;
    return ESP_OK;
}

esp_err_t sen0291_read(sen0291_handle_t handle, sen0291_reading_t *reading)
{
    ESP_RETURN_ON_FALSE(handle && reading, ESP_ERR_INVALID_ARG, TAG, "null argument");
    ESP_RETURN_ON_ERROR(read_shunt_mv(handle, &reading->shunt_mv), TAG, "read failed");
    ESP_RETURN_ON_ERROR(sen0291_read_bus_voltage(handle, &reading->bus_v), TAG, "read failed");
    reading->current_ma = reading->shunt_mv * 1000.0f / handle->shunt_milliohm * handle->current_gain;
    reading->power_mw = reading->bus_v * reading->current_ma;
    return ESP_OK;
}

i2c_master_bus_handle_t sen0291_get_bus(sen0291_handle_t handle)
{
    return handle ? handle->bus : NULL;
}
