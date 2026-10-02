# button: debounced pushbutton

Driver for pushbuttons. Each button is sampled every 5 ms by an `esp_timer`, and a change only counts once the pin has held steady for the debounce time. Contact bounce never reaches your code. You can poll the button, get callbacks, or both.

Header: `button.h` · Depends on: `esp_driver_gpio`, `esp_timer`

## Wiring

Connect one leg to the GPIO and the other to GND. The internal pull-up is used, so no resistor is needed.

| Button | ESP32 |
|---|---|
| Button 1 | GPIO14 (`PIN_BTN1`) |
| Button 2 | GPIO13 (`PIN_BTN2`) |
| Button 3 | GPIO23 (`PIN_BTN3`) |

## Configuration: `button_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Button pin. |
| `active_low` | `bool` | `true` | True when the button connects the pin to GND; this uses the internal pull-up. False when it connects to 3V3; this uses the internal pull-down. |
| `debounce_ms` | `uint32_t` | `30` | How long the pin must stay steady before a change counts. Rounded up to a multiple of 5 ms. |
| `long_press_ms` | `uint32_t` | `1000` | Hold time before a `LONG_PRESS` event. 0 turns long presses off. |
| `callback` | `button_cb_t` | `NULL` | Optional function called on each event. |
| `user_ctx` | `void *` | `NULL` | Passed to the callback unchanged. |

Get the defaults with `BUTTON_DEFAULT_CONFIG(pin)`.

## Events: `button_event_t`

| Event | When |
|---|---|
| `BUTTON_EVENT_PRESSED` | The button goes down (after debounce). |
| `BUTTON_EVENT_RELEASED` | The button comes back up (after debounce). |
| `BUTTON_EVENT_LONG_PRESS` | The button has been held for `long_press_ms`. Sent once per hold. |

Callback signature: `void callback(button_handle_t button, button_event_t event, void *user_ctx)`

## Functions

### `esp_err_t button_create(const button_config_t *config, button_handle_t *ret_handle)`
Sets up the pin, starts the 5 ms sampling timer, and returns a handle. A button held down at boot doesn't fire a press. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

### `esp_err_t button_delete(button_handle_t handle)`
Stops the timer, resets the pin and frees the button.

### `bool button_is_pressed(button_handle_t handle)`
Returns true while the button is held down (debounced).

### `bool button_was_pressed(button_handle_t handle)`
Returns true once for each press since the last call, then false until the next press. Ideal for a loop that checks every 100 ms or so, because it never misses a quick tap.

### `int button_get_gpio(button_handle_t handle)`
Returns the button's pin, or −1 for a NULL handle. Useful in a shared callback to tell buttons apart.

## Examples

Polling:

```c
#include "board_pins.h"
#include "button.h"

button_config_t cfg = BUTTON_DEFAULT_CONFIG(PIN_BTN1);
button_handle_t b1;
ESP_ERROR_CHECK(button_create(&cfg, &b1));

while (true) {
    if (button_was_pressed(b1)) {
        printf("Button 1 pressed\n");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

Callback:

```c
static void on_button(button_handle_t b, button_event_t ev, void *ctx)
{
    if (ev == BUTTON_EVENT_LONG_PRESS) {
        // set a flag or post to a queue; don't block here
    }
}

button_config_t cfg = BUTTON_DEFAULT_CONFIG(PIN_BTN3);
cfg.callback = on_button;
button_handle_t b3;
ESP_ERROR_CHECK(button_create(&cfg, &b3));
```

## Notes

- The callback runs in the `esp_timer` task, which all timers share. Keep it short and never block in it: set a flag, give a semaphore, or post to a queue.
- `RELEASED` follows every `PRESSED`, including after a long press.
