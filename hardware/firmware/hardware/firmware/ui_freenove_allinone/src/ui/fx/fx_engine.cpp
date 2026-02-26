#include "ui/fx/fx_engine.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <cmath>
#include <cstring>
#include <string>

#include "drivers/display/display_hal.h"
#include "runtime/memory/caps_allocator.h"
#include "runtime/memory/safe_size.h"
#include "ui/fx/fx_blit_fast.h"
#include "ui/fx/v9/effects/registry.h"
#include "ui/fx/v9/engine/timeline_load.h"
#include "ui/fx/v9/boing/boing_shadow_darken.h"
#include "ui/fx/v8/font_select.h"
#include "ui/fx/v8/fx_luts.h"
#include "ui/fx/v8/fx_utils.h"
#include "ui_freenove_config.h"

namespace ui::fx {

namespace {

constexpr uint16_t kMinSpriteWidth = 96U;
constexpr uint16_t kMinSpriteHeight = 72U;
constexpr uint16_t kMaxSpriteWidth = 240U;
constexpr uint16_t kMaxSpriteHeight = 240U;
constexpr uint8_t kMinTargetFps = 12U;
constexpr uint8_t kMaxTargetFps = 30U;
constexpr uint16_t kLineBufAlignment = 16U;
constexpr uint16_t kDisplaySpanMax =
    (FREENOVE_LCD_WIDTH > FREENOVE_LCD_HEIGHT) ? FREENOVE_LCD_WIDTH : FREENOVE_LCD_HEIGHT;
constexpr uint16_t kRotoTexSize = 128U;
constexpr uint16_t kBoingGridSize = 128U;
constexpr uint16_t kBoingRadiusLrDefault = 54U;
constexpr uint8_t kBoingCheckerShift = 4U;
constexpr uint8_t kSafeBgScale = 170U;
constexpr uint8_t kSafeBandMarginTop = 10U;
constexpr uint8_t kSafeBandMarginBottom = 14U;
constexpr uint8_t kSafeFeatherPx = 18U;
constexpr uint8_t kScrollerCharWidth = 7U;
constexpr uint8_t kScrollerGlyphHeight = 8U;
constexpr uint8_t kScrollerAmpPx = 18U;
constexpr uint8_t kScrollerBaseYOffset = 130U;
constexpr uint16_t kFxBpmDefault = 125U;
constexpr uint8_t kShadeBobCountDefault = 16U;
constexpr uint8_t kFireworkSpawnPerBeat = 24U;
constexpr uint8_t kFireworkDecayPerFrame = 6U;
constexpr uint16_t kBoingFloorBottomPadding = 12U;

#ifndef UI_FX_LINEBUF_RGB565
#define UI_FX_LINEBUF_RGB565 1
#endif

#ifndef UI_FX_LINEBUF_LINES
#define UI_FX_LINEBUF_LINES 8U
#endif

#ifndef UI_ENABLE_SIMD_PATH
#define UI_ENABLE_SIMD_PATH 0
#endif

#ifndef UI_SIMD_EXPERIMENTAL
#define UI_SIMD_EXPERIMENTAL 0
#endif

#ifndef UI_FX_DMA_BLIT
#define UI_FX_DMA_BLIT 0
#endif

#ifndef UI_FX_BLIT_FAST_2X
#define UI_FX_BLIT_FAST_2X 1
#endif

constexpr uint16_t kFxLineBufLinesRequested = static_cast<uint16_t>(UI_FX_LINEBUF_LINES);
[[maybe_unused]] constexpr bool kFxLineBufUseRgb565 = (UI_FX_LINEBUF_RGB565 != 0U);
[[maybe_unused]] constexpr bool kFxEnableSimdPath = (UI_ENABLE_SIMD_PATH != 0U);
[[maybe_unused]] constexpr bool kUiEnableSimdExperimental = (UI_SIMD_EXPERIMENTAL != 0U);
constexpr bool kFxUseDmaBlit = (UI_FX_DMA_BLIT != 0U);
constexpr bool kFxUseFast2xBlit = (UI_FX_BLIT_FAST_2X != 0U);

constexpr uint32_t kFxDmaWaitBudgetUs = 6000U;
constexpr const char* kTimelineDemo3dPath = "/ui/fx/timelines/demo_3d.json";
constexpr const char* kTimelineDemoFallbackPath = "/ui/fx/timelines/demo_90s.json";
constexpr const char* kTimelineWinnerPath = "/ui/fx/timelines/winner.json";
constexpr const char* kTimelineFireworksPath = "/ui/fx/timelines/fireworks.json";
constexpr const char* kTimelineBoingPath = "/ui/fx/timelines/boingball.json";

class NullJsonParser final : public ::fx::IJsonParser {
 public:
  const ::fx::JsonValue* parse(const std::string& jsonText) override {
    (void)jsonText;
    return nullptr;
  }

  void free(const ::fx::JsonValue* root) override {
    (void)root;
  }
};

template <typename T>
T clampValue(T value, T min_value, T max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

template <typename T>
T wrapValue(T value, T modulo) {
  if (modulo == 0) {
    return 0;
  }
  while (value < 0) {
    value += modulo;
  }
  return static_cast<T>(value % modulo);
}

uint16_t* allocateAlignedDmaBuffer(size_t bytes, const char* tag) {
  if (bytes == 0U) {
    return nullptr;
  }
  return static_cast<uint16_t*>(
      runtime::memory::CapsAllocator::allocInternalDmaAligned(kLineBufAlignment, bytes, tag));
}

bool readFsTextFile(const char* path, std::string* out_text) {
  if (path == nullptr || path[0] == '\0' || out_text == nullptr) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }
  String text = file.readString();
  file.close();
  if (text.isEmpty()) {
    return false;
  }
  out_text->assign(text.c_str(), text.length());
  return !out_text->empty();
}

uint8_t safeBandScaleForY(int y, int base_y, int height) {
  const int top0 = base_y - static_cast<int>(kScrollerAmpPx) - static_cast<int>(kSafeBandMarginTop);
  const int bot0 = base_y + static_cast<int>(kScrollerAmpPx) + static_cast<int>(kScrollerGlyphHeight) +
                   static_cast<int>(kSafeBandMarginBottom);
  if (y < (top0 - static_cast<int>(kSafeFeatherPx)) || y > (bot0 + static_cast<int>(kSafeFeatherPx))) {
    return 255U;
  }
  if (y < top0) {
    const int d = clampValue<int>(y - (top0 - static_cast<int>(kSafeFeatherPx)), 0, kSafeFeatherPx);
    const uint8_t t = static_cast<uint8_t>((d * 255) / kSafeFeatherPx);
    const uint8_t eased = fx_fade_curve(t);
    const int k = 255 - ((255 - static_cast<int>(kSafeBgScale)) * static_cast<int>(eased) / 255);
    return static_cast<uint8_t>(clampValue<int>(k, kSafeBgScale, 255));
  }
  if (y <= bot0) {
    return kSafeBgScale;
  }
  const int d = clampValue<int>(y - bot0, 0, kSafeFeatherPx);
  const uint8_t t = static_cast<uint8_t>((d * 255) / kSafeFeatherPx);
  const uint8_t eased = fx_fade_curve(t);
  const int k = static_cast<int>(kSafeBgScale) +
                ((255 - static_cast<int>(kSafeBgScale)) * static_cast<int>(eased) / 255);
  (void)height;
  return static_cast<uint8_t>(clampValue<int>(k, kSafeBgScale, 255));
}

template <bool kPack32>
[[maybe_unused]] void convertGray8ToRgb565Line(const uint8_t* src,
                                              uint16_t* dst,
                                              uint16_t px_count) {
  static bool g_lut_ready = false;
  static uint16_t g_gray8_to_rgb565[256];
  if (!g_lut_ready) {
    for (uint16_t gray = 0U; gray < 256U; ++gray) {
      const uint16_t r5 = static_cast<uint16_t>(gray >> 3U) & 0x1FU;
      const uint16_t g6 = static_cast<uint16_t>(gray >> 2U) & 0x3FU;
      const uint16_t b5 = static_cast<uint16_t>(gray >> 3U) & 0x1FU;
      g_gray8_to_rgb565[gray] = static_cast<uint16_t>((r5 << 11U) | (g6 << 5U) | b5);
    }
    g_lut_ready = true;
  }

  if (px_count == 0U) {
    return;
  }
  if (kPack32) {
    if ((reinterpret_cast<uintptr_t>(dst) & 0x3U) == 0U) {
      uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst);
      uint16_t index = 0U;
      while (index + 1U < px_count) {
        const uint16_t c0 = g_gray8_to_rgb565[static_cast<size_t>(src[index])];
        const uint16_t c1 = g_gray8_to_rgb565[static_cast<size_t>(src[index + 1U])];
        dst32[index >> 1U] = (static_cast<uint32_t>(c1) << 16U) | static_cast<uint32_t>(c0);
        index += 2U;
      }
      if (index < px_count) {
        dst[index] = g_gray8_to_rgb565[static_cast<size_t>(src[index])];
      }
      return;
    }
  }

  for (uint16_t index = 0U; index < px_count; ++index) {
    dst[index] = g_gray8_to_rgb565[static_cast<size_t>(src[index])];
  }
}

}  // namespace

bool FxEngine::begin(const FxEngineConfig& config) {
  config_ = config;
  config_.sprite_width = clampValue<uint16_t>(config_.sprite_width, kMinSpriteWidth, kMaxSpriteWidth);
  config_.sprite_height = clampValue<uint16_t>(config_.sprite_height, kMinSpriteHeight, kMaxSpriteHeight);
  config_.target_fps = clampValue<uint8_t>(config_.target_fps, kMinTargetFps, kMaxTargetFps);
  enabled_ = config_.lgfx_backend;
  scroll_phase_px_q16_ = 0U;
  scroll_wave_phase_ = 0U;
  scroll_highlight_phase_ = 0U;
  scroll_text_custom_ = false;
  bpm_ = kFxBpmDefault;
  fx_luts_init();
  fx_sync_init(&sync_, bpm_);
  initTrigLutIfNeeded();

  if (sprite_pixels_ != nullptr) {
    runtime::memory::CapsAllocator::release(sprite_pixels_);
    sprite_pixels_ = nullptr;
  }
  if (roto_texture_ != nullptr) {
    runtime::memory::CapsAllocator::release(roto_texture_);
    roto_texture_ = nullptr;
  }
  releaseBoingAssets();
  releaseLineBuffers();
  resetV9Runtime();
  sprite_pixel_count_ = 0U;
  blit_cpu_time_total_us_ = 0U;
  blit_dma_submit_time_total_us_ = 0U;
  blit_dma_wait_time_total_us_ = 0U;
  blit_cpu_time_max_us_ = 0U;
  blit_dma_submit_time_max_us_ = 0U;
  blit_dma_wait_time_max_us_ = 0U;
  blit_dma_tail_wait_time_total_us_ = 0U;
  blit_dma_tail_wait_time_max_us_ = 0U;
  blit_dma_timeout_count_ = 0U;
  blit_fail_busy_count_ = 0U;

  if (config_.lgfx_backend) {
    sprite_pixel_count_ =
        static_cast<size_t>(config_.sprite_width) * static_cast<size_t>(config_.sprite_height);
    sprite_pixels_ = static_cast<uint16_t*>(
        runtime::memory::CapsAllocator::allocPsram(sprite_pixel_count_ * sizeof(uint16_t), "fx_sprite"));
    if (sprite_pixels_ == nullptr) {
      ready_ = false;
      return false;
    }

    if (!allocateLineBuffers()) {
      runtime::memory::CapsAllocator::release(sprite_pixels_);
      sprite_pixels_ = nullptr;
      ready_ = false;
      return false;
    }

    const size_t roto_pixels = static_cast<size_t>(kRotoTexSize) * static_cast<size_t>(kRotoTexSize);
    roto_texture_ = static_cast<uint16_t*>(
        runtime::memory::CapsAllocator::allocPsram(roto_pixels * sizeof(uint16_t), "fx_roto_tex"));
    if (roto_texture_ == nullptr) {
      runtime::memory::CapsAllocator::release(sprite_pixels_);
      sprite_pixels_ = nullptr;
      releaseLineBuffers();
      ready_ = false;
      return false;
    }
    for (uint16_t y = 0U; y < kRotoTexSize; ++y) {
      for (uint16_t x = 0U; x < kRotoTexSize; ++x) {
        const float cx = static_cast<float>(x) - (static_cast<float>(kRotoTexSize) * 0.5f);
        const float cy = static_cast<float>(y) - (static_cast<float>(kRotoTexSize) * 0.5f);
        float rr = std::sqrt(cx * cx + cy * cy) / (static_cast<float>(kRotoTexSize) * 0.5f);
        if (rr > 1.0f) {
          rr = 1.0f;
        }
        const bool checker = (((x >> 4U) ^ (y >> 4U)) & 1U) != 0U;
        const uint8_t r = checker ? static_cast<uint8_t>(40U + static_cast<uint8_t>(200.0f * (1.0f - rr)))
                                  : 200U;
        const uint8_t g = checker ? static_cast<uint8_t>(60U + static_cast<uint8_t>(160.0f * (1.0f - rr)))
                                  : static_cast<uint8_t>(50U + static_cast<uint8_t>(120.0f * (1.0f - rr)));
        const uint8_t b = checker ? 200U
                                  : static_cast<uint8_t>(60U + static_cast<uint8_t>(120.0f * (1.0f - rr)));
        roto_texture_[static_cast<size_t>(y) * kRotoTexSize + static_cast<size_t>(x)] = rgb565(r, g, b);
      }
    }

    boing_ready_ = initBoingAssets();
    (void)initV9Runtime();
    Serial.printf("[FX] boing_shadow_path=%s\n", boing_shadow_asm_enabled() ? "asm" : "c");
  }

  setQualityLevel(0U);
  initModeState();
  reset();
  setPreset(FxPreset::kDemo);
  setScrollFont(FxScrollFont::kBasic);
  scale_map_width_ = 0U;
  scale_map_height_ = 0U;
  ready_ = true;
  return true;
}

void FxEngine::reset() {
  stats_ = {};
  fps_window_start_ms_ = 0U;
  fps_window_frames_ = 0U;
  last_render_ms_ = 0U;
  next_frame_ms_ = 0U;
  blit_cpu_time_total_us_ = 0U;
  blit_dma_submit_time_total_us_ = 0U;
  blit_dma_wait_time_total_us_ = 0U;
  blit_cpu_time_max_us_ = 0U;
  blit_dma_submit_time_max_us_ = 0U;
  blit_dma_wait_time_max_us_ = 0U;
  blit_dma_tail_wait_time_total_us_ = 0U;
  blit_dma_tail_wait_time_max_us_ = 0U;
  blit_dma_timeout_count_ = 0U;
  blit_fail_busy_count_ = 0U;
  mode_ = FxMode::kClassic;
  scroll_phase_px_q16_ = 0U;
  scroll_wave_phase_ = 0U;
  scroll_highlight_phase_ = 0U;
  std::memset(fireworks_, 0, sizeof(fireworks_));
  firework_live_count_ = 0U;
  boing_phase_ = 0U;
  boing_x_ = static_cast<float>(config_.sprite_width) * 0.5f;
  boing_y_ = static_cast<float>(config_.sprite_height) * 0.3f;
  boing_vx_ = 80.0f;
  boing_vy_ = 0.0f;
  boing_floor_y_ =
      static_cast<float>(config_.sprite_height) - static_cast<float>(kBoingFloorBottomPadding);
  fx_sync_init(&sync_, bpm_);
  v9_timeline_dirty_ = true;
  v9_use_runtime_ = v9_runtime_ready_;
  if (v9_runtime_ready_) {
    v9_engine_.init();
  }
  initModeState();
}

void FxEngine::releaseLineBuffers() {
  for (uint8_t index = 0U; index < 2U; ++index) {
    if (line_buffers_[index] != nullptr) {
      runtime::memory::CapsAllocator::release(line_buffers_[index]);
      line_buffers_[index] = nullptr;
    }
  }
  line_buffer_count_ = 0U;
  line_buffer_lines_ = 0U;
  line_buffer_width_ = 0U;
}

bool FxEngine::allocateLineBuffers() {
  releaseLineBuffers();
  line_buffer_width_ = kDisplaySpanMax;
  if (line_buffer_width_ == 0U) {
    line_buffer_width_ = 0U;
    return false;
  }

  constexpr uint16_t kLineBufferFallbacks[] = {kFxLineBufLinesRequested, 8U, 6U, 4U, 2U, 1U};

  uint16_t candidates[6] = {kLineBufferFallbacks[0U], kLineBufferFallbacks[1U], kLineBufferFallbacks[2U],
                            kLineBufferFallbacks[3U], kLineBufferFallbacks[4U], kLineBufferFallbacks[5U]};
  uint8_t candidate_count = 0U;
  for (uint8_t index = 0U; index < sizeof(kLineBufferFallbacks) / sizeof(kLineBufferFallbacks[0]); ++index) {
    const uint16_t candidate = kLineBufferFallbacks[index];
    if (candidate == 0U) {
      continue;
    }
    bool duplicate = false;
    for (uint8_t i = 0U; i < candidate_count; ++i) {
      if (candidates[i] == candidate) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      candidates[candidate_count++] = candidate;
    }
  }

  for (uint8_t index = 0U; index < candidate_count; ++index) {
    const uint16_t candidate = candidates[index];
    if (candidate == 0U) {
      continue;
    }
    size_t candidate_pixels = 0U;
    size_t candidate_bytes = 0U;
    if (!runtime::memory::safeMulSize(static_cast<size_t>(line_buffer_width_), static_cast<size_t>(candidate), &candidate_pixels) ||
        !runtime::memory::safeMulSize(candidate_pixels, sizeof(uint16_t), &candidate_bytes)) {
      continue;
    }

    uint16_t* first = allocateAlignedDmaBuffer(candidate_bytes, "fx_line");
    if (first == nullptr) {
      continue;
    }

    line_buffers_[0U] = first;
    line_buffer_lines_ = candidate;
    line_buffer_count_ = 1U;
    uint16_t* second = allocateAlignedDmaBuffer(candidate_bytes, "fx_line");
    if (second != nullptr) {
      line_buffers_[1U] = second;
      line_buffer_count_ = 2U;
    } else {
      line_buffers_[1U] = nullptr;
    }
    return true;
  }

  line_buffer_lines_ = 0U;
  line_buffer_count_ = 0U;
  return false;
}

void FxEngine::setEnabled(bool enabled) {
  enabled_ = enabled && config_.lgfx_backend;
}

bool FxEngine::enabled() const {
  return enabled_;
}

void FxEngine::setQualityLevel(uint8_t quality_level) {
  quality_level_ = quality_level;
  const uint32_t area = static_cast<uint32_t>(config_.sprite_width) * static_cast<uint32_t>(config_.sprite_height);
  uint16_t stars = static_cast<uint16_t>(clampValue<uint32_t>(area / 1200U, 60U, kMaxStars));
  if (quality_level_ == 1U) {
    stars = static_cast<uint16_t>((stars * 3U) / 5U);
  } else if (quality_level_ == 2U) {
    stars = static_cast<uint16_t>((stars * 4U) / 5U);
  } else if (quality_level_ >= 3U) {
    stars = clampValue<uint16_t>(static_cast<uint16_t>((stars * 6U) / 5U), 60U, kMaxStars);
  }
  seedStars(stars);
}

void FxEngine::setPreset(FxPreset preset) {
  applyPreset(preset);
  markV9TimelineDirty();
}

FxPreset FxEngine::preset() const {
  return preset_;
}

void FxEngine::setMode(FxMode mode) {
  if (mode_ == mode) {
    return;
  }
  mode_ = mode;
  initModeState();
  if (mode_ == FxMode::kClassic) {
    markV9TimelineDirty();
  }
}

FxMode FxEngine::mode() const {
  return mode_;
}

void FxEngine::setScrollText(const char* text) {
  if (text == nullptr || text[0] == '\0') {
    scroll_text_custom_ = false;
    ensureDefaultScrollText(preset_);
    return;
  }
  std::strncpy(scroll_text_, text, sizeof(scroll_text_) - 1U);
  scroll_text_[sizeof(scroll_text_) - 1U] = '\0';
  scroll_text_len_ = static_cast<uint16_t>(std::strlen(scroll_text_));
  scroll_text_custom_ = true;
}

void FxEngine::setScrollFont(FxScrollFont font) {
  scroll_font_ = font;
}

FxScrollFont FxEngine::scrollFont() const {
  return scroll_font_;
}

void FxEngine::setScrollerCentered(bool centered) {
  scroller_centered_ = centered;
}

bool FxEngine::scrollerCentered() const {
  return scroller_centered_;
}

void FxEngine::setBpm(uint16_t bpm) {
  bpm_ = clampValue<uint16_t>(bpm, 60U, 220U);
  fx_sync_init(&sync_, bpm_);
  markV9TimelineDirty();
}

void FxEngine::applyPreset(FxPreset preset) {
  preset_ = preset;
  switch (preset_) {
    case FxPreset::kDemo:
      bg_mode_ = BgMode::kPlasma;
      mid_mode_ = MidMode::kShadeBobs;
      if (!scroll_text_custom_) {
        scroll_font_ = FxScrollFont::kBasic;
      }
      break;
    case FxPreset::kWinner:
      bg_mode_ = BgMode::kRasterBars;
      mid_mode_ = MidMode::kRotoZoom;
      if (!scroll_text_custom_) {
        scroll_font_ = FxScrollFont::kOutline;
      }
      break;
    case FxPreset::kFireworks:
      bg_mode_ = BgMode::kStarfield;
      mid_mode_ = MidMode::kFireworks;
      if (!scroll_text_custom_) {
        scroll_font_ = FxScrollFont::kBold;
      }
      break;
    case FxPreset::kBoingball:
      bg_mode_ = BgMode::kStarfield;
      mid_mode_ = MidMode::kBoingball;
      if (!scroll_text_custom_) {
        scroll_font_ = FxScrollFont::kItalic;
      }
      break;
  }
  ensureDefaultScrollText(preset_);
}

void FxEngine::ensureDefaultScrollText(FxPreset preset) {
  if (scroll_text_custom_) {
    return;
  }
  const char* text = "HELLO FROM ESP32-S3 DEMOSCENE!   GREETZ: YOU!   ";
  switch (preset) {
    case FxPreset::kWinner:
      text = "WINNER! BRAVO BRIGADE Z!   ";
      break;
    case FxPreset::kFireworks:
      text = "FIREWORKS MODE!   VICTOIRE!   ";
      break;
    case FxPreset::kBoingball:
      text = "BOING BALL MODE!   SCENE WIN ETAPE   ";
      break;
    case FxPreset::kDemo:
    default:
      break;
  }
  std::strncpy(scroll_text_, text, sizeof(scroll_text_) - 1U);
  scroll_text_[sizeof(scroll_text_) - 1U] = '\0';
  scroll_text_len_ = static_cast<uint16_t>(std::strlen(scroll_text_));
}

void FxEngine::markV9TimelineDirty() {
  v9_timeline_dirty_ = true;
}

const char* FxEngine::timelinePathForPreset(FxPreset preset) const {
  switch (preset) {
    case FxPreset::kDemo:
      return kTimelineDemo3dPath;
    case FxPreset::kWinner:
      return kTimelineWinnerPath;
    case FxPreset::kFireworks:
      return kTimelineFireworksPath;
    case FxPreset::kBoingball:
      return kTimelineBoingPath;
  }
  return nullptr;
}

bool FxEngine::initV9Runtime() {
  if (v9_internal_pixels_ != nullptr) {
    runtime::memory::CapsAllocator::release(v9_internal_pixels_);
    v9_internal_pixels_ = nullptr;
  }
  v9_internal_pixel_count_ = 0U;
  v9_runtime_ready_ = false;
  v9_use_runtime_ = true;

  size_t pixel_count = 0U;
  if (!runtime::memory::safeMulSize(static_cast<size_t>(config_.sprite_width),
                                    static_cast<size_t>(config_.sprite_height),
                                    &pixel_count)) {
    v9_use_runtime_ = false;
    return false;
  }
  const size_t byte_count = pixel_count * sizeof(uint8_t);
  v9_internal_pixels_ =
      static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(byte_count, "fx_v9_i8"));
  if (v9_internal_pixels_ == nullptr) {
    v9_use_runtime_ = false;
    return false;
  }

  v9_internal_pixel_count_ = pixel_count;
  std::memset(v9_internal_pixels_, 0, byte_count);
  v9_internal_rt_.pixels = v9_internal_pixels_;
  v9_internal_rt_.w = config_.sprite_width;
  v9_internal_rt_.h = config_.sprite_height;
  v9_internal_rt_.strideBytes = static_cast<int>(config_.sprite_width);
  v9_internal_rt_.fmt = ::fx::PixelFormat::I8;
  v9_internal_rt_.palette565 = v9_assets_.getPalette565("default");
  v9_internal_rt_.aligned16 = ((reinterpret_cast<uintptr_t>(v9_internal_pixels_) & 15U) == 0U) &&
                              ((config_.sprite_width & 15U) == 0U);

  v9_output_rt_.pixels = sprite_pixels_;
  v9_output_rt_.w = config_.sprite_width;
  v9_output_rt_.h = config_.sprite_height;
  v9_output_rt_.strideBytes = static_cast<int>(config_.sprite_width * sizeof(uint16_t));
  v9_output_rt_.fmt = ::fx::PixelFormat::RGB565;
  v9_output_rt_.palette565 = nullptr;
  v9_output_rt_.aligned16 = ((reinterpret_cast<uintptr_t>(sprite_pixels_) & 15U) == 0U) &&
                            (((config_.sprite_width * sizeof(uint16_t)) & 15U) == 0U);

  v9_luts_.init();
  ::fx::effects::FxServices services{};
  services.assets = &v9_assets_;
  services.luts = &v9_luts_;
  ::fx::effects::registerAll(v9_engine_, services);
  v9_engine_.setAssetManager(&v9_assets_);
  v9_engine_.setInternalTarget(v9_internal_rt_);
  v9_engine_.setOutputTarget(v9_output_rt_);
  v9_timeline_dirty_ = true;
  v9_runtime_ready_ = true;
  return true;
}

void FxEngine::resetV9Runtime() {
  if (v9_internal_pixels_ != nullptr) {
    runtime::memory::CapsAllocator::release(v9_internal_pixels_);
    v9_internal_pixels_ = nullptr;
  }
  v9_internal_pixel_count_ = 0U;
  v9_internal_rt_ = {};
  v9_output_rt_ = {};
  v9_runtime_ready_ = false;
  v9_timeline_dirty_ = true;
  v9_use_runtime_ = true;
}

bool FxEngine::ensureV9TimelineLoaded() {
  if (!v9_runtime_ready_ || !v9_use_runtime_) {
    return false;
  }
  if (!v9_timeline_dirty_ && v9_loaded_preset_ == preset_) {
    return true;
  }

  const char* path = timelinePathForPreset(preset_);
  if (path == nullptr) {
    return false;
  }

  std::string timeline_json;
  bool loaded = readFsTextFile(path, &timeline_json);
  if (!loaded && preset_ == FxPreset::kDemo) {
    loaded = readFsTextFile(kTimelineDemoFallbackPath, &timeline_json);
  }
  if (!loaded || timeline_json.empty()) {
    v9_loaded_preset_ = preset_;
    v9_timeline_dirty_ = false;
    return false;
  }

  ::fx::Timeline timeline;
  NullJsonParser parser;
  if (!::fx::loadTimelineFromJson(timeline, parser, timeline_json)) {
    v9_loaded_preset_ = preset_;
    v9_timeline_dirty_ = false;
    return false;
  }
  timeline.meta.bpm = static_cast<float>(bpm_);
  timeline.meta.internal.w = config_.sprite_width;
  timeline.meta.internal.h = config_.sprite_height;
  timeline.meta.internal.fmt = ::fx::PixelFormat::I8;

  if (!v9_engine_.loadTimeline(timeline)) {
    v9_loaded_preset_ = preset_;
    v9_timeline_dirty_ = false;
    return false;
  }
  v9_internal_rt_.palette565 = v9_assets_.getPalette565("default");
  v9_engine_.setInternalTarget(v9_internal_rt_);
  v9_engine_.setOutputTarget(v9_output_rt_);
  v9_engine_.init();

  v9_loaded_preset_ = preset_;
  v9_timeline_dirty_ = false;
  return true;
}

bool FxEngine::renderLowResV9(uint32_t dt_ms) {
  if (!ensureV9TimelineLoaded()) {
    return false;
  }
  v9_output_rt_.pixels = sprite_pixels_;
  v9_output_rt_.w = config_.sprite_width;
  v9_output_rt_.h = config_.sprite_height;
  v9_output_rt_.strideBytes = static_cast<int>(config_.sprite_width * sizeof(uint16_t));
  v9_engine_.setOutputTarget(v9_output_rt_);

  float dt = static_cast<float>(dt_ms) / 1000.0f;
  if (dt <= 0.0f) {
    dt = 1.0f / static_cast<float>(config_.target_fps == 0U ? 18U : config_.target_fps);
  }
  if (dt > 0.12f) {
    dt = 0.12f;
  }
  v9_engine_.tick(dt);
  v9_engine_.render(v9_internal_rt_, v9_output_rt_);
  return true;
}

void FxEngine::releaseBoingAssets() {
  if (boing_mask_ != nullptr) {
    runtime::memory::CapsAllocator::release(boing_mask_);
    boing_mask_ = nullptr;
  }
  if (boing_u_ != nullptr) {
    runtime::memory::CapsAllocator::release(boing_u_);
    boing_u_ = nullptr;
  }
  if (boing_v_ != nullptr) {
    runtime::memory::CapsAllocator::release(boing_v_);
    boing_v_ = nullptr;
  }
  if (boing_shade_ != nullptr) {
    runtime::memory::CapsAllocator::release(boing_shade_);
    boing_shade_ = nullptr;
  }
  boing_ready_ = false;
}

bool FxEngine::initBoingAssets() {
  releaseBoingAssets();
  size_t nn = 0U;
  if (!runtime::memory::safeMulSize(static_cast<size_t>(kBoingGridSize),
                                    static_cast<size_t>(kBoingGridSize),
                                    &nn)) {
    return false;
  }

  boing_mask_ = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(nn, "fx_boing_mask"));
  boing_u_ = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(nn, "fx_boing_u"));
  boing_v_ = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(nn, "fx_boing_v"));
  boing_shade_ = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(nn, "fx_boing_shade"));
  if (boing_mask_ == nullptr || boing_u_ == nullptr || boing_v_ == nullptr || boing_shade_ == nullptr) {
    releaseBoingAssets();
    return false;
  }

  const float cx = (static_cast<float>(kBoingGridSize) - 1.0f) * 0.5f;
  const float cy = (static_cast<float>(kBoingGridSize) - 1.0f) * 0.5f;
  const float radius = (static_cast<float>(kBoingGridSize) - 1.0f) * 0.5f;
  const float radius2 = radius * radius;
  float lx = 0.35f;
  float ly = -0.25f;
  float lz = 0.90f;
  const float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
  lx /= ll;
  ly /= ll;
  lz /= ll;

  for (uint16_t y = 0U; y < kBoingGridSize; ++y) {
    const float dy = static_cast<float>(y) - cy;
    for (uint16_t x = 0U; x < kBoingGridSize; ++x) {
      const float dx = static_cast<float>(x) - cx;
      const float d2 = dx * dx + dy * dy;
      const size_t idx = static_cast<size_t>(y) * kBoingGridSize + static_cast<size_t>(x);
      if (d2 > radius2) {
        boing_mask_[idx] = 0U;
        boing_u_[idx] = 0U;
        boing_v_[idx] = 0U;
        boing_shade_[idx] = 0U;
        continue;
      }
      const float z = std::sqrt(std::fmax(0.0f, radius2 - d2));
      const float nx = dx / radius;
      const float ny = dy / radius;
      const float nz = z / radius;
      const float u = (std::atan2(nx, nz) + 3.1415926f) / (2.0f * 3.1415926f);
      const float v = std::acos(clampValue<float>(ny, -1.0f, 1.0f)) / 3.1415926f;
      boing_u_[idx] = static_cast<uint8_t>(clampValue<int>(static_cast<int>(std::lround(u * 255.0f)), 0, 255));
      boing_v_[idx] = static_cast<uint8_t>(clampValue<int>(static_cast<int>(std::lround(v * 255.0f)), 0, 255));
      boing_mask_[idx] = 1U;

      float dot = nx * lx + ny * ly + nz * lz;
      dot = clampValue<float>((dot + 0.10f) / 1.10f, 0.0f, 1.0f);
      boing_shade_[idx] =
          static_cast<uint8_t>(clampValue<int>(static_cast<int>(std::lround(dot * 255.0f)), 0, 255));
    }
  }
  boing_ready_ = true;
  return true;
}

uint16_t FxEngine::selectBoingColor(uint8_t shade, bool checker_red) const {
  static constexpr uint8_t kLevels[4] = {255U, 200U, 150U, 110U};
  const uint8_t level = static_cast<uint8_t>(shade >> 6U);
  const uint8_t s = kLevels[level & 0x03U];
  if (!checker_red) {
    return fx_rgb565(s, s, s);
  }
  const uint8_t rg = static_cast<uint8_t>((static_cast<uint16_t>(s) * 20U) / 255U);
  return fx_rgb565(s, rg, rg);
}

bool FxEngine::renderFrame(uint32_t now_ms,
                           drivers::display::DisplayHal& display,
                           uint16_t display_width,
                           uint16_t display_height,
                           FxScenePhase phase) {
  if (!ready_ || !enabled_ || !config_.lgfx_backend || sprite_pixels_ == nullptr) {
    return false;
  }
  if (display_width == 0U || display_height == 0U) {
    return false;
  }
  if (next_frame_ms_ != 0U && static_cast<int32_t>(now_ms - next_frame_ms_) < 0) {
    return false;
  }

  const uint32_t frame_period_ms = 1000U / static_cast<uint32_t>(config_.target_fps);
  next_frame_ms_ = now_ms + ((frame_period_ms == 0U) ? 1U : frame_period_ms);

  if (!buildScaleMaps(display_width, display_height)) {
    return false;
  }

  renderLowRes(now_ms, phase);
  if (!blitUpscaled(display, display_width, display_height)) {
    return false;
  }
  noteFrame(now_ms);
  return true;
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
  FxEngineStats snapshot = stats_;
  const uint32_t frame_count = (snapshot.frame_count == 0U) ? 1U : snapshot.frame_count;
  snapshot.blit_cpu_us = static_cast<uint32_t>(blit_cpu_time_total_us_ / frame_count);
  snapshot.blit_dma_submit_us = static_cast<uint32_t>(blit_dma_submit_time_total_us_ / frame_count);
  snapshot.blit_dma_wait_us = static_cast<uint32_t>(blit_dma_wait_time_total_us_ / frame_count);
  snapshot.blit_cpu_max_us = blit_cpu_time_max_us_;
  snapshot.blit_dma_submit_max_us = blit_dma_submit_time_max_us_;
  snapshot.blit_dma_wait_max_us = blit_dma_wait_time_max_us_;
  snapshot.dma_tail_wait_us = static_cast<uint32_t>(blit_dma_tail_wait_time_total_us_ / frame_count);
  snapshot.dma_tail_wait_max_us = blit_dma_tail_wait_time_max_us_;
  snapshot.dma_timeout_count = blit_dma_timeout_count_;
  snapshot.blit_fail_busy = blit_fail_busy_count_;
  return snapshot;
}

uint16_t FxEngine::rgb565(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t red = static_cast<uint16_t>((r & 0xF8U) << 8U);
  const uint16_t green = static_cast<uint16_t>((g & 0xFCU) << 3U);
  const uint16_t blue = static_cast<uint16_t>(b >> 3U);
  return static_cast<uint16_t>(red | green | blue);
}

uint16_t FxEngine::addSat565(uint16_t a, uint16_t b) {
  const uint16_t ar = static_cast<uint16_t>((a >> 11U) & 31U);
  const uint16_t ag = static_cast<uint16_t>((a >> 5U) & 63U);
  const uint16_t ab = static_cast<uint16_t>(a & 31U);
  const uint16_t br = static_cast<uint16_t>((b >> 11U) & 31U);
  const uint16_t bg = static_cast<uint16_t>((b >> 5U) & 63U);
  const uint16_t bb = static_cast<uint16_t>(b & 31U);
  const uint16_t rr = static_cast<uint16_t>((ar + br > 31U) ? 31U : (ar + br));
  const uint16_t gg = static_cast<uint16_t>((ag + bg > 63U) ? 63U : (ag + bg));
  const uint16_t bb2 = static_cast<uint16_t>((ab + bb > 31U) ? 31U : (ab + bb));
  return static_cast<uint16_t>((rr << 11U) | (gg << 5U) | bb2);
}

uint16_t FxEngine::mul565_u8(uint16_t c, uint8_t v) {
  uint16_t r = static_cast<uint16_t>((c >> 11U) & 31U);
  uint16_t g = static_cast<uint16_t>((c >> 5U) & 63U);
  uint16_t b = static_cast<uint16_t>(c & 31U);
  r = static_cast<uint16_t>((r * v + 128U) >> 8U);
  g = static_cast<uint16_t>((g * v + 128U) >> 8U);
  b = static_cast<uint16_t>((b * v + 128U) >> 8U);
  return static_cast<uint16_t>((r << 11U) | (g << 5U) | b);
}

uint32_t FxEngine::nextRand() {
  rng_state_ ^= (rng_state_ << 13U);
  rng_state_ ^= (rng_state_ >> 17U);
  rng_state_ ^= (rng_state_ << 5U);
  return rng_state_;
}

void FxEngine::initTrigLutIfNeeded() {
  if (trig_ready_) {
    return;
  }
  for (uint16_t i = 0U; i < 256U; ++i) {
    const float angle = static_cast<float>(i) * (2.0f * 3.14159265358979323846f / 256.0f);
    sin_q15_[i] = static_cast<int16_t>(std::sin(angle) * 32767.0f);
  }
  trig_ready_ = true;
}

int16_t FxEngine::sinQ15(uint8_t a) const {
  return sin_q15_[a];
}

int16_t FxEngine::cosQ15(uint8_t a) const {
  return sin_q15_[static_cast<uint8_t>(a + 64U)];
}

void FxEngine::seedStars(uint16_t star_count) {
  star_count_ = clampValue<uint16_t>(star_count, 0U, kMaxStars);
  const int32_t width_q8 = static_cast<int32_t>(config_.sprite_width) << 8;
  const int32_t height_q8 = static_cast<int32_t>(config_.sprite_height) << 8;
  for (uint16_t i = 0U; i < star_count_; ++i) {
    Star& star = stars_[i];
    star.layer = static_cast<uint8_t>(i % 3U);
    const uint16_t min_speed = (star.layer == 0U) ? 20U : (star.layer == 1U) ? 70U : 150U;
    const uint16_t max_speed = (star.layer == 0U) ? 60U : (star.layer == 1U) ? 140U : 260U;
    const uint16_t speed_px_per_sec =
        static_cast<uint16_t>(min_speed + (nextRand() % static_cast<uint32_t>((max_speed - min_speed) + 1U)));
    star.speed_q8 = static_cast<uint16_t>((speed_px_per_sec << 8U) / 1000U);
    star.x_q8 = static_cast<int32_t>(nextRand() % static_cast<uint32_t>(width_q8));
    star.y_q8 = static_cast<int32_t>(nextRand() % static_cast<uint32_t>(height_q8));
  }
}

void FxEngine::updateStars(uint32_t dt_ms) {
  if (star_count_ == 0U) {
    return;
  }
  const int32_t width_q8 = static_cast<int32_t>(config_.sprite_width) << 8;
  const int32_t height_q8 = static_cast<int32_t>(config_.sprite_height) << 8;
  for (uint16_t i = 0U; i < star_count_; ++i) {
    Star& star = stars_[i];
    const int32_t delta = (static_cast<int32_t>(star.speed_q8) * static_cast<int32_t>(dt_ms));
    star.x_q8 -= delta;
    if (star.x_q8 < 0) {
      star.x_q8 = width_q8 - 256;
      star.y_q8 = static_cast<int32_t>(nextRand() % static_cast<uint32_t>(height_q8));
    }
    if ((nextRand() & 0x0FU) == 0U) {
      const int16_t jitter = static_cast<int16_t>((nextRand() & 0x03U) - 1);
      const int32_t next_y = star.y_q8 + (static_cast<int32_t>(jitter) << 7);
      if (next_y > 0 && next_y < height_q8) {
        star.y_q8 = next_y;
      }
    }
  }
}

void FxEngine::drawPixel(int16_t x, int16_t y, uint16_t color565) {
  if (sprite_pixels_ == nullptr || x < 0 || y < 0) {
    return;
  }
  if (x >= static_cast<int16_t>(config_.sprite_width) || y >= static_cast<int16_t>(config_.sprite_height)) {
    return;
  }
  const size_t index =
      static_cast<size_t>(y) * static_cast<size_t>(config_.sprite_width) + static_cast<size_t>(x);
  if (index >= sprite_pixel_count_) {
    return;
  }
  sprite_pixels_[index] = color565;
}

void FxEngine::addPixel(int16_t x, int16_t y, uint16_t color565) {
  if (sprite_pixels_ == nullptr || x < 0 || y < 0) {
    return;
  }
  if (x >= static_cast<int16_t>(config_.sprite_width) || y >= static_cast<int16_t>(config_.sprite_height)) {
    return;
  }
  const size_t index =
      static_cast<size_t>(y) * static_cast<size_t>(config_.sprite_width) + static_cast<size_t>(x);
  if (index >= sprite_pixel_count_) {
    return;
  }
  sprite_pixels_[index] = addSat565(sprite_pixels_[index], color565);
}

void FxEngine::fillSprite(uint16_t color565) {
  if (sprite_pixels_ == nullptr || sprite_pixel_count_ == 0U) {
    return;
  }
  const uint32_t packed = static_cast<uint32_t>(color565) | (static_cast<uint32_t>(color565) << 16U);
  uint16_t* dst16 = sprite_pixels_;
  size_t count = sprite_pixel_count_;
  if ((reinterpret_cast<uintptr_t>(dst16) & 0x3U) != 0U && count > 0U) {
    *dst16++ = color565;
    --count;
  }
  uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst16);
  const size_t count2 = count / 2U;
  for (size_t i = 0U; i < count2; ++i) {
    dst32[i] = packed;
  }
  if ((count & 1U) != 0U) {
    uint16_t* tail = reinterpret_cast<uint16_t*>(dst32 + count2);
    *tail = color565;
  }
}

void FxEngine::initModeState() {
  initTrigLutIfNeeded();
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;

  if (mode_ == FxMode::kStarfield3D) {
    const uint32_t area = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
    star3d_count_ = static_cast<uint16_t>(clampValue<uint32_t>(area / 50U, 220U, kMaxStars3D));
    for (uint16_t i = 0U; i < star3d_count_; ++i) {
      Star3D& star = stars3d_[i];
      star.x = static_cast<int16_t>(static_cast<int32_t>(nextRand() & 511U) - 256);
      star.y = static_cast<int16_t>(static_cast<int32_t>((nextRand() >> 9U) & 511U) - 256);
      star.z = static_cast<uint16_t>(128U + (nextRand() % 896U));
    }
  }

  if (mode_ == FxMode::kDotSphere3D) {
    const uint16_t base = rgb565(40U, 80U, 240U);
    const uint16_t high = rgb565(255U, 255U, 255U);
    for (uint16_t i = 0U; i < 256U; ++i) {
      const uint8_t diffuse = static_cast<uint8_t>(i);
      uint8_t spec = (i > 220U) ? static_cast<uint8_t>((i - 220U) * 7U) : 0U;
      if (spec > 255U) {
        spec = 255U;
      }
      dot_shade_lut_[i] = addSat565(mul565_u8(base, diffuse), mul565_u8(high, spec));
    }
    dot_count_ = static_cast<uint16_t>(
        clampValue<uint32_t>((static_cast<uint32_t>(width) * static_cast<uint32_t>(height)) / 75U,
                             140U,
                             kMaxDots));
    const uint16_t min_dim = (width < height) ? width : height;
    dot_radius_px_ = static_cast<uint8_t>(clampValue<uint16_t>(static_cast<uint16_t>(min_dim / 2U) - 8U, 24U, 72U));

    const uint32_t saved_rng = rng_state_;
    rng_state_ = 0xBADC0FFEUL ^ static_cast<uint32_t>(width) ^ (static_cast<uint32_t>(height) << 16U);
    for (uint16_t i = 0U; i < dot_count_; ++i) {
      const uint8_t a = static_cast<uint8_t>(nextRand() & 0xFFU);
      const uint8_t b = static_cast<uint8_t>((nextRand() >> 8U) & 0xFFU);
      const int16_t ca = cosQ15(a);
      const int16_t sa = sinQ15(a);
      const int16_t cb = cosQ15(b);
      const int16_t sb = sinQ15(b);
      dots_[i].x = static_cast<int16_t>((static_cast<int32_t>(ca) * static_cast<int32_t>(cb) >> 15) >> 8);
      dots_[i].y = static_cast<int16_t>(static_cast<int32_t>(sb) >> 8);
      dots_[i].z = static_cast<int16_t>((static_cast<int32_t>(sa) * static_cast<int32_t>(cb) >> 15) >> 8);
    }
    rng_state_ = saved_rng;
  }

  if (mode_ == FxMode::kVoxelLandscape) {
    for (uint16_t i = 0U; i < 256U; ++i) {
      const int16_t s1 = sinQ15(static_cast<uint8_t>(i));
      const int16_t s2 = sinQ15(static_cast<uint8_t>(i * 3U));
      int height_v = static_cast<int>(s1 / 512) + static_cast<int>(s2 / 1024) + 128;
      height_v = clampValue<int>(height_v, 0, 255);
      voxel_height_[i] = static_cast<uint8_t>(height_v);
      const uint16_t ground = rgb565(30U, 220U, 80U);
      voxel_pal_[i] = mul565_u8(ground, static_cast<uint8_t>(i));
    }
    voxel_max_dist_ = 96U;
    constexpr int kZOffset = 8;
    constexpr int kTerrainScale = 70;
    for (uint16_t z = 1U; z <= voxel_max_dist_ && z < (sizeof(voxel_proj_q8_) / sizeof(voxel_proj_q8_[0])); ++z) {
      const int denom = static_cast<int>(z) + kZOffset;
      int value = (kTerrainScale * 256) / denom;
      value = clampValue<int>(value, 0, 65535);
      voxel_proj_q8_[z] = static_cast<uint16_t>(value);
    }
  }

  if (mode_ == FxMode::kRayCorridor) {
    for (uint16_t y = 0U; y < kRayTexSize; ++y) {
      for (uint16_t x = 0U; x < kRayTexSize; ++x) {
        const bool checker = (((x >> 3U) ^ (y >> 3U)) & 1U) != 0U;
        int c = checker ? 190 : 70;
        if ((y & 7U) == 0U) {
          c = 40;
        }
        ray_tex_[static_cast<size_t>(y) * kRayTexSize + x] =
            rgb565(static_cast<uint8_t>(c), static_cast<uint8_t>(c / 2), static_cast<uint8_t>(c / 3));
      }
    }
    const int16_t half = static_cast<int16_t>(width / 2U);
    for (uint16_t x = 0U; x < width && x < kMaxSpriteWidth; ++x) {
      const int16_t dx = static_cast<int16_t>(x) - half;
      int16_t off = (half > 0) ? static_cast<int16_t>((dx * 24) / half) : 0;
      off = clampValue<int16_t>(off, -64, 64);
      ray_col_off_[x] = static_cast<int8_t>(off);
    }
    const int horizon = static_cast<int>(height / 2U);
    for (uint16_t y = 0U; y < height && y < kMaxSpriteHeight; ++y) {
      const int dy = static_cast<int>(y) - horizon;
      if (dy <= 0) {
        ray_floor_scale_q12_[y] = 0U;
        continue;
      }
      uint32_t value = (64UL << 12U) / static_cast<uint32_t>(dy);
      if (value > 65535UL) {
        value = 65535UL;
      }
      ray_floor_scale_q12_[y] = static_cast<uint16_t>(value);
    }
  }
}

void FxEngine::renderMode3D(uint32_t now_ms) {
  switch (mode_) {
    case FxMode::kStarfield3D:
      renderStarfield3D(now_ms);
      break;
    case FxMode::kDotSphere3D:
      renderDotSphere3D(now_ms);
      break;
    case FxMode::kVoxelLandscape:
      renderVoxelLandscape(now_ms);
      break;
    case FxMode::kRayCorridor:
      renderRayCorridor(now_ms);
      break;
    case FxMode::kClassic:
    default:
      break;
  }
}

void FxEngine::renderStarfield3D(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  fillSprite(rgb565(2U, 4U, 10U));
  const uint8_t angle = static_cast<uint8_t>(now_ms >> 4U);
  const int16_t cs = cosQ15(angle);
  const int16_t sn = sinQ15(angle);
  const int fov = static_cast<int>((width < height ? width : height) + 24U);
  const uint16_t z_min = 32U;
  const int dz = static_cast<int>(10U + ((now_ms >> 6U) & 7U));
  const int cx = static_cast<int>(width / 2U);
  const int cy = static_cast<int>(height / 2U);
  const uint16_t base = rgb565(240U, 248U, 255U);

  for (uint16_t i = 0U; i < star3d_count_; ++i) {
    Star3D& star = stars3d_[i];
    const uint16_t z_prev = star.z;
    int z_next = static_cast<int>(star.z) - dz;
    if (z_next < static_cast<int>(z_min)) {
      star.x = static_cast<int16_t>(static_cast<int32_t>(nextRand() & 511U) - 256);
      star.y = static_cast<int16_t>(static_cast<int32_t>((nextRand() >> 9U) & 511U) - 256);
      star.z = static_cast<uint16_t>(256U + (nextRand() % 768U));
      continue;
    }
    star.z = static_cast<uint16_t>(z_next);

    const int xr = (static_cast<int32_t>(star.x) * cs - static_cast<int32_t>(star.y) * sn) >> 15;
    const int yr = (static_cast<int32_t>(star.x) * sn + static_cast<int32_t>(star.y) * cs) >> 15;
    const int sx = cx + (xr * fov) / static_cast<int>(star.z);
    const int sy = cy + (yr * fov) / static_cast<int>(star.z);
    const int sx0 = cx + (xr * fov) / static_cast<int>(z_prev);
    const int sy0 = cy + (yr * fov) / static_cast<int>(z_prev);
    if (sx < 0 || sy < 0 || sx >= static_cast<int>(width) || sy >= static_cast<int>(height)) {
      continue;
    }

    uint8_t brightness = static_cast<uint8_t>(255U - (star.z >> 2U));
    if (brightness < 40U) {
      brightness = 40U;
    }
    drawPixel(static_cast<int16_t>(sx), static_cast<int16_t>(sy), mul565_u8(base, brightness));

    const int dx = sx - sx0;
    const int dy = sy - sy0;
    int steps = std::max(std::abs(dx), std::abs(dy));
    steps = clampValue<int>(steps, 0, 10);
    for (int s = 1; s <= steps; ++s) {
      const int x = sx0 + (dx * s) / steps;
      const int y = sy0 + (dy * s) / steps;
      if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) {
        continue;
      }
      const uint8_t fade = static_cast<uint8_t>((brightness * (steps - s)) / (steps + 1));
      addPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), mul565_u8(base, fade));
    }
  }
  stats_.stars = star3d_count_;
  stats_.object_count = star3d_count_;
}

void FxEngine::renderDotSphere3D(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  fillSprite(0x0000U);
  const uint8_t ax = static_cast<uint8_t>(now_ms >> 4U);
  const uint8_t ay = static_cast<uint8_t>(now_ms >> 5U);
  const uint8_t az = static_cast<uint8_t>(now_ms >> 6U);
  const int16_t cx = cosQ15(ax);
  const int16_t sx = sinQ15(ax);
  const int16_t cy = cosQ15(ay);
  const int16_t sy = sinQ15(ay);
  const int16_t cz = cosQ15(az);
  const int16_t sz = sinQ15(az);
  const int16_t lx = static_cast<int16_t>(0.30f * 32767.0f);
  const int16_t ly = static_cast<int16_t>(-0.20f * 32767.0f);
  const int16_t lz = static_cast<int16_t>(0.93f * 32767.0f);
  const int center_x = static_cast<int>(width / 2U);
  const int center_y = static_cast<int>(height / 2U);
  const int fov = static_cast<int>(width < height ? width : height);
  const int radius = static_cast<int>(dot_radius_px_);
  const int blob_r = static_cast<int>(dot_blob_radius_);

  for (uint16_t i = 0U; i < dot_count_; ++i) {
    const DotPt& dot = dots_[i];
    int32_t x = (static_cast<int32_t>(dot.x) * radius) >> 7;
    int32_t y = (static_cast<int32_t>(dot.y) * radius) >> 7;
    int32_t z = (static_cast<int32_t>(dot.z) * radius) >> 7;

    const int32_t y1 = (y * cx - z * sx) >> 15;
    const int32_t z1 = (y * sx + z * cx) >> 15;
    const int32_t x2 = (x * cy + z1 * sy) >> 15;
    const int32_t z2 = (-x * sy + z1 * cy) >> 15;
    const int32_t x3 = (x2 * cz - y1 * sz) >> 15;
    const int32_t y3 = (x2 * sz + y1 * cz) >> 15;

    const int32_t depth = z2 + (radius * 3);
    if (depth <= 1) {
      continue;
    }

    const int sxp = center_x + static_cast<int>((x3 * fov) / depth);
    const int syp = center_y + static_cast<int>((y3 * fov) / depth);
    if (sxp < 0 || syp < 0 || sxp >= static_cast<int>(width) || syp >= static_cast<int>(height)) {
      continue;
    }

    const int32_t nd = (x3 * lx + y3 * ly + z2 * lz) >> 15;
    int32_t ndotl = ((nd * 128) / (radius == 0 ? 1 : radius)) + 128;
    ndotl = clampValue<int32_t>(ndotl, 0, 255);
    const uint16_t base = dot_shade_lut_[ndotl];

    for (int yy = -blob_r; yy <= blob_r; ++yy) {
      for (int xx = -blob_r; xx <= blob_r; ++xx) {
        if ((xx * xx) + (yy * yy) > (blob_r * blob_r)) {
          continue;
        }
        int atten = 255 - ((xx * xx + yy * yy) * 28);
        atten = clampValue<int>(atten, 0, 255);
        addPixel(static_cast<int16_t>(sxp + xx),
                 static_cast<int16_t>(syp + yy),
                 mul565_u8(base, static_cast<uint8_t>(atten)));
      }
    }
  }
  stats_.object_count = dot_count_;
  stats_.stars = 0U;
}

void FxEngine::renderVoxelLandscape(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  for (uint16_t y = 0U; y < height; ++y) {
    const uint8_t t = static_cast<uint8_t>((static_cast<uint32_t>(y) * 255U) / (height == 0U ? 1U : height));
    const uint8_t r = static_cast<uint8_t>((8U * (255U - t)) >> 8U);
    const uint8_t g = static_cast<uint8_t>((12U * (255U - t)) >> 8U);
    const uint8_t b = static_cast<uint8_t>((32U * (255U - t)) >> 8U);
    const uint16_t color = rgb565(r, g, b);
    const size_t row = static_cast<size_t>(y) * width;
    for (uint16_t x = 0U; x < width; ++x) {
      sprite_pixels_[row + x] = color;
    }
  }

  const int horizon = static_cast<int>(height / 2U);
  const uint8_t angle = static_cast<uint8_t>(now_ms >> 6U);
  const uint16_t cam_x = static_cast<uint16_t>((now_ms >> 5U) & 255U);
  const uint16_t cam_y = static_cast<uint16_t>((now_ms >> 6U) & 255U);
  const int16_t half = static_cast<int16_t>(width / 2U);

  for (uint16_t x = 0U; x < width; ++x) {
    const int16_t dx = static_cast<int16_t>(x) - half;
    const int16_t off = (half > 0) ? static_cast<int16_t>((dx * 24) / half) : 0;
    const uint8_t ray_angle = static_cast<uint8_t>(angle + static_cast<uint8_t>(off));
    const int16_t dir_x = cosQ15(ray_angle);
    const int16_t dir_y = sinQ15(ray_angle);
    int max_y = static_cast<int>(height) - 1;
    for (uint8_t z = 1U; z <= voxel_max_dist_; ++z) {
      const int map_x = (static_cast<int>(cam_x) + ((dir_x * static_cast<int>(z)) >> 15)) & 255;
      const int map_y = (static_cast<int>(cam_y) + ((dir_y * static_cast<int>(z)) >> 15)) & 255;
      const uint8_t hh = voxel_height_[static_cast<uint8_t>((map_x + (map_y * 3)) & 255)];
      const uint16_t proj = voxel_proj_q8_[z];
      int y = horizon - static_cast<int>((hh * proj) >> 8U);
      if (y < 0) {
        y = 0;
      }
      if (y > max_y) {
        continue;
      }
      const uint8_t shade = static_cast<uint8_t>((z * 3U < 255U) ? (255U - (z * 3U)) : 0U);
      const uint16_t color = voxel_pal_[shade];
      for (int yy = y; yy <= max_y; ++yy) {
        sprite_pixels_[static_cast<size_t>(yy) * width + x] = color;
      }
      max_y = y - 1;
      if (max_y < 0) {
        break;
      }
    }
  }
  stats_.object_count = 0U;
  stats_.stars = 0U;
}

void FxEngine::renderRayCorridor(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  fillSprite(rgb565(0U, 0U, 0U));
  const int horizon = static_cast<int>(height / 2U);
  const uint32_t zscroll = now_ms >> 3U;
  const uint8_t camera_angle = static_cast<uint8_t>(now_ms >> 6U);

  for (uint16_t x = 0U; x < width; ++x) {
    const int8_t off = ray_col_off_[x];
    const uint8_t ray_angle = static_cast<uint8_t>(camera_angle + static_cast<uint8_t>(off));
    const int16_t dir_x = sinQ15(ray_angle);
    const int16_t dir_z = cosQ15(ray_angle);
    const int16_t abs_dir_x = static_cast<int16_t>(std::abs(dir_x));
    if (abs_dir_x < 64) {
      for (int y = horizon; y < static_cast<int>(height); ++y) {
        const int dy = y - horizon;
        const uint8_t shade = static_cast<uint8_t>(120 + (dy * 2));
        sprite_pixels_[static_cast<size_t>(y) * width + x] = mul565_u8(rgb565(6U, 5U, 2U), shade);
      }
      continue;
    }

    const uint32_t t_q15 = (1UL << 30U) / static_cast<uint32_t>(abs_dir_x);
    const uint16_t corr = static_cast<uint16_t>(cosQ15(static_cast<uint8_t>(off)));
    uint32_t dist_q15 = static_cast<uint32_t>((static_cast<uint64_t>(t_q15) * corr) >> 15U);
    if (dist_q15 == 0U) {
      dist_q15 = 1U;
    }
    int slice = static_cast<int>((static_cast<uint32_t>(height) << 15U) / dist_q15);
    slice = clampValue<int>(slice, 1, static_cast<int>(height));
    int y0 = horizon - (slice / 2);
    int y1 = y0 + slice - 1;
    y0 = clampValue<int>(y0, 0, static_cast<int>(height) - 1);
    y1 = clampValue<int>(y1, 0, static_cast<int>(height) - 1);

    const int32_t zhit_q15 = static_cast<int32_t>((static_cast<int64_t>(dir_z) * static_cast<int64_t>(t_q15)) >> 15);
    int u = static_cast<int>(((zhit_q15 >> 9) + static_cast<int32_t>(zscroll)) & 63);
    if (dir_x < 0) {
      u ^= 63;
    }
    int shade = 255 - static_cast<int>(dist_q15 >> 9U);
    shade = clampValue<int>(shade, 0, 255);
    for (int y = y0; y <= y1; ++y) {
      const int v = ((y - y0) * 64) / (slice == 0 ? 1 : slice);
      uint16_t color = ray_tex_[static_cast<size_t>(v & 63) * 64U + static_cast<size_t>(u & 63)];
      color = mul565_u8(color, static_cast<uint8_t>(shade));
      sprite_pixels_[static_cast<size_t>(y) * width + x] = color;
    }
    for (int y = y1 + 1; y < static_cast<int>(height); ++y) {
      const uint16_t k = ray_floor_scale_q12_[y];
      if (k == 0U) {
        continue;
      }
      const int32_t uu_q12 = static_cast<int32_t>((static_cast<int64_t>(dir_x) * k) >> 15);
      const int32_t vv_q12 = static_cast<int32_t>((static_cast<int64_t>(dir_z) * k) >> 15);
      const int uf = static_cast<int>(((uu_q12 >> 6) + static_cast<int32_t>(zscroll)) & 63);
      const int vf = static_cast<int>(((vv_q12 >> 6) + static_cast<int32_t>(zscroll >> 1U)) & 63);
      uint16_t color = ray_tex_[static_cast<size_t>(vf & 63) * 64U + static_cast<size_t>(uf & 63)];
      const int dy = y - horizon;
      int fade = 255 - (dy * 2);
      fade = clampValue<int>(fade, 0, 255);
      color = mul565_u8(color, static_cast<uint8_t>(fade));
      sprite_pixels_[static_cast<size_t>(y) * width + x] = color;
    }
  }
  stats_.object_count = 0U;
  stats_.stars = 0U;
}

void FxEngine::renderBackgroundPlasma(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  const uint8_t p1 = static_cast<uint8_t>(now_ms / 22U);
  const uint8_t p2 = static_cast<uint8_t>(85U + (now_ms / 30U));
  const uint8_t p3 = static_cast<uint8_t>(170U + (now_ms / 40U));
  for (uint16_t y = 0U; y < height; ++y) {
    const size_t row_offset = static_cast<size_t>(y) * width;
    for (uint16_t x = 0U; x < width; ++x) {
      const uint8_t ax = static_cast<uint8_t>(x * 3U);
      const uint8_t ay = static_cast<uint8_t>(y * 4U);
      int sum = 0;
      sum += fx_sin8(static_cast<uint8_t>(ax + p1));
      sum += fx_sin8(static_cast<uint8_t>(ay + p2));
      sum += fx_sin8(static_cast<uint8_t>(ax + ay + p3));
      int v = (sum + 381) * 64 / ((381 * 2) + 1);
      v = clampValue<int>(v, 0, 63);
      sprite_pixels_[row_offset + x] = fx_palette_plasma565(static_cast<uint8_t>(v));
    }
  }
}

void FxEngine::renderBackgroundStarfield(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  const uint16_t bg = rgb565(2U, 3U, 8U);
  for (size_t i = 0U; i < sprite_pixel_count_; ++i) {
    sprite_pixels_[i] = bg;
  }

  for (uint16_t i = 0U; i < star_count_; ++i) {
    const Star& star = stars_[i];
    int16_t x = static_cast<int16_t>(star.x_q8 >> 8U);
    int16_t y = static_cast<int16_t>(star.y_q8 >> 8U);
    const int16_t drift = static_cast<int16_t>(fx_sin8(static_cast<uint8_t>((now_ms >> 4U) + (i * 5U))) /
                                               ((star.layer == 2U) ? 42 : ((star.layer == 1U) ? 64 : 96)));
    y = static_cast<int16_t>(y + drift);
    x = static_cast<int16_t>(wrapValue<int16_t>(x, static_cast<int16_t>(width)));
    y = static_cast<int16_t>(wrapValue<int16_t>(y, static_cast<int16_t>(height)));
    const uint8_t b = (star.layer == 2U) ? 255U : (star.layer == 1U) ? 210U : 170U;
    drawPixel(x, y, rgb565(b, b, b));
    if (star.layer >= 1U) {
      drawPixel(static_cast<int16_t>(x + 1), y, rgb565(b, b, b));
    }
  }
}

void FxEngine::renderBackgroundRasterBars(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  const uint16_t bg = rgb565(4U, 8U, 16U);
  for (size_t i = 0U; i < sprite_pixel_count_; ++i) {
    sprite_pixels_[i] = bg;
  }

  const uint8_t ph = static_cast<uint8_t>(now_ms / 8U);
  const uint8_t bars = 5U;
  const int16_t base = static_cast<int16_t>(height / 2U + (fx_sin8(ph) / 6));
  const int16_t thick = 16;
  for (uint8_t b = 0U; b < bars; ++b) {
    int16_t cy = static_cast<int16_t>(base + (fx_sin8(static_cast<uint8_t>(ph + b * 33U)) / 6) +
                                      static_cast<int16_t>(b * 10U) - static_cast<int16_t>(bars * 5U));
    int16_t y0 = static_cast<int16_t>(cy - (thick / 2));
    int16_t y1 = static_cast<int16_t>(cy + (thick / 2));
    y0 = clampValue<int16_t>(y0, 0, static_cast<int16_t>(height - 1U));
    y1 = clampValue<int16_t>(y1, 0, static_cast<int16_t>(height - 1U));
    for (int16_t y = y0; y <= y1; ++y) {
      const uint8_t u = static_cast<uint8_t>(((y - y0) * 255) / ((thick == 0) ? 1 : thick));
      const uint16_t color = fx_palette_copper565(u);
      const size_t row_offset = static_cast<size_t>(y) * width;
      for (uint16_t x = 0U; x < width; ++x) {
        sprite_pixels_[row_offset + x] = fx_rgb565_add(sprite_pixels_[row_offset + x], color);
      }
    }
  }
}

void FxEngine::renderBackground(uint32_t now_ms, FxScenePhase phase) {
  (void)phase;
  switch (bg_mode_) {
    case BgMode::kPlasma:
      renderBackgroundPlasma(now_ms);
      break;
    case BgMode::kStarfield:
      renderBackgroundStarfield(now_ms);
      break;
    case BgMode::kRasterBars:
      renderBackgroundRasterBars(now_ms);
      break;
  }
}

void FxEngine::renderMidShadeBobs(uint32_t now_ms) {
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  const int16_t cx = static_cast<int16_t>(width / 2U);
  const int16_t cy = static_cast<int16_t>(height / 2U);
  const int16_t ax = static_cast<int16_t>(width / 2U - 12U);
  const int16_t ay = static_cast<int16_t>(height / 2U - 20U);
  const uint8_t radius = (quality_level_ >= 2U) ? 9U : 7U;
  const uint8_t bob_count = static_cast<uint8_t>(kShadeBobCountDefault + ((quality_level_ >= 3U) ? 8U : 0U));
  const uint8_t phase = static_cast<uint8_t>(now_ms / 6U);

  for (uint8_t i = 0U; i < bob_count; ++i) {
    const uint8_t p1 = static_cast<uint8_t>(phase + i * 17U);
    const uint8_t p2 = static_cast<uint8_t>(phase * 2U + i * 29U);
    const int16_t x = static_cast<int16_t>(cx + (fx_sin8(p1) * ax) / 127);
    const int16_t y = static_cast<int16_t>(cy + (fx_cos8(p2) * ay) / 127);
    const uint16_t base = fx_palette_plasma565(static_cast<uint8_t>((phase + (i * 9U)) & 0x3FU));

    for (int16_t dy = -radius; dy <= static_cast<int16_t>(radius); ++dy) {
      const int16_t yy = static_cast<int16_t>(y + dy);
      if (yy < 0 || yy >= static_cast<int16_t>(height)) {
        continue;
      }
      for (int16_t dx = -radius; dx <= static_cast<int16_t>(radius); ++dx) {
        const int16_t xx = static_cast<int16_t>(x + dx);
        if (xx < 0 || xx >= static_cast<int16_t>(width)) {
          continue;
        }
        const int16_t dist2 = static_cast<int16_t>(dx * dx + dy * dy);
        if (dist2 > static_cast<int16_t>(radius * radius)) {
          continue;
        }
        const uint8_t alpha = static_cast<uint8_t>(255U - ((dist2 * 255U) / (radius * radius)));
        const size_t idx = static_cast<size_t>(yy) * width + static_cast<size_t>(xx);
        sprite_pixels_[idx] = fx_rgb565_add(sprite_pixels_[idx], fx_rgb565_scale(base, alpha));
      }
    }
  }
}

void FxEngine::renderMidRotoZoom(uint32_t now_ms) {
  if (roto_texture_ == nullptr) {
    return;
  }
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  const int16_t cx = static_cast<int16_t>(width / 2U);
  const int16_t cy = static_cast<int16_t>(height / 2U);
  const uint8_t phase = static_cast<uint8_t>((now_ms / 10U) & 0xFFU);
  const int16_t s = static_cast<int16_t>(fx_sin8(phase));
  const int16_t c = static_cast<int16_t>(fx_cos8(phase));
  const int16_t pulse = static_cast<int16_t>(fx_sin8(static_cast<uint8_t>(phase * 2U)) >> 1U);
  const int16_t zoom_q8 = static_cast<int16_t>(256 + pulse);

  for (uint16_t y = 0U; y < height; ++y) {
    const int16_t dy = static_cast<int16_t>(y - cy);
    const size_t row_offset = static_cast<size_t>(y) * width;
    for (uint16_t x = 0U; x < width; ++x) {
      const int16_t dx = static_cast<int16_t>(x - cx);
      int32_t u = (c * dx - s * dy);
      int32_t v = (s * dx + c * dy);
      u = (u * zoom_q8) >> 8;
      v = (v * zoom_q8) >> 8;
      const uint16_t tx =
          static_cast<uint16_t>((u + static_cast<int32_t>(kRotoTexSize / 2U)) & (kRotoTexSize - 1U));
      const uint16_t ty =
          static_cast<uint16_t>((v + static_cast<int32_t>(kRotoTexSize / 2U)) & (kRotoTexSize - 1U));
      const uint16_t tex = roto_texture_[static_cast<size_t>(ty) * kRotoTexSize + static_cast<size_t>(tx)];
      sprite_pixels_[row_offset + x] = fx_rgb565_add(sprite_pixels_[row_offset + x], fx_rgb565_scale(tex, 180U));
    }
  }
}

void FxEngine::stepFireworks(uint32_t dt_ms) {
  if (sync_.on_beat || sync_.on_bar) {
    const uint8_t target_spawn = sync_.on_bar ? static_cast<uint8_t>(kFireworkSpawnPerBeat + 8U)
                                              : kFireworkSpawnPerBeat;
    const int16_t cx = static_cast<int16_t>(nextRand() % config_.sprite_width);
    const int16_t cy = static_cast<int16_t>(nextRand() % config_.sprite_height);
    uint8_t spawned = 0U;
    for (uint16_t i = 0U; i < kMaxFireworkParticles && spawned < target_spawn; ++i) {
      FireworkParticle& p = fireworks_[i];
      if (p.life != 0U) {
        continue;
      }
      const uint32_t r = nextRand();
      const float angle = static_cast<float>(r & 1023U) * (2.0f * 3.1415926f / 1024.0f);
      const float speed = 0.8f + (static_cast<float>((r >> 10U) & 255U) / 255.0f) * 1.8f;
      p.x_q8 = static_cast<int32_t>(cx) << 8;
      p.y_q8 = static_cast<int32_t>(cy) << 8;
      p.vx_q8 = static_cast<int32_t>(std::lround(std::cos(angle) * speed * 256.0f));
      p.vy_q8 = static_cast<int32_t>(std::lround(std::sin(angle) * speed * 256.0f));
      p.life = 255U;
      p.color6 = static_cast<uint8_t>((r >> 18U) & 0x3FU);
      ++spawned;
    }
  }

  uint16_t live = 0U;
  for (uint16_t i = 0U; i < kMaxFireworkParticles; ++i) {
    FireworkParticle& p = fireworks_[i];
    if (p.life == 0U) {
      continue;
    }
    p.vy_q8 += static_cast<int32_t>((30 * static_cast<int32_t>(dt_ms)) / 16);
    p.x_q8 += static_cast<int32_t>((p.vx_q8 * static_cast<int32_t>(dt_ms)) / 16);
    p.y_q8 += static_cast<int32_t>((p.vy_q8 * static_cast<int32_t>(dt_ms)) / 16);
    p.life = (p.life > kFireworkDecayPerFrame) ? static_cast<uint8_t>(p.life - kFireworkDecayPerFrame) : 0U;
    const int16_t x = static_cast<int16_t>(p.x_q8 >> 8);
    const int16_t y = static_cast<int16_t>(p.y_q8 >> 8);
    if (x < 0 || y < 0 || x >= static_cast<int16_t>(config_.sprite_width) ||
        y >= static_cast<int16_t>(config_.sprite_height)) {
      p.life = 0U;
    }
    if (p.life != 0U) {
      ++live;
    }
  }
  firework_live_count_ = live;
}

void FxEngine::renderMidFireworks(uint32_t now_ms, uint32_t dt_ms) {
  (void)now_ms;
  stepFireworks(dt_ms);
  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  for (uint16_t i = 0U; i < kMaxFireworkParticles; ++i) {
    const FireworkParticle& p = fireworks_[i];
    if (p.life == 0U) {
      continue;
    }
    const int16_t x = static_cast<int16_t>(p.x_q8 >> 8);
    const int16_t y = static_cast<int16_t>(p.y_q8 >> 8);
    const uint16_t col = fx_rgb565_scale(fx_palette_plasma565(p.color6), p.life);
    for (int16_t dy = -1; dy <= 1; ++dy) {
      const int16_t yy = static_cast<int16_t>(y + dy);
      if (yy < 0 || yy >= static_cast<int16_t>(height)) {
        continue;
      }
      for (int16_t dx = -1; dx <= 1; ++dx) {
        const int16_t xx = static_cast<int16_t>(x + dx);
        if (xx < 0 || xx >= static_cast<int16_t>(width)) {
          continue;
        }
        const size_t idx = static_cast<size_t>(yy) * width + static_cast<size_t>(xx);
        sprite_pixels_[idx] = fx_rgb565_add(sprite_pixels_[idx], col);
      }
    }
  }
}

void FxEngine::stepBoing(uint32_t dt_ms) {
  const float dt = static_cast<float>(dt_ms) / 1000.0f;
  boing_phase_ = static_cast<uint8_t>(boing_phase_ + (dt_ms / 11U));
  boing_vy_ += 1200.0f * dt;
  boing_x_ += boing_vx_ * dt;
  boing_y_ += boing_vy_ * dt;

  const float radius = static_cast<float>(kBoingRadiusLrDefault);
  if (boing_y_ + radius > boing_floor_y_) {
    boing_y_ = boing_floor_y_ - radius;
    boing_vy_ = -boing_vy_ * 0.82f;
  }
  if (boing_x_ < radius) {
    boing_x_ = radius;
    boing_vx_ = -boing_vx_;
  }
  const float right = static_cast<float>(config_.sprite_width - 1U) - radius;
  if (boing_x_ > right) {
    boing_x_ = right;
    boing_vx_ = -boing_vx_;
  }
}

void FxEngine::renderMidBoingball(uint32_t now_ms, uint32_t dt_ms) {
  (void)now_ms;
  if (!boing_ready_ || boing_mask_ == nullptr || boing_u_ == nullptr || boing_v_ == nullptr ||
      boing_shade_ == nullptr) {
    return;
  }

  stepBoing(dt_ms);
  const int16_t width = static_cast<int16_t>(config_.sprite_width);
  const int16_t height = static_cast<int16_t>(config_.sprite_height);
  const int16_t radius = clampValue<int16_t>(static_cast<int16_t>(kBoingRadiusLrDefault), 22,
                                             static_cast<int16_t>((height < width ? height : width) / 2U));
  const int16_t xc = static_cast<int16_t>(boing_x_);
  const int16_t yc = static_cast<int16_t>(boing_y_);

  for (int16_t y = static_cast<int16_t>(yc - radius); y <= static_cast<int16_t>(yc + radius); ++y) {
    if (y < 0 || y >= height) {
      continue;
    }
    uint16_t* row_ptr = sprite_pixels_ + static_cast<size_t>(y) * config_.sprite_width;
    const int16_t dy = static_cast<int16_t>(y - static_cast<int16_t>(boing_floor_y_));
    const int16_t shadow_ry = static_cast<int16_t>((radius * 22) / 100);
    if (shadow_ry > 0 && std::abs(dy) <= shadow_ry) {
      const float n = 1.0f - (static_cast<float>(dy * dy) /
                              static_cast<float>(shadow_ry * shadow_ry + 1));
      if (n > 0.0f) {
        const int16_t shadow_rx = static_cast<int16_t>(radius * 0.9f * std::sqrt(n));
        int16_t x0 = static_cast<int16_t>(xc - shadow_rx);
        int16_t x1 = static_cast<int16_t>(xc + shadow_rx + 1);
        if (x0 < 0) {
          x0 = 0;
        }
        if (x1 > width) {
          x1 = width;
        }
        boing_shadow_darken_span_half_rgb565(row_ptr, x0, x1);
      }
    }

    const int16_t ly = static_cast<int16_t>(y - yc);
    const uint16_t ay = static_cast<uint16_t>((static_cast<int32_t>(ly + radius) * kBoingGridSize) /
                                              static_cast<int32_t>(2 * radius));
    if (ay >= kBoingGridSize) {
      continue;
    }
    for (int16_t x = static_cast<int16_t>(xc - radius); x <= static_cast<int16_t>(xc + radius); ++x) {
      if (x < 0 || x >= width) {
        continue;
      }
      const int16_t lx = static_cast<int16_t>(x - xc);
      const uint16_t ax = static_cast<uint16_t>((static_cast<int32_t>(lx + radius) * kBoingGridSize) /
                                                static_cast<int32_t>(2 * radius));
      if (ax >= kBoingGridSize) {
        continue;
      }
      const size_t asset_idx = static_cast<size_t>(ay) * kBoingGridSize + static_cast<size_t>(ax);
      if (boing_mask_[asset_idx] == 0U) {
        continue;
      }
      const uint8_t u = static_cast<uint8_t>(boing_u_[asset_idx] + boing_phase_);
      const uint8_t v = boing_v_[asset_idx];
      const bool checker_red = (((u >> kBoingCheckerShift) ^ (v >> kBoingCheckerShift)) & 1U) != 0U;
      const uint16_t color = selectBoingColor(boing_shade_[asset_idx], checker_red);
      row_ptr[x] = color;
    }
  }
}

void FxEngine::renderMid(uint32_t now_ms, uint32_t dt_ms, FxScenePhase phase) {
  (void)phase;
  switch (mid_mode_) {
    case MidMode::kNone:
      break;
    case MidMode::kShadeBobs:
      renderMidShadeBobs(now_ms);
      break;
    case MidMode::kRotoZoom:
      renderMidRotoZoom(now_ms);
      break;
    case MidMode::kFireworks:
      renderMidFireworks(now_ms, dt_ms);
      break;
    case MidMode::kBoingball:
      renderMidBoingball(now_ms, dt_ms);
      break;
  }
}

void FxEngine::drawChar6x8(int16_t x, int16_t y_top, char c, uint16_t color565, uint16_t shadow565) {
  fx_font_t font = FX_FONT_BASIC;
  switch (scroll_font_) {
    case FxScrollFont::kBold:
      font = FX_FONT_BOLD;
      break;
    case FxScrollFont::kOutline:
      font = FX_FONT_OUTLINE;
      break;
    case FxScrollFont::kItalic:
      font = FX_FONT_ITALIC;
      break;
    case FxScrollFont::kBasic:
    default:
      font = FX_FONT_BASIC;
      break;
  }
  unsigned char glyph = static_cast<unsigned char>(c);
  if (glyph >= static_cast<unsigned char>('a') && glyph <= static_cast<unsigned char>('z')) {
    glyph = static_cast<unsigned char>(glyph - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
  }
  if (glyph < 32U || glyph > 95U) {
    glyph = static_cast<unsigned char>(' ');
  }
  const uint8_t* rows = fx_font_get_rows(font, static_cast<char>(glyph));
  if (rows == nullptr) {
    return;
  }
  for (uint8_t row = 0U; row < kScrollerGlyphHeight; ++row) {
    const int16_t y = static_cast<int16_t>(y_top + row);
    if (y < 0 || y >= static_cast<int16_t>(config_.sprite_height)) {
      continue;
    }
    const uint8_t bits = rows[row];
    for (uint8_t col = 0U; col < 6U; ++col) {
      if ((bits & (1U << col)) == 0U) {
        continue;
      }
      drawPixel(static_cast<int16_t>(x + 1 + col), static_cast<int16_t>(y + 1), shadow565);
      drawPixel(static_cast<int16_t>(x + col), y, color565);
    }
  }
}

void FxEngine::renderScroller(uint32_t now_ms) {
  if (scroll_text_len_ == 0U || scroll_text_[0] == '\0') {
    return;
  }
  const int16_t width = static_cast<int16_t>(config_.sprite_width);
  const int16_t height = static_cast<int16_t>(config_.sprite_height);
  const int16_t base_y = scroller_centered_
                             ? static_cast<int16_t>(height / 2)
                             : static_cast<int16_t>(
                                   clampValue<int>(height - static_cast<int>(kScrollerBaseYOffset), 12, height - 18));
  const int16_t top = static_cast<int16_t>(base_y - kScrollerAmpPx - kSafeBandMarginTop - kSafeFeatherPx);
  const int16_t bottom = static_cast<int16_t>(base_y + kScrollerAmpPx + kScrollerGlyphHeight +
                                              kSafeBandMarginBottom + kSafeFeatherPx);
  const int16_t y0 = clampValue<int16_t>(top, 0, static_cast<int16_t>(height - 1));
  const int16_t y1 = clampValue<int16_t>(bottom, 0, static_cast<int16_t>(height - 1));
  for (int16_t y = y0; y <= y1; ++y) {
    const uint8_t scale = safeBandScaleForY(y, base_y, height);
    if (scale == 255U) {
      continue;
    }
    for (int16_t x = 0; x < width; ++x) {
      const size_t idx = static_cast<size_t>(y) * config_.sprite_width + static_cast<size_t>(x);
      sprite_pixels_[idx] = fx_rgb565_scale(sprite_pixels_[idx], scale);
    }
  }

  const int total_px = static_cast<int>(scroll_text_len_) * static_cast<int>(kScrollerCharWidth);
  if (total_px <= 0) {
    return;
  }
  const uint32_t scroll_px = scroll_phase_px_q16_ >> 16U;
  int x0 = width - static_cast<int16_t>(scroll_px % static_cast<uint32_t>(total_px));
  const uint16_t base_col = fx_rgb565(255U, 210U, 90U);
  const uint16_t shadow_col = fx_rgb565(0U, 0U, 0U);
  const uint8_t ph = scroll_highlight_phase_;
  const uint8_t tint = static_cast<uint8_t>(128 + (fx_sin8(ph) >> 1U));
  const uint16_t accent = fx_rgb565(40U, static_cast<uint8_t>(120U + tint / 2U),
                                    static_cast<uint8_t>(180U + tint / 3U));
  const uint16_t color_cycle[6] = {
      fx_rgb565(255U, 0U, 0U),     // rouge
      fx_rgb565(0U, 255U, 0U),     // vert
      fx_rgb565(0U, 0U, 255U),     // bleu
      fx_rgb565(0U, 255U, 255U),   // cyan
      fx_rgb565(255U, 0U, 255U),   // magenta
      fx_rgb565(255U, 255U, 0U),   // jaune
  };

  const int repeat = (width / total_px) + 3;
  for (int r = 0; r < repeat; ++r) {
    for (uint16_t i = 0U; i < scroll_text_len_; ++i) {
      const int x_char = x0 + static_cast<int>(i * kScrollerCharWidth);
      const int16_t y_off = static_cast<int16_t>(
          (fx_sin8(static_cast<uint8_t>(scroll_wave_phase_ + static_cast<uint8_t>(i * 9U))) * kScrollerAmpPx) / 127);
      const uint16_t col = scroller_centered_
                               ? color_cycle[i % 6U]
                               : (((i & 7U) == 0U) ? accent : base_col);
      drawChar6x8(static_cast<int16_t>(x_char), static_cast<int16_t>(base_y + y_off), scroll_text_[i], col,
                  shadow_col);
    }
    x0 += total_px;
  }
  if (sync_.on_bar) {
    const uint16_t flash = fx_rgb565(32U, 32U, 32U);
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = 0; x < width; ++x) {
        const size_t idx = static_cast<size_t>(y) * config_.sprite_width + static_cast<size_t>(x);
        sprite_pixels_[idx] = fx_rgb565_add(sprite_pixels_[idx], flash);
      }
    }
  }
}

void FxEngine::renderLowRes(uint32_t now_ms, FxScenePhase phase) {
  if (sprite_pixels_ == nullptr || sprite_pixel_count_ == 0U) {
    return;
  }

  uint32_t dt_ms = now_ms - last_render_ms_;
  if (last_render_ms_ == 0U) {
    dt_ms = 0U;
  } else if (dt_ms > 120U) {
    dt_ms = 120U;
  }
  last_render_ms_ = now_ms;

  updateStars(dt_ms);
  fx_sync_step(&sync_, dt_ms, 8U);
  if (sync_.on_beat) {
    scroll_wave_phase_ = static_cast<uint8_t>(scroll_wave_phase_ + 18U);
  }
  if (sync_.on_bar) {
    scroll_highlight_phase_ = static_cast<uint8_t>(scroll_highlight_phase_ + 64U);
  }

  uint16_t speed_px_per_sec = 90U;
  if (phase == FxScenePhase::kPhaseA) {
    speed_px_per_sec = 110U;
  } else if (phase == FxScenePhase::kPhaseB) {
    speed_px_per_sec = 92U;
  } else if (phase == FxScenePhase::kPhaseC) {
    speed_px_per_sec = 72U;
  }
  scroll_phase_px_q16_ += (static_cast<uint32_t>(dt_ms) * static_cast<uint32_t>(speed_px_per_sec) << 16U) / 1000U;
  scroll_wave_phase_ = static_cast<uint8_t>(scroll_wave_phase_ + (dt_ms / 10U));
  scroll_highlight_phase_ = static_cast<uint8_t>(scroll_highlight_phase_ + (dt_ms / 30U));

  if (mode_ != FxMode::kClassic) {
    renderMode3D(now_ms);
    renderScroller(now_ms);
    return;
  }

  bool rendered_v9 = false;
  if (preset_ != FxPreset::kBoingball && v9_runtime_ready_ && v9_use_runtime_) {
    rendered_v9 = renderLowResV9(dt_ms);
  }
  if (!rendered_v9) {
    renderBackground(now_ms, phase);
    renderMid(now_ms, dt_ms, phase);
  }
  renderScroller(now_ms);
}

bool FxEngine::buildScaleMaps(uint16_t display_width, uint16_t display_height) {
  if (display_width == 0U || display_height == 0U) {
    return false;
  }
  if (display_width > kScaleMapAxisMax || display_height > kScaleMapAxisMax ||
      config_.sprite_width == 0U || config_.sprite_height == 0U) {
    return false;
  }
  if (scale_map_width_ == display_width && scale_map_height_ == display_height) {
    return true;
  }

  for (uint16_t y = 0U; y < display_height; ++y) {
    y_scale_map_[y] = static_cast<uint16_t>(
        (static_cast<uint32_t>(y) * static_cast<uint32_t>(config_.sprite_height)) /
        static_cast<uint32_t>(display_height));
  }
  for (uint16_t x = 0U; x < display_width; ++x) {
    x_scale_map_[x] = static_cast<uint16_t>(
        (static_cast<uint32_t>(x) * static_cast<uint32_t>(config_.sprite_width)) /
        static_cast<uint32_t>(display_width));
  }
  scale_map_width_ = display_width;
  scale_map_height_ = display_height;
  return true;
}

bool FxEngine::blitUpscaled(drivers::display::DisplayHal& display, uint16_t display_width, uint16_t display_height) {
  if (sprite_pixels_ == nullptr) {
    return false;
  }
  if (display_width == 0U || display_height == 0U || display_width > kDisplaySpanMax) {
    return false;
  }

  const uint16_t src_width = config_.sprite_width;
  const uint16_t src_height = config_.sprite_height;
  if (src_width == 0U) {
    return false;
  }
  const bool exact_2x =
      kFxUseFast2xBlit &&
      (display_width == static_cast<uint16_t>(src_width * 2U)) &&
      (display_height == static_cast<uint16_t>(src_height * 2U));

  if (!kFxLineBufUseRgb565) {
    return false;
  }

  const uint16_t chunk_lines = (line_buffer_lines_ == 0U) ? 1U : line_buffer_lines_;
  const uint8_t buffers_in_use = (line_buffer_count_ == 0U) ? 1U : line_buffer_count_;

  if (line_buffers_[0U] == nullptr) {
    return false;
  }
  if (display_width > line_buffer_width_) {
    return false;
  }

  const uint32_t frame_cpu_start = micros();
  uint32_t frame_submit_us = 0U;
  uint32_t frame_wait_us = 0U;
  uint32_t frame_tail_wait_us = 0U;
  uint16_t frame_cpu_lines = 0U;
  bool dma_in_flight = false;

  if (!display.startWrite()) {
    blit_fail_busy_count_ += 1U;
    return false;
  }

  for (uint16_t y = 0U; y < display_height; y += chunk_lines) {
    const uint16_t lines_in_chunk =
        static_cast<uint16_t>((y + chunk_lines > display_height) ? (display_height - y) : chunk_lines);
    if (lines_in_chunk == 0U) {
      continue;
    }

    const uint32_t chunk_index = static_cast<uint32_t>(y / chunk_lines);
    const uint16_t* const src_chunk = &sprite_pixels_[0];
    const uint8_t buffer_index = (line_buffer_count_ >= 2U) ? static_cast<uint8_t>(chunk_index & 1U) : 0U;

    // With a single line buffer we must wait before writing into it again.
    if (kFxUseDmaBlit && dma_in_flight && buffers_in_use < 2U) {
      const uint32_t wait_start_us = micros();
      if (!display.waitDmaComplete(kFxDmaWaitBudgetUs)) {
        blit_dma_timeout_count_ += 1U;
        blit_fail_busy_count_ += 1U;
        display.endWrite();
        return false;
      }
      frame_wait_us += (micros() - wait_start_us);
      dma_in_flight = false;
    }

    uint16_t* dst = line_buffers_[buffer_index];
    if (dst == nullptr) {
      blit_fail_busy_count_ += 1U;
      display.endWrite();
      return false;
    }
    for (uint16_t row = 0U; row < lines_in_chunk; ++row) {
      const uint16_t dst_row = static_cast<uint16_t>(row * display_width);
      if (exact_2x) {
        const uint16_t src_y = static_cast<uint16_t>((y + row) >> 1U);
        uint16_t* dst_row_ptr = dst + dst_row;
        if (((y + row) & 1U) != 0U && row > 0U) {
          blit::copy_rgb565_line(dst_row_ptr, dst_row_ptr - display_width, display_width);
        } else {
          const uint16_t* src_row = src_chunk + (static_cast<size_t>(src_y) * src_width);
          blit::scale2x_rgb565_line(dst_row_ptr, src_row, src_width);
        }
      } else {
        const uint16_t src_y = y_scale_map_[static_cast<size_t>(y + row)];
        const uint16_t* src_row = src_chunk + (static_cast<size_t>(src_y) * src_width);
        for (uint16_t x = 0U; x < display_width; ++x) {
          const uint16_t src_x = x_scale_map_[x];
          dst[dst_row + x] = src_row[src_x];
        }
      }
      frame_cpu_lines++;
    }

    // Keep one DMA transfer in flight while CPU prepares the next chunk.
    if (kFxUseDmaBlit && dma_in_flight) {
      const uint32_t wait_start_us = micros();
      if (!display.waitDmaComplete(kFxDmaWaitBudgetUs)) {
        blit_dma_timeout_count_ += 1U;
        blit_fail_busy_count_ += 1U;
        display.endWrite();
        return false;
      }
      frame_wait_us += (micros() - wait_start_us);
      dma_in_flight = false;
    }

    const uint32_t submit_start_us = micros();
    display.setAddrWindow(static_cast<int16_t>(0),
                          static_cast<int16_t>(y),
                          static_cast<int16_t>(display_width),
                          static_cast<int16_t>(lines_in_chunk));
    if (kFxUseDmaBlit) {
      display.pushImageDma(static_cast<int16_t>(0),
                           static_cast<int16_t>(y),
                           static_cast<int16_t>(display_width),
                           static_cast<int16_t>(lines_in_chunk),
                           dst);
      dma_in_flight = true;
    } else {
      const uint32_t chunk_pixel_count =
          static_cast<uint32_t>(display_width) * static_cast<uint32_t>(lines_in_chunk);
      display.pushColors(dst, chunk_pixel_count, true);
    }
    frame_submit_us += (micros() - submit_start_us);
  }

  if (kFxUseDmaBlit && dma_in_flight) {
    const uint32_t wait_start_us = micros();
    if (!display.waitDmaComplete(kFxDmaWaitBudgetUs)) {
      blit_dma_timeout_count_ += 1U;
      blit_fail_busy_count_ += 1U;
      display.endWrite();
      return false;
    }
    frame_tail_wait_us += (micros() - wait_start_us);
  }
  display.endWrite();

  const uint32_t frame_cpu_us = micros() - frame_cpu_start;
  blit_cpu_time_total_us_ += frame_cpu_us;
  blit_dma_submit_time_total_us_ += frame_submit_us;
  blit_dma_wait_time_total_us_ += frame_wait_us;
  blit_dma_tail_wait_time_total_us_ += frame_tail_wait_us;
  if (frame_cpu_us > blit_cpu_time_max_us_) {
    blit_cpu_time_max_us_ = frame_cpu_us;
  }
  if (frame_submit_us > blit_dma_submit_time_max_us_) {
    blit_dma_submit_time_max_us_ = frame_submit_us;
  }
  if (frame_wait_us > blit_dma_wait_time_max_us_) {
    blit_dma_wait_time_max_us_ = frame_wait_us;
  }
  if (frame_tail_wait_us > blit_dma_tail_wait_time_max_us_) {
    blit_dma_tail_wait_time_max_us_ = frame_tail_wait_us;
  }
  stats_.blit_cpu_us = frame_cpu_us;
  stats_.blit_dma_submit_us = frame_submit_us;
  stats_.blit_dma_wait_us = frame_wait_us;
  stats_.dma_tail_wait_us = frame_tail_wait_us;
  stats_.dma_timeout_count = blit_dma_timeout_count_;
  stats_.blit_fail_busy = blit_fail_busy_count_;
  stats_.blit_lines = static_cast<uint16_t>((frame_cpu_lines > 0xFFFFU) ? 0xFFFFU : frame_cpu_lines);
  return true;
}

}  // namespace ui::fx
