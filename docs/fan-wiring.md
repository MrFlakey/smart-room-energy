# Wiring the fan (ESP32)

## Your fan: 3-pin module (driver built in)

| Fan module pin | Goes to |
|---|---|
| **VCC** (+) | Wattmeter **IN−** (the metered 5 V, see [wattmeter-wiring.md](wattmeter-wiring.md)). If you're not using the wattmeter yet, connect it to the ESP32's **VIN** instead. |
| **GND** (−) | **GND** |
| **Signal** (S, IN or PWM) | **GPIO25** |

The fan motor gets its power from 5 V, and the ESP32 only sends a 3.3 V control signal, so no extra parts are needed.

**Test:** upload the firmware, open the serial monitor and type `{"mode":"manual"}`, then `{"fan":255}` (full speed), `{"fan":120}` (slower) and `{"fan":0}` (off). Type `{"mode":"auto"}` to go back.

- **The fan doesn't spin at all:** some modules need a 5 V signal. Tell me the module's name or model and I'll suggest a fix, usually a small transistor.
- **It only switches on and off without changing speed:** the module doesn't support speed control. That still works, and I can switch the logic to plain on/off.

## If you ever use a bare 2-wire motor instead

A bare motor needs a separate switch, because an ESP32 pin supplies only a few milliamps and a motor needs hundreds.

- **Switch:** an N-MOSFET that fully turns on from 3.3 V, such as the **IRLB8721** or **AO3400**. The IRLZ44N is only partly on at 3.3 V, and the TIP120 wastes a lot of voltage at 3.3 V.
- **220 Ω** resistor from GPIO25 to the Gate, and **10 kΩ** from the Gate to GND.
- **Source** to GND, **Drain** to the motor's −, and the motor's + to 5 V (VIN).
- **1N4007 diode** across the motor, with the striped end on the 5 V side.
