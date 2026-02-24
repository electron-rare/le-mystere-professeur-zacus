#include "ui/fx/fx_engine.h"

#include "drivers/display/display_hal.h"
#include "runtime/memory/caps_allocator.h"
#include "ui_freenove_config.h"

namespace ui::fx {

namespace {

constexpr uint16_t kMinSpriteWidth = 96U;
constexpr uint16_t kMinSpriteHeight = 72U;
constexpr uint16_t kMaxSpriteWidth = 240U;
constexpr uint16_t kMaxSpriteHeight = 180U;
constexpr uint8_t kMinTargetFps = 12U;
constexpr uint8_t kMaxTargetFps = 30U;
constexpr uint16_t kDisplaySpanMax =
    (FREENOVE_LCD_WIDTH > FREENOVE_LCD_HEIGHT) ? FREENOVE_LCD_WIDTH : FREENOVE_LCD_HEIGHT;

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

}  // namespace

bool FxEngine::begin(const FxEngineConfig& config) {
  config_ = config;
  config_.sprite_width = clampValue<uint16_t>(config_.sprite_width, kMinSpriteWidth, kMaxSpriteWidth);
  config_.sprite_height = clampValue<uint16_t>(config_.sprite_height, kMinSpriteHeight, kMaxSpriteHeight);
  config_.target_fps = clampValue<uint8_t>(config_.target_fps, kMinTargetFps, kMaxTargetFps);
  enabled_ = config_.lgfx_backend;

  if (sprite_pixels_ != nullptr) {
    runtime::memory::CapsAllocator::release(sprite_pixels_);
    sprite_pixels_ = nullptr;
  }
  if (line_buffer_ != nullptr) {
    runtime::memory::CapsAllocator::release(line_buffer_);
    line_buffer_ = nullptr;
  }
  sprite_pixel_count_ = 0U;

  if (config_.lgfx_backend) {
    sprite_pixel_count_ =
        static_cast<size_t>(config_.sprite_width) * static_cast<size_t>(config_.sprite_height);
    sprite_pixels_ = static_cast<uint16_t*>(
        runtime::memory::CapsAllocator::allocPsram(sprite_pixel_count_ * sizeof(uint16_t), "fx_sprite"));
    if (sprite_pixels_ == nullptr) {
      ready_ = false;
      return false;
    }

    line_buffer_ = static_cast<uint16_t*>(
        runtime::memory::CapsAllocator::allocInternalDma(static_cast<size_t>(kDisplaySpanMax) * sizeof(uint16_t),
                                                         "fx_line"));
    if (line_buffer_ == nullptr) {
      runtime::memory::CapsAllocator::release(sprite_pixels_);
      sprite_pixels_ = nullptr;
      ready_ = false;
      return false;
    }
  }

  setQualityLevel(0U);
  reset();
  ready_ = true;
  return true;
}

void FxEngine::reset() {
  stats_ = {};
  fps_window_start_ms_ = 0U;
  fps_window_frames_ = 0U;
  last_render_ms_ = 0U;
  next_frame_ms_ = 0U;
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
  return stats_;
}

uint16_t FxEngine::rgb565(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t red = static_cast<uint16_t>((r & 0xF8U) << 8U);
  const uint16_t green = static_cast<uint16_t>((g & 0xFCU) << 3U);
  const uint16_t blue = static_cast<uint16_t>(b >> 3U);
  return static_cast<uint16_t>(red | green | blue);
}

uint32_t FxEngine::nextRand() {
  rng_state_ ^= (rng_state_ << 13U);
  rng_state_ ^= (rng_state_ >> 17U);
  rng_state_ ^= (rng_state_ << 5U);
  return rng_state_;
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

  uint8_t base_r = 8U;
  uint8_t base_g = 20U;
  uint8_t base_b = 44U;
  if (phase == FxScenePhase::kPhaseB) {
    base_r = 26U;
    base_g = 10U;
    base_b = 38U;
  } else if (phase == FxScenePhase::kPhaseC) {
    base_r = 10U;
    base_g = 24U;
    base_b = 34U;
  }

  const uint16_t width = config_.sprite_width;
  const uint16_t height = config_.sprite_height;
  for (uint16_t y = 0U; y < height; ++y) {
    const uint8_t wave = static_cast<uint8_t>((((y * 7U) + (now_ms >> 2U)) & 0x3FU));
    const uint16_t y_offset = static_cast<uint16_t>(y) * width;
    for (uint16_t x = 0U; x < width; ++x) {
      const uint8_t shimmer = static_cast<uint8_t>(((x + (y >> 1U) + (now_ms >> 3U)) & 0x1FU));
      uint8_t r = static_cast<uint8_t>(base_r + ((wave + shimmer) >> 2U));
      uint8_t g = static_cast<uint8_t>(base_g + ((wave + shimmer) >> 1U));
      uint8_t b = static_cast<uint8_t>(base_b + ((wave + shimmer) >> 1U));
      if ((y & 0x03U) == 0U) {
        r = static_cast<uint8_t>((r * 3U) / 4U);
        g = static_cast<uint8_t>((g * 3U) / 4U);
        b = static_cast<uint8_t>((b * 3U) / 4U);
      }
      sprite_pixels_[y_offset + x] = rgb565(r, g, b);
    }
  }

  for (uint16_t i = 0U; i < star_count_; ++i) {
    const Star& star = stars_[i];
    const int16_t x = static_cast<int16_t>(star.x_q8 >> 8U);
    const int16_t y = static_cast<int16_t>(star.y_q8 >> 8U);
    const uint16_t color = (star.layer == 0U) ? rgb565(110U, 160U, 220U)
                                               : (star.layer == 1U) ? rgb565(170U, 220U, 255U)
                                                                    : rgb565(245U, 252U, 255U);
    drawPixel(x, y, color);
    if (star.layer >= 1U) {
      drawPixel(static_cast<int16_t>(x + 1), y, color);
    }
  }
}

bool FxEngine::blitUpscaled(drivers::display::DisplayHal& display, uint16_t display_width, uint16_t display_height) {
  if (sprite_pixels_ == nullptr || line_buffer_ == nullptr) {
    return false;
  }
  if (display_width == 0U || display_height == 0U || display_width > kDisplaySpanMax) {
    return false;
  }

  const uint16_t src_width = config_.sprite_width;
  const uint16_t src_height = config_.sprite_height;

  display.startWrite();
  display.setAddrWindow(0, 0, static_cast<int16_t>(display_width), static_cast<int16_t>(display_height));
  for (uint16_t y = 0U; y < display_height; ++y) {
    const uint16_t src_y = static_cast<uint16_t>((static_cast<uint32_t>(y) * src_height) / display_height);
    const uint16_t* src_row = &sprite_pixels_[static_cast<size_t>(src_y) * src_width];
    for (uint16_t x = 0U; x < display_width; ++x) {
      const uint16_t src_x = static_cast<uint16_t>((static_cast<uint32_t>(x) * src_width) / display_width);
      line_buffer_[x] = src_row[src_x];
    }
    display.pushColors(line_buffer_, display_width, true);
  }
  display.endWrite();
  return true;
}

}  // namespace ui::fx
