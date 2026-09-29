# Smart Room Energy Management

This is the room controller for an **ESP32 DevKit v1** (the Arduino Uno WiFi Rev2 is still supported). It detects occupancy, temperature, light and power, runs the energy-saving rules on the board, drives two LED "lights" and a DC fan, and streams everything over MQTT to a digital twin.

## Hardware (ESP32)

**ESP32 pins are 3.3 V only.** The full wiring guide, with power rails and a safety checklist, is in [docs/esp32-wiring.md](docs/esp32-wiring.md).

| Part | ESP32 pin | Power | Notes |
|---|---|---|---|
| PIR | GPIO27 | VIN (5 V) | |
| Ultrasonic TRIG / ECHO | GPIO18 / GPIO19 | VIN (5 V) | **ECHO needs a 1 kΩ / 2 kΩ divider** |
| LM35 | GPIO34 | VIN (5 V) | |
| Ambient light sensor | GPIO35 | 3V3 | Set `LIGHT_INVERTED` in `config.h` if it reads lower when brighter |
| Rotation sensor | GPIO32 | 3V3 | Sets the temperature setpoint, 20 to 30 °C |
| SEN0291 wattmeter | SDA GPIO21 / SCL GPIO22 | **3V3** | IN+ from VIN, IN− to the fan. See [docs/wattmeter-wiring.md](docs/wattmeter-wiring.md) |
| Fan module | GPIO25 (signal) | Wattmeter IN− (5 V) | See [docs/fan-wiring.md](docs/fan-wiring.md) |
| LED 1 / LED 2 | GPIO26 / GPIO33 | from the pin | 220 Ω (or 100 Ω for brighter), short leg to GND |
| Buttons B1 lights / B2 fan / B3 mode | GPIO14 / GPIO13 / GPIO23 | none | Button between the pin and GND |

The Uno WiFi Rev2 pins are in `firmware/include/board.h`.

## Setup

1. Install [PlatformIO](https://platformio.org/install), either the VS Code extension or `pip install platformio`.
2. Copy `firmware/include/secrets.example.h` to `firmware/include/secrets.h` and fill in your Wi-Fi and MQTT broker details.
3. Plug in the ESP32 over USB, then build and upload. If the upload hangs on "Connecting...", hold the board's **BOOT** button until it starts.
   ```
   cd firmware
   pio run -t upload        # builds for the ESP32 by default
   pio device monitor
   ```
4. Optional: run the control-logic tests on your PC with `pio test -e native`.

To run without Wi-Fi, set `ENABLE_MQTT 0` in `config.h`. Everything still works over serial.

## How it behaves (auto mode)

- **Occupied** means the PIR fired in the last 60 s or something is within 100 cm of the ultrasonic sensor.
- **Lights** come on when the room is occupied and dark, and dim as daylight rises. They turn off 5 minutes after the last detection.
- **Fan** turns on above the setpoint and speeds up to 100% at 3 °C over it. It turns off 1 °C below the setpoint, or 5 minutes after the last detection.
- **Buttons:** B1 and B2 override the lights and fan until the room is vacant. B3 toggles manual mode, where only buttons and commands control the loads.
- **Energy:** power from the SEN0291 is integrated into Wh.

All thresholds are in `firmware/lib/room_logic/src/room_logic.h` (`RoomSettings`) and `firmware/include/config.h`.

## Talking to it

See [docs/mqtt-contract.md](docs/mqtt-contract.md) for the topics and JSON the digital twin uses. You can also type commands such as `{"fan":200}` or `{"mode":"manual"}` into the serial monitor.

## Using the Uno WiFi Rev2 instead

Build with `pio run -e uno_wifi_rev2 -t upload`. The Rev2 pin map is in `board.h`. On the Rev2 you can also wire the LEDs through the wattmeter by setting `LEDS_ACTIVE_LOW = true`.

## Project layout

```
firmware/
  include/board.h        pins + per-board helpers (ESP32 / Rev2)
  include/config.h       timing, topics, sensor settings
  lib/room_logic/        control rules + energy meter (no Arduino code, unit tested)
  src/main.cpp           scheduler, outputs, command handling
  src/sensors.*          PIR, ultrasonic, LM35, light, knob, SEN0291
  src/buttons.*          debounced buttons
  src/comms.*            serial + MQTT
  test/                  native unit tests
docs/esp32-wiring.md     full wiring guide
docs/mqtt-contract.md    interface for the digital twin
```
