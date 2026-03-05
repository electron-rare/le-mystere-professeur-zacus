#include "serial_commands_boot.h"

#include <cstring>

bool serialIsBootCommand(const char* token) {
  return token != nullptr && strncmp(token, "BOOT_", 5U) == 0;
}
