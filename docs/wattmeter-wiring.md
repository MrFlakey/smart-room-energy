# Wiring the SEN0291 wattmeter (ESP32)

The SEN0291 has two sides:

1. **Data side:** a 4-pin connector that talks to the ESP32 over I2C.
2. **Power side:** two screw terminals, **IN+** and **IN−**. The current you want to measure has to **flow through** them, like water through a meter. The board measures the current and the voltage, and works out the watts.

## 1. Data side (to the ESP32 DevKit v1)

| SEN0291 pin | ESP32 |
|---|---|
| **+** (VCC) | **3V3** (not VIN) |
| **−** (GND) | **GND** |
| **C** (SCL, clock) | **GPIO22** |
| **D** (SDA, data) | **GPIO21** |

Go by the letters printed next to the pins, not by wire colours.

**Why 3V3 and not 5 V:** the wattmeter pulls its data lines up to whatever voltage powers it. Powered from 5 V, it would put 5 V onto GPIO21 and GPIO22, which the ESP32 can't take. Powered from 3V3, everything stays at a safe 3.3 V, and it still measures the 5 V fan circuit perfectly (the power side is separate).

Leave both address switches at **1** (address 0x45), which is what the firmware expects.

## 2. Power side (measuring the fan)

The idea is to make a "metered 5 V rail": the ESP32's VIN (5 V from USB) goes **into IN+**, comes **out of IN−**, and the fan takes its power from IN−.

```
 ESP32 VIN (5 V) ──► IN+ ┌──────────┐ IN− ──► Fan module VCC
                         │ SEN0291  │
                         └──────────┘         Fan signal → GPIO25
                                              Fan GND    → GND

 All GNDs (ESP32, SEN0291 "−", fan GND) connected together.
```

### Step by step

1. Wire the ESP32's **VIN** pin to the **IN+** screw terminal.
2. Wire **IN−** to the fan module's **VCC** pin. (If you use a breadboard row for this, keep it separate from your normal 5 V rail.)
3. Connect the fan module's **GND** to GND and its **signal** pin to **GPIO25**.
4. Make sure every GND is connected together.

The LEDs stay on their GPIO pins and aren't measured (see [esp32-wiring.md](esp32-wiring.md#differences-youll-notice-from-the-uno)). Keep `LEDS_ACTIVE_LOW = false` in `config.h`.

## Limits and safety

- Up to 26 V and 8 A on the power side. Your 5 V fan is far below that.
- Don't swap IN+ and IN−: `cur_ma` then shows up as negative.
- If the fan later gets its own supply (for example a 5 V adapter), put that supply's + through IN+ → IN− instead, and connect its GND to the ESP32 GND.

## Check it works

Upload the firmware and open the serial monitor. Each `telemetry` line shows `bus_v` (about 4.7–5 V over USB), `cur_ma` and `power_w`. Turn the fan on with `{"mode":"manual"}` then `{"fan":255}`, and `cur_ma` should jump by the fan's current. If those three values show `null`, the ESP32 can't see the wattmeter: check that SDA is on GPIO21 and SCL on GPIO22 (they're easy to swap), check the 3V3/GND wires, and check that both switches are at 1.
