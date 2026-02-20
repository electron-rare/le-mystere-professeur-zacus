// button_manager.cpp - button scan + long press detection.
#include "button_manager.h"

#include "ui_freenove_config.h"

namespace {

constexpr uint32_t kDebounceMs = 30U;
constexpr uint32_t kLongPressMs = FREENOVE_BTN_LONG_PRESS_MS;
constexpr int kNoAnalogButtonMv = 2800;

const int kDigitalButtonPins[4] = {
    FREENOVE_BTN_1,
    FREENOVE_BTN_2,
    FREENOVE_BTN_3,
    FREENOVE_BTN_4,
};

}  // namespace

bool ButtonManager::begin() {
#if FREENOVE_BTN_ANALOG_PIN >= 0
  analog_mode_ = true;
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(FREENOVE_BTN_ANALOG_PIN, INPUT);
  analog_key_ = 0U;
  analog_raw_key_ = 0U;
  analog_pressed_at_ms_ = 0U;
  analog_raw_changed_ms_ = millis();
  last_analog_mv_ = kNoAnalogButtonMv;
  Serial.printf("[BTN] analog ladder mode on GPIO %d\n", FREENOVE_BTN_ANALOG_PIN);
#else
  analog_mode_ = false;
  last_analog_mv_ = -1;
  for (uint8_t index = 0; index < 4; ++index) {
    if (kDigitalButtonPins[index] >= 0) {
      pinMode(kDigitalButtonPins[index], INPUT_PULLUP);
    }
  }
  Serial.println("[BTN] digital mode");
#endif
  return true;
}

bool ButtonManager::pollEvent(ButtonEvent* out_event) {
  if (out_event == nullptr) {
    return false;
  }
  if (analog_mode_) {
    return pollAnalog(out_event);
  }
  return pollDigital(out_event);
}

bool ButtonManager::isPressed(uint8_t key) const {
  if (key < 1U || key > 5U) {
    return false;
  }
  if (analog_mode_) {
    return analog_key_ == key;
  }
  if (key > 4U) {
    return false;
  }
  return digital_pressed_[key - 1U];
}

uint8_t ButtonManager::currentKey() const {
  if (analog_mode_) {
    return analog_key_;
  }
  for (uint8_t index = 0; index < 4; ++index) {
    if (digital_pressed_[index]) {
      return static_cast<uint8_t>(index + 1U);
    }
  }
  return 0U;
}

int ButtonManager::lastAnalogMilliVolts() const {
  return last_analog_mv_;
}

uint8_t ButtonManager::decodeAnalogKey(int millivolts) const {
  if (millivolts >= (kNoAnalogButtonMv - threshold_range_mv_)) {
    return 0U;
  }
  for (int index = 1; index < 6; ++index) {
    const int threshold = voltage_thresholds_[index];
    if (millivolts >= (threshold - threshold_range_mv_) &&
        millivolts <= (threshold + threshold_range_mv_)) {
      if (index >= 1 && index <= 5) {
        return static_cast<uint8_t>(index);
      }
      return 0U;
    }
  }
  return 0U;
}

bool ButtonManager::pollAnalog(ButtonEvent* out_event) {
#if FREENOVE_BTN_ANALOG_PIN < 0
  (void)out_event;
  return false;
#else
  const int analog_mv = analogReadMilliVolts(FREENOVE_BTN_ANALOG_PIN);
  last_analog_mv_ = analog_mv;
  const uint8_t raw_key = decodeAnalogKey(analog_mv);
  const uint32_t now_ms = millis();

  if (raw_key != analog_raw_key_) {
    analog_raw_key_ = raw_key;
    analog_raw_changed_ms_ = now_ms;
  }
  if ((now_ms - analog_raw_changed_ms_) < kDebounceMs) {
    return false;
  }

  const uint8_t stable_key = analog_raw_key_;
  if (stable_key == analog_key_) {
    return false;
  }

  if (analog_key_ == 0U && stable_key > 0U) {
    analog_key_ = stable_key;
    analog_pressed_at_ms_ = now_ms;
    return false;
  }
  if (analog_key_ == 0U) {
    return false;
  }

  const uint8_t released_key = analog_key_;
  const uint32_t hold_ms = now_ms - analog_pressed_at_ms_;
  analog_key_ = stable_key;
  analog_pressed_at_ms_ = (stable_key > 0U) ? now_ms : 0U;

  if (hold_ms < kDebounceMs) {
    return false;
  }
  out_event->key = released_key;
  out_event->long_press = hold_ms >= kLongPressMs;
  return true;
#endif
}

bool ButtonManager::pollDigital(ButtonEvent* out_event) {
  const uint32_t now_ms = millis();
  for (uint8_t index = 0; index < 4; ++index) {
    if (kDigitalButtonPins[index] < 0) {
      continue;
    }
    const bool pressed = (digitalRead(kDigitalButtonPins[index]) == LOW);
    if (pressed && !digital_pressed_[index]) {
      digital_pressed_[index] = true;
      digital_pressed_at_ms_[index] = now_ms;
      continue;
    }
    if (!pressed && digital_pressed_[index]) {
      const uint32_t hold_ms = now_ms - digital_pressed_at_ms_[index];
      digital_pressed_[index] = false;
      if (hold_ms < kDebounceMs) {
        continue;
      }
      out_event->key = static_cast<uint8_t>(index + 1U);
      out_event->long_press = hold_ms >= kLongPressMs;
      return true;
    }
  }
  return false;
}
