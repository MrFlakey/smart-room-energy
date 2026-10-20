# sen0307: ultrasonic distance sensor

Driver for the DFRobot SEN0307 (Gravity URM09, analog version). The module measures distance itself, up to 30 times a second with built-in temperature compensation, and outputs a voltage that grows with distance. The driver reads that voltage through `analog_in` and converts it with the datasheet formula: distance (cm) = Vout (mV) × 520 / Vsupply (mV).

Header: `sen0307.h` · Depends on: [`analog_in`](../analog_in/README.md)

## Wiring

| Sensor pin | ESP32 |
|---|---|
| + | **3V3** (not 5 V) |
| − | GND |
| A | GPIO33 (`PIN_ULTRASONIC`) |

The module runs on 3.3 to 5.5 V. Power it from 3V3: its output can reach the supply voltage, and 5 V would damage the ESP32 pin. No divider is needed.

GPIO33 is used because it's one of the two ADC1 pins with internal pull resistors (GPIO32 and GPIO33), which `sen0307_check_connected()` needs. Any other ADC1 pin works for reading distances.

## Configuration: `sen0307_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to A. |
| `samples` | `int` | `16` | Readings averaged per call. |
| `supply_mv` | `int` | `3300` | The sensor's supply voltage. Its output scales with it, so this must match how it's powered. |
| `range_cm` | `float` | `520` | Distance at full output, from the datasheet formula. |

Get the defaults with `SEN0307_DEFAULT_CONFIG(pin)`. The ADC always uses the 12 dB range, which is about 0 to 3.1 V, or about 0 to 490 cm on a 3.3 V supply.

## Functions

### `esp_err_t sen0307_create(const sen0307_config_t *config, sen0307_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument, a non-ADC pin, or a `supply_mv` or `range_cm` that isn't above 0. Returns `ESP_ERR_NO_MEM` if memory runs out.

### `esp_err_t sen0307_delete(sen0307_handle_t handle)`
Frees the sensor and its ADC input.

### `esp_err_t sen0307_read_mv(sen0307_handle_t handle, int *mv)`
Writes the average output voltage in millivolts.

### `esp_err_t sen0307_read_cm(sen0307_handle_t handle, float *distance_cm)`
Writes the distance in centimetres: `mv × range_cm / supply_mv`.

### `esp_err_t sen0307_check_connected(sen0307_handle_t handle, bool *connected)`
Writes false if nothing drives the A pin, which is what happens when the cable is unplugged. It switches on the pin's internal pull-up and then its pull-down for a moment: a loose wire follows them to 3.3 V and 0 V, while the sensor's output holds its voltage. Takes two reads plus about 0.2 ms. Returns `ESP_ERR_NOT_SUPPORTED` on GPIO34 to 39, which have no internal pulls.

## Example

```c
#include "board_pins.h"
#include "sen0307.h"

sen0307_config_t cfg = SEN0307_DEFAULT_CONFIG(PIN_ULTRASONIC);
sen0307_handle_t us;
ESP_ERROR_CHECK(sen0307_create(&cfg, &us));

bool connected;
float cm;
if (sen0307_check_connected(us, &connected) == ESP_OK && !connected) {
    printf("sensor unplugged\n");
} else if (sen0307_read_cm(us, &cm) == ESP_OK) {
    printf("%.0f cm\n", cm);
}
```

## Notes

- The ESP32 ADC can't read the bottom ~140 mV of its range, so anything closer than about 20 cm reads as about 20 cm. Keep the threshold you compare against above that.
- The sensor's measuring range is 2 to 500 cm, with 1 cm resolution. Soft or angled surfaces may not reflect enough to measure.
- If the 3V3 rail isn't close to 3.3 V, measure it and set `supply_mv` to match.
