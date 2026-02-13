#pragma once

#include <Arduino.h>

#include "../../screen/screen_frame.h"
#include "../../screen/screen_link.h"

struct ScreenSyncStats {
  uint32_t sequence = 0U;
  uint32_t txSuccess = 0U;
  uint32_t txDrop = 0U;
  uint32_t keyframes = 0U;
  uint32_t watchdogResync = 0U;
  uint32_t lastTxSuccessMs = 0U;
  uint32_t linkTxFrames = 0U;
  uint32_t linkTxDrop = 0U;
  uint32_t linkLastTxMs = 0U;
};

class ScreenSyncService {
 public:
  explicit ScreenSyncService(ScreenLink& link);

  void reset();
  void resetStats();
  void update(ScreenFrame* frame, uint32_t nowMs);
  uint32_t sequence() const;
  ScreenSyncStats snapshot() const;

 private:
  ScreenLink& link_;
  uint32_t sequence_ = 0U;
  uint32_t lastKeyframeMs_ = 0U;
  uint32_t lastTxSuccessMs_ = 0U;
  uint32_t lastStatsLogMs_ = 0U;
  uint32_t txSuccessCount_ = 0U;
  uint32_t txDropCount_ = 0U;
  uint32_t keyframeCount_ = 0U;
  uint32_t watchdogResyncCount_ = 0U;
};
