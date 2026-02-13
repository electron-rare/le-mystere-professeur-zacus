#pragma once

#include <Arduino.h>

#include "../../audio/effects/audio_effect_id.h"
#include "../../services/audio/audio_service.h"
#include "../../story/story_engine.h"

class StoryController {
 public:
  struct Hooks {
    bool (*startRandomTokenBase)(const char* token,
                                 const char* source,
                                 bool allowSdFallback,
                                 uint32_t maxDurationMs);
    bool (*startFallbackBaseFx)(AudioEffectId effect,
                                uint32_t durationMs,
                                float gain,
                                const char* source);
    float fallbackGain = 0.22f;
    const char* winToken = "WIN";
    const char* etape2Token = "ETAPE_2";
    uint32_t winMaxDurationMs = 6000U;
    uint32_t etape2MaxDurationMs = 6000U;
    uint32_t winFallbackDurationMs = 1800U;
    uint32_t etape2FallbackDurationMs = 1800U;
  };

  StoryController(StoryEngine& engine, AudioService& audio, const Hooks& hooks);

  void reset(const char* source);
  void onUnlock(uint32_t nowMs, const char* source);
  void armAfterUnlock(uint32_t nowMs, const char* source);
  bool isMp3GateOpen() const;

  void update(uint32_t nowMs);

  void forceEtape2DueNow(uint32_t nowMs, const char* source);
  void setTestMode(bool enabled, uint32_t nowMs, const char* source);
  void setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source);
  void printStatus(uint32_t nowMs, const char* source) const;

 private:
  StoryEngine& engine_;
  AudioService& audio_;
  Hooks hooks_;
  bool winAudioPending_ = false;
  bool etape2AudioPending_ = false;

  bool triggerWinAudio(uint32_t nowMs, const char* source);
  bool triggerEtape2Audio(uint32_t nowMs, const char* source);
};
