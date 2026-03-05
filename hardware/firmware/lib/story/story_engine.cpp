#include "story_engine.h"

#include <cstdio>

StoryEngine::StoryEngine(const Options& options)
    : options_(options),
      testDelayMs_(options.etape2TestDelayMs) {}

void StoryEngine::recomputeDueFrom(uint32_t nowMs) {
  const uint32_t delayMs = activeDelayMs();
  unlockMs_ = nowMs;
  etape2DueMs_ = nowMs + delayMs;
}

void StoryEngine::reset(const char* source) {
  unlockArmed_ = false;
  winPlayed_ = false;
  winAudioPlayed_ = false;
  etape2Played_ = false;
  unlockMs_ = 0U;
  etape2DueMs_ = 0U;
  Serial.printf("[STORY] reset (%s)\n", source);
}

void StoryEngine::armAfterUnlock(uint32_t nowMs, const char* source) {
  unlockArmed_ = true;
  winPlayed_ = false;
  winAudioPlayed_ = false;
  etape2Played_ = false;
  recomputeDueFrom(nowMs);
  Serial.printf("[STORY] unlock armed (%s): ETAPE_2 due in %lus%s\n",
                source,
                static_cast<unsigned long>(activeDelayMs() / 1000UL),
                testMode_ ? " [TEST_MODE]" : "");
}

bool StoryEngine::isMp3GateOpen() const {
  return !unlockArmed_ || etape2Played_;
}

void StoryEngine::markWinPlayed(uint32_t nowMs, bool audioPlayed, const char* source) {
  (void)nowMs;
  winPlayed_ = true;
  winAudioPlayed_ = audioPlayed;
  Serial.printf("[STORY] WIN done (%s) audio=%u\n", source, audioPlayed ? 1U : 0U);
}

bool StoryEngine::shouldTriggerEtape2(uint32_t nowMs) const {
  if (!unlockArmed_ || !winPlayed_ || etape2Played_) {
    return false;
  }
  return static_cast<int32_t>(nowMs - etape2DueMs_) >= 0;
}

void StoryEngine::markEtape2Played(uint32_t nowMs, bool audioPlayed, const char* source) {
  (void)nowMs;
  etape2Played_ = true;
  Serial.printf("[STORY] ETAPE_2 done (%s) audio=%u\n",
                source,
                audioPlayed ? 1U : 0U);
}

void StoryEngine::forceEtape2DueNow(uint32_t nowMs, const char* source) {
  if (!unlockArmed_) {
    Serial.printf("[STORY] force due ignored (%s): unlock not armed.\n", source);
    return;
  }
  if (!winPlayed_) {
    winPlayed_ = true;
    winAudioPlayed_ = false;
    Serial.printf("[STORY] force due (%s): WIN bypassed.\n", source);
  }
  etape2DueMs_ = nowMs;
  Serial.printf("[STORY] force due now (%s).\n", source);
}

void StoryEngine::setTestMode(bool enabled, uint32_t nowMs, const char* source) {
  if (testMode_ == enabled) {
    Serial.printf("[STORY] test mode unchanged (%s): %s\n",
                  source,
                  testMode_ ? "ON" : "OFF");
    return;
  }
  testMode_ = enabled;
  if (unlockArmed_ && !etape2Played_) {
    recomputeDueFrom(nowMs);
  }
  Serial.printf("[STORY] test mode %s (%s), delay=%lums\n",
                testMode_ ? "ON" : "OFF",
                source,
                static_cast<unsigned long>(activeDelayMs()));
}

void StoryEngine::setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source) {
  if (delayMs < 100U) {
    delayMs = 100U;
  } else if (delayMs > 300000U) {
    delayMs = 300000U;
  }
  testDelayMs_ = delayMs;
  if (testMode_ && unlockArmed_ && !etape2Played_) {
    recomputeDueFrom(nowMs);
  }
  Serial.printf("[STORY] test delay set %lums (%s)\n",
                static_cast<unsigned long>(testDelayMs_),
                source);
}

void StoryEngine::printStatus(uint32_t nowMs, const char* source) const {
  uint32_t leftMs = 0U;
  if (unlockArmed_ && !etape2Played_ && static_cast<int32_t>(etape2DueMs_ - nowMs) > 0) {
    leftMs = etape2DueMs_ - nowMs;
  }
  const char* stage = "WAIT_UNLOCK";
  if (unlockArmed_ && !winPlayed_) {
    stage = "WIN_PENDING";
  } else if (unlockArmed_ && winPlayed_ && !etape2Played_) {
    stage = "WAIT_ETAPE2";
  } else if (etape2Played_) {
    stage = "ETAPE2_DONE";
  }
  Serial.printf("[STORY] STATUS via=%s stage=%s armed=%u win=%u win_audio=%u etape2=%u test=%u delay=%lus left=%lus\n",
                source,
                stage,
                unlockArmed_ ? 1U : 0U,
                winPlayed_ ? 1U : 0U,
                winAudioPlayed_ ? 1U : 0U,
                etape2Played_ ? 1U : 0U,
                testMode_ ? 1U : 0U,
                static_cast<unsigned long>(activeDelayMs() / 1000UL),
                static_cast<unsigned long>(leftMs / 1000UL));
}

bool StoryEngine::unlockArmed() const {
  return unlockArmed_;
}

bool StoryEngine::winPlayed() const {
  return winPlayed_;
}

bool StoryEngine::winAudioPlayed() const {
  return winAudioPlayed_;
}

bool StoryEngine::etape2Played() const {
  return etape2Played_;
}

bool StoryEngine::testMode() const {
  return testMode_;
}

uint32_t StoryEngine::unlockMs() const {
  return unlockMs_;
}

uint32_t StoryEngine::dueMs() const {
  return etape2DueMs_;
}

uint32_t StoryEngine::activeDelayMs() const {
  return testMode_ ? testDelayMs_ : options_.etape2DelayMs;
}
