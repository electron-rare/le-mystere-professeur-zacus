#pragma once

#include <Arduino.h>

#include "../../screen/screen_frame.h"
#include "../../screen/screen_link.h"

class ScreenSyncService {
 public:
  explicit ScreenSyncService(ScreenLink& link);

  void reset();
  void update(ScreenFrame* frame, uint32_t nowMs);
  uint32_t sequence() const;

 private:
  ScreenLink& link_;
  uint32_t sequence_ = 0U;
  uint32_t lastKeyframeMs_ = 0U;
  uint32_t lastTxSuccessMs_ = 0U;
  uint32_t lastStatsLogMs_ = 0U;
  uint32_t txSuccessCount_ = 0U;
  uint32_t txDropCount_ = 0U;
};
