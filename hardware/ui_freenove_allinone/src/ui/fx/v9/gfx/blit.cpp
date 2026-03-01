#include "ui/fx/v9/gfx/blit.h"
#include <cstring>
#include <algorithm>

namespace fx::gfx {

void fill_i8(RenderTarget& rt, uint8_t v)
{
  if (rt.fmt != PixelFormat::I8 || !rt.pixels) return;
  for (int y = 0; y < rt.h; y++) {
    std::memset(rt.rowPtr<uint8_t>(y), v, (size_t)rt.w);
  }
}

void fill_rgb565(RenderTarget& rt, uint16_t c)
{
  if (rt.fmt != PixelFormat::RGB565 || !rt.pixels) return;
  for (int y = 0; y < rt.h; y++) {
    uint16_t* row = rt.rowPtr<uint16_t>(y);
    for (int x = 0; x < rt.w; x++) row[x] = c;
  }
}

void upscale_nearest_i8_to_rgb565(const RenderTarget& srcI8, RenderTarget& dst565)
{
  if (srcI8.fmt != PixelFormat::I8 || dst565.fmt != PixelFormat::RGB565) return;
  if (!srcI8.pixels || !dst565.pixels || !srcI8.palette565) return;

  const int sx = srcI8.w;
  const int sy = srcI8.h;
  const int dx = dst565.w;
  const int dy = dst565.h;
  if (dx < sx || dy < sy) return;

  const int scaleX = dx / sx;
  const int scaleY = dy / sy;

  for (int y = 0; y < sy; y++) {
    const uint8_t* srow = srcI8.rowPtr<uint8_t>(y);
    for (int yy = 0; yy < scaleY; yy++) {
      uint16_t* drow = dst565.rowPtr<uint16_t>(y * scaleY + yy);
      for (int x = 0; x < sx; x++) {
        uint16_t c = srcI8.palette565[srow[x]];
        for (int xx = 0; xx < scaleX; xx++) drow[x * scaleX + xx] = c;
      }
    }
  }
}

static inline uint8_t add_sat_u8(uint8_t a, uint8_t b)
{
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return (s > 255) ? 255 : (uint8_t)s;
}

void blend_i8(RenderTarget& dst, const RenderTarget& src, BlendMode mode)
{
  if (dst.fmt != PixelFormat::I8 || src.fmt != PixelFormat::I8) return;
  if (dst.w != src.w || dst.h != src.h) return;

  for (int y = 0; y < dst.h; y++) {
    uint8_t* d = dst.rowPtr<uint8_t>(y);
    const uint8_t* s = src.rowPtr<const uint8_t>(y);
    if (mode == BlendMode::REPLACE) {
      std::memcpy(d, s, (size_t)dst.w);
    } else if (mode == BlendMode::ADD_CLAMP) {
      for (int x = 0; x < dst.w; x++) d[x] = add_sat_u8(d[x], s[x]);
    }
  }
}

static inline uint16_t rgb565_half(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }

// Default implementation (no SIMD). You can override/hook for ESP32-S3.
void darken_span_rgb565_half(uint16_t* line, int x0, int x1, bool /*aligned16*/)
{
  if (!line || x1 <= x0) return;
  for (int x = x0; x < x1; x++) line[x] = rgb565_half(line[x]);
}

} // namespace fx::gfx
