#include "screen_sync_service.h"

namespace {

constexpr uint32_t kScreenKeyframePeriodMs = 1000U;
constexpr uint32_t kScreenWatchdogMs = 1500U;
constexpr uint32_t kScreenStatsLogPeriodMs = 5000U;

}  // namespace

ScreenSyncService::ScreenSyncService(ScreenLink& link) : link_(link) {}

void ScreenSyncService::reset() {
  sequence_ = 0U;
  lastKeyframeMs_ = 0U;
  lastTxSuccessMs_ = 0U;
  lastStatsLogMs_ = 0U;
  txSuccessCount_ = 0U;
  txDropCount_ = 0U;
}

void ScreenSyncService::update(ScreenFrame* frame, uint32_t nowMs) {
  if (frame == nullptr) {
    return;
  }

  const bool keyframeDue =
      (lastKeyframeMs_ == 0U) || (static_cast<uint32_t>(nowMs - lastKeyframeMs_) >= kScreenKeyframePeriodMs);
  const uint32_t nextSequence = sequence_ + 1U;
  frame->sequence = nextSequence;
  frame->nowMs = nowMs;

  const bool sent = link_.update(*frame, keyframeDue);
  if (sent) {
    sequence_ = nextSequence;
    lastTxSuccessMs_ = nowMs;
    ++txSuccessCount_;
    if (keyframeDue) {
      lastKeyframeMs_ = nowMs;
    }
  } else {
    ++txDropCount_;
    if (keyframeDue && static_cast<uint32_t>(nowMs - lastTxSuccessMs_) > kScreenWatchdogMs) {
      lastKeyframeMs_ = 0U;
    }
  }

  if (lastStatsLogMs_ == 0U || static_cast<uint32_t>(nowMs - lastStatsLogMs_) >= kScreenStatsLogPeriodMs) {
    Serial.printf("[SCREEN_SYNC] seq=%lu tx_ok=%lu tx_drop=%lu link_ok=%lu link_drop=%lu\n",
                  static_cast<unsigned long>(sequence_),
                  static_cast<unsigned long>(txSuccessCount_),
                  static_cast<unsigned long>(txDropCount_),
                  static_cast<unsigned long>(link_.txFrameCount()),
                  static_cast<unsigned long>(link_.txDropCount()));
    lastStatsLogMs_ = nowMs;
  }
}

uint32_t ScreenSyncService::sequence() const {
  return sequence_;
}
