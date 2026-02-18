#pragma once

#include <Arduino.h>

enum class InputEventSource : uint8_t {
  kLocalKeypad = 0,
  kUiSerial,
};

enum class InputEventType : uint8_t {
  kButton = 0,
  kTouch,
};

enum class InputButtonAction : uint8_t {
  kUnknown = 0,
  kDown,
  kUp,
  kClick,
  kLong,
};

struct InputEvent {
  InputEventSource source = InputEventSource::kLocalKeypad;
  InputEventType type = InputEventType::kButton;
  uint16_t code = 0;   // Logical key id (1..6) for button events.
  int32_t value = 0;   // Optional payload (touch y, extra data).
  InputButtonAction action = InputButtonAction::kUnknown;
  uint32_t tsMs = 0;
  uint16_t raw = 0;    // ADC raw sample for local keypad events.
};
