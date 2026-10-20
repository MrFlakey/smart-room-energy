# light_sensor: ambient light sensor

Driver for an analog ambient light module (photoresistor or phototransistor). It reads the module's output voltage through `analog_in` and can scale it to 0 to 100 %.

Header: `light_sensor.h` · Depends on: [`analog_in`](../analog_in/README.md)

## Wiring

| Sensor pin | ESP32 |
|---|---|
| VCC | **3V3** (not 5 V) |
| GND | GND |
| Signal | GPIO35 (`PIN_LIGHT`) |

Power it from 3V3 so its output can never go above what the ESP32 pin can take.

## Configuration: `light_sensor_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the signal pin. |
| `samples` | `int` | `16` | Readings averaged per call. |
| `min_mv` | `int` | `0` | Voltage that reads as 0 %. |
| `max_mv` | `int` | `3100` | Voltage that reads as 100 %. It must be above `min_mv`. |
| `invert` | `bool` | `false` | Flips the scale, so `min_mv` reads as 100 %. |

Get the defaults with `LIGHT_SENSOR_DEFAULT_CONFIG(pin)`. The ADC always uses the 12 dB range, which is about 0 to 3.1 V.

## Functions

### `esp_err_t light_sensor_create(const light_sensor_config_t *config, light_sensor_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument, a non-ADC pin, or `max_mv` not above `min_mv`. Returns `ESP_ERR_NO_MEM` if memory runs out.

### `esp_err_t light_sensor_delete(light_sensor_handle_t handle)`
Frees the sensor and its ADC input.

### `esp_err_t light_sensor_read_raw(light_sensor_handle_t handle, int *raw)`
Writes the average raw ADC reading, 0 to 4095.

### `esp_err_t light_sensor_read_mv(light_sensor_handle_t handle, int *mv)`
Writes the average voltage in millivolts.

### `esp_err_t light_sensor_read_percent(light_sensor_handle_t handle, float *percent)`
Writes the reading as 0 to 100 %, scaled between `min_mv` and `max_mv` and clamped to that range. If `invert` is set, the scale is flipped.

## Example

```c
#include "board_pins.h"
#include "light_sensor.h"

light_sensor_config_t cfg = LIGHT_SENSOR_DEFAULT_CONFIG(PIN_LIGHT);
light_sensor_handle_t sensor;
ESP_ERROR_CHECK(light_sensor_create(&cfg, &sensor));

float pct;
if (light_sensor_read_percent(sensor, &pct) == ESP_OK) {
    printf("%.0f %%\n", pct);
}
```

## Calibrating the 0 to 100 % range

Cover the sensor and note `light_sensor_read_mv()`, then shine a light on it and note it again. Put those values in `min_mv` and `max_mv`. Most modules give a higher voltage in brighter light. If yours reads high in the dark, set `invert = true`.
