# Smart Room drivers (ESP-IDF)

Drivers for every sensor and actuator in the Smart Room, written for **ESP-IDF v6.0** on an **ESP32 DevKit v1**. There's no application code: `main/main.c` is an empty `app_main` for you to fill in.

Everything uses the current IDF drivers only (ADC oneshot with calibration, `i2c_master`, LEDC, GPIO, `esp_timer`), so nothing depends on the legacy drivers that v6 removed.

## Layout

```
esp-idf-drivers/
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

Every driver works the same way: fill a config struct (each has a `*_DEFAULT_CONFIG(...)` macro), call `*_create()` to get a handle, then call the read/set functions with that handle. All functions return `esp_err_t` (`ESP_OK` on success) unless they return a plain value. `main` can include any driver header directly, because IDF links every component into `main` automatically.

## Building with the ESP-IDF VS Code extension

1. **File → Open Folder** and pick `esp-idf-drivers` (the folder with the top-level `CMakeLists.txt`).
2. Set the target: Command Palette → **ESP-IDF: Set Espressif Device Target** → `esp32` → *ESP32 chip (via ESP USB Bridge)* or your board's USB-serial option.
3. Pick the port: **ESP-IDF: Select Port to Use** (the COM port your board shows up on).
4. Click **Build, Flash and Monitor** (the flame icon in the bottom bar), or run **ESP-IDF: Build your Project**.

From an ESP-IDF terminal it's `idf.py set-target esp32`, then `idf.py -p COM5 flash monitor`.

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

### LM35 temperature (`lm35.h`)

```c
lm35_config_t cfg = LM35_DEFAULT_CONFIG(PIN_LM35);   // 32-sample average, 0..~125 C range
cfg.offset_c = -0.5f;                                // optional correction
lm35_handle_t temp;
ESP_ERROR_CHECK(lm35_create(&cfg, &temp));

float c;
lm35_read_celsius(temp, &c);
```

Also `lm35_read_mv()`. The LM35 needs 5 V to run but outputs only about 10 mV per °C, so it's safe on the pin.

### Light sensor and rotation sensor (`light_sensor.h`, `rotation_sensor.h`)

Both have the same API.

```c
light_sensor_config_t cfg = LIGHT_SENSOR_DEFAULT_CONFIG(PIN_LIGHT);
cfg.invert = false;            // set true if yours reads high in the dark
light_sensor_handle_t light;
ESP_ERROR_CHECK(light_sensor_create(&cfg, &light));

float pct;                     // 0..100
light_sensor_read_percent(light, &pct);
```

Also `*_read_raw()` (0..4095) and `*_read_mv()`. `min_mv`/`max_mv` in the config set which voltages count as 0 % and 100 %: put the knob at each end, note `*_read_mv()`, and use those numbers for a full 0..100 sweep.

### Ultrasonic distance (`hcsr04.h`)

```c
hcsr04_config_t cfg = HCSR04_DEFAULT_CONFIG(PIN_US_TRIG, PIN_US_ECHO);
hcsr04_handle_t us;
ESP_ERROR_CHECK(hcsr04_create(&cfg, &us));

float cm;
if (hcsr04_measure_cm(us, &cm) == ESP_ERR_TIMEOUT) {
    // nothing within about 4 m (or ECHO isn't connected)
}
hcsr04_set_air_temp(us, room_c);   // optional, makes the distance a little more accurate
```

A measurement blocks the calling task for at most `timeout_ms` (30 ms). Leave at least 60 ms between pings. `hcsr04_measure_us()` gives the raw echo time.

### PIR motion (`pir.h`)

```c
pir_config_t cfg = PIR_DEFAULT_CONFIG(PIN_PIR);
pir_handle_t pir;
ESP_ERROR_CHECK(pir_create(&cfg, &pir));

if (pir_is_motion(pir)) { ... }
if (pir_seconds_since_motion(pir) > 300) { /* room empty for 5 minutes */ }
```

`pir_seconds_since_motion()` returns 0 while motion is ongoing and −1 if there has been none since boot. `pir_last_motion_us()` gives the raw `esp_timer` timestamp. The sensor needs up to a minute after power-up to settle.

### SEN0291 wattmeter (`sen0291.h`)

```c
sen0291_config_t cfg = SEN0291_DEFAULT_CONFIG(PIN_I2C_SDA, PIN_I2C_SCL);  // address 0x45
sen0291_handle_t meter;
ESP_ERROR_CHECK(sen0291_create(&cfg, &meter));   // ESP_ERR_NOT_FOUND if it doesn't answer

sen0291_reading_t r;
sen0291_read(meter, &r);   // r.bus_v, r.shunt_mv, r.current_ma, r.power_mw
```

Also `sen0291_read_bus_voltage()` and `sen0291_read_current()`. To share the I2C bus with another device, pass an existing `i2c_master_bus_handle_t` in `cfg.bus`, or get the meter's bus with `sen0291_get_bus()`. If the current is a little off compared with a multimeter, set `cfg.current_gain` to `multimeter_mA / sen0291_mA`. A negative current means IN+ and IN− are swapped.

### Buttons (`button.h`)

```c
button_config_t cfg = BUTTON_DEFAULT_CONFIG(PIN_BTN1);   // to GND, internal pull-up
button_handle_t b1;
ESP_ERROR_CHECK(button_create(&cfg, &b1));

if (button_was_pressed(b1)) { ... }   // true once per press
if (button_is_pressed(b1))  { ... }   // held right now
```

Or event-driven:

```c
static void on_button(button_handle_t b, button_event_t ev, void *ctx)
{
    if (ev == BUTTON_EVENT_PRESSED) { /* set a flag or post to a queue */ }
}
cfg.callback = on_button;    // also BUTTON_EVENT_RELEASED and BUTTON_EVENT_LONG_PRESS (after 1 s)
```

The callback runs in the `esp_timer` task, so keep it short and never block in it. Debounce is 30 ms by default.

### LEDs (`led.h`)

```c
led_config_t c1 = LED_DEFAULT_CONFIG(PIN_LED1, LEDC_CHANNEL_0);
led_config_t c2 = LED_DEFAULT_CONFIG(PIN_LED2, LEDC_CHANNEL_1);
led_handle_t led1, led2;
ESP_ERROR_CHECK(led_create(&c1, &led1));
ESP_ERROR_CHECK(led_create(&c2, &led2));

led_on(led1);
led_set_brightness(led2, 64);   // 0..255
led_toggle(led1);
```

Also `led_off()`, `led_get_brightness()` and `led_is_on()`. Every LED needs its own LEDC channel; the defaults use LEDC timer 0 at 5 kHz.

### Fan (`fan.h`)

```c
fan_config_t cfg = FAN_DEFAULT_CONFIG(PIN_FAN);   // LEDC timer 1, channel 2, 1 kHz
cfg.min_speed = 80;                               // optional: stops it stalling at low speed
fan_handle_t fan;
ESP_ERROR_CHECK(fan_create(&cfg, &fan));

fan_set_percent(fan, 60);    // 0..100
fan_set_speed(fan, 255);     // 0..255
fan_off(fan);
```

Also `fan_on()`, `fan_get_speed()` and `fan_is_on()`. If the fan whines, try `cfg.freq_hz = 20000`. If it runs when it should be off, set `cfg.invert = true`. If it only switches on and off without changing speed, the module doesn't support PWM, but on/off still works.

### Shared ADC helper (`analog_in.h`)

The LM35, light and rotation drivers all use this, so they can share ADC1. You only need it directly for another analog part:

```c
analog_in_config_t cfg = ANALOG_IN_DEFAULT_CONFIG(36);
analog_in_handle_t in;
analog_in_create(&cfg, &in);
int mv;
analog_in_read_mv(in, &mv);
```

## Using the drivers from several tasks

Reads and sets are safe to call from any task. Create and delete the drivers from one task, normally at the start of `app_main`.
