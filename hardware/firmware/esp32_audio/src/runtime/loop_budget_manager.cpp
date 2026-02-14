#include "loop_budget_manager.h"

LoopBudgetManager::LoopBudgetManager(const LoopBudgetConfig& config)
    : config_(config) {
}

void LoopBudgetManager::reset(uint32_t nowMs) {
  lastWarnMs_ = nowMs;
  maxLoopMs_ = 0U;
  warnCount_ = 0U;
  sampleCount_ = 0U;
  totalLoopMs_ = 0U;
  overBootThresholdCount_ = 0U;
  overRuntimeThresholdCount_ = 0U;
}

void LoopBudgetManager::record(uint32_t nowMs,
                               uint32_t loopElapsedMs,
                               bool bootActive,
                               Print& out,
                               uint8_t runtimeMode,
                               bool mp3Active) {
  ++sampleCount_;
  totalLoopMs_ += loopElapsedMs;
  if (loopElapsedMs > maxLoopMs_) {
    maxLoopMs_ = loopElapsedMs;
  }

  const uint32_t thresholdMs = bootActive ? config_.bootThresholdMs : config_.runtimeThresholdMs;
  if (loopElapsedMs > config_.bootThresholdMs) {
    ++overBootThresholdCount_;
  }
  if (loopElapsedMs > config_.runtimeThresholdMs) {
    ++overRuntimeThresholdCount_;
  }
  if (loopElapsedMs > thresholdMs && static_cast<int32_t>(nowMs - lastWarnMs_) >= 0) {
    ++warnCount_;
    out.printf("[LOOP_BUDGET] warn loop=%lums max=%lums mode=%u boot=%u mp3=%u\n",
               static_cast<unsigned long>(loopElapsedMs),
               static_cast<unsigned long>(maxLoopMs_),
               static_cast<unsigned int>(runtimeMode),
               bootActive ? 1U : 0U,
               mp3Active ? 1U : 0U);
    lastWarnMs_ = nowMs + config_.warnThrottleMs;
  }
}

LoopBudgetSnapshot LoopBudgetManager::snapshot() const {
  LoopBudgetSnapshot snap;
  snap.maxLoopMs = maxLoopMs_;
  snap.warnCount = warnCount_;
  snap.sampleCount = sampleCount_;
  snap.totalLoopMs = totalLoopMs_;
  snap.overBootThresholdCount = overBootThresholdCount_;
  snap.overRuntimeThresholdCount = overRuntimeThresholdCount_;
  snap.bootThresholdMs = config_.bootThresholdMs;
  snap.runtimeThresholdMs = config_.runtimeThresholdMs;
  snap.warnThrottleMs = config_.warnThrottleMs;
  return snap;
}
