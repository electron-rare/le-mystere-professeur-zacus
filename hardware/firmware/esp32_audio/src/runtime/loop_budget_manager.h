#pragma once

#include <Arduino.h>

struct LoopBudgetConfig {
  uint32_t bootThresholdMs = 40U;
  uint32_t runtimeThresholdMs = 25U;
  uint32_t warnThrottleMs = 2500U;
};

struct LoopBudgetSnapshot {
  uint32_t maxLoopMs = 0U;
  uint32_t warnCount = 0U;
  uint32_t sampleCount = 0U;
  uint32_t totalLoopMs = 0U;
  uint32_t overBootThresholdCount = 0U;
  uint32_t overRuntimeThresholdCount = 0U;
  uint32_t bootThresholdMs = 40U;
  uint32_t runtimeThresholdMs = 25U;
  uint32_t warnThrottleMs = 2500U;
};

class LoopBudgetManager {
 public:
  explicit LoopBudgetManager(const LoopBudgetConfig& config);

  void reset(uint32_t nowMs);
  void record(uint32_t nowMs,
              uint32_t loopElapsedMs,
              bool bootActive,
              Print& out,
              uint8_t runtimeMode,
              bool mp3Active);

  LoopBudgetSnapshot snapshot() const;

 private:
  LoopBudgetConfig config_;
  uint32_t lastWarnMs_ = 0U;
  uint32_t maxLoopMs_ = 0U;
  uint32_t warnCount_ = 0U;
  uint32_t sampleCount_ = 0U;
  uint32_t totalLoopMs_ = 0U;
  uint32_t overBootThresholdCount_ = 0U;
  uint32_t overRuntimeThresholdCount_ = 0U;
};
