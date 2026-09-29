// Energy-saving control logic. Pure C++ with no Arduino dependencies, so it
// runs unchanged on any board and is unit tested on a PC (pio test -e native).
#pragma once
#include <stdint.h>

struct RoomSettings {
  uint32_t presenceHoldMs  = 60000UL;   // "occupied" stays true this long after the last detection
  uint32_t vacancyOffMs    = 300000UL;  // loads switch off after this long with nobody detected
  float    presenceDistCm  = 100.0f;    // ultrasonic reading closer than this counts as presence
  float    lightOnPct      = 40.0f;     // lights turn on below this ambient level
  float    lightOffPct     = 50.0f;     // ...and off above this one (hysteresis)
  uint8_t  ledMinPwm       = 60;        // dimmest auto brightness (at lightOnPct)
  float    fanOffBelowC    = 1.0f;      // fan stops at setpoint - this
  float    fanFullAboveC   = 3.0f;      // fan reaches 100% at setpoint + this
  uint8_t  fanMinPwm       = 102;       // ~40%, below this most small DC fans stall
  float    knobDeadbandC   = 0.5f;      // knob must move this much to change the setpoint
};

struct RoomInputs {
  uint32_t nowMs;
  bool     pir;
  float    distCm;      // < 0 when there is no valid echo
  float    tempC;       // NAN when unavailable
  float    lightPct;    // 0 = dark, 100 = bright
  float    knobSetpointC;
};

struct RoomOutputs {
  uint8_t led1;
  uint8_t led2;
  uint8_t fan;
};

enum class RoomMode : uint8_t { Auto, Manual };

class RoomController {
 public:
  explicit RoomController(const RoomSettings& s = RoomSettings()) : s_(s) {}

  void update(const RoomInputs& in);

  // Buttons
  void toggleLights();
  void toggleFan();
  void toggleMode();

  // Remote commands (acts as an override in Auto mode)
  void setMode(RoomMode m);
  void setLed(uint8_t index, uint8_t value);  // index 0 or 1
  void setFan(uint8_t value);
  void setSetpoint(float c);

  bool        occupied() const { return occupied_; }
  RoomMode    mode() const { return mode_; }
  float       setpoint() const { return setpoint_; }
  RoomOutputs outputs() const { return out_; }
  bool        lightsOverridden() const { return led_[0].overridden || led_[1].overridden; }
  bool        fanOverridden() const { return fan_.overridden; }

  // True once after mode, outputs, setpoint, overrides or occupancy changed.
  bool consumeStateChanged();

 private:
  struct Load { bool overridden = false; uint8_t value = 0; };

  uint8_t autoLed(const RoomInputs& in);
  uint8_t autoFan(const RoomInputs& in);
  void    clearOverrides();
  uint32_t signature() const;

  RoomSettings s_;
  RoomMode mode_ = RoomMode::Auto;
  Load led_[2];
  Load fan_;
  RoomOutputs out_ = {0, 0, 0};

  bool     seenPresence_ = false;
  uint32_t lastPresenceMs_ = 0;
  bool     occupied_ = false;
  bool     loadsAllowed_ = false;
  bool     lightsOn_ = false;
  bool     fanOn_ = false;

  float setpoint_ = 25.0f;
  float lastKnobC_ = -1000.0f;

  bool     changed_ = true;
  uint32_t lastSignature_ = 0;
};

// Integrates power into energy.
class EnergyMeter {
 public:
  void  add(float powerW, uint32_t nowMs);
  float wh() const { return wh_; }
  void  reset() { wh_ = 0; }
 private:
  float    wh_ = 0;
  bool     started_ = false;
  uint32_t lastMs_ = 0;
};
