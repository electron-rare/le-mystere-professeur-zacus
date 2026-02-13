#include "audio_pack_registry.h"

#include <cstring>

namespace {

constexpr StoryAudioPackDef kAudioPacks[] = {
    {"PACK_WIN", "WIN", AudioEffectId::kWin, 6000U, 1800U, 0.22f, true},
    {"PACK_ETAPE2", "ETAPE_2", AudioEffectId::kWin, 6000U, 1800U, 0.22f, true},
};

}  // namespace

const StoryAudioPackDef* storyFindAudioPack(const char* packId) {
  if (packId == nullptr || packId[0] == '\0') {
    return nullptr;
  }
  for (const StoryAudioPackDef& pack : kAudioPacks) {
    if (pack.id != nullptr && strcmp(pack.id, packId) == 0) {
      return &pack;
    }
  }
  return nullptr;
}
