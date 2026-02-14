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

bool InputService::enqueueUiEvent(const InputEvent& event) {
  const uint8_t next = static_cast<uint8_t>((uiHead_ + 1u) % kUiQueueSize);
  if (next == uiTail_) {
    return false;
  }
  uiQueue_[uiHead_] = event;
  uiHead_ = next;
  return true;
}

bool InputService::consumeEvent(InputEvent* event) {
  if (event == nullptr) {
    return false;
  }

  KeyEvent key = {};
  if (consumePress(&key)) {
    event->source = InputEventSource::kLocalKeypad;
    event->type = InputEventType::kButton;
    event->code = key.key;
    event->value = 0;
    event->action = InputButtonAction::kClick;
    event->tsMs = millis();
    event->raw = key.raw;
    return true;
  }

  if (uiTail_ == uiHead_) {
    return false;
  }
  *event = uiQueue_[uiTail_];
  uiTail_ = static_cast<uint8_t>((uiTail_ + 1u) % kUiQueueSize);
  return true;
}

uint16_t InputService::lastRaw() const {
  return keypad_.lastRaw();
}

uint8_t InputService::stableKey() const {
  return keypad_.currentKey();
}
