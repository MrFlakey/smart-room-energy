# analog_in: shared ADC helper

Reads one analog pin and returns calibrated millivolts, using the ESP-IDF ADC oneshot driver. The `lm35`, `light_sensor` and `rotation_sensor` drivers are built on it. Inputs on the same ADC unit share one unit handle, which is what lets those three sensors all use ADC1 together. You only need to use it directly for an analog part that has no driver of its own.

Header: `analog_in.h` · Depends on: `esp_adc`

## Pins

Any ADC-capable GPIO. Use ADC1 pins (GPIO32 to GPIO39 on the ESP32), because ADC2 stops working while Wi-Fi is on. The driver logs a warning if you pick an ADC2 pin.

## Configuration: `analog_in_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin to read. |
| `atten` | `adc_atten_t` | `ADC_ATTEN_DB_12` | Input range. `DB_12` covers about 0 to 3.1 V, `DB_6` about 0 to 1.75 V, `DB_2_5` about 0 to 1.25 V, `DB_0` about 0 to 0.95 V. A smaller range gives finer resolution. |
| `samples` | `int` | `16` | Readings averaged per call. 1 means no averaging. Values below 1 are treated as 1. |

Get the defaults with `ANALOG_IN_DEFAULT_CONFIG(pin)`.

## Functions

### `esp_err_t analog_in_create(const analog_in_config_t *config, analog_in_handle_t *ret_handle)`
Sets up the pin and returns a handle in `ret_handle`. It also sets up the chip's factory calibration when available. Returns `ESP_ERR_INVALID_ARG` for a NULL argument or a pin that isn't an ADC pin, and `ESP_ERR_NO_MEM` if memory runs out.

### `esp_err_t analog_in_delete(analog_in_handle_t handle)`
Frees the input. The ADC unit is released when the last input on it is deleted.

### `esp_err_t analog_in_read_raw(analog_in_handle_t handle, int *raw)`
Writes the average raw reading, 0 to 4095 on the ESP32, to `raw`.

### `esp_err_t analog_in_read_mv(analog_in_handle_t handle, int *mv)`
Writes the average reading in millivolts to `mv`. Uses the factory calibration if there is one. Otherwise it estimates linearly from the range set by `atten`.

### `bool analog_in_is_calibrated(analog_in_handle_t handle)`
Returns true if readings use the factory calibration. False means `read_mv` is an estimate.

## Example

```c
#include "analog_in.h"

analog_in_config_t cfg = ANALOG_IN_DEFAULT_CONFIG(36);
analog_in_handle_t in;
ESP_ERROR_CHECK(analog_in_create(&cfg, &in));

int mv;
if (analog_in_read_mv(in, &mv) == ESP_OK) {
    printf("%d mV\n", mv);
}
```

## Notes

- Create and delete inputs from one task, normally at startup. Reads are safe from any task.
- Every ESP32 ADC reads poorly below about 100 mV and near the top of its range.
