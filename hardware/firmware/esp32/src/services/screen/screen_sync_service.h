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
};
