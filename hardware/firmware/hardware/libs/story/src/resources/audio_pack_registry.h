#pragma once

#include <Arduino.h>

#include "audio/effects/audio_effect_id.h"

struct StoryAudioPackDef {
  const char* id;
  const char* token;
  AudioEffectId fallbackEffect;
  uint32_t maxDurationMs;
  uint32_t fallbackDurationMs;
  float gain;
  bool allowSdFallback;
};

const StoryAudioPackDef* storyFindAudioPack(const char* packId);
