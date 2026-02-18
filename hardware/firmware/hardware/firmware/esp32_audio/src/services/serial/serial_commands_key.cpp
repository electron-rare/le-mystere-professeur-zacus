#include "serial_commands_key.h"

#include <cstring>

bool serialIsKeyCommand(const char* token) {
  return token != nullptr && strncmp(token, "KEY_", 4U) == 0;
}
