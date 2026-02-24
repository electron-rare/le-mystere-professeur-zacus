#pragma once

#include <cstddef>
#include <cstdint>

namespace drivers::display {
class DisplayHal;
}

namespace ui::fx {

enum class FxScenePhase : uint8_t {
  kIdle = 0,
  kPhaseA,
  kPhaseB,
  kPhaseC,
};

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
  FxEngine() = default;
  ~FxEngine() = default;
  FxEngine(const FxEngine&) = delete;
  FxEngine& operator=(const FxEngine&) = delete;
  FxEngine(FxEngine&&) = delete;
  FxEngine& operator=(FxEngine&&) = delete;

  bool begin(const FxEngineConfig& config);
  void reset();
  void setEnabled(bool enabled);
  bool enabled() const;
  void setQualityLevel(uint8_t quality_level);
  bool renderFrame(uint32_t now_ms,
                   drivers::display::DisplayHal& display,
                   uint16_t display_width,
                   uint16_t display_height,
                   FxScenePhase phase);

  void noteFrame(uint32_t now_ms);
  void setSceneCounts(uint16_t object_count, uint16_t stars, uint16_t particles);

  FxEngineConfig config() const;
  FxEngineStats stats() const;

 private:
  static constexpr uint16_t kMaxStars = 220U;

  struct Star {
    int32_t x_q8 = 0;
    int32_t y_q8 = 0;
    uint16_t speed_q8 = 0U;
    uint8_t layer = 0U;
  };

  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
  uint32_t nextRand();
  void seedStars(uint16_t star_count);
  void updateStars(uint32_t dt_ms);
  void renderLowRes(uint32_t now_ms, FxScenePhase phase);
  void drawPixel(int16_t x, int16_t y, uint16_t color565);
  bool blitUpscaled(drivers::display::DisplayHal& display, uint16_t display_width, uint16_t display_height);

  FxEngineConfig config_ = {};
  FxEngineStats stats_ = {};
  uint32_t fps_window_start_ms_ = 0U;
  uint16_t fps_window_frames_ = 0U;
  uint32_t last_render_ms_ = 0U;
  uint32_t next_frame_ms_ = 0U;
  uint16_t* sprite_pixels_ = nullptr;
  uint16_t* line_buffer_ = nullptr;
  size_t sprite_pixel_count_ = 0U;
  uint16_t star_count_ = 0U;
  uint8_t quality_level_ = 0U;
  bool enabled_ = false;
  bool ready_ = false;
  Star stars_[kMaxStars] = {};
  uint32_t rng_state_ = 0x13579BDFUL;
};

}  // namespace ui::fx
