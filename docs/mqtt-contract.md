# MQTT contract (board ↔ digital twin)

The base topic is `smartroom/<room_id>/`, and `room_id` is `ROOM_ID` in `firmware/include/config.h` (default `room1`).
Every payload is JSON except `status`. The same messages appear on the USB serial monitor, prefixed with the subtopic name, for example `telemetry {...}`.

| Topic | Direction | Retained | When |
|---|---|---|---|
| `smartroom/room1/telemetry` | board → twin | no | Every 2 s |
| `smartroom/room1/state` | board → twin | yes | On change: mode, occupancy, a load switching on or off, overrides, setpoint. Also re-sent on every (re)connect. |
| `smartroom/room1/status` | board → twin | yes | `online` on connect. The broker publishes `offline` (Last Will) if the board drops. |
| `smartroom/room1/event` | board → twin | no | Occupancy changes and button presses |
| `smartroom/room1/cmd` | twin → board | no | Commands |

## telemetry

```json
{"ts":123456,"occ":1,"pir":1,"dist_cm":82.4,"temp_c":26.4,"set_c":25,"light_pct":41.2,
 "bus_v":5.02,"cur_ma":368.1,"power_w":1.84,"energy_wh":12.7,
 "led1":180,"led2":180,"fan":160,"mode":"auto"}
```

| Field | Meaning |
|---|---|
| `ts` | Milliseconds since the board booted |
| `occ` | 1 if someone was detected in the last 60 s |
| `pir` | Raw PIR state right now |
| `dist_cm` | Ultrasonic distance, `-1` when there's no echo in range |
| `temp_c` | LM35 temperature, `null` if the sensor reading is implausible |
| `set_c` | Current temperature setpoint |
| `light_pct` | Ambient light, 0 is dark and 100 is bright |
| `bus_v`, `cur_ma`, `power_w` | SEN0291 readings for the load supply, `null` if the wattmeter isn't found |
| `energy_wh` | Energy used since boot or since the last `reset_energy` |
| `led1`, `led2`, `fan` | Output levels, 0–255 |
| `mode` | `auto` or `manual` |

## state (retained)

```json
{"mode":"auto","occ":1,"set_c":25,"led1":180,"led2":180,"fan":0,
 "lights_override":false,"fan_override":false,"board":"esp32"}
```

## event

```json
{"type":"occupancy","value":1}
{"type":"button","id":1}      // 1 = lights, 2 = fan, 3 = mode
```

## cmd

Send one or more keys in a single JSON object:

```json
{"mode":"auto"}            {"mode":"manual"}
{"led1":255}  {"led2":0}   {"fan":128}          // 0–255
{"setpoint":24.5}
{"reset_energy":true}
```

In `auto` mode, `led1`, `led2` and `fan` act as overrides that hold until the room has been vacant for 5 minutes. In `manual` mode they simply set the level. The knob can also change the setpoint. Whichever changed last wins, and the knob only counts once it moves more than 0.5 °C.

You can type the same JSON into the serial monitor to test without a broker.
