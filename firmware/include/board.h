// Everything that differs between boards lives here, so the rest of the
// firmware is the same on the ESP32 (current board) and the Uno WiFi Rev2.
// Wiring for the ESP32 is in docs/esp32-wiring.md.
#pragma once
#include <Arduino.h>

#if defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
  #define BOARD_NAME "uno_wifi_rev2"

  const uint8_t PIN_PIR        = 2;
  const uint8_t PIN_FAN        = 3;   // PWM -> MOSFET gate
  const uint8_t PIN_BTN_LIGHTS = 4;
  const uint8_t PIN_LED1       = 5;   // PWM
  const uint8_t PIN_LED2       = 6;   // PWM
  const uint8_t PIN_BTN_FAN    = 7;
  const uint8_t PIN_US_TRIG    = 8;
  const uint8_t PIN_US_ECHO    = 9;
  const uint8_t PIN_BTN_MODE   = 12;
  const uint8_t PIN_LM35       = A0;
  const uint8_t PIN_LIGHT      = A1;
  const uint8_t PIN_KNOB       = A2;

  // Supply voltage seen by the light sensor and knob (their full scale)
  const uint16_t SENSOR_VCC_MV = 5000;

  // On the megaAVR core DEFAULT means the 0.55 V internal reference, so ask for VDD (5 V)
  inline void boardInit() { analogReference(VDD); }

  inline uint16_t readMillivolts(uint8_t pin) {
    return (uint32_t)analogRead(pin) * 5000UL / 1023UL;
  }

#elif defined(ESP32)
  // ESP32 DevKit v1. GPIOs are 3.3 V and NOT 5 V tolerant.
  #include <WiFi.h>
  #define BOARD_NAME "esp32"

  const uint8_t PIN_PIR        = 27;
  const uint8_t PIN_FAN        = 25;  // fan module signal (PWM)
  const uint8_t PIN_BTN_LIGHTS = 14;
  const uint8_t PIN_LED1       = 26;
  const uint8_t PIN_LED2       = 33;
  const uint8_t PIN_BTN_FAN    = 13;
  const uint8_t PIN_US_TRIG    = 18;
  const uint8_t PIN_US_ECHO    = 19;  // through a 5V -> 3.3V divider!
  const uint8_t PIN_BTN_MODE   = 23;
  const uint8_t PIN_LM35       = 34;  // ADC1 only: ADC2 is unusable with Wi-Fi on
  const uint8_t PIN_LIGHT      = 35;
  const uint8_t PIN_KNOB       = 32;
  // I2C uses the default pins: SDA = 21, SCL = 22

  // Light sensor and knob are powered from 3V3, so that is their full scale
  const uint16_t SENSOR_VCC_MV = 3300;

  inline void boardInit() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // 0..~3.1 V input range
  }

  // Uses the chip's factory calibration, so the LM35 reads true millivolts
  inline uint16_t readMillivolts(uint8_t pin) {
    return analogReadMilliVolts(pin);
  }

#else
  #error "Unsupported board: add a pin block to include/board.h"
#endif

inline void setPwm(uint8_t pin, uint8_t value) { analogWrite(pin, value); }
