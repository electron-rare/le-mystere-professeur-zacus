#include "boot_protocol_controller.h"

BootProtocolController::BootProtocolController(const Hooks& hooks) : hooks_(hooks) {}

void BootProtocolController::start(uint32_t nowMs) {
  if (hooks_.start != nullptr) {
    hooks_.start(nowMs);
  }
}

void BootProtocolController::update(uint32_t nowMs) {
  if (hooks_.update != nullptr) {
    hooks_.update(nowMs);
  }
}

void BootProtocolController::onKey(uint8_t key, uint32_t nowMs) {
  if (hooks_.onKey != nullptr) {
    hooks_.onKey(key, nowMs);
  }
}

bool BootProtocolController::isActive() const {
  if (hooks_.isActive == nullptr) {
    return false;
  }
  return hooks_.isActive();
}
