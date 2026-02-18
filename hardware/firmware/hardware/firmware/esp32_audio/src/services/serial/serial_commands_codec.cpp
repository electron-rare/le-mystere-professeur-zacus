#include "serial_commands_codec.h"

#include <cstring>

bool serialIsCodecCommand(const char* token) {
  return token != nullptr && strncmp(token, "CODEC_", 6U) == 0;
}
