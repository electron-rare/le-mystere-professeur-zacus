// button_manager.h - button scan + long press detection.
#pragma once

#include <Arduino.h>

struct ButtonEvent {
  uint8_t key = 0U;
  bool long_press = false;
};

class ButtonManager {
 public:
  bool begin();
  bool pollEvent(ButtonEvent* out_event);

  bool isPressed(uint8_t key) const;
  uint8_t currentKey() const;
  int lastAnalogMilliVolts() const;

 private:
  uint8_t decodeAnalogKey(int millivolts) const;
  bool pollAnalog(ButtonEvent* out_event);
  bool pollDigital(ButtonEvent* out_event);

  bool analog_mode_ = false;
  int last_analog_mv_ = -1;
  int voltage_thresholds_[6] = {0, 447, 730, 1008, 1307, 1659};
  int threshold_range_mv_ = 70;

  uint8_t analog_key_ = 0U;
  uint8_t analog_raw_key_ = 0U;
  uint32_t analog_pressed_at_ms_ = 0U;
  uint32_t analog_raw_changed_ms_ = 0U;

  bool digital_pressed_[4] = {false, false, false, false};
  uint32_t digital_pressed_at_ms_[4] = {0U, 0U, 0U, 0U};
};
