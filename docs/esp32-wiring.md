# Wiring everything to the ESP32

This guide assumes a common **ESP32 DevKit v1** (the 30-pin ESP-WROOM-32 board). Other ESP32 dev boards use the same GPIO numbers, but the pins may sit in different places. Go by the **GPIO number printed next to each pin** (for example "D25" or "G25" means GPIO25).

## The one rule that's different from the Uno

**ESP32 pins work at 3.3 V and are not 5 V tolerant.** Putting 5 V on any GPIO can damage the chip. Some of your parts need 5 V to run, which is fine, as long as nothing sends 5 V *back into* a GPIO pin. The only part that does that is the ultrasonic sensor's ECHO pin, which needs a simple two-resistor voltage divider (see below).

## Power rails on the board

| Board pin | What it gives | Use it for |
|---|---|---|
| **VIN** (sometimes marked 5V) | About 5 V straight from USB | Parts that need 5 V: PIR, ultrasonic, LM35, and the fan (through the wattmeter) |
| **3V3** | 3.3 V | Parts that send an analog signal to the ESP32, plus the wattmeter's logic: light sensor, knob, SEN0291 "+" |
| **GND** | Ground | Everything. All grounds must be connected together. |

Make a **5 V rail** (from VIN) and a **3.3 V rail** (from 3V3) on the breadboard's two + rows, and connect both − rows to GND.

## Pin map

| Part | Part pin | Goes to | Notes |
|---|---|---|---|
| **PIR** | VCC / GND / OUT | 5 V rail / GND / **GPIO27** | Its output is 3.3 V, so it's safe to connect directly |
| **Ultrasonic** | VCC / GND | 5 V rail / GND | |
| | TRIG | **GPIO18** | 3.3 V is enough to trigger it |
| | ECHO | **GPIO19 through a divider** | See below. **Never connect it directly.** |
| **LM35** (flat side facing you, legs down) | left / middle / right | 5 V rail / **GPIO34** / GND | Needs at least 4 V to work. Its output is only about 0.2–0.4 V, so it's safe. |
| **Ambient light sensor** | VCC / GND / signal | 3.3 V rail / GND / **GPIO35** | Must be on 3.3 V so the signal can't exceed 3.3 V |
| **Rotation sensor** (knob) | VCC / GND / signal | 3.3 V rail / GND / **GPIO32** | Same reason |
| **SEN0291 wattmeter**, data side | + / − / C / D | **3.3 V rail** / GND / **GPIO22** (SCL) / **GPIO21** (SDA) | See [wattmeter-wiring.md](wattmeter-wiring.md) |
| **Fan module** | VCC / GND / signal | Metered 5 V (wattmeter IN−) / GND / **GPIO25** | See [fan-wiring.md](fan-wiring.md) |
| **LED 1** | long leg (+) | **GPIO26** → 220 Ω → LED | Short leg to GND |
| **LED 2** | long leg (+) | **GPIO33** → 220 Ω → LED | Short leg to GND |
| **Button B1** (lights) | two legs | **GPIO14** and GND | Uses the internal pull-up, no resistor needed |
| **Button B2** (fan) | two legs | **GPIO13** and GND | |
| **Button B3** (auto/manual) | two legs | **GPIO23** and GND | |

## Ultrasonic ECHO divider

The ECHO pin sends back 5 V. Two resistors bring that down to about 3.3 V:

```
 ECHO ──[ 1 kΩ ]──┬── GPIO19
                  │
               [ 2 kΩ ]
                  │
                 GND
```

If you don't have a 2 kΩ resistor, use two 1 kΩ in series, or a 10 kΩ + 20 kΩ pair (always keep the smaller one on the ECHO side).

## Why these pins

- **All analog inputs (GPIO32, 34, 35) are on ADC1.** The ESP32's second ADC stops working while Wi-Fi is on.
- GPIO34 and 35 are input-only, which suits sensors.
- Pins that affect booting (GPIO0, 2, 5, 12, 15) are avoided, so the board always starts normally.
- GPIO6–11 are used by the board's flash memory and must never be connected.

## Differences you'll notice from the Uno

- **LEDs are a bit dimmer** because they run on 3.3 V. For more brightness, use 100 Ω resistors instead of 220 Ω (still safe).
- **The LEDs aren't measured by the wattmeter.** On the ESP32 they have to be powered from the pin, so their current never passes through the meter. It's only about 5–10 mA each, which is tiny next to the fan. Leave `LEDS_ACTIVE_LOW = false` in `config.h`; the firmware refuses to build if it's set to true on an ESP32, to protect the pins.
- **The LM35 is a little noisier** on the ESP32's ADC. The firmware averages 16 readings to smooth it out.

## Checklist before plugging in USB

1. Nothing from the 5 V rail touches a GPIO directly, and ECHO goes through the divider.
2. The light sensor, knob and SEN0291 "+" are on **3V3**, not VIN.
3. Every GND is connected together.
4. Nothing is connected to GPIO6–11.
