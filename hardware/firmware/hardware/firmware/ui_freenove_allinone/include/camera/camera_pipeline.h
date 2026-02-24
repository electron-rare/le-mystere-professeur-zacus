#pragma once

#include <cstddef>
#include <cstdint>

namespace camera {

struct CameraFrameMeta {
  uint32_t timestamp_ms = 0U;
  uint16_t width = 0U;
  uint16_t height = 0U;
  size_t bytes = 0U;
  bool jpeg = false;
};

class CameraPipeline {
 public:
  static constexpr uint8_t kFrameQueueDepth = 4U;

  bool pushFrame(const CameraFrameMeta& frame) {
    if (count_ >= kFrameQueueDepth) {
      return false;
    }
    frames_[write_] = frame;
    write_ = static_cast<uint8_t>((write_ + 1U) % kFrameQueueDepth);
    ++count_;
    return true;
  }

  bool popFrame(CameraFrameMeta* out_frame) {
    if (out_frame == nullptr || count_ == 0U) {
      return false;
    }
    *out_frame = frames_[read_];
    read_ = static_cast<uint8_t>((read_ + 1U) % kFrameQueueDepth);
    --count_;
    return true;
  }

  uint8_t pendingFrames() const {
    return count_;
  }

 private:
  CameraFrameMeta frames_[kFrameQueueDepth] = {};
  uint8_t read_ = 0U;
  uint8_t write_ = 0U;
  uint8_t count_ = 0U;
};

}  // namespace camera
