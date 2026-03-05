#pragma once

#include <Arduino.h>

#include "input_event.h"
#include "ui_link_v2.h"

class InputRouter {
 public:
  bool mapUiButton(UiBtnId id, UiBtnAction action, uint32_t tsMs, InputEvent* outEvent) const;
  bool mapUiTouch(int16_t x,
                  int16_t y,
                  UiTouchAction action,
                  uint32_t tsMs,
                  InputEvent* outEvent) const;
};
