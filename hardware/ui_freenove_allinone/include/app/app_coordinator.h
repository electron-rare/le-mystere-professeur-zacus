// app_coordinator.h - runtime orchestration facade for setup/loop.
#pragma once

#include <Arduino.h>

#include "runtime/runtime_services.h"
#include "app/serial_command_router.h"

class AppCoordinator {
 public:
  bool begin(RuntimeServices* services);
  void tick(uint32_t now_ms);
  void onSerialLine(const char* command_line, uint32_t now_ms);

 private:
  RuntimeServices* services_ = nullptr;
  SerialCommandRouter serial_router_;
};
