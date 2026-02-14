#include "story_controller.h"

#include <cstring>

StoryController::StoryController(StoryEngine& engine, AudioService& audio, const Hooks& hooks)
    : engine_(engine), audio_(audio), hooks_(hooks) {}

void StoryController::reset(const char* source) {
  winAudioPending_ = false;
  etape2AudioPending_ = false;
  engine_.reset(source);
}

void StoryController::onUnlock(uint32_t nowMs, const char* source) {
  winAudioPending_ = false;
  etape2AudioPending_ = false;
  engine_.armAfterUnlock(nowMs, source);
  if (triggerWinAudio(nowMs, "unlock_story_win")) {
    return;
  }
  engine_.markWinPlayed(nowMs, false, "unlock_story_no_audio");
}

void StoryController::armAfterUnlock(uint32_t nowMs, const char* source) {
  onUnlock(nowMs, source);
}

bool StoryController::isMp3GateOpen() const {
  return engine_.isMp3GateOpen();
}

void StoryController::update(uint32_t nowMs) {
  if (winAudioPending_) {
    if (audio_.isBaseBusy()) {
      return;
    }
    winAudioPending_ = false;
    engine_.markWinPlayed(nowMs, true, "unlock_story_async_done");
  }

  if (etape2AudioPending_) {
    if (audio_.isBaseBusy()) {
      return;
    }
    etape2AudioPending_ = false;
    engine_.markEtape2Played(nowMs, true, "timeline_async_done");
    return;
  }

  if (!engine_.shouldTriggerEtape2(nowMs)) {
    return;
  }

  Serial.println("[STORY] ETAPE_2 trigger.");
  if (triggerEtape2Audio(nowMs, "story_etape2")) {
    etape2AudioPending_ = true;
    return;
  }

  Serial.println("[STORY] ETAPE_2 absent: passage sans audio.");
  engine_.markEtape2Played(nowMs, false, "timeline_no_audio");
}

void StoryController::forceEtape2DueNow(uint32_t nowMs, const char* source) {
  engine_.forceEtape2DueNow(nowMs, source);
}

void StoryController::setTestMode(bool enabled, uint32_t nowMs, const char* source) {
  engine_.setTestMode(enabled, nowMs, source);
}

void StoryController::setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source) {
  engine_.setTestDelayMs(delayMs, nowMs, source);
}

void StoryController::printStatus(uint32_t nowMs, const char* source) const {
  engine_.printStatus(nowMs, source);
}

bool StoryController::triggerWinAudio(uint32_t nowMs, const char* source) {
  (void)nowMs;
  bool started = false;
  if (hooks_.startRandomTokenBase != nullptr &&
      hooks_.winToken != nullptr &&
      hooks_.winToken[0] != '\0') {
    started = hooks_.startRandomTokenBase(hooks_.winToken, source, true, hooks_.winMaxDurationMs);
  }

  if (!started && hooks_.startFallbackBaseFx != nullptr) {
    Serial.println("[STORY] WIN absent: fallback FX WIN.");
    started = hooks_.startFallbackBaseFx(AudioEffectId::kWin,
                                         hooks_.winFallbackDurationMs,
                                         hooks_.fallbackGain,
                                         "story_win_fallback");
  }

  if (started) {
    winAudioPending_ = true;
    return true;
  }

  Serial.println("[STORY] WIN absent: passage sans audio.");
  return false;
}

bool StoryController::triggerEtape2Audio(uint32_t nowMs, const char* source) {
  (void)nowMs;
  bool started = false;
  if (hooks_.startRandomTokenBase != nullptr &&
      hooks_.etape2Token != nullptr &&
      hooks_.etape2Token[0] != '\0') {
    started = hooks_.startRandomTokenBase(hooks_.etape2Token, source, true, hooks_.etape2MaxDurationMs);
  }

  if (!started && hooks_.startFallbackBaseFx != nullptr) {
    Serial.println("[STORY] ETAPE_2 absent: fallback FX WIN.");
    started = hooks_.startFallbackBaseFx(AudioEffectId::kWin,
                                         hooks_.etape2FallbackDurationMs,
                                         hooks_.fallbackGain,
                                         "story_etape2_fallback");
  }

  return started;
}
