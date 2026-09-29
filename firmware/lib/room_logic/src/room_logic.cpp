#include "room_logic.h"
#include <math.h>

static float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

void RoomController::update(const RoomInputs& in) {
  bool detected = in.pir || (in.distCm >= 0 && in.distCm < s_.presenceDistCm);
  if (detected) {
    seenPresence_ = true;
    lastPresenceMs_ = in.nowMs;
  }
  uint32_t since = in.nowMs - lastPresenceMs_;  // wraps safely after ~49 days
  occupied_ = seenPresence_ && since < s_.presenceHoldMs;

  bool wasAllowed = loadsAllowed_;
  loadsAllowed_ = seenPresence_ && since < s_.vacancyOffMs;
  if (wasAllowed && !loadsAllowed_) clearOverrides();  // room went vacant: back to auto

  if (!isnan(in.knobSetpointC) && fabsf(in.knobSetpointC - lastKnobC_) > s_.knobDeadbandC) {
    setpoint_ = in.knobSetpointC;
    lastKnobC_ = in.knobSetpointC;
  }

  uint8_t autoL = autoLed(in);
  uint8_t autoF = autoFan(in);

  if (mode_ == RoomMode::Manual) {
    out_ = {led_[0].value, led_[1].value, fan_.value};
  } else {
    out_.led1 = led_[0].overridden ? led_[0].value : autoL;
    out_.led2 = led_[1].overridden ? led_[1].value : autoL;
    out_.fan  = fan_.overridden ? fan_.value : autoF;
  }
}

uint8_t RoomController::autoLed(const RoomInputs& in) {
  if (!loadsAllowed_) { lightsOn_ = false; return 0; }
  if (lightsOn_ && in.lightPct > s_.lightOffPct) lightsOn_ = false;
  else if (!lightsOn_ && in.lightPct < s_.lightOnPct) lightsOn_ = true;
  if (!lightsOn_) return 0;
  // Dark room -> full brightness; near the threshold -> dimmest (daylight harvesting)
  float frac = clamp01(in.lightPct / s_.lightOnPct);
  return (uint8_t)(255 - frac * (255 - s_.ledMinPwm));
}

uint8_t RoomController::autoFan(const RoomInputs& in) {
  if (!loadsAllowed_ || isnan(in.tempC)) { fanOn_ = false; return 0; }
  if (!fanOn_ && in.tempC > setpoint_) fanOn_ = true;
  else if (fanOn_ && in.tempC < setpoint_ - s_.fanOffBelowC) fanOn_ = false;
  if (!fanOn_) return 0;
  float frac = clamp01((in.tempC - setpoint_) / s_.fanFullAboveC);
  return (uint8_t)(s_.fanMinPwm + frac * (255 - s_.fanMinPwm));
}

void RoomController::clearOverrides() {
  led_[0].overridden = led_[1].overridden = fan_.overridden = false;
}

void RoomController::toggleLights() {
  uint8_t v = (out_.led1 || out_.led2) ? 0 : 255;
  setLed(0, v);
  setLed(1, v);
}

void RoomController::toggleFan() { setFan(out_.fan ? 0 : 255); }

void RoomController::toggleMode() {
  setMode(mode_ == RoomMode::Auto ? RoomMode::Manual : RoomMode::Auto);
}

void RoomController::setMode(RoomMode m) {
  if (m == mode_) return;
  if (m == RoomMode::Manual) {
    // Keep whatever is on right now, so switching modes causes no jump
    led_[0].value = out_.led1;
    led_[1].value = out_.led2;
    fan_.value = out_.fan;
  }
  clearOverrides();
  mode_ = m;
  changed_ = true;
}

void RoomController::setLed(uint8_t index, uint8_t value) {
  if (index > 1) return;
  led_[index].overridden = true;
  led_[index].value = value;
  if (index == 0) out_.led1 = value; else out_.led2 = value;
}

void RoomController::setFan(uint8_t value) {
  fan_.overridden = true;
  fan_.value = value;
  out_.fan = value;
}

void RoomController::setSetpoint(float c) {
  if (isnan(c)) return;
  setpoint_ = c;
}

// State that the twin keeps as a retained message. Exact PWM levels are left
// out (they drift with daylight and go in telemetry instead).
uint32_t RoomController::signature() const {
  uint32_t sig = (uint32_t)mode_;
  sig = sig * 2 + occupied_;
  sig = sig * 2 + (out_.led1 > 0);
  sig = sig * 2 + (out_.led2 > 0);
  sig = sig * 2 + (out_.fan > 0);
  sig = sig * 2 + lightsOverridden();
  sig = sig * 2 + fan_.overridden;
  sig = sig * 1024 + (uint32_t)(int32_t)lroundf(setpoint_ * 10) % 1024;
  return sig;
}

bool RoomController::consumeStateChanged() {
  uint32_t sig = signature();
  if (!changed_ && sig == lastSignature_) return false;
  changed_ = false;
  lastSignature_ = sig;
  return true;
}

void EnergyMeter::add(float powerW, uint32_t nowMs) {
  if (started_ && !isnan(powerW) && powerW > 0) {
    wh_ += powerW * (float)(nowMs - lastMs_) / 3600000.0f;
  }
  started_ = true;
  lastMs_ = nowMs;
}
