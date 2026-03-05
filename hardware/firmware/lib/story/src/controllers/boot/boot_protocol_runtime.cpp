#include "boot_protocol_runtime.h"

BootProtocolRuntime::BootProtocolRuntime(const Hooks& hooks) : hooks_(hooks) {}

void BootProtocolRuntime::start(uint32_t nowMs) {
  if (hooks_.start != nullptr) {
    hooks_.start(nowMs);
  }
}

void BootProtocolRuntime::update(uint32_t nowMs) {
  if (hooks_.update != nullptr) {
    hooks_.update(nowMs);
  }
}

void BootProtocolRuntime::onKey(uint8_t key, uint32_t nowMs) {
  if (hooks_.onKey != nullptr) {
    hooks_.onKey(key, nowMs);
  }
}

bool BootProtocolRuntime::isActive() const {
  if (hooks_.isActive == nullptr) {
    return false;
  }
  return hooks_.isActive();
}
