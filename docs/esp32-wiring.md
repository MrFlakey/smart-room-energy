# ESP32 wiring

Author: Mohamed Khamis

How every DFRobot Gravity module in the Smart Room kit connects to the ESP32 DevKit v1 (30-pin). The pin numbers match `firmware/main/board_pins.h`.

![ESP32 wiring diagram](esp32-wiring-diagram.png)

More detail: [wattmeter-wiring.md](wattmeter-wiring.md) and [fan-wiring.md](fan-wiring.md).

## Parts

| Part | DFRobot SKU | Type |
|---|---|---|
| PIR motion sensor | SEN0018 | digital |
| Ultrasonic sensor (URM09, analog) | SEN0307 | analog |
| Temperature sensor (LM35) | DFR0023 | analog |
| Ambient light sensor | DFR0026 | analog |
| Rotation sensor (10-turn) | DFR0058 | analog |
| Wattmeter (I2C) | SEN0291 | I2C |
| 130 DC motor module (the "AC" fan) | DFR0411 | digital / PWM |
| LED module × 2 | DFR0021 | digital / PWM |
| Push button module × 3 | DFR0029 | digital |

Gravity cables: red = VCC, black = GND, green or blue = signal.

## Connections

| Part | Signal | ESP32 pin | VCC | GND |
|---|---|---|---|---|
| Temperature · DFR0023 | A | GPIO34 | 5 V (VIN) | GND |
| Ambient light · DFR0026 | A | GPIO35 | 3V3 | GND |
| Rotation · DFR0058 | A | GPIO32 | 3V3 | GND |
| Ultrasonic · SEN0307 | A | GPIO33 | 3V3 (+) | GND (−) |
| PIR · SEN0018 | D | GPIO27 | 3V3 | GND |
| Wattmeter · SEN0291 | C (SCL) / D (SDA) | GPIO22 / GPIO21 | 3V3 (+) | GND (−) |
| Wattmeter power side | IN+ / IN− | – | IN+ from 5 V (VIN), IN− to the motor's VCC | – |
| DC motor · DFR0411 | D (PWM) | GPIO25 | from SEN0291 IN− | GND |
| LED 1 · DFR0021 | D (PWM) | GPIO26 | 3V3 | GND |
| LED 2 · DFR0021 | D (PWM) | GPIO19 | 3V3 | GND |
| Button 1 (Lights) · DFR0029 | D | GPIO14 | 3V3 | GND |
| Button 2 (AC) · DFR0029 | D | GPIO13 | 3V3 | GND |
| Button 3 (Auto/Manual) · DFR0029 | D | GPIO23 | 3V3 | GND |

SEN0291 I2C address: 0x45 (both address switches at 1).

## Power rails

| Board pin | What it gives | Use it for |
|---|---|---|
| **VIN** (sometimes marked 5V) | About 5 V from USB | DFR0023 (LM35), and the DFR0411 motor through the wattmeter |
| **3V3** | 3.3 V | Every other module |
| **GND** | Ground | Everything. All grounds must be connected together. |

Make a 5 V rail (from VIN) and a 3.3 V rail (from 3V3) on the breadboard, and connect both − rows to GND.

## Why these pins

- All analog inputs (GPIO32 to 35) are on ADC1, because ADC2 stops working while Wi-Fi is on.
- The ultrasonic is on GPIO33 because GPIO32 and 33 are the only ADC1 pins with internal pull resistors, which the firmware uses to notice an unplugged sensor. That's why LED 2 is on GPIO19.
- GPIO34 and 35 are input-only, which suits sensors.
- Pins that affect booting (GPIO0, 2, 5, 12, 15) are avoided, so the board always starts normally.

## Rules

- ESP32 pins take 3.3 V only. Every module whose output can reach its supply voltage runs on 3V3: SEN0307, DFR0026, DFR0058, SEN0018, the DFR0029 buttons and the DFR0021 LEDs.
- The SEN0018 PIR outputs about 4 V when powered from 5 V, so it runs on 3V3 (DFRobot rates it for 3 to 5 V).
- Only the DFR0023 (LM35) and the DFR0411 motor use 5 V. The LM35 chip needs at least 4 V, and its output is only 10 mV per °C, so GPIO34 is safe. The motor's signal is 3.3 V PWM from GPIO25.
- The motor's 5 V goes through the SEN0291 (VIN → IN+ → IN− → motor VCC), so the wattmeter measures the fan.
- The DFR0021 LEDs have their own driver and resistor, so no external resistor is needed. HIGH turns them on.
- Each DFR0029 button has its own pull resistor and an indicator LED. Check with the serial monitor whether a press reads high or low before relying on the button logic.
- All grounds join: the ESP32 GND, every module's GND and the motor's GND.
- All analog inputs are on ADC1 (GPIO32 to 35) because ADC2 stops working while Wi-Fi is on. Keep GPIO0, 2, 5, 12 and 15 free, and never use GPIO6 to 11 (flash).

## Checklist before plugging in USB

1. Nothing from the 5 V rail touches a GPIO directly.
2. Only the DFR0023 and the motor (through SEN0291 IN+/IN−) are on 5 V. Everything else is on 3V3.
3. Every GND is connected together.
4. Nothing is connected to GPIO6 to 11.
