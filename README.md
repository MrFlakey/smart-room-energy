# Smart Room Energy Management

This is the room controller for an Arduino Uno WiFi Rev2 (ESP32 ready). It detects occupancy, temperature, light and power, runs the energy-saving rules on the board, drives two LED "lights" and a DC fan, and streams everything over MQTT to a digital twin.

## Hardware

| Part | Rev2 pin | Notes |
|---|---|---|
| PIR | D2 | |
| Fan (MOSFET gate) | D3 | Logic-level N-MOSFET (IRLZ44N) or TIP120, a 220 Ω gate resistor, and a 1N4007 diode across the motor |
| Button B1: lights | D4 | Wire the button between the pin and GND (internal pull-up) |
| LED 1 | D5 | 220 Ω resistor |
| LED 2 | D6 | 220 Ω resistor |
| Button B2: fan | D7 | Pin to GND |
| Ultrasonic TRIG / ECHO | D8 / D9 | |
| Button B3: auto/manual | D12 | Pin to GND |
| LM35 | A0 | Powered from 5 V |
| Ambient light sensor | A1 | Set `LIGHT_INVERTED` in `config.h` if it reads lower when brighter |
| Rotation sensor | A2 | Sets the temperature setpoint, 20 to 30 °C |
| SEN0291 wattmeter | SDA / SCL | Wire it in series (high side) with the supply feeding the LEDs and fan. The address is set by its switches, default 0x45. |

The ESP32 pin map is in `firmware/include/board.h` and in the plan.

## Setup

1. Install [PlatformIO](https://platformio.org/install), either the VS Code extension or `pip install platformio`.
2. Copy `firmware/include/secrets.example.h` to `firmware/include/secrets.h` and fill in your Wi-Fi and MQTT broker details.
3. Build and upload:
   ```
   cd firmware
   pio run -e uno_wifi_rev2 -t upload
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

## Moving to ESP32

Rewire according to the ESP32 column in `board.h`. Put a 5 V to 3.3 V divider on the ultrasonic ECHO line, power the light sensor and knob from 3.3 V, and keep the LM35 on 5 V. Then build with `pio run -e esp32dev`. No other code changes are needed.

## Project layout

```
firmware/
  include/board.h        pins + per-board helpers (Rev2 / ESP32)
  include/config.h       timing, topics, sensor settings
  lib/room_logic/        control rules + energy meter (no Arduino code, unit tested)
  src/main.cpp           scheduler, outputs, command handling
  src/sensors.*          PIR, ultrasonic, LM35, light, knob, SEN0291
  src/buttons.*          debounced buttons
  src/comms.*            serial + MQTT
  test/                  native unit tests
docs/mqtt-contract.md    interface for the digital twin
```
