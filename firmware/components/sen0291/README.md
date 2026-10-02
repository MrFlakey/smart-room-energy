# sen0291: I2C wattmeter

Driver for the DFRobot SEN0291 wattmeter, which is an INA219 chip with a 0.01 Ω shunt resistor. It measures the voltage on the load side and the current through the meter, and works out the power. It uses the ESP-IDF `i2c_master` driver.

Header: `sen0291.h` · Depends on: `esp_driver_i2c`

## Wiring

**Data side** (4-pin connector):

| SEN0291 pin | ESP32 |
|---|---|
| + (VCC) | **3V3** (not 5 V) |
| − (GND) | GND |
| C (SCL) | GPIO22 (`PIN_I2C_SCL`) |
| D (SDA) | GPIO21 (`PIN_I2C_SDA`) |

Power it from 3V3. The board pulls SDA and SCL up to its own supply, and 5 V there would damage the ESP32.

**Power side** (screw terminals): the current you want to measure must flow through the meter. For the fan, connect VIN (5 V) to IN+, and IN− to the fan's VCC.

**Address switches:**

| A0 | A1 | Address |
|---|---|---|
| 0 | 0 | 0x40 |
| 1 | 0 | 0x41 |
| 0 | 1 | 0x44 |
| 1 | 1 | **0x45** (default) |

## Configuration: `sen0291_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `bus` | `i2c_master_bus_handle_t` | `NULL` | An existing I2C bus to share. If NULL, the driver creates its own bus on the pins below. |
| `i2c_port` | `i2c_port_num_t` | `-1` | I2C port when the driver creates the bus. −1 picks a free one. |
| `sda_gpio` | `int` | the pin you pass | SDA pin when the driver creates the bus. |
| `scl_gpio` | `int` | the pin you pass | SCL pin when the driver creates the bus. |
| `address` | `uint8_t` | `0x45` | Must match the address switches. |
| `scl_speed_hz` | `uint32_t` | `100000` | I2C clock. 0 means 100 kHz. |
| `shunt_milliohm` | `uint32_t` | `10` | Shunt resistor. It's 10 mΩ on the SEN0291. Must be above 0. |
| `current_gain` | `float` | `1.0` | Multiplies every current reading. Set it to multimeter mA ÷ SEN0291 mA to correct small errors. |

Get the defaults with `SEN0291_DEFAULT_CONFIG(sda, scl)`.

The driver sets the chip to a 32 V bus range and a ±320 mV shunt range (up to about 32 A with this shunt), with 16-sample averaging on both channels. Each new reading takes about 8.5 ms.

## Data: `sen0291_reading_t`

| Field | Unit | Meaning |
|---|---|---|
| `bus_v` | V | Voltage on IN−, which is what the load gets. 4 mV resolution. |
| `shunt_mv` | mV | Voltage across the shunt. 0.01 mV resolution. |
| `current_ma` | mA | Current through the meter. About 1 mA resolution. Negative if IN+ and IN− are swapped. |
| `power_mw` | mW | `bus_v` × `current_ma`. |

## Functions

### `esp_err_t sen0291_create(const sen0291_config_t *config, sen0291_handle_t *ret_handle)`
Creates the I2C bus if needed, checks that the meter answers at `address`, and configures it. Returns `ESP_ERR_NOT_FOUND` if nothing answers, `ESP_ERR_INVALID_ARG` for a NULL argument or a zero shunt, and the I2C driver's own error if the bus can't be set up.

### `esp_err_t sen0291_delete(sen0291_handle_t handle)`
Removes the meter from the bus. Deletes the bus too if the driver created it.

### `esp_err_t sen0291_read(sen0291_handle_t handle, sen0291_reading_t *reading)`
Reads voltage, current and power in one call.

### `esp_err_t sen0291_read_bus_voltage(sen0291_handle_t handle, float *volts)`
Reads only the load-side voltage.

### `esp_err_t sen0291_read_current(sen0291_handle_t handle, float *milliamps)`
Reads only the current, with `current_gain` applied.

### `i2c_master_bus_handle_t sen0291_get_bus(sen0291_handle_t handle)`
Returns the I2C bus the meter uses, so you can add other I2C devices to it.

## Example

```c
#include "board_pins.h"
#include "sen0291.h"

sen0291_config_t cfg = SEN0291_DEFAULT_CONFIG(PIN_I2C_SDA, PIN_I2C_SCL);
sen0291_handle_t meter;
ESP_ERROR_CHECK(sen0291_create(&cfg, &meter));

sen0291_reading_t r;
if (sen0291_read(meter, &r) == ESP_OK) {
    printf("%.2f V  %.1f mA  %.0f mW\n", r.bus_v, r.current_ma, r.power_mw);
}
```

## Troubleshooting

- **`ESP_ERR_NOT_FOUND` at startup:** SDA and SCL are easy to swap. Also check 3V3 and GND, and that the switches match `address`.
- **Negative current:** IN+ and IN− are swapped.
- **Current always about 0:** the load isn't powered through IN+ → IN−.
