# rotation_sensor: rotation sensor (knob)

Driver for a rotation sensor (potentiometer knob). It reads the knob's voltage through `analog_in` and can scale it to 0 to 100 %.

Header: `rotation_sensor.h` · Depends on: [`analog_in`](../analog_in/README.md)

## Wiring

| Sensor pin | ESP32 |
|---|---|
| VCC | **3V3** (not 5 V) |
| GND | GND |
| Signal | GPIO32 (`PIN_ROTATION`) |

Power it from 3V3 so its output can never go above what the ESP32 pin can take.

## Configuration: `rotation_sensor_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the signal pin. |
| `samples` | `int` | `16` | Readings averaged per call. |
| `min_mv` | `int` | `0` | Voltage that reads as 0 %. |
| `max_mv` | `int` | `3100` | Voltage that reads as 100 %. It must be above `min_mv`. |
| `invert` | `bool` | `false` | Flips the scale, so `min_mv` reads as 100 %. |

Get the defaults with `ROTATION_SENSOR_DEFAULT_CONFIG(pin)`. The ADC always uses the 12 dB range, which is about 0 to 3.1 V.

## Functions

### `esp_err_t rotation_sensor_create(const rotation_sensor_config_t *config, rotation_sensor_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument, a non-ADC pin, or `max_mv` not above `min_mv`. Returns `ESP_ERR_NO_MEM` if memory runs out.

### `esp_err_t rotation_sensor_delete(rotation_sensor_handle_t handle)`
Frees the sensor and its ADC input.

### `esp_err_t rotation_sensor_read_raw(rotation_sensor_handle_t handle, int *raw)`
Writes the average raw ADC reading, 0 to 4095.

### `esp_err_t rotation_sensor_read_mv(rotation_sensor_handle_t handle, int *mv)`
Writes the average voltage in millivolts.

### `esp_err_t rotation_sensor_read_percent(rotation_sensor_handle_t handle, float *percent)`
Writes the reading as 0 to 100 %, scaled between `min_mv` and `max_mv` and clamped to that range. If `invert` is set, the scale is flipped.

## Example

```c
#include "board_pins.h"
#include "rotation_sensor.h"

rotation_sensor_config_t cfg = ROTATION_SENSOR_DEFAULT_CONFIG(PIN_ROTATION);
rotation_sensor_handle_t sensor;
ESP_ERROR_CHECK(rotation_sensor_create(&cfg, &sensor));

float pct;
if (rotation_sensor_read_percent(sensor, &pct) == ESP_OK) {
    printf("%.0f %%\n", pct);
}
```

## Calibrating the 0 to 100 % range

Turn the knob fully to each end and note `rotation_sensor_read_mv()` at each. Put those values in `min_mv` and `max_mv` to get a full 0 to 100 % sweep. If turning clockwise makes the reading go down, set `invert = true`.
