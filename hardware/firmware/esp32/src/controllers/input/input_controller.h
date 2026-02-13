#pragma once

#include <Arduino.h>

#include "../../services/input/input_service.h"

class InputController {
 public:
  using KeyHandler = void (*)(const KeyEvent& event, uint32_t nowMs, void* ctx);

  explicit InputController(InputService& inputService);

  void setKeyHandler(KeyHandler handler, void* ctx);
  void update(uint32_t nowMs);

 private:
  InputService& inputService_;
  KeyHandler keyHandler_ = nullptr;
  void* keyHandlerCtx_ = nullptr;
};
