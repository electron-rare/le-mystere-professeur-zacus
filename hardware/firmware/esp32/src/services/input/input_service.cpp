#include "input_service.h"

InputService::InputService(KeypadAnalog& keypad) : keypad_(keypad) {}

void InputService::begin() {
  keypad_.begin();
}

void InputService::update(uint32_t nowMs) {
  keypad_.update(nowMs);
}

bool InputService::consumePress(KeyEvent* event) {
  if (event == nullptr) {
    return false;
  }

  uint8_t key = 0U;
  uint16_t raw = 0U;
  if (!keypad_.consumePress(&key, &raw)) {
    return false;
  }

  event->key = key;
  event->raw = raw;
  return true;
}

uint16_t InputService::lastRaw() const {
  return keypad_.lastRaw();
}

uint8_t InputService::stableKey() const {
  return keypad_.currentKey();
}
