# Smart Room Energy Management: Starting Plan

Revised 2026-09-29. The board is now an **ESP32 DevKit v1** (assumed model). The Uno WiFi Rev2 build is kept as a fallback. The wattmeter is a DFRobot SEN0291. The user interface and dashboard will be a digital twin that talks over MQTT.

## 1. What the system does

Watches a room (is anyone there, how warm is it, how bright is it, how much power is being drawn) and automatically controls the loads (two LED "lights" and a DC fan) to avoid wasting energy. It streams everything over MQTT to a digital twin, which mirrors the room live and can send commands back. The room keeps working with no network.

## 2. Hardware and roles

| Part | Role |
|---|---|
| ESP32 DevKit v1 (ESP-WROOM-32) | Controller, with built-in Wi-Fi, **3.3 V logic (not 5 V tolerant)** and a 12-bit ADC. |
| PIR sensor | Motion, the main occupancy signal. |
| Ultrasonic sensor (HC-SR04 style) | Presence at a desk or doorway, to catch people sitting still. The room is occupied if either sensor fires. |
| LM35 | Room temperature, which drives the fan. |
| Ambient light sensor (analog) | Daylight. Lights stay off or dim when the room is already bright. |
| Rotation sensor (potentiometer) | Sets the temperature setpoint by hand, 20 to 30 °C. |
| DFRobot SEN0291 (INA219, I2C, 0 to 26 V, up to 8 A) | Measures voltage, current and power of the load supply, used for W and Wh. It's wired high-side in series with the fan's 5 V supply and powered from 3V3, so its I2C lines stay at 3.3 V. The default I2C address is 0x45 (both switches at 1). It uses the `DFRobot_INA219` library. |
| 3 push buttons | B1 toggles light override, B2 toggles fan override, B3 switches between Auto and Manual mode. |
| 2 LEDs | Stand in for the room lights (zone 1 and 2), dimmable with PWM. They're powered from GPIOs on the ESP32, so the wattmeter doesn't measure them (only a few mA). |
| DC fan module (3-pin, driver built in) | Cooling load, with PWM speed control from GPIO25. |

The full ESP32 wiring guide is in `smart-room-energy/docs/esp32-wiring.md`.

## 3. Pin map (ESP32 now, Rev2 fallback)

| Signal | Uno WiFi Rev2 | ESP32 DevKit v1 | ESP32 note |
|---|---|---|---|
| PIR | D2 | GPIO27 | PIR output is 3.3 V, so it's safe. |
| Fan module signal | D3 | GPIO25 | Module VCC from the wattmeter's IN− (5 V) |
| Button B1 lights | D4 | GPIO14 | `INPUT_PULLUP`, button to GND |
| LED 1 | D5 | GPIO26 | 220 Ω resistor |
| LED 2 | D6 | GPIO33 | 220 Ω resistor |
| Button B2 fan | D7 | GPIO13 | |
| Ultrasonic TRIG | D8 | GPIO18 | |
| Ultrasonic ECHO | D9 | GPIO19 | **Needs a voltage divider (5 V to 3.3 V)**, e.g. 1 kΩ / 2 kΩ. |
| Button B3 mode | D12 | GPIO23 | |
| LM35 | A0 | GPIO34 | Keep the LM35 on 5 V (it needs at least 4 V). Its output of 1.5 V or less is safe. |
| Ambient light | A1 | GPIO35 | Power the sensor from 3.3 V on the ESP32. |
| Rotation knob | A2 | GPIO32 | Power it from 3.3 V on the ESP32. |
| I2C (SEN0291) | SDA / SCL | GPIO21 / GPIO22 | Power the SEN0291 from 3V3 |

ESP32 analog inputs are all on ADC1, because ADC2 stops working while Wi-Fi is on. Strapping pins (0, 2, 5, 12, 15) are avoided.

**How the port is handled:** every pin and board difference lives in `include/board.h`, with one `#if` block per board. The rest of the firmware only calls small helpers (`readMillivolts()`, `setPwm()`, `wifiConnect()`). Moving to the ESP32 only needed the board block and rewiring. The ESP32 (`esp32dev`) is now the default build. `readMillivolts()` hides the ADC difference (5 V/10-bit on the Rev2, `analogReadMilliVolts()` on the ESP32).

## 4. Control rules (Auto mode)

- **Occupied** means the PIR fired in the last 60 s, or the ultrasonic distance is below 100 cm.
- **Lights:** when occupied and ambient light is below the threshold, the LEDs turn on. They dim as daylight rises and turn off after 5 minutes vacant.
- **Fan:** when occupied and temperature is above the setpoint, the fan turns on. Speed ramps from 40% at the setpoint to 100% at 3 °C over it. It turns off at the setpoint minus 1 °C (hysteresis) or after 5 minutes vacant.
- **Setpoint:** the knob sets it, and the twin can also set it over MQTT. The most recent change wins. The knob only counts as changed when it moves more than 0.5 °C.
- **Overrides:** a button press or a twin command forces a load. The override holds until the room goes vacant, then Auto takes back over. Manual mode ignores the rules entirely.
- **Energy:** the board integrates power into Wh. The twin can compare that against a "loads always on" baseline to show savings.

## 5. MQTT contract for the digital twin

Broker: Mosquitto on a PC or Raspberry Pi (or any broker the twin already uses). The base topic is `smartroom/<room_id>/`, for example `smartroom/room1/`.

| Topic | Direction | Retained | Content |
|---|---|---|---|
| `…/telemetry` | board → twin | no | Full reading every 2 s |
| `…/state` | board → twin | yes | Mode, outputs, setpoint and overrides; sent whenever any of them changes |
| `…/status` | board → twin | yes | `online` / `offline` (offline is sent by the broker as a Last Will) |
| `…/event` | board → twin | no | `{"type":"occupancy","value":1}`, `{"type":"button","id":3}` |
| `…/cmd` | twin → board | no | Commands (see below) |

Example telemetry:
```json
{"ts":123456,"occ":1,"pir":1,"dist_cm":82,"temp_c":26.4,"set_c":25.0,"light_pct":41,
 "bus_v":5.02,"cur_ma":368,"power_w":1.84,"energy_wh":12.7,
 "led1":180,"led2":180,"fan":160,"mode":"auto"}
```
Commands the board accepts on `…/cmd`:
```json
{"mode":"auto"}   {"mode":"manual"}
{"led1":255}   {"led2":0}   {"fan":128}      (0–255; acts as an override in auto mode)
{"setpoint":24.5}
{"reset_energy":true}
```
Library: `PubSubClient` (it works with both `WiFiNINA` and the ESP32 `WiFi` library). Payloads stay under 512 bytes.

## 6. Data flow

```
 PIR, ultrasonic, LM35, light, knob, buttons ─► firmware (rules run on board) ─► LEDs, fan
 SEN0291 wattmeter (I2C) ─────────────────────►         │   ▲
                                          telemetry/state│   │cmd
                                                        ▼   │
                                                   MQTT broker
                                                        │   ▲
                                                        ▼   │
                                                  Digital twin (UI + dashboard)
```
The twin can store its own history. A separate logger (for example a small Python subscriber writing to SQLite) is optional, if the twin doesn't keep history.

## 7. Milestones

1. **Standalone controller:** all sensors read, the rules drive the LEDs and fan, the buttons and modes work, and the SEN0291 gives W and Wh. Everything prints as the same JSON over serial that MQTT will carry later.
2. **MQTT:** the board connects over Wi-Fi, publishes telemetry, state, status and events, and handles commands, and reconnects on its own if the connection drops.
3. **Digital twin integration:** the twin subscribes, mirrors the room live, and sends commands back, with Wh and savings charts.
4. **ESP32 port:** done in firmware (default build). Remaining: wire it per the guide and check the LM35 and light readings against a reference.

## 8. Project layout

```
smart-room-energy/
├── firmware/                  # PlatformIO: envs esp32dev (default), uno_wifi_rev2
│   ├── platformio.ini
│   ├── include/
│   │   ├── board.h            # pins + per-board helpers (ESP32 / Rev2)
│   │   ├── config.h           # thresholds, timeouts, room id, topics
│   │   └── secrets.h          # Wi-Fi + broker credentials (git-ignored)
│   ├── src/
│   │   ├── main.cpp           # setup/loop, non-blocking millis() scheduler
│   │   ├── sensors.*          # PIR, ultrasonic, LM35, light, knob, SEN0291
│   │   ├── actuators.*        # LED + fan PWM
│   │   ├── buttons.*          # debounce, overrides, mode
│   │   ├── rules.*            # occupancy + energy logic (hardware-free, unit tested)
│   │   └── comms.*            # serial JSON now, MQTT in Milestone 2
│   └── test/
├── docs/
│   ├── wiring.md
│   └── mqtt-contract.md       # section 5, for the twin side
└── README.md
```
