#pragma once

#include <Arduino.h>

#include "scenario_def.h"

class StoryEventQueue {
 public:
  static constexpr uint8_t kCapacity = 12U;

  void clear() {
    head_ = 0U;
    tail_ = 0U;
    size_ = 0U;
    dropped_ = 0U;
  }

  bool push(const StoryEvent& event) {
    if (size_ >= kCapacity) {
      ++dropped_;
      return false;
    }
    data_[tail_] = event;
    tail_ = static_cast<uint8_t>((tail_ + 1U) % kCapacity);
    ++size_;
    return true;
  }

  bool pop(StoryEvent* outEvent) {
    if (size_ == 0U || outEvent == nullptr) {
      return false;
    }
    *outEvent = data_[head_];
    head_ = static_cast<uint8_t>((head_ + 1U) % kCapacity);
    --size_;
    return true;
  }

  uint8_t size() const {
    return size_;
  }

  uint32_t droppedCount() const {
    return dropped_;
  }

 private:
  StoryEvent data_[kCapacity] = {};
  uint8_t head_ = 0U;
  uint8_t tail_ = 0U;
  uint8_t size_ = 0U;
  uint32_t dropped_ = 0U;
};
