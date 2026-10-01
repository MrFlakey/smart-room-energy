// Default ESP32 DevKit v1 pin map for the Smart Room parts.
// Wiring details: README.md, "Wiring".
#pragma once

// Ultrasonic (HC-SR04 style). ECHO goes through a 1k/2k divider.
#define PIN_US_TRIG     18
#define PIN_US_ECHO     19

#define PIN_PIR         27

// Analog inputs, all on ADC1 (ADC2 stops working while Wi-Fi is on)
#define PIN_LM35        34
#define PIN_LIGHT       35
#define PIN_ROTATION    32

// SEN0291 wattmeter (I2C)
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define SEN0291_ADDR    0x45  // both address switches at 1

#define PIN_FAN         25
#define PIN_LED1        26
#define PIN_LED2        33

// Buttons connect the pin to GND; internal pull-ups are used
#define PIN_BTN1        14
#define PIN_BTN2        13
#define PIN_BTN3        23
