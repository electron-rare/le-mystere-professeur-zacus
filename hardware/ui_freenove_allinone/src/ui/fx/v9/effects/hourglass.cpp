#include "ui/fx/v9/effects/hourglass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fx::effects {

namespace {

uint32_t hash32(uint32_t x) {
  x ^= x >> 16U;
  x *= 0x7FEB352DU;
  x ^= x >> 15U;
  x *= 0x846CA68BU;
  x ^= x >> 16U;
  return x;
}

int signedNoise(uint32_t seed, int amplitude) {
  if (amplitude <= 0) {
    return 0;
  }
  const uint32_t span = static_cast<uint32_t>(amplitude * 2 + 1);
  return static_cast<int>(hash32(seed) % span) - amplitude;
}

void putPixel(RenderTarget& rt, int x, int y, uint8_t color) {
  if (x < 0 || y < 0 || x >= rt.w || y >= rt.h) {
    return;
  }
  rt.rowPtr<uint8_t>(y)[x] = color;
}

void drawLine(RenderTarget& rt, int x0, int y0, int x1, int y1, uint8_t color) {
  int dx = std::abs(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;
  while (true) {
    putPixel(rt, x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

}  // namespace

HourglassFx::HourglassFx(FxServices s) : FxBase(s) {}

void HourglassFx::init(const FxContext& ctx) {
  start_frame_ = ctx.frame;
}

void HourglassFx::update(const FxContext& /*ctx*/) {}

void HourglassFx::render(const FxContext& ctx, RenderTarget& rt) {
  if (rt.fmt != PixelFormat::I8 || rt.w < 16 || rt.h < 16) {
    return;
  }

  const uint8_t bg_dark = 4U;
  const uint8_t bg_band = 18U;
  const uint8_t frame_color = 98U;
  const uint8_t frame_hi = 142U;
  const uint8_t sand = 214U;
  const uint8_t sand_hi = 244U;

  const uint32_t frame = (ctx.frame - start_frame_);
  const int bg_shift = signedNoise((frame / 5U) ^ 0xA52FU, 2);
  for (int y = 0; y < rt.h; ++y) {
    uint8_t* row = rt.rowPtr<uint8_t>(y);
    const int fade = (y * 8) / std::max(1, rt.h);
    const int base = static_cast<int>(bg_dark) + fade + bg_shift;
    const int clamped = (base < 0) ? 0 : ((base > 255) ? 255 : base);
    const uint8_t tone = static_cast<uint8_t>(clamped);
    for (int x = 0; x < rt.w; ++x) {
      row[x] = tone;
    }
  }

  const int cx = rt.w / 2;
  const int cy = rt.h / 2;
  const int outer_h = std::max(18, static_cast<int>(rt.h * 0.70f));
  const int outer_w = std::max(22, static_cast<int>(rt.w * 0.34f));
  const int top_y = std::max(2, cy - outer_h / 2);
  const int bottom_y = std::min(rt.h - 3, cy + outer_h / 2);
  const int left_x = std::max(2, cx - outer_w / 2);
  const int right_x = std::min(rt.w - 3, cx + outer_w / 2);

  drawLine(rt, left_x, top_y, right_x, top_y, frame_hi);
  drawLine(rt, left_x, bottom_y, right_x, bottom_y, frame_hi);
  drawLine(rt, left_x + 1, top_y + 1, right_x - 1, top_y + 1, frame_color);
  drawLine(rt, left_x + 1, bottom_y - 1, right_x - 1, bottom_y - 1, frame_color);
  drawLine(rt, left_x, top_y, cx, cy, frame_color);
  drawLine(rt, right_x, top_y, cx, cy, frame_color);
  drawLine(rt, left_x, bottom_y, cx, cy, frame_color);
  drawLine(rt, right_x, bottom_y, cx, cy, frame_color);

  float phase = std::fmod(ctx.demoTime * std::max(0.02f, speed), 1.0f);
  if (phase < 0.0f) {
    phase += 1.0f;
  }
  // Background direction requested by scene tuning: invert flow orientation.
  phase = 1.0f - phase;
  const int chamber_h = std::max(4, (outer_h - 6) / 2);
  const int top_rows = static_cast<int>((1.0f - phase) * static_cast<float>(chamber_h));
  const int bottom_rows = static_cast<int>(phase * static_cast<float>(chamber_h));

  for (int row = 0; row < top_rows; ++row) {
    const float t = static_cast<float>(row) / static_cast<float>(std::max(1, chamber_h - 1));
    const int half = std::max(1, static_cast<int>((1.0f - t) * static_cast<float>((outer_w / 2) - 3)));
    const int y = top_y + 2 + row;
    if (y >= cy - 1) {
      break;
    }
    for (int x = cx - half; x <= cx + half; ++x) {
      const bool highlight = ((x + y) & 0x3) == 0;
      putPixel(rt, x, y, highlight ? sand_hi : sand);
    }
  }

  for (int row = 0; row < bottom_rows; ++row) {
    const float t = static_cast<float>(row) / static_cast<float>(std::max(1, chamber_h - 1));
    const int half = std::max(1, static_cast<int>((1.0f - t) * static_cast<float>((outer_w / 2) - 3)));
    const int y = bottom_y - 2 - row;
    if (y <= cy + 1) {
      break;
    }
    for (int x = cx - half; x <= cx + half; ++x) {
      const bool highlight = ((x + y) & 0x3) == 1;
      putPixel(rt, x, y, highlight ? sand_hi : sand);
    }
  }

  const int neck_jitter = signedNoise(frame ^ 0x5AF0U, static_cast<int>(std::round(glitch * 6.0f)));
  const int stream_len = std::max(2, static_cast<int>(2 + std::round(phase * 8.0f)));
  for (int i = 0; i < stream_len; ++i) {
    const int y = cy - 1 + i;
    const int x = cx + ((i & 1) ? neck_jitter : 0);
    putPixel(rt, x, y, (i & 1) ? sand_hi : sand);
  }

  const int scan_y = top_y + static_cast<int>(phase * static_cast<float>(bottom_y - top_y));
  if (scan_y > top_y + 2 && scan_y < bottom_y - 2) {
    for (int x = left_x + 2; x <= right_x - 2; ++x) {
      uint8_t* row = rt.rowPtr<uint8_t>(scan_y);
      row[x] = static_cast<uint8_t>(std::max<int>(row[x], bg_band));
    }
  }

  const int pulse_y = top_y + static_cast<int>((0.5f + 0.5f * std::sin(ctx.demoTime * 2.4f)) * static_cast<float>(outer_h - 6));
  if (pulse_y > top_y + 2 && pulse_y < bottom_y - 2) {
    drawLine(rt, left_x + 3, pulse_y, right_x - 3, pulse_y, static_cast<uint8_t>(bg_band + 10U));
  }
}

}  // namespace fx::effects
