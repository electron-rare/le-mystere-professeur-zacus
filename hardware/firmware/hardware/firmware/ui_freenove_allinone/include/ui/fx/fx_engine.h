#pragma once

#include <cstddef>
#include <cstdint>

#include "ui_freenove_config.h"
#include "ui/fx/v8/fx_sync.h"
#include "ui/fx/v9/assets/assets_fs.h"
#include "ui/fx/v9/engine/engine.h"

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

enum class FxPreset : uint8_t {
  kDemo = 0,
  kWinner,
  kFireworks,
  kBoingball,
};

enum class FxMode : uint8_t {
  kClassic = 0,
  kStarfield3D,
  kDotSphere3D,
  kVoxelLandscape,
  kRayCorridor,
};

enum class FxScrollFont : uint8_t {
  kBasic = 0,
  kBold,
  kOutline,
  kItalic,
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
  uint32_t blit_cpu_us = 0U;
  uint32_t blit_dma_submit_us = 0U;
  uint32_t blit_dma_wait_us = 0U;
  uint32_t blit_cpu_max_us = 0U;
  uint32_t blit_dma_submit_max_us = 0U;
  uint32_t blit_dma_wait_max_us = 0U;
  uint32_t dma_tail_wait_us = 0U;
  uint32_t dma_tail_wait_max_us = 0U;
  uint32_t dma_timeout_count = 0U;
  uint32_t blit_fail_busy = 0U;
  uint16_t blit_lines = 0U;
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
  void setPreset(FxPreset preset);
  FxPreset preset() const;
  void setMode(FxMode mode);
  FxMode mode() const;
  void setScrollText(const char* text);
  void setScrollFont(FxScrollFont font);
  FxScrollFont scrollFont() const;
  void setScrollerCentered(bool centered);
  bool scrollerCentered() const;
  void setBpm(uint16_t bpm);
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
  static constexpr uint16_t kMaxSpriteWidth = 240U;
  static constexpr uint16_t kMaxSpriteHeight = 240U;
  static constexpr uint16_t kMaxStars = 220U;
  static constexpr uint16_t kMaxStars3D = 512U;
  static constexpr uint16_t kMaxDots = 384U;
  static constexpr uint16_t kRayTexSize = 64U;
  static constexpr uint16_t kRayTexCount = kRayTexSize * kRayTexSize;
  static constexpr uint16_t kMaxFireworkParticles = 96U;

  struct Star {
    int32_t x_q8 = 0;
    int32_t y_q8 = 0;
    uint16_t speed_q8 = 0U;
    uint8_t layer = 0U;
  };

  struct FireworkParticle {
    int32_t x_q8 = 0;
    int32_t y_q8 = 0;
    int32_t vx_q8 = 0;
    int32_t vy_q8 = 0;
    uint8_t life = 0U;
    uint8_t color6 = 0U;
  };

  struct Star3D {
    int16_t x = 0;
    int16_t y = 0;
    uint16_t z = 0;
  };

  struct DotPt {
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
  };

  enum class BgMode : uint8_t {
    kPlasma = 0,
    kStarfield,
    kRasterBars,
  };

  enum class MidMode : uint8_t {
    kNone = 0,
    kShadeBobs,
    kRotoZoom,
    kFireworks,
    kBoingball,
  };

  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
  static uint16_t addSat565(uint16_t a, uint16_t b);
  static uint16_t mul565_u8(uint16_t c, uint8_t v);
  uint32_t nextRand();
  void initTrigLutIfNeeded();
  int16_t sinQ15(uint8_t a) const;
  int16_t cosQ15(uint8_t a) const;
  void seedStars(uint16_t star_count);
  void updateStars(uint32_t dt_ms);
  void initModeState();
  void renderMode3D(uint32_t now_ms);
  void renderStarfield3D(uint32_t now_ms);
  void renderDotSphere3D(uint32_t now_ms);
  void renderVoxelLandscape(uint32_t now_ms);
  void renderRayCorridor(uint32_t now_ms);
  void applyPreset(FxPreset preset);
  void ensureDefaultScrollText(FxPreset preset);
  void renderBackground(uint32_t now_ms, FxScenePhase phase);
  void renderBackgroundPlasma(uint32_t now_ms);
  void renderBackgroundStarfield(uint32_t now_ms);
  void renderBackgroundRasterBars(uint32_t now_ms);
  void renderMid(uint32_t now_ms, uint32_t dt_ms, FxScenePhase phase);
  void renderMidShadeBobs(uint32_t now_ms);
  void renderMidRotoZoom(uint32_t now_ms);
  void stepFireworks(uint32_t dt_ms);
  void renderMidFireworks(uint32_t now_ms, uint32_t dt_ms);
  bool initBoingAssets();
  void releaseBoingAssets();
  void stepBoing(uint32_t dt_ms);
  void renderMidBoingball(uint32_t now_ms, uint32_t dt_ms);
  void renderScroller(uint32_t now_ms);
  void drawChar6x8(int16_t x, int16_t y_top, char c, uint16_t color565, uint16_t shadow565);
  uint16_t selectBoingColor(uint8_t shade, bool checker_red) const;
  bool initV9Runtime();
  void resetV9Runtime();
  void markV9TimelineDirty();
  bool ensureV9TimelineLoaded();
  const char* timelinePathForPreset(FxPreset preset) const;
  bool renderLowResV9(uint32_t dt_ms);
  void renderLowRes(uint32_t now_ms, FxScenePhase phase);
  void drawPixel(int16_t x, int16_t y, uint16_t color565);
  void addPixel(int16_t x, int16_t y, uint16_t color565);
  void fillSprite(uint16_t color565);
  bool buildScaleMaps(uint16_t display_width, uint16_t display_height);
  bool blitUpscaled(drivers::display::DisplayHal& display, uint16_t display_width, uint16_t display_height);
  bool allocateLineBuffers();
  void releaseLineBuffers();

  FxEngineConfig config_ = {};
  FxEngineStats stats_ = {};
  uint32_t fps_window_start_ms_ = 0U;
  uint16_t fps_window_frames_ = 0U;
  uint32_t last_render_ms_ = 0U;
  uint32_t next_frame_ms_ = 0U;
  uint16_t* sprite_pixels_ = nullptr;
  uint16_t* line_buffers_[2] = {nullptr, nullptr};
  uint16_t line_buffer_lines_ = 0U;
  uint16_t line_buffer_width_ = 0U;
  uint8_t line_buffer_count_ = 0U;
  size_t sprite_pixel_count_ = 0U;
  uint16_t star_count_ = 0U;
  uint8_t quality_level_ = 0U;
  bool enabled_ = false;
  bool ready_ = false;
  FxPreset preset_ = FxPreset::kDemo;
  FxMode mode_ = FxMode::kClassic;
  FxScrollFont scroll_font_ = FxScrollFont::kBasic;
  uint16_t bpm_ = 125U;
  bool scroll_text_custom_ = false;
  char scroll_text_[256] = {0};
  uint16_t scroll_text_len_ = 0U;
  uint32_t scroll_phase_px_q16_ = 0U;
  uint8_t scroll_wave_phase_ = 0U;
  uint8_t scroll_highlight_phase_ = 0U;
  bool scroller_centered_ = false;
  BgMode bg_mode_ = BgMode::kPlasma;
  MidMode mid_mode_ = MidMode::kShadeBobs;
  fx_sync_t sync_ = {};
  uint16_t* roto_texture_ = nullptr;
  FireworkParticle fireworks_[kMaxFireworkParticles] = {};
  uint32_t fireworks_seed_ = 0x1234ABCDUL;
  uint16_t firework_live_count_ = 0U;
  uint8_t* boing_mask_ = nullptr;
  uint8_t* boing_u_ = nullptr;
  uint8_t* boing_v_ = nullptr;
  uint8_t* boing_shade_ = nullptr;
  bool boing_ready_ = false;
  uint8_t boing_phase_ = 0U;
  float boing_x_ = 0.0f;
  float boing_y_ = 0.0f;
  float boing_vx_ = 80.0f;
  float boing_vy_ = 0.0f;
  float boing_floor_y_ = 0.0f;
  bool trig_ready_ = false;
  int16_t sin_q15_[256] = {};
  uint16_t star3d_count_ = 384U;
  Star3D stars3d_[kMaxStars3D] = {};
  uint16_t dot_count_ = 256U;
  uint8_t dot_blob_radius_ = 2U;
  uint8_t dot_radius_px_ = 48U;
  uint16_t dot_shade_lut_[256] = {};
  DotPt dots_[kMaxDots] = {};
  uint8_t voxel_height_[256] = {};
  uint16_t voxel_pal_[256] = {};
  uint16_t voxel_proj_q8_[128] = {};
  uint8_t voxel_max_dist_ = 96U;
  uint16_t ray_tex_[kRayTexCount] = {};
  int8_t ray_col_off_[kMaxSpriteWidth] = {};
  uint16_t ray_floor_scale_q12_[kMaxSpriteHeight] = {};
  Star stars_[kMaxStars] = {};
  uint32_t rng_state_ = 0x13579BDFUL;
  static constexpr uint16_t kScaleMapAxisMax =
      (FREENOVE_LCD_WIDTH > FREENOVE_LCD_HEIGHT) ? FREENOVE_LCD_WIDTH : FREENOVE_LCD_HEIGHT;
  uint16_t x_scale_map_[kScaleMapAxisMax] = {};
  uint16_t y_scale_map_[kScaleMapAxisMax] = {};
  uint16_t scale_map_width_ = 0U;
  uint16_t scale_map_height_ = 0U;
  ::fx::assets::FsAssetManager v9_assets_{"/ui/fx"};
  ::fx::SinCosLUT v9_luts_ = {};
  ::fx::Engine v9_engine_ = {};
  ::fx::RenderTarget v9_internal_rt_ = {};
  ::fx::RenderTarget v9_output_rt_ = {};
  uint8_t* v9_internal_pixels_ = nullptr;
  size_t v9_internal_pixel_count_ = 0U;
  bool v9_runtime_ready_ = false;
  bool v9_timeline_dirty_ = true;
  bool v9_use_runtime_ = true;
  FxPreset v9_loaded_preset_ = FxPreset::kDemo;
  uint32_t blit_cpu_time_total_us_ = 0U;
  uint32_t blit_dma_submit_time_total_us_ = 0U;
  uint32_t blit_dma_wait_time_total_us_ = 0U;
  uint32_t blit_cpu_time_max_us_ = 0U;
  uint32_t blit_dma_submit_time_max_us_ = 0U;
  uint32_t blit_dma_wait_time_max_us_ = 0U;
  uint32_t blit_dma_tail_wait_time_total_us_ = 0U;
  uint32_t blit_dma_tail_wait_time_max_us_ = 0U;
  uint32_t blit_dma_timeout_count_ = 0U;
  uint32_t blit_fail_busy_count_ = 0U;
};

}  // namespace ui::fx
