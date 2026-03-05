#pragma once

#include <Arduino.h>

enum class AudioEffectId : uint8_t {
  kFmSweep = 0,
  kSonar,
  kMorse,
  kWin,
};

const char* audioEffectLabel(AudioEffectId effect);
bool parseAudioEffectToken(const char* token, AudioEffectId* outEffect);
