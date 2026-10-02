# hcsr04: ultrasonic distance sensor

Driver for HC-SR04 style ultrasonic sensors. It sends a 10 µs trigger pulse, then times the echo pulse with a GPIO interrupt and `esp_timer`, to 1 µs resolution. The calling task only waits until the echo comes back, or until the timeout.

Header: `hcsr04.h` · Depends on: `esp_driver_gpio`, `esp_timer`

## Wiring

| HC-SR04 pin | ESP32 |
|---|---|
| VCC | 5 V (VIN) |
| GND | GND |
| TRIG | GPIO18 (`PIN_US_TRIG`) |
| ECHO | GPIO19 (`PIN_US_ECHO`) **through a divider** |

ECHO outputs 5 V, which would damage the ESP32. Bring it down with two resistors:

```
 ECHO ──[ 1 kΩ ]──┬── GPIO19
                  │
               [ 2 kΩ ]
                  │
                 GND
```

## Configuration: `hcsr04_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `trig_gpio` | `int` | the pin you pass | Output pin to TRIG. |
| `echo_gpio` | `int` | the pin you pass | Input pin from ECHO, through the divider. The internal pull-down is enabled. |
| `timeout_ms` | `uint32_t` | `30` | How long to wait for an echo. 30 ms covers about 5 m. 0 means 30. |
| `air_temp_c` | `float` | `20.0` | Air temperature used to work out the speed of sound. |

Get the defaults with `HCSR04_DEFAULT_CONFIG(trig, echo)`.

## Functions

### `esp_err_t hcsr04_create(const hcsr04_config_t *config, hcsr04_handle_t *ret_handle)`
Sets up both pins and the echo interrupt, and returns a handle. It installs the shared GPIO interrupt service if it isn't installed yet. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

### `esp_err_t hcsr04_delete(hcsr04_handle_t handle)`
Removes the interrupt, resets both pins and frees the sensor.

### `esp_err_t hcsr04_measure_us(hcsr04_handle_t handle, uint32_t *echo_us)`
Sends one ping and writes the echo pulse length in microseconds. Returns `ESP_ERR_TIMEOUT` if no echo arrives within `timeout_ms`. That means nothing is in range, or ECHO isn't connected.

### `esp_err_t hcsr04_measure_cm(hcsr04_handle_t handle, float *distance_cm)`
Sends one ping and writes the distance in centimetres. The speed of sound is (331.3 + 0.606 × `air_temp_c`) m/s, halved because the sound goes there and back. Returns `ESP_ERR_TIMEOUT` like `measure_us`.

### `void hcsr04_set_air_temp(hcsr04_handle_t handle, float celsius)`
Updates the air temperature used by `measure_cm`. The speed of sound changes about 0.17 % per °C, so feeding in the LM35 reading makes distances slightly more accurate.

## Example

```c
#include "board_pins.h"
#include "hcsr04.h"

hcsr04_config_t cfg = HCSR04_DEFAULT_CONFIG(PIN_US_TRIG, PIN_US_ECHO);
hcsr04_handle_t us;
ESP_ERROR_CHECK(hcsr04_create(&cfg, &us));

float cm;
esp_err_t err = hcsr04_measure_cm(us, &cm);
if (err == ESP_OK) {
    printf("%.1f cm\n", cm);
} else if (err == ESP_ERR_TIMEOUT) {
    printf("nothing in range\n");
}
```

## Notes

- Leave at least 60 ms between pings, so echoes from the last ping have died out.
- One ping at a time per sensor. Calls from several tasks are safe, because they queue up.
- Useful range is about 2 cm to 4 m. Soft or angled surfaces may not reflect enough to measure.
