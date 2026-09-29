#include "buttons.h"

static const unsigned long DEBOUNCE_MS = 30;

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
  stable_ = lastRaw_ = digitalRead(pin_);
}

bool Button::pressed() {
  bool raw = digitalRead(pin_);
  if (raw != lastRaw_) {
    lastRaw_ = raw;
    changedAt_ = millis();
  }
  if (raw != stable_ && millis() - changedAt_ >= DEBOUNCE_MS) {
    stable_ = raw;
    return stable_ == LOW;
  }
  return false;
}
