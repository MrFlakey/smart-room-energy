# Smart Room Energy Management: ESP32 firmware

Author: Mohamed Khamis

A smart room that saves energy on its own. An ESP32 watches occupancy, the door, temperature, daylight and power use, then switches the lights and the "AC" (a DC fan) only when they are needed. Everything is shown live on a 3D digital twin over MQTT: [Smart-Room-Digital-Twin](https://github.com/MrFlakey/Smart-Room-Digital-Twin).

This repository holds the ESP-IDF driver components for every sensor and actuator in the room.

## How the room behaves

- **Occupancy:** the PIR sensor detects people; the room counts as empty after 15 s without motion.
- **Door:** an ultrasonic sensor watches the door. If it stays open for about 10 s, the AC pauses and resumes when it closes.
- **AC:** the fan stands in for the AC. A rotation knob sets the target temperature, the fan speed grows with how far the room is above target, and it is always off in an empty room.
- **Lights:** two LEDs are dimmed from the ambient light sensor (daylight harvesting), only while the room is occupied.
- **Buttons:** B1 toggles the lights, B2 toggles the AC, B3 switches Auto/Manual. In Auto a press overrides the automatic control until the room is empty.
- **Energy:** a DFRobot SEN0291 wattmeter measures live power, total energy and the percentage saved against "always on".
- **Reliability:** the room runs offline on its own. A failed sensor switches off what depends on it and is reported to the twin.

## Hardware

| Part | Model |
|---|---|
| Board | ESP32 DevKit v1 |
| Motion | PIR sensor |
| Door | HC-SR04 style ultrasonic sensor |
| Temperature | LM35 |
| Ambient light | analog light sensor |
| Target temperature | rotation sensor (potentiometer) |
| Power | DFRobot SEN0291 I2C wattmeter |
| Controls | 3 pushbuttons |
| Lights | 2 LEDs |
| AC | DC fan module |

The pin map is in [`firmware/main/board_pins.h`](firmware/main/board_pins.h) and the full wiring table is in [`firmware/README.md`](firmware/README.md). ESP32 pins are 3.3 V only; the ultrasonic ECHO line needs a 1 kΩ / 2 kΩ divider.

## Drivers

Written for **ESP-IDF v6.0** using only the current IDF drivers (ADC oneshot with calibration, `i2c_master`, LEDC, GPIO, `esp_timer`).

| Component | Part |
|---|---|
| `analog_in` | shared ADC1 helper used by the analog drivers |
| `lm35` | temperature |
| `light_sensor` | ambient light |
| `rotation_sensor` | target temperature knob |
| `hcsr04` | ultrasonic distance |
| `pir` | motion |
| `sen0291` | I2C wattmeter |
| `button` | debounced pushbuttons with callbacks |
| `led` | dimmable LEDs |
| `fan` | PWM fan module |

Every driver works the same way: fill a config struct with its `*_DEFAULT_CONFIG(...)` macro, call `*_create()` to get a handle, then use the read/set functions. The reference for each driver is in [`firmware/README.md`](firmware/README.md) and in its folder under [`firmware/components/`](firmware/components).

## Build and flash

1. Install ESP-IDF v6.0 (for example with the ESP-IDF VS Code extension).
2. Open the `firmware` folder.
3. Set the target to `esp32`, pick the board's serial port, then **Build, Flash and Monitor**.

From a terminal with ESP-IDF loaded:

```
cd firmware
idf.py set-target esp32
idf.py -p <PORT> flash monitor
```

## Branches

- `main`: the driver components with an empty `app_main`.
- `testing`: hardware tests (PIR + fan) and per-driver documentation.

## Network

The ESP32 talks MQTT to a local Mosquitto broker on base topic `smartroom/room1/`. The message format is defined in the twin repository's [`docs/mqtt-contract.md`](https://github.com/MrFlakey/Smart-Room-Digital-Twin/blob/main/docs/mqtt-contract.md).
