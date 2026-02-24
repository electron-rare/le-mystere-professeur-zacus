#pragma once

#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace drivers::display {

// Shared SPI arbitration for display backends and optional FX overlay.
class SpiBusManager {
 public:
  class Guard {
   public:
    explicit Guard(uint32_t timeout_ms = 100U);
    ~Guard();

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    bool locked() const;

   private:
    bool locked_ = false;
  };

  static SpiBusManager& instance();

  bool begin();
  bool lock(uint32_t timeout_ms = 100U);
  void unlock();

  SpiBusManager(const SpiBusManager&) = delete;
  SpiBusManager& operator=(const SpiBusManager&) = delete;

 private:
  SpiBusManager() = default;

#if defined(ARDUINO_ARCH_ESP32)
  SemaphoreHandle_t mutex_ = nullptr;
#endif
};

}  // namespace drivers::display
