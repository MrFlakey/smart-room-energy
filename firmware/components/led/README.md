# led: dimmable LED

Driver for LEDs with brightness control. It drives each LED with PWM from the ESP32's LEDC peripheral, at 10-bit resolution.

Header: `led.h` · Depends on: `esp_driver_ledc`

## Wiring

Connect each LED as GPIO → 220 Ω resistor → long leg (+), with the short leg (−) to GND.

| LED | ESP32 |
|---|---|
| LED 1 | GPIO26 (`PIN_LED1`) |
| LED 2 | GPIO33 (`PIN_LED2`) |

For a brighter LED you can use 100 Ω instead of 220 Ω.

## Configuration: `led_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | LED pin. |
| `timer` | `ledc_timer_t` | `LEDC_TIMER_0` | LEDC timer. LEDs can share a timer if they use the same frequency. |
| `channel` | `ledc_channel_t` | the channel you pass | LEDC channel. **Every LED needs its own channel.** The fan uses `LEDC_CHANNEL_2` by default. |
| `freq_hz` | `uint32_t` | `5000` | PWM frequency. 0 means 5000. |
| `active_low` | `bool` | `false` | Set true if the LED is wired from 3V3 to the pin, so it lights when the pin is low. |

Get the defaults with `LED_DEFAULT_CONFIG(pin, channel)`.

## Functions

### `esp_err_t led_create(const led_config_t *config, led_handle_t *ret_handle)`
Sets up the LEDC timer and channel with the LED off, and returns a handle. Returns an LEDC error if the timer or channel can't be set up, or `ESP_ERR_NO_MEM`.

### `esp_err_t led_delete(led_handle_t handle)`
Turns the LED off and frees it.

### `esp_err_t led_set_brightness(led_handle_t handle, uint8_t level)`
Sets the brightness: 0 is off, 255 is fully on.

### `uint8_t led_get_brightness(led_handle_t handle)`
Returns the brightness last set, 0 to 255.

### `esp_err_t led_on(led_handle_t handle)`
Full brightness. Same as `led_set_brightness(handle, 255)`.

### `esp_err_t led_off(led_handle_t handle)`
Off. Same as `led_set_brightness(handle, 0)`.

### `esp_err_t led_toggle(led_handle_t handle)`
Switches between off and full brightness. Any brightness above 0 counts as on, so a dimmed LED goes off.

### `bool led_is_on(led_handle_t handle)`
Returns true if the brightness is above 0.

## Example

```c
#include "board_pins.h"
#include "led.h"

led_config_t c1 = LED_DEFAULT_CONFIG(PIN_LED1, LEDC_CHANNEL_0);
led_config_t c2 = LED_DEFAULT_CONFIG(PIN_LED2, LEDC_CHANNEL_1);
led_handle_t led1, led2;
ESP_ERROR_CHECK(led_create(&c1, &led1));
ESP_ERROR_CHECK(led_create(&c2, &led2));

led_on(led1);
led_set_brightness(led2, 64);   // about a quarter
led_toggle(led1);               // now off
```

## Notes

- Brightness isn't linear to the eye: going from 0 to 32 looks like a bigger step than going from 200 to 255.
- The ESP32 has 4 LEDC timers and 8 channels in this mode.
