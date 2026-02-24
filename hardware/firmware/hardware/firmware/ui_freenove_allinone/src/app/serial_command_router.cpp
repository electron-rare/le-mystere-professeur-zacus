#include "app/serial_command_router.h"

bool SerialCommandRouter::dispatch(const char* command_line,
                                   uint32_t now_ms,
                                   RuntimeServices* services) const {
  if (command_line == nullptr || command_line[0] == '\0' || services == nullptr) {
    return false;
  }
  if (services->dispatch_serial == nullptr) {
    return false;
  }
  services->dispatch_serial(command_line, now_ms, services);
  return true;
}
