#include "drivers/display/spi_bus_manager.h"

namespace drivers::display {

SpiBusManager& SpiBusManager::instance() {
  static SpiBusManager manager;
  return manager;
}

bool SpiBusManager::begin() {
#if defined(ARDUINO_ARCH_ESP32)
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
  }
  return mutex_ != nullptr;
#else
  return true;
#endif
}

bool SpiBusManager::lock(uint32_t timeout_ms) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!begin()) {
    return false;
  }
  const TickType_t timeout_ticks = (timeout_ms == 0U)
                                       ? 0
                                       : pdMS_TO_TICKS(static_cast<TickType_t>(timeout_ms));
  return xSemaphoreTake(mutex_, timeout_ticks) == pdTRUE;
#else
  (void)timeout_ms;
  return true;
#endif
}

void SpiBusManager::unlock() {
#if defined(ARDUINO_ARCH_ESP32)
  if (mutex_ != nullptr) {
    xSemaphoreGive(mutex_);
  }
#endif
}

SpiBusManager::Guard::Guard(uint32_t timeout_ms) {
  locked_ = SpiBusManager::instance().lock(timeout_ms);
}

SpiBusManager::Guard::~Guard() {
  if (locked_) {
    SpiBusManager::instance().unlock();
  }
}

bool SpiBusManager::Guard::locked() const {
  return locked_;
}

}  // namespace drivers::display
