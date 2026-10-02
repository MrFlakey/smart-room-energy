# Smart Room drivers (ESP-IDF)

Drivers for every sensor and actuator in the Smart Room, written for **ESP-IDF v6.0** on an **ESP32 DevKit v1**. There's no application code: `main/main.c` is an empty `app_main` for you to fill in.

Everything uses the current IDF drivers only (ADC oneshot with calibration, `i2c_master`, LEDC, GPIO, `esp_timer`), so nothing depends on the legacy drivers that v6 removed.

## Layout

```
firmware/
├── CMakeLists.txt          project file
├── main/
│   ├── main.c              your code (empty app_main)
│   └── board_pins.h        default pin map, change it to match your wiring
└── components/
    ├── analog_in/          shared ADC helper used by the three analog drivers
    ├── lm35/               temperature
    ├── light_sensor/       ambient light
    ├── rotation_sensor/    knob
    ├── hcsr04/             ultrasonic distance
    ├── pir/                motion
    ├── sen0291/            I2C wattmeter
    ├── button/             debounced pushbuttons
    ├── led/                dimmable LEDs
    └── fan/                PWM fan module
```

Each driver has its own `README.md` in its folder; the full reference is also collected below. Every driver works the same way: fill a config struct (each has a `*_DEFAULT_CONFIG(...)` macro), call `*_create()` to get a handle, then call the read/set functions with that handle. All functions return `esp_err_t` (`ESP_OK` on success) unless they return a plain value. `main` can include any driver header directly, because IDF links every component into `main` automatically.

## Building with the ESP-IDF VS Code extension

1. **File → Open Folder** and pick `firmware` (the folder with the top-level `CMakeLists.txt`).
2. Set the target: Command Palette → **ESP-IDF: Set Espressif Device Target** → `esp32` → *ESP32 chip (via ESP USB Bridge)* or your board's USB-serial option.
3. Pick the port: **ESP-IDF: Select Port to Use** (the COM port your board shows up on).
4. Click **Build, Flash and Monitor** (the flame icon in the bottom bar), or run **ESP-IDF: Build your Project**.

From PowerShell, load ESP-IDF first with `. "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"`, then in this folder run `idf.py set-target esp32` once and `idf.py -p COMx flash monitor`.

## Wiring

Defaults in `main/board_pins.h`. ESP32 pins are **3.3 V only**; nothing from the 5 V rail may reach a GPIO directly.

| Part | Pins | ESP32 |
|---|---|---|
| Ultrasonic | VCC / GND / TRIG / ECHO | 5 V (VIN) / GND / GPIO18 / **GPIO19 through a 1k/2k divider** |
| PIR | VCC / GND / OUT | 5 V / GND / GPIO27 |
| LM35 (flat side facing you) | left / middle / right | 5 V / GPIO34 / GND |
| Light sensor | VCC / GND / signal | **3V3** / GND / GPIO35 |
| Rotation sensor | VCC / GND / signal | **3V3** / GND / GPIO32 |
| SEN0291 data side | + / − / C / D | **3V3** / GND / GPIO22 (SCL) / GPIO21 (SDA), both address switches at 1 (0x45) |
| SEN0291 power side | IN+ / IN− | VIN (5 V) in, fan VCC out |
| Fan module | VCC / GND / signal | SEN0291 IN− (or VIN) / GND / GPIO25 |
| LED 1 / LED 2 | long leg | GPIO26 / GPIO33, each through 220 Ω; short leg to GND |
| Buttons 1 / 2 / 3 | two legs | GPIO14 / GPIO13 / GPIO23 and GND (internal pull-ups) |

Ultrasonic ECHO divider:

```
 ECHO ──[ 1 kΩ ]──┬── GPIO19
                  │
               [ 2 kΩ ]
                  │
                 GND
```

All analog inputs are on ADC1 on purpose: ADC2 stops working while Wi-Fi is on. Avoid GPIO0, 2, 5, 12 and 15 (they affect booting) and never use GPIO6 to 11 (flash).

## Driver reference

Each section below is the same as the `README.md` in that driver's folder.

Contents: [lm35](#lm35), [light_sensor](#light_sensor), [rotation_sensor](#rotation_sensor), [hcsr04](#hcsr04), [pir](#pir), [sen0291](#sen0291), [button](#button), [led](#led), [fan](#fan), [analog_in](#analog_in)

### lm35

**Temperature sensor**

Driver for the LM35 analog temperature sensor, which outputs 10 mV per °C. It reads the pin through [`analog_in`](components/analog_in/README.md) and converts the voltage to degrees Celsius.

Header: `lm35.h` · Depends on: `analog_in`

#### Wiring

| LM35 pin (flat side facing you, legs down) | ESP32 |
|---|---|
| Left | 5 V (VIN) |
| Middle (output) | GPIO34 (`PIN_LM35`) |
| Right | GND |

The LM35 needs at least 4 V to run, so power it from 5 V. Its output is only about 0.2 to 0.4 V at room temperature, so it's safe on the pin.

#### Configuration: `lm35_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the middle leg. |
| `samples` | `int` | `32` | Readings averaged per call. The ESP32 ADC is noisy, so more samples give steadier readings. |
| `offset_c` | `float` | `0.0` | Added to every reading. Use it to correct against a thermometer you trust. |
| `atten` | `adc_atten_t` | `ADC_ATTEN_DB_2_5` | ADC range. `DB_2_5` covers 0 to about 1.25 V, which is up to about 125 °C. |

Get the defaults with `LM35_DEFAULT_CONFIG(pin)`.

#### Functions

##### `esp_err_t lm35_create(const lm35_config_t *config, lm35_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument or a non-ADC pin, and `ESP_ERR_NO_MEM` if memory runs out.

##### `esp_err_t lm35_delete(lm35_handle_t handle)`
Frees the sensor and its ADC input.

##### `esp_err_t lm35_read_celsius(lm35_handle_t handle, float *celsius)`
Writes the temperature to `celsius`, calculated as millivolts ÷ 10 + `offset_c`.

##### `esp_err_t lm35_read_mv(lm35_handle_t handle, int *mv)`
Writes the raw output voltage in millivolts. Handy for checking the wiring: room temperature should read about 200 to 300 mV.

#### Example

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

#### Troubleshooting

- **About 0 °C or wildly wrong:** check the leg order. A reversed LM35 gets hot quickly.
- **Readings jump around:** raise `samples`, or add a 100 nF capacitor between the output and GND.
- The ESP32 ADC is inaccurate below about 100 mV, so temperatures under about 10 °C read less accurately.

### light_sensor

**Ambient light sensor**

Driver for an analog ambient light module (photoresistor or phototransistor). It reads the module's output voltage through `analog_in` and can scale it to 0 to 100 %.

Header: `light_sensor.h` · Depends on: [`analog_in`](components/analog_in/README.md)

#### Wiring

| Sensor pin | ESP32 |
|---|---|
| VCC | **3V3** (not 5 V) |
| GND | GND |
| Signal | GPIO35 (`PIN_LIGHT`) |

Power it from 3V3 so its output can never go above what the ESP32 pin can take.

#### Configuration: `light_sensor_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the signal pin. |
| `samples` | `int` | `16` | Readings averaged per call. |
| `min_mv` | `int` | `0` | Voltage that reads as 0 %. |
| `max_mv` | `int` | `3100` | Voltage that reads as 100 %. It must be above `min_mv`. |
| `invert` | `bool` | `false` | Flips the scale, so `min_mv` reads as 100 %. |

Get the defaults with `LIGHT_SENSOR_DEFAULT_CONFIG(pin)`. The ADC always uses the 12 dB range, which is about 0 to 3.1 V.

#### Functions

##### `esp_err_t light_sensor_create(const light_sensor_config_t *config, light_sensor_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument, a non-ADC pin, or `max_mv` not above `min_mv`. Returns `ESP_ERR_NO_MEM` if memory runs out.

##### `esp_err_t light_sensor_delete(light_sensor_handle_t handle)`
Frees the sensor and its ADC input.

##### `esp_err_t light_sensor_read_raw(light_sensor_handle_t handle, int *raw)`
Writes the average raw ADC reading, 0 to 4095.

##### `esp_err_t light_sensor_read_mv(light_sensor_handle_t handle, int *mv)`
Writes the average voltage in millivolts.

##### `esp_err_t light_sensor_read_percent(light_sensor_handle_t handle, float *percent)`
Writes the reading as 0 to 100 %, scaled between `min_mv` and `max_mv` and clamped to that range. If `invert` is set, the scale is flipped.

#### Example

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

#### Calibrating the 0 to 100 % range

Cover the sensor and note `light_sensor_read_mv()`, then shine a light on it and note it again. Put those values in `min_mv` and `max_mv`. Most modules give a higher voltage in brighter light. If yours reads high in the dark, set `invert = true`.

### rotation_sensor

**Rotation sensor (knob)**

Driver for a rotation sensor (potentiometer knob). It reads the knob's voltage through `analog_in` and can scale it to 0 to 100 %.

Header: `rotation_sensor.h` · Depends on: [`analog_in`](components/analog_in/README.md)

#### Wiring

| Sensor pin | ESP32 |
|---|---|
| VCC | **3V3** (not 5 V) |
| GND | GND |
| Signal | GPIO32 (`PIN_ROTATION`) |

Power it from 3V3 so its output can never go above what the ESP32 pin can take.

#### Configuration: `rotation_sensor_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | ADC pin connected to the signal pin. |
| `samples` | `int` | `16` | Readings averaged per call. |
| `min_mv` | `int` | `0` | Voltage that reads as 0 %. |
| `max_mv` | `int` | `3100` | Voltage that reads as 100 %. It must be above `min_mv`. |
| `invert` | `bool` | `false` | Flips the scale, so `min_mv` reads as 100 %. |

Get the defaults with `ROTATION_SENSOR_DEFAULT_CONFIG(pin)`. The ADC always uses the 12 dB range, which is about 0 to 3.1 V.

#### Functions

##### `esp_err_t rotation_sensor_create(const rotation_sensor_config_t *config, rotation_sensor_handle_t *ret_handle)`
Sets up the sensor and returns a handle. Returns `ESP_ERR_INVALID_ARG` for a NULL argument, a non-ADC pin, or `max_mv` not above `min_mv`. Returns `ESP_ERR_NO_MEM` if memory runs out.

##### `esp_err_t rotation_sensor_delete(rotation_sensor_handle_t handle)`
Frees the sensor and its ADC input.

##### `esp_err_t rotation_sensor_read_raw(rotation_sensor_handle_t handle, int *raw)`
Writes the average raw ADC reading, 0 to 4095.

##### `esp_err_t rotation_sensor_read_mv(rotation_sensor_handle_t handle, int *mv)`
Writes the average voltage in millivolts.

##### `esp_err_t rotation_sensor_read_percent(rotation_sensor_handle_t handle, float *percent)`
Writes the reading as 0 to 100 %, scaled between `min_mv` and `max_mv` and clamped to that range. If `invert` is set, the scale is flipped.

#### Example

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

#### Calibrating the 0 to 100 % range

Turn the knob fully to each end and note `rotation_sensor_read_mv()` at each. Put those values in `min_mv` and `max_mv` to get a full 0 to 100 % sweep. If turning clockwise makes the reading go down, set `invert = true`.

### hcsr04

**Ultrasonic distance sensor**

Driver for HC-SR04 style ultrasonic sensors. It sends a 10 µs trigger pulse, then times the echo pulse with a GPIO interrupt and `esp_timer`, to 1 µs resolution. The calling task only waits until the echo comes back, or until the timeout.

Header: `hcsr04.h` · Depends on: `esp_driver_gpio`, `esp_timer`

#### Wiring

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

#### Configuration: `hcsr04_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `trig_gpio` | `int` | the pin you pass | Output pin to TRIG. |
| `echo_gpio` | `int` | the pin you pass | Input pin from ECHO, through the divider. The internal pull-down is enabled. |
| `timeout_ms` | `uint32_t` | `30` | How long to wait for an echo. 30 ms covers about 5 m. 0 means 30. |
| `air_temp_c` | `float` | `20.0` | Air temperature used to work out the speed of sound. |

Get the defaults with `HCSR04_DEFAULT_CONFIG(trig, echo)`.

#### Functions

##### `esp_err_t hcsr04_create(const hcsr04_config_t *config, hcsr04_handle_t *ret_handle)`
Sets up both pins and the echo interrupt, and returns a handle. It installs the shared GPIO interrupt service if it isn't installed yet. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

##### `esp_err_t hcsr04_delete(hcsr04_handle_t handle)`
Removes the interrupt, resets both pins and frees the sensor.

##### `esp_err_t hcsr04_measure_us(hcsr04_handle_t handle, uint32_t *echo_us)`
Sends one ping and writes the echo pulse length in microseconds. Returns `ESP_ERR_TIMEOUT` if no echo arrives within `timeout_ms`. That means nothing is in range, or ECHO isn't connected.

##### `esp_err_t hcsr04_measure_cm(hcsr04_handle_t handle, float *distance_cm)`
Sends one ping and writes the distance in centimetres. The speed of sound is (331.3 + 0.606 × `air_temp_c`) m/s, halved because the sound goes there and back. Returns `ESP_ERR_TIMEOUT` like `measure_us`.

##### `void hcsr04_set_air_temp(hcsr04_handle_t handle, float celsius)`
Updates the air temperature used by `measure_cm`. The speed of sound changes about 0.17 % per °C, so feeding in the LM35 reading makes distances slightly more accurate.

#### Example

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

#### Notes

- Leave at least 60 ms between pings, so echoes from the last ping have died out.
- One ping at a time per sensor. Calls from several tasks are safe, because they queue up.
- Useful range is about 2 cm to 4 m. Soft or angled surfaces may not reflect enough to measure.

### pir

**Motion sensor**

Driver for PIR motion sensors such as the HC-SR501. A GPIO interrupt tracks the output, so you can ask "is there motion now?" and "how long since the last motion?" without polling quickly.

Header: `pir.h` · Depends on: `esp_driver_gpio`, `esp_timer`

#### Wiring

| PIR pin | ESP32 |
|---|---|
| VCC | 5 V (VIN) |
| GND | GND |
| OUT | GPIO27 (`PIN_PIR`) |

The output is 3.3 V, so it connects straight to the pin.

#### Configuration: `pir_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin connected to OUT. |
| `active_low` | `bool` | `false` | Set true only if your module's output goes low on motion. Nearly every module goes high. |
| `pull_down` | `bool` | `true` | Turns on the internal pull-down, so the pin reads "no motion" if OUT is disconnected. |

Get the defaults with `PIR_DEFAULT_CONFIG(pin)`.

#### Functions

##### `esp_err_t pir_create(const pir_config_t *config, pir_handle_t *ret_handle)`
Sets up the pin and its interrupt, and returns a handle. It installs the shared GPIO interrupt service if it isn't installed yet. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

##### `esp_err_t pir_delete(pir_handle_t handle)`
Removes the interrupt, resets the pin and frees the sensor.

##### `bool pir_is_motion(pir_handle_t handle)`
Returns true while the sensor reports motion.

##### `int64_t pir_last_motion_us(pir_handle_t handle)`
Returns when motion was last seen, as microseconds since boot on the `esp_timer` clock. While motion is ongoing it returns the current time. Returns 0 if there has been no motion since boot.

##### `float pir_seconds_since_motion(pir_handle_t handle)`
Returns the seconds since motion was last seen. Returns 0 while motion is ongoing, and −1 if there has been no motion since boot.

#### Example

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

#### Notes

- After power-up the sensor needs up to a minute to settle, and it may trigger on its own during that time.
- HC-SR501 modules have two knobs. **Time** sets how long OUT stays high after motion; turn it fully anticlockwise for the shortest time, about 3 s. **Sensitivity** sets the range.
- The jumper selects single trigger (L) or repeat trigger (H). Repeat trigger (H) keeps OUT high while motion continues, which suits this driver best.

### sen0291

**I2C wattmeter**

Driver for the DFRobot SEN0291 wattmeter, which is an INA219 chip with a 0.01 Ω shunt resistor. It measures the voltage on the load side and the current through the meter, and works out the power. It uses the ESP-IDF `i2c_master` driver.

Header: `sen0291.h` · Depends on: `esp_driver_i2c`

#### Wiring

**Data side** (4-pin connector):

| SEN0291 pin | ESP32 |
|---|---|
| + (VCC) | **3V3** (not 5 V) |
| − (GND) | GND |
| C (SCL) | GPIO22 (`PIN_I2C_SCL`) |
| D (SDA) | GPIO21 (`PIN_I2C_SDA`) |

Power it from 3V3. The board pulls SDA and SCL up to its own supply, and 5 V there would damage the ESP32.

**Power side** (screw terminals): the current you want to measure must flow through the meter. For the fan, connect VIN (5 V) to IN+, and IN− to the fan's VCC.

**Address switches:**

| A0 | A1 | Address |
|---|---|---|
| 0 | 0 | 0x40 |
| 1 | 0 | 0x41 |
| 0 | 1 | 0x44 |
| 1 | 1 | **0x45** (default) |

#### Configuration: `sen0291_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `bus` | `i2c_master_bus_handle_t` | `NULL` | An existing I2C bus to share. If NULL, the driver creates its own bus on the pins below. |
| `i2c_port` | `i2c_port_num_t` | `-1` | I2C port when the driver creates the bus. −1 picks a free one. |
| `sda_gpio` | `int` | the pin you pass | SDA pin when the driver creates the bus. |
| `scl_gpio` | `int` | the pin you pass | SCL pin when the driver creates the bus. |
| `address` | `uint8_t` | `0x45` | Must match the address switches. |
| `scl_speed_hz` | `uint32_t` | `100000` | I2C clock. 0 means 100 kHz. |
| `shunt_milliohm` | `uint32_t` | `10` | Shunt resistor. It's 10 mΩ on the SEN0291. Must be above 0. |
| `current_gain` | `float` | `1.0` | Multiplies every current reading. Set it to multimeter mA ÷ SEN0291 mA to correct small errors. |

Get the defaults with `SEN0291_DEFAULT_CONFIG(sda, scl)`.

The driver sets the chip to a 32 V bus range and a ±320 mV shunt range (up to about 32 A with this shunt), with 16-sample averaging on both channels. Each new reading takes about 8.5 ms.

#### Data: `sen0291_reading_t`

| Field | Unit | Meaning |
|---|---|---|
| `bus_v` | V | Voltage on IN−, which is what the load gets. 4 mV resolution. |
| `shunt_mv` | mV | Voltage across the shunt. 0.01 mV resolution. |
| `current_ma` | mA | Current through the meter. About 1 mA resolution. Negative if IN+ and IN− are swapped. |
| `power_mw` | mW | `bus_v` × `current_ma`. |

#### Functions

##### `esp_err_t sen0291_create(const sen0291_config_t *config, sen0291_handle_t *ret_handle)`
Creates the I2C bus if needed, checks that the meter answers at `address`, and configures it. Returns `ESP_ERR_NOT_FOUND` if nothing answers, `ESP_ERR_INVALID_ARG` for a NULL argument or a zero shunt, and the I2C driver's own error if the bus can't be set up.

##### `esp_err_t sen0291_delete(sen0291_handle_t handle)`
Removes the meter from the bus. Deletes the bus too if the driver created it.

##### `esp_err_t sen0291_read(sen0291_handle_t handle, sen0291_reading_t *reading)`
Reads voltage, current and power in one call.

##### `esp_err_t sen0291_read_bus_voltage(sen0291_handle_t handle, float *volts)`
Reads only the load-side voltage.

##### `esp_err_t sen0291_read_current(sen0291_handle_t handle, float *milliamps)`
Reads only the current, with `current_gain` applied.

##### `i2c_master_bus_handle_t sen0291_get_bus(sen0291_handle_t handle)`
Returns the I2C bus the meter uses, so you can add other I2C devices to it.

#### Example

```c
#include "board_pins.h"
#include "sen0291.h"

sen0291_config_t cfg = SEN0291_DEFAULT_CONFIG(PIN_I2C_SDA, PIN_I2C_SCL);
sen0291_handle_t meter;
ESP_ERROR_CHECK(sen0291_create(&cfg, &meter));

sen0291_reading_t r;
if (sen0291_read(meter, &r) == ESP_OK) {
    printf("%.2f V  %.1f mA  %.0f mW\n", r.bus_v, r.current_ma, r.power_mw);
}
```

#### Troubleshooting

- **`ESP_ERR_NOT_FOUND` at startup:** SDA and SCL are easy to swap. Also check 3V3 and GND, and that the switches match `address`.
- **Negative current:** IN+ and IN− are swapped.
- **Current always about 0:** the load isn't powered through IN+ → IN−.

### button

**Debounced pushbutton**

Driver for pushbuttons. Each button is sampled every 5 ms by an `esp_timer`, and a change only counts once the pin has held steady for the debounce time. Contact bounce never reaches your code. You can poll the button, get callbacks, or both.

Header: `button.h` · Depends on: `esp_driver_gpio`, `esp_timer`

#### Wiring

Connect one leg to the GPIO and the other to GND. The internal pull-up is used, so no resistor is needed.

| Button | ESP32 |
|---|---|
| Button 1 | GPIO14 (`PIN_BTN1`) |
| Button 2 | GPIO13 (`PIN_BTN2`) |
| Button 3 | GPIO23 (`PIN_BTN3`) |

#### Configuration: `button_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Button pin. |
| `active_low` | `bool` | `true` | True when the button connects the pin to GND; this uses the internal pull-up. False when it connects to 3V3; this uses the internal pull-down. |
| `debounce_ms` | `uint32_t` | `30` | How long the pin must stay steady before a change counts. Rounded up to a multiple of 5 ms. |
| `long_press_ms` | `uint32_t` | `1000` | Hold time before a `LONG_PRESS` event. 0 turns long presses off. |
| `callback` | `button_cb_t` | `NULL` | Optional function called on each event. |
| `user_ctx` | `void *` | `NULL` | Passed to the callback unchanged. |

Get the defaults with `BUTTON_DEFAULT_CONFIG(pin)`.

#### Events: `button_event_t`

| Event | When |
|---|---|
| `BUTTON_EVENT_PRESSED` | The button goes down (after debounce). |
| `BUTTON_EVENT_RELEASED` | The button comes back up (after debounce). |
| `BUTTON_EVENT_LONG_PRESS` | The button has been held for `long_press_ms`. Sent once per hold. |

Callback signature: `void callback(button_handle_t button, button_event_t event, void *user_ctx)`

#### Functions

##### `esp_err_t button_create(const button_config_t *config, button_handle_t *ret_handle)`
Sets up the pin, starts the 5 ms sampling timer, and returns a handle. A button held down at boot doesn't fire a press. Returns `ESP_ERR_INVALID_ARG` or `ESP_ERR_NO_MEM` on failure.

##### `esp_err_t button_delete(button_handle_t handle)`
Stops the timer, resets the pin and frees the button.

##### `bool button_is_pressed(button_handle_t handle)`
Returns true while the button is held down (debounced).

##### `bool button_was_pressed(button_handle_t handle)`
Returns true once for each press since the last call, then false until the next press. Ideal for a loop that checks every 100 ms or so, because it never misses a quick tap.

##### `int button_get_gpio(button_handle_t handle)`
Returns the button's pin, or −1 for a NULL handle. Useful in a shared callback to tell buttons apart.

#### Examples

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

#### Notes

- The callback runs in the `esp_timer` task, which all timers share. Keep it short and never block in it: set a flag, give a semaphore, or post to a queue.
- `RELEASED` follows every `PRESSED`, including after a long press.

### led

**Dimmable LED**

Driver for LEDs with brightness control. It drives each LED with PWM from the ESP32's LEDC peripheral, at 10-bit resolution.

Header: `led.h` · Depends on: `esp_driver_ledc`

#### Wiring

Connect each LED as GPIO → 220 Ω resistor → long leg (+), with the short leg (−) to GND.

| LED | ESP32 |
|---|---|
| LED 1 | GPIO26 (`PIN_LED1`) |
| LED 2 | GPIO33 (`PIN_LED2`) |

For a brighter LED you can use 100 Ω instead of 220 Ω.

#### Configuration: `led_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | LED pin. |
| `timer` | `ledc_timer_t` | `LEDC_TIMER_0` | LEDC timer. LEDs can share a timer if they use the same frequency. |
| `channel` | `ledc_channel_t` | the channel you pass | LEDC channel. **Every LED needs its own channel.** The fan uses `LEDC_CHANNEL_2` by default. |
| `freq_hz` | `uint32_t` | `5000` | PWM frequency. 0 means 5000. |
| `active_low` | `bool` | `false` | Set true if the LED is wired from 3V3 to the pin, so it lights when the pin is low. |

Get the defaults with `LED_DEFAULT_CONFIG(pin, channel)`.

#### Functions

##### `esp_err_t led_create(const led_config_t *config, led_handle_t *ret_handle)`
Sets up the LEDC timer and channel with the LED off, and returns a handle. Returns an LEDC error if the timer or channel can't be set up, or `ESP_ERR_NO_MEM`.

##### `esp_err_t led_delete(led_handle_t handle)`
Turns the LED off and frees it.

##### `esp_err_t led_set_brightness(led_handle_t handle, uint8_t level)`
Sets the brightness: 0 is off, 255 is fully on.

##### `uint8_t led_get_brightness(led_handle_t handle)`
Returns the brightness last set, 0 to 255.

##### `esp_err_t led_on(led_handle_t handle)`
Full brightness. Same as `led_set_brightness(handle, 255)`.

##### `esp_err_t led_off(led_handle_t handle)`
Off. Same as `led_set_brightness(handle, 0)`.

##### `esp_err_t led_toggle(led_handle_t handle)`
Switches between off and full brightness. Any brightness above 0 counts as on, so a dimmed LED goes off.

##### `bool led_is_on(led_handle_t handle)`
Returns true if the brightness is above 0.

#### Example

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

#### Notes

- Brightness isn't linear to the eye: going from 0 to 32 looks like a bigger step than going from 200 to 255.
- The ESP32 has 4 LEDC timers and 8 channels in this mode.

### fan

**PWM fan module**

Driver for a DC fan module with a built-in switch (3 pins: VCC, GND, signal). It sets the speed with PWM on the signal pin, from the ESP32's LEDC peripheral at 10-bit resolution.

Header: `fan.h` · Depends on: `esp_driver_ledc`

#### Wiring

| Fan module pin | ESP32 |
|---|---|
| VCC | 5 V. Use wattmeter IN− to measure the fan, or VIN without the wattmeter. |
| GND | GND |
| Signal | GPIO25 (`PIN_FAN`) |

The motor runs from 5 V. The ESP32 only sends the 3.3 V control signal, so no extra parts are needed.

#### Configuration: `fan_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin connected to the signal pin. |
| `timer` | `ledc_timer_t` | `LEDC_TIMER_1` | LEDC timer. Kept apart from the LEDs' timer 0 because the frequency is different. |
| `channel` | `ledc_channel_t` | `LEDC_CHANNEL_2` | LEDC channel. Must not clash with an LED's channel. |
| `freq_hz` | `uint32_t` | `1000` | PWM frequency. 1 kHz suits most modules. Try 20000 or more if the fan whines. 0 means 1000. |
| `invert` | `bool` | `false` | Set true if the module runs the fan when the signal is low. |
| `min_speed` | `uint8_t` | `0` | Speeds from 1 up to `min_speed` are raised to `min_speed`, so the fan doesn't stall at low speed. 0 turns this off. |

Get the defaults with `FAN_DEFAULT_CONFIG(pin)`.

#### Functions

##### `esp_err_t fan_create(const fan_config_t *config, fan_handle_t *ret_handle)`
Sets up the LEDC timer and channel with the fan off, and returns a handle. Returns an LEDC error if the timer or channel can't be set up, or `ESP_ERR_NO_MEM`.

##### `esp_err_t fan_delete(fan_handle_t handle)`
Turns the fan off and frees it.

##### `esp_err_t fan_set_speed(fan_handle_t handle, uint8_t speed)`
Sets the speed: 0 is off, 255 is full speed. `min_speed` applies.

##### `esp_err_t fan_set_percent(fan_handle_t handle, uint8_t percent)`
Sets the speed as 0 to 100 %. Values above 100 are treated as 100. It's converted to 0 to 255 and passed to `fan_set_speed`.

##### `uint8_t fan_get_speed(fan_handle_t handle)`
Returns the speed actually set, 0 to 255, after `min_speed` is applied.

##### `esp_err_t fan_on(fan_handle_t handle)`
Full speed. Same as `fan_set_speed(handle, 255)`.

##### `esp_err_t fan_off(fan_handle_t handle)`
Off. Same as `fan_set_speed(handle, 0)`.

##### `bool fan_is_on(fan_handle_t handle)`
Returns true if the speed is above 0.

#### Example

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

#### Troubleshooting

- **Doesn't spin at all:** some modules need a 5 V signal. A small transistor fixes that.
- **Only switches on and off, no speed change:** the module doesn't support PWM. `fan_on` and `fan_off` still work.
- **Runs when it should be off:** set `invert = true`.
- **Whines:** raise `freq_hz` to 20000 or more.

### analog_in

**Shared ADC helper**

Reads one analog pin and returns calibrated millivolts, using the ESP-IDF ADC oneshot driver. The `lm35`, `light_sensor` and `rotation_sensor` drivers are built on it. Inputs on the same ADC unit share one unit handle, which is what lets those three sensors all use ADC1 together. You only need to use it directly for an analog part that has no driver of its own.

Header: `analog_in.h` · Depends on: `esp_adc`

#### Pins

Any ADC-capable GPIO. Use ADC1 pins (GPIO32 to GPIO39 on the ESP32), because ADC2 stops working while Wi-Fi is on. The driver logs a warning if you pick an ADC2 pin.

#### Configuration: `analog_in_config_t`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `gpio` | `int` | the pin you pass | Pin to read. |
| `atten` | `adc_atten_t` | `ADC_ATTEN_DB_12` | Input range. `DB_12` covers about 0 to 3.1 V, `DB_6` about 0 to 1.75 V, `DB_2_5` about 0 to 1.25 V, `DB_0` about 0 to 0.95 V. A smaller range gives finer resolution. |
| `samples` | `int` | `16` | Readings averaged per call. 1 means no averaging. Values below 1 are treated as 1. |

Get the defaults with `ANALOG_IN_DEFAULT_CONFIG(pin)`.

#### Functions

##### `esp_err_t analog_in_create(const analog_in_config_t *config, analog_in_handle_t *ret_handle)`
Sets up the pin and returns a handle in `ret_handle`. It also sets up the chip's factory calibration when available. Returns `ESP_ERR_INVALID_ARG` for a NULL argument or a pin that isn't an ADC pin, and `ESP_ERR_NO_MEM` if memory runs out.

##### `esp_err_t analog_in_delete(analog_in_handle_t handle)`
Frees the input. The ADC unit is released when the last input on it is deleted.

##### `esp_err_t analog_in_read_raw(analog_in_handle_t handle, int *raw)`
Writes the average raw reading, 0 to 4095 on the ESP32, to `raw`.

##### `esp_err_t analog_in_read_mv(analog_in_handle_t handle, int *mv)`
Writes the average reading in millivolts to `mv`. Uses the factory calibration if there is one. Otherwise it estimates linearly from the range set by `atten`.

##### `bool analog_in_is_calibrated(analog_in_handle_t handle)`
Returns true if readings use the factory calibration. False means `read_mv` is an estimate.

#### Example

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

#### Notes

- Create and delete inputs from one task, normally at startup. Reads are safe from any task.
- Every ESP32 ADC reads poorly below about 100 mV and near the top of its range.

## Using the drivers from several tasks

Reads and sets are safe to call from any task. Create and delete the drivers from one task, normally at the start of `app_main`.
