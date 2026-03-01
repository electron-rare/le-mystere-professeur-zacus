// serial_command_router.h - serial command dispatch bridge.
#pragma once

#include <Arduino.h>

#include "runtime/runtime_services.h"

class SerialCommandRouter {
 public:
  bool dispatch(const char* command_line, uint32_t now_ms, RuntimeServices* services) const;
};
