# fan: PWM fan module

Driver for a DC fan module with a built-in switch (3 pins: VCC, GND, signal). It sets the speed with PWM on the signal pin, from the ESP32's LEDC peripheral at 10-bit resolution.

Header: `fan.h` · Depends on: `esp_driver_ledc`

## Wiring

| Fan module pin | ESP32 |
|---|---|
| VCC | 5 V. Use wattmeter IN− to measure the fan, or VIN without the wattmeter. |
| GND | GND |
| Signal | GPIO25 (`PIN_FAN`) |

The motor runs from 5 V. The ESP32 only sends the 3.3 V control signal, so no extra parts are needed.

## Configuration: `fan_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin connected to the signal pin. |
| `timer` | `ledc_timer_t` | `LEDC_TIMER_1` | LEDC timer. Kept apart from the LEDs' timer 0 because the frequency is different. |
| `channel` | `ledc_channel_t` | `LEDC_CHANNEL_2` | LEDC channel. Must not clash with an LED's channel. |
| `freq_hz` | `uint32_t` | `1000` | PWM frequency. 1 kHz suits most modules. Try 20000 or more if the fan whines. 0 means 1000. |
| `invert` | `bool` | `false` | Set true if the module runs the fan when the signal is low. |
| `min_speed` | `uint8_t` | `0` | Speeds from 1 up to `min_speed` are raised to `min_speed`, so the fan doesn't stall at low speed. 0 turns this off. |

Get the defaults with `FAN_DEFAULT_CONFIG(pin)`.

## Functions

### `esp_err_t fan_create(const fan_config_t *config, fan_handle_t *ret_handle)`
Sets up the LEDC timer and channel with the fan off, and returns a handle. Returns an LEDC error if the timer or channel can't be set up, or `ESP_ERR_NO_MEM`.

### `esp_err_t fan_delete(fan_handle_t handle)`
Turns the fan off and frees it.

### `esp_err_t fan_set_speed(fan_handle_t handle, uint8_t speed)`
Sets the speed: 0 is off, 255 is full speed. `min_speed` applies.

### `esp_err_t fan_set_percent(fan_handle_t handle, uint8_t percent)`
Sets the speed as 0 to 100 %. Values above 100 are treated as 100. It's converted to 0 to 255 and passed to `fan_set_speed`.

### `uint8_t fan_get_speed(fan_handle_t handle)`
Returns the speed actually set, 0 to 255, after `min_speed` is applied.

### `esp_err_t fan_on(fan_handle_t handle)`
Full speed. Same as `fan_set_speed(handle, 255)`.

### `esp_err_t fan_off(fan_handle_t handle)`
Off. Same as `fan_set_speed(handle, 0)`.

### `bool fan_is_on(fan_handle_t handle)`
Returns true if the speed is above 0.

## Example

```c
#include "board_pins.h"
#include "fan.h"

fan_config_t cfg = FAN_DEFAULT_CONFIG(PIN_FAN);
cfg.min_speed = 80;   // optional: avoid stalling at low speed
fan_handle_t fan;
ESP_ERROR_CHECK(fan_create(&cfg, &fan));

fan_set_percent(fan, 60);
fan_off(fan);
```

## Troubleshooting

- **Doesn't spin at all:** some modules need a 5 V signal. A small transistor fixes that.
- **Only switches on and off, no speed change:** the module doesn't support PWM. `fan_on` and `fan_off` still work.
- **Runs when it should be off:** set `invert = true`.
- **Whines:** raise `freq_hz` to 20000 or more.
