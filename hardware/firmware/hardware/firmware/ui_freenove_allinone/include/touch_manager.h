// touch_manager.h - Interface gestion tactile
#pragma once

class TouchManager {
 public:
  void begin();
  bool getTouch(int16_t* x, int16_t* y);
};
