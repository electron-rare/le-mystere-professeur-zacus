#include "serial_commands_mp3.h"

#include <cstring>

bool serialIsMp3Command(const char* token) {
  return token != nullptr && strncmp(token, "MP3_", 4U) == 0;
}
