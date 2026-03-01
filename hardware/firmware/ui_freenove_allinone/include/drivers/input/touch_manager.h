// touch_manager.h - optional touch bridge.
#pragma once

#include <Arduino.h>

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  bool touched = false;
};

class TouchManager {
 public:
  TouchManager() = default;
  ~TouchManager() = default;
  TouchManager(const TouchManager&) = delete;
  TouchManager& operator=(const TouchManager&) = delete;
  TouchManager(TouchManager&&) = delete;
  TouchManager& operator=(TouchManager&&) = delete;

  bool begin();
  bool poll(TouchPoint* out_point);
};
