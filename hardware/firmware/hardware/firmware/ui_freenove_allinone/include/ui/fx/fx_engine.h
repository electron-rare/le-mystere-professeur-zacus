#pragma once

#include <cstdint>

namespace ui::fx {

struct FxEngineConfig {
  uint16_t sprite_width = 160U;
  uint16_t sprite_height = 120U;
  uint8_t target_fps = 18U;
  bool lgfx_backend = false;
};

struct FxEngineStats {
  uint16_t fps = 0U;
  uint16_t stars = 0U;
  uint16_t particles = 0U;
  uint16_t object_count = 0U;
  uint32_t frame_count = 0U;
};

class FxEngine {
 public:
  bool begin(const FxEngineConfig& config);
  void reset();

  void noteFrame(uint32_t now_ms);
  void setSceneCounts(uint16_t object_count, uint16_t stars, uint16_t particles);

  FxEngineConfig config() const;
  FxEngineStats stats() const;

 private:
  FxEngineConfig config_ = {};
  FxEngineStats stats_ = {};
  uint32_t fps_window_start_ms_ = 0U;
  uint16_t fps_window_frames_ = 0U;
  bool ready_ = false;
};

}  // namespace ui::fx
