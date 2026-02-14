#include "serial_commands_system.h"

#include <cstring>

bool serialIsSystemCommand(const char* token) {
  if (token == nullptr) {
    return false;
  }
  return strncmp(token, "SYS_", 4U) == 0 || strncmp(token, "UI_LINK_", 8U) == 0 ||
         strncmp(token, "SCREEN_LINK_", 12U) == 0;
}
