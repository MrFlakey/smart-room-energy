#pragma once
#include <Arduino.h>

// Debounced push button wired between the pin and GND (uses the internal pull-up).
class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}
  void begin();
  // True once per press.
  bool pressed();

 private:
  uint8_t pin_;
  bool stable_ = HIGH;
  bool lastRaw_ = HIGH;
  unsigned long changedAt_ = 0;
};
