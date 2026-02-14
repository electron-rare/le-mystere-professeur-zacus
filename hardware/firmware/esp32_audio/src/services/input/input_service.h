#pragma once

#include <Arduino.h>

#include "../../input/keypad_analog.h"
#include "input_event.h"

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
  bool consumeEvent(InputEvent* event);
  bool enqueueUiEvent(const InputEvent& event);
  uint16_t lastRaw() const;
  uint8_t stableKey() const;

 private:
  static constexpr uint8_t kUiQueueSize = 16u;
  InputEvent uiQueue_[kUiQueueSize] = {};
  uint8_t uiHead_ = 0u;
  uint8_t uiTail_ = 0u;
  KeypadAnalog& keypad_;
};
