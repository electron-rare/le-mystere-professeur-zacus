#pragma once

#include <Arduino.h>

#include "../../input/keypad_analog.h"

struct KeyEvent {
  uint8_t key = 0;
  uint16_t raw = 0;
};

class InputService {
 public:
  explicit InputService(KeypadAnalog& keypad);

  void begin();
  void update(uint32_t nowMs);
  bool consumePress(KeyEvent* event);
  uint16_t lastRaw() const;
  uint8_t stableKey() const;

 private:
  KeypadAnalog& keypad_;
};
