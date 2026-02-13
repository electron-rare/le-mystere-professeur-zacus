#include "screen_sync_service.h"

ScreenSyncService::ScreenSyncService(ScreenLink& link) : link_(link) {}

void ScreenSyncService::reset() {
  sequence_ = 0U;
}

void ScreenSyncService::update(ScreenFrame* frame, uint32_t nowMs) {
  if (frame == nullptr) {
    return;
  }
  frame->sequence = ++sequence_;
  frame->nowMs = nowMs;
  link_.update(*frame);
}

uint32_t ScreenSyncService::sequence() const {
  return sequence_;
}
