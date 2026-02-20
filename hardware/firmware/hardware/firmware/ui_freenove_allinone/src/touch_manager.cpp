// touch_manager.cpp - optional touch bridge.
#include "touch_manager.h"

#include "ui_freenove_config.h"

bool TouchManager::begin() {
#if FREENOVE_HAS_TOUCH
  Serial.printf("[TOUCH] enabled cs=%d irq=%d\n", FREENOVE_TOUCH_CS, FREENOVE_TOUCH_IRQ);
#else
  Serial.println("[TOUCH] disabled");
#endif
  return true;
}

bool TouchManager::poll(TouchPoint* out_point) {
  if (out_point == nullptr) {
    return false;
  }
#if FREENOVE_HAS_TOUCH
  (void)out_point;
  // Touchscreen support is optional and disabled by default on Freenove.
  return false;
#else
  out_point->x = 0;
  out_point->y = 0;
  out_point->touched = false;
  return false;
#endif
}
