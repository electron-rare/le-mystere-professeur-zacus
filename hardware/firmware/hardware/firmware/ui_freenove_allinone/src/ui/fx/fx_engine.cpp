#include "ui/fx/fx_engine.h"

namespace ui::fx {

bool FxEngine::begin(const FxEngineConfig& config) {
  config_ = config;
  reset();
  ready_ = true;
  return true;
}

void FxEngine::reset() {
  stats_ = {};
  fps_window_start_ms_ = 0U;
  fps_window_frames_ = 0U;
}

void FxEngine::noteFrame(uint32_t now_ms) {
  if (!ready_) {
    return;
  }
  ++stats_.frame_count;
  if (fps_window_start_ms_ == 0U) {
    fps_window_start_ms_ = now_ms;
    fps_window_frames_ = 0U;
  }
  ++fps_window_frames_;

  const uint32_t elapsed = now_ms - fps_window_start_ms_;
  if (elapsed >= 1000U) {
    stats_.fps = static_cast<uint16_t>((static_cast<uint32_t>(fps_window_frames_) * 1000U) / elapsed);
    fps_window_start_ms_ = now_ms;
    fps_window_frames_ = 0U;
  }
}

void FxEngine::setSceneCounts(uint16_t object_count, uint16_t stars, uint16_t particles) {
  stats_.object_count = object_count;
  stats_.stars = stars;
  stats_.particles = particles;
}

FxEngineConfig FxEngine::config() const {
  return config_;
}

FxEngineStats FxEngine::stats() const {
  return stats_;
}

}  // namespace ui::fx
