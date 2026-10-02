# lm35: temperature sensor

Driver for the LM35 analog temperature sensor, which outputs 10 mV per °C. It reads the pin through [`analog_in`](../analog_in/README.md) and converts the voltage to degrees Celsius.

Header: `lm35.h` · Depends on: `analog_in`

## Wiring

| LM35 pin (flat side facing you, legs down) | ESP32 |
|---|---|
| Left | 5 V (VIN) |
| Middle (output) | GPIO34 (`PIN_LM35`) |
| Right | GND |

The LM35 needs at least 4 V to run, so power it from 5 V. Its output is only about 0.2 to 0.4 V at room temperature, so it's safe on the pin.

## Configuration: `lm35_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the middle leg. |
| `samples` | `int` | `32` | Readings averaged per call. The ESP32 ADC is noisy, so more samples give steadier readings. |
| `offset_c` | `float` | `0.0` | Added to every reading. Use it to correct against a thermometer you trust. |
| `atten` | `adc_atten_t` | `ADC_ATTEN_DB_2_5` | ADC range. `DB_2_5` covers 0 to about 1.25 V, which is up to about 125 °C. |

Get the defaults with `LM35_DEFAULT_CONFIG(pin)`.

## Functions

### `esp_err_t lm35_create(const lm35_config_t *config, lm35_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument or a non-ADC pin, and `ESP_ERR_NO_MEM` if memory runs out.

### `esp_err_t lm35_delete(lm35_handle_t handle)`
Frees the sensor and its ADC input.

### `esp_err_t lm35_read_celsius(lm35_handle_t handle, float *celsius)`
Writes the temperature to `celsius`, calculated as millivolts ÷ 10 + `offset_c`.

### `esp_err_t lm35_read_mv(lm35_handle_t handle, int *mv)`
Writes the raw output voltage in millivolts. Handy for checking the wiring: room temperature should read about 200 to 300 mV.

## Example

```c
#include "board_pins.h"
#include "lm35.h"

lm35_config_t cfg = LM35_DEFAULT_CONFIG(PIN_LM35);
cfg.offset_c = -0.5f;   // optional correction
lm35_handle_t temp;
ESP_ERROR_CHECK(lm35_create(&cfg, &temp));

float c;
if (lm35_read_celsius(temp, &c) == ESP_OK) {
    printf("%.1f C\n", c);
}
```

## Troubleshooting

- **About 0 °C or wildly wrong:** check the leg order. A reversed LM35 gets hot quickly.
- **Readings jump around:** raise `samples`, or add a 100 nF capacitor between the output and GND.
- The ESP32 ADC is inaccurate below about 100 mV, so temperatures under about 10 °C read less accurately.
