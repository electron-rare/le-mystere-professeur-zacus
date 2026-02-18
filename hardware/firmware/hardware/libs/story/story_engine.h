#pragma once

#include <Arduino.h>

class StoryEngine {
 public:
  struct Options {
    uint32_t etape2DelayMs = 15UL * 60UL * 1000UL;
    uint32_t etape2TestDelayMs = 5000U;
  };

  explicit StoryEngine(const Options& options);

  void reset(const char* source);
  void armAfterUnlock(uint32_t nowMs, const char* source);
  bool isMp3GateOpen() const;

  void markWinPlayed(uint32_t nowMs, bool audioPlayed, const char* source);
  bool shouldTriggerEtape2(uint32_t nowMs) const;
  void markEtape2Played(uint32_t nowMs, bool audioPlayed, const char* source);
  void forceEtape2DueNow(uint32_t nowMs, const char* source);

  void setTestMode(bool enabled, uint32_t nowMs, const char* source);
  void setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source);

  void printStatus(uint32_t nowMs, const char* source) const;

  bool unlockArmed() const;
  bool winPlayed() const;
  bool winAudioPlayed() const;
  bool etape2Played() const;
  bool testMode() const;
  uint32_t unlockMs() const;
  uint32_t dueMs() const;
  uint32_t activeDelayMs() const;

 private:
  void recomputeDueFrom(uint32_t nowMs);

  Options options_;
  bool unlockArmed_ = false;
  bool winPlayed_ = false;
  bool winAudioPlayed_ = false;
  bool etape2Played_ = false;
  bool testMode_ = false;
  uint32_t unlockMs_ = 0;
  uint32_t etape2DueMs_ = 0;
  uint32_t testDelayMs_ = 5000U;
};
