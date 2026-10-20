# pir: motion sensor

Driver for PIR motion sensors such as the HC-SR501. A GPIO interrupt tracks the output, so you can ask "is there motion now?" and "how long since the last motion?" without polling quickly.

Header: `pir.h` · Depends on: `esp_driver_gpio`, `esp_timer`

## Wiring

| PIR pin | ESP32 |
|---|---|
| VCC | 5 V (VIN) |
| GND | GND |
| OUT | GPIO27 (`PIN_PIR`) |

The output is 3.3 V, so it connects straight to the pin.

## Configuration: `pir_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin connected to OUT. |
| `active_low` | `bool` | `false` | Set true only if your module's output goes low on motion. Nearly every module goes high. |
| `pull_down` | `bool` | `true` | Turns on the internal pull-down, so the pin reads "no motion" if OUT is disconnected. |

Get the defaults with `PIR_DEFAULT_CONFIG(pin)`.

## Functions

### `esp_err_t pir_create(const pir_config_t *config, pir_handle_t *ret_handle)`
Sets up the pin and its interrupt, and returns a handle. It installs the shared GPIO interrupt service if it isn't installed yet. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

### `esp_err_t pir_delete(pir_handle_t handle)`
Removes the interrupt, resets the pin and frees the sensor.

### `bool pir_is_motion(pir_handle_t handle)`
Returns true while the sensor reports motion.

### `int64_t pir_last_motion_us(pir_handle_t handle)`
Returns when motion was last seen, as microseconds since boot on the `esp_timer` clock. While motion is ongoing it returns the current time. Returns 0 if there has been no motion since boot.

### `float pir_seconds_since_motion(pir_handle_t handle)`
Returns the seconds since motion was last seen. Returns 0 while motion is ongoing, and −1 if there has been no motion since boot.

## Example

Turn the fan on with motion and off after 5 seconds without it. This is the test on the `testing` branch.

```c
#include "board_pins.h"
#include "pir.h"
#include "fan.h"

pir_config_t pir_cfg = PIR_DEFAULT_CONFIG(PIN_PIR);
pir_handle_t pir;
ESP_ERROR_CHECK(pir_create(&pir_cfg, &pir));

fan_config_t fan_cfg = FAN_DEFAULT_CONFIG(PIN_FAN);
fan_handle_t fan;
ESP_ERROR_CHECK(fan_create(&fan_cfg, &fan));

while (true) {
    if (pir_is_motion(pir)) {
        fan_on(fan);
    } else if (fan_is_on(fan) && pir_seconds_since_motion(pir) >= 5.0f) {
        fan_off(fan);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

## Notes

- After power-up the sensor needs up to a minute to settle, and it may trigger on its own during that time.
- HC-SR501 modules have two knobs. **Time** sets how long OUT stays high after motion; turn it fully anticlockwise for the shortest time, about 3 s. **Sensitivity** sets the range.
- The jumper selects single trigger (L) or repeat trigger (H). Repeat trigger (H) keeps OUT high while motion continues, which suits this driver best.
