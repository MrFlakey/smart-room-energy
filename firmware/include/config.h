// Tunable settings. Pins are in board.h, credentials in secrets.h.
#pragma once

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "include/secrets.h not found: copy secrets.example.h to secrets.h and fill it in"
  #include "secrets.example.h"
#endif

// Set to 0 to run fully offline (serial only)
#define ENABLE_MQTT 1

// Identity / MQTT topics: smartroom/<ROOM_ID>/{telemetry,state,status,event,cmd}
#define ROOM_ID      "room1"
#define TOPIC_BASE   "smartroom/" ROOM_ID "/"

// Timing (ms)
const unsigned long SENSOR_PERIOD_MS    = 200;
const unsigned long TELEMETRY_PERIOD_MS = 2000;
const unsigned long WIFI_RETRY_MS       = 30000;  // WiFi.begin blocks for a few s on the Rev2
const unsigned long MQTT_RETRY_MS       = 5000;

// Sensors
const float    DIST_MAX_CM      = 400.0f;
const uint8_t  LM35_SAMPLES     = 10;
const bool     LIGHT_INVERTED   = false;  // true if your light sensor reads lower when brighter
const float    KNOB_MIN_C       = 20.0f;
const float    KNOB_MAX_C       = 30.0f;

// SEN0291 I2C address: 0x40, 0x41, 0x44 or 0x45 (default, both switches at 1)
#define WATTMETER_ADDR INA219_I2C_ADDRESS4
