#include "input_controller.h"

InputController::InputController(InputService& inputService) : inputService_(inputService) {}

void InputController::setKeyHandler(KeyHandler handler, void* ctx) {
  keyHandler_ = handler;
  keyHandlerCtx_ = ctx;
}

void InputController::update(uint32_t nowMs) {
  inputService_.update(nowMs);
  if (keyHandler_ == nullptr) {
    return;
  }

  KeyEvent event;
  if (!inputService_.consumePress(&event)) {
    return;
  }
  keyHandler_(event, nowMs, keyHandlerCtx_);
}
