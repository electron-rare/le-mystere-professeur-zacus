#include "audio_effect_id.h"

#include <cctype>
#include <cstring>

namespace {

void upperTokenCopy(const char* src, char* dst, size_t dstLen) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }
  dst[0] = '\0';
  if (src == nullptr) {
    return;
  }

  size_t i = 0U;
  while (src[i] != '\0' && i < (dstLen - 1U)) {
    dst[i] = static_cast<char>(toupper(static_cast<unsigned char>(src[i])));
    ++i;
  }
  dst[i] = '\0';
}

}  // namespace

const char* audioEffectLabel(AudioEffectId effect) {
  switch (effect) {
    case AudioEffectId::kFmSweep:
      return "FM";
    case AudioEffectId::kSonar:
      return "SONAR";
    case AudioEffectId::kMorse:
      return "MORSE";
    case AudioEffectId::kWin:
      return "WIN";
    default:
      return "UNKNOWN";
  }
}

bool parseAudioEffectToken(const char* token, AudioEffectId* outEffect) {
  if (token == nullptr || outEffect == nullptr) {
    return false;
  }

  char upper[16] = {};
  upperTokenCopy(token, upper, sizeof(upper));
  if (strcmp(upper, "FM") == 0 || strcmp(upper, "FMSWEEP") == 0) {
    *outEffect = AudioEffectId::kFmSweep;
    return true;
  }
  if (strcmp(upper, "SONAR") == 0) {
    *outEffect = AudioEffectId::kSonar;
    return true;
  }
  if (strcmp(upper, "MORSE") == 0) {
    *outEffect = AudioEffectId::kMorse;
    return true;
  }
  if (strcmp(upper, "WIN") == 0) {
    *outEffect = AudioEffectId::kWin;
    return true;
  }
  return false;
}
