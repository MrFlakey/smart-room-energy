// Smart Room Energy Management: room controller firmware.
// Reads the room sensors, runs the energy-saving rules on the board, drives the
// LEDs and fan, and reports everything over serial and MQTT (for the digital twin).
#include <Arduino.h>
#include <math.h>
#include "board.h"
#include "config.h"
#include "sensors.h"
#include "buttons.h"
#include "comms.h"
#include "room_logic.h"

static RoomController room;
static EnergyMeter energy;
static SensorReadings last;

static Button btnLights(PIN_BTN_LIGHTS);
static Button btnFan(PIN_BTN_FAN);
static Button btnMode(PIN_BTN_MODE);

static unsigned long lastSensorMs = 0;
static unsigned long lastTelemetryMs = 0;
static bool lastOccupied = false;

static const char* modeName() { return room.mode() == RoomMode::Auto ? "auto" : "manual"; }

static uint8_t clampPwm(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static void applyOutputs() {
  RoomOutputs o = room.outputs();
  setPwm(PIN_LED1, o.led1);
  setPwm(PIN_LED2, o.led2);
  setPwm(PIN_FAN, o.fan);
}

static float round1(float v) { return isnan(v) ? v : roundf(v * 10) / 10; }

static void publishTelemetry() {
  RoomOutputs o = room.outputs();
  JsonDocument doc;
  doc["ts"] = millis();
  doc["occ"] = room.occupied() ? 1 : 0;
  doc["pir"] = last.pir ? 1 : 0;
  doc["dist_cm"] = round1(last.distCm);
  doc["temp_c"] = round1(last.tempC);
  doc["set_c"] = room.setpoint();
  doc["light_pct"] = round1(last.lightPct);
  doc["bus_v"] = last.busV;
  doc["cur_ma"] = round1(last.currentMa);
  doc["power_w"] = last.powerW;
  doc["energy_wh"] = energy.wh();
  doc["led1"] = o.led1;
  doc["led2"] = o.led2;
  doc["fan"] = o.fan;
  doc["mode"] = modeName();
  commsPublish("telemetry", doc);
}

static void publishState() {
  RoomOutputs o = room.outputs();
  JsonDocument doc;
  doc["mode"] = modeName();
  doc["occ"] = room.occupied() ? 1 : 0;
  doc["set_c"] = room.setpoint();
  doc["led1"] = o.led1;
  doc["led2"] = o.led2;
  doc["fan"] = o.fan;
  doc["lights_override"] = room.lightsOverridden();
  doc["fan_override"] = room.fanOverridden();
  doc["board"] = BOARD_NAME;
  commsPublish("state", doc, true);
}

static void publishEvent(const char* type, const char* key, int value) {
  JsonDocument doc;
  doc["type"] = type;
  doc[key] = value;
  commsPublish("event", doc);
}

// Commands from the twin (MQTT) or the serial monitor, e.g. {"fan":128}
static void onCommand(JsonDocument& cmd) {
  if (cmd["mode"].is<const char*>()) {
    room.setMode(strcmp(cmd["mode"], "manual") == 0 ? RoomMode::Manual : RoomMode::Auto);
  }
  if (cmd["led1"].is<int>()) room.setLed(0, clampPwm(cmd["led1"]));
  if (cmd["led2"].is<int>()) room.setLed(1, clampPwm(cmd["led2"]));
  if (cmd["fan"].is<int>()) room.setFan(clampPwm(cmd["fan"]));
  if (cmd["setpoint"].is<float>()) room.setSetpoint(cmd["setpoint"]);
  if (cmd["reset_energy"] == true) energy.reset();
  applyOutputs();
}

static void pollButtons() {
  bool any = false;
  if (btnLights.pressed()) { room.toggleLights(); publishEvent("button", "id", 1); any = true; }
  if (btnFan.pressed())    { room.toggleFan();    publishEvent("button", "id", 2); any = true; }
  if (btnMode.pressed())   { room.toggleMode();   publishEvent("button", "id", 3); any = true; }
  if (any) applyOutputs();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  applyOutputs();  // everything off at boot
  btnLights.begin();
  btnFan.begin();
  btnMode.begin();
  sensorsBegin();
  commsBegin(onCommand);
  Serial.println(F("info {\"msg\":\"smart room controller started\",\"board\":\"" BOARD_NAME "\"}"));
}

void loop() {
  unsigned long now = millis();
  bool reconnected = commsLoop();
  pollButtons();

  if (now - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = now;
    last = sensorsRead();
    energy.add(last.powerW, now);
    room.update({(uint32_t)now, last.pir, last.distCm, last.tempC, last.lightPct, last.knobSetpointC});
    applyOutputs();

    if (room.occupied() != lastOccupied) {
      lastOccupied = room.occupied();
      publishEvent("occupancy", "value", lastOccupied ? 1 : 0);
    }
  }

  if (room.consumeStateChanged() || reconnected) publishState();

  if (now - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
    lastTelemetryMs = now;
    publishTelemetry();
  }
}
