#include "serial_commands_story.h"

#include <cstring>

bool serialIsStoryCommand(const char* token) {
  return token != nullptr && strncmp(token, "STORY_", 6U) == 0;
}
