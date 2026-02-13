#include "apps_gen.h"

#include <cstring>

namespace {
constexpr AppBindingDef kGeneratedAppBindings[] = {
    {"APP_AUDIO", StoryAppType::kAudioPack},
    {"APP_GATE", StoryAppType::kMp3Gate},
    {"APP_LA", StoryAppType::kLaDetector},
    {"APP_SCREEN", StoryAppType::kScreenScene},
};
}  // namespace

const AppBindingDef* generatedAppBindingById(const char* id) {
  if (id == nullptr || id[0] == '\0') {
    return nullptr;
  }
  for (const AppBindingDef& binding : kGeneratedAppBindings) {
    if (binding.id != nullptr && strcmp(binding.id, id) == 0) {
      return &binding;
    }
  }
  return nullptr;
}

uint8_t generatedAppBindingCount() {
  return static_cast<uint8_t>(sizeof(kGeneratedAppBindings) / sizeof(kGeneratedAppBindings[0]));
}

const char* generatedAppBindingIdAt(uint8_t index) {
  if (index >= generatedAppBindingCount()) {
    return nullptr;
  }
  return kGeneratedAppBindings[index].id;
}

