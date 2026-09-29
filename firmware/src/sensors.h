#pragma once
#include <Arduino.h>

struct SensorReadings {
  bool  pir;
  float distCm;        // -1 when there is no echo in range
  float tempC;         // NAN if the reading is implausible
  float lightPct;      // 0 dark .. 100 bright
  float knobSetpointC;
  bool  wattmeterOk;
  float busV;
  float currentMa;
  float powerW;
};

void sensorsBegin();
SensorReadings sensorsRead();
