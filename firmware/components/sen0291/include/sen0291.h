// DFRobot SEN0291 I2C wattmeter (INA219 chip with a 0.01 ohm shunt).
// Uses the ESP-IDF i2c_master driver. Power its logic side ("+") from 3V3 so
// its pull-ups keep SDA/SCL at 3.3 V.
//
// Address switches: A0/A1 = 0/0 -> 0x40, 1/0 -> 0x41, 0/1 -> 0x44, 1/1 -> 0x45.
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sen0291_t *sen0291_handle_t;

typedef struct {
    // Pass an existing bus to share it with other I2C parts. Leave NULL and the
    // driver creates its own bus on i2c_port/sda_gpio/scl_gpio.
    i2c_master_bus_handle_t bus;
    i2c_port_num_t i2c_port;
    int sda_gpio;
    int scl_gpio;
    uint8_t address;
    uint32_t scl_speed_hz;
    uint32_t shunt_milliohm;  // 10 on the SEN0291
    float current_gain;       // reading multiplier, from comparing against a multimeter
} sen0291_config_t;

#define SEN0291_DEFAULT_CONFIG(sda, scl) { \
    .bus = NULL,                           \
    .i2c_port = -1,                        \
    .sda_gpio = (sda),                     \
    .scl_gpio = (scl),                     \
    .address = 0x45,                       \
    .scl_speed_hz = 100000,                \
    .shunt_milliohm = 10,                  \
    .current_gain = 1.0f,                  \
}

typedef struct {
    float bus_v;        // voltage on IN- (what the load sees)
    float shunt_mv;     // voltage across the shunt
    float current_ma;   // negative if IN+ and IN- are swapped
    float power_mw;     // bus_v * current_ma
} sen0291_reading_t;

// Fails with ESP_ERR_NOT_FOUND if nothing answers at the address
esp_err_t sen0291_create(const sen0291_config_t *config, sen0291_handle_t *ret_handle);
esp_err_t sen0291_delete(sen0291_handle_t handle);

esp_err_t sen0291_read(sen0291_handle_t handle, sen0291_reading_t *reading);
esp_err_t sen0291_read_bus_voltage(sen0291_handle_t handle, float *volts);
esp_err_t sen0291_read_current(sen0291_handle_t handle, float *milliamps);

// The bus the driver uses, so you can add other I2C devices to it
i2c_master_bus_handle_t sen0291_get_bus(sen0291_handle_t handle);

#ifdef __cplusplus
}
#endif
