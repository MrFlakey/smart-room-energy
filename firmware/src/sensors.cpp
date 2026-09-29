#include "sensors.h"
#include <Wire.h>
#include <DFRobot_INA219.h>
#include "board.h"
#include "config.h"

static DFRobot_INA219_IIC wattmeter(&Wire, WATTMETER_ADDR);
static bool wattmeterOk = false;
static unsigned long lastWattmeterTry = 0;

static float readDistanceCm() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  // 25 ms timeout ~ 4 m round trip; returns 0 when nothing echoes back
  unsigned long us = pulseIn(PIN_US_ECHO, HIGH, 25000UL);
  if (us == 0) return -1;
  float cm = us / 58.0f;
  return cm > DIST_MAX_CM ? -1 : cm;
}

static float readTempC() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LM35_SAMPLES; i++) sum += readMillivolts(PIN_LM35);
  float c = (sum / (float)LM35_SAMPLES) / 10.0f;  // LM35: 10 mV per degree C
  return (c < -5 || c > 100) ? NAN : c;           // disconnected pin reads garbage
}

static float readPercent(uint8_t pin) {
  float pct = readMillivolts(pin) * 100.0f / SENSOR_VCC_MV;
  return constrain(pct, 0.0f, 100.0f);
}

void sensorsBegin() {
  boardInit();
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  Wire.begin();
  wattmeterOk = wattmeter.begin();
  lastWattmeterTry = millis();
}

SensorReadings sensorsRead() {
  SensorReadings r;
  r.pir = digitalRead(PIN_PIR) == HIGH;
  r.distCm = readDistanceCm();
  r.tempC = readTempC();

  float light = readPercent(PIN_LIGHT);
  r.lightPct = LIGHT_INVERTED ? 100.0f - light : light;

  float knob = readPercent(PIN_KNOB) / 100.0f;
  float setpoint = KNOB_MIN_C + knob * (KNOB_MAX_C - KNOB_MIN_C);
  r.knobSetpointC = roundf(setpoint * 2) / 2;  // 0.5 degree steps

  // Retry the wattmeter every 5 s if it was missing at boot
  if (!wattmeterOk && millis() - lastWattmeterTry > 5000) {
    wattmeterOk = wattmeter.begin();
    lastWattmeterTry = millis();
  }
  r.wattmeterOk = wattmeterOk;
  if (wattmeterOk) {
    r.busV = wattmeter.getBusVoltage_V();
    r.currentMa = wattmeter.getCurrent_mA();
    r.powerW = wattmeter.getPower_mW() / 1000.0f;
  } else {
    r.busV = r.currentMa = r.powerW = NAN;
  }
  return r;
}
