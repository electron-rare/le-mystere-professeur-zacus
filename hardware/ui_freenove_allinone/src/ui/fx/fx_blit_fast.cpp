#include "ui/fx/fx_blit_fast.h"

namespace ui::fx::blit {

void scale2x_rgb565_line(uint16_t* dst, const uint16_t* src, uint16_t src_w)
{
  if (!dst || !src || src_w == 0) return;

  // Fast path: 32-bit stores write two identical 16-bit pixels at once.
  if (is_aligned4(dst)) {
    uint32_t* d32 = reinterpret_cast<uint32_t*>(dst);
    for (uint16_t x = 0; x < src_w; ++x) {
      const uint32_t p = static_cast<uint32_t>(src[x]);
      d32[x] = p | (p << 16);
    }
    return;
  }

  // Fallback: 16-bit stores (safe for any alignment).
  for (uint16_t x = 0; x < src_w; ++x) {
    const uint16_t p = src[x];
    dst[(uint16_t)(2 * x)] = p;
    dst[(uint16_t)(2 * x + 1)] = p;
  }
}

void copy_rgb565_line(uint16_t* dst, const uint16_t* src, uint16_t w)
{
  if (!dst || !src || w == 0) return;
  __builtin_memcpy(dst, src, (size_t)w * sizeof(uint16_t));
}

void darken_span_half_rgb565(uint16_t* line, uint16_t x0, uint16_t x1)
{
  if (!line || x1 <= x0) return;

  const uint16_t n = static_cast<uint16_t>(x1 - x0);
  uint16_t* p = line + x0;

  // 2 pixels at once with a single shift+mask works because mask clears the cross-half carry bit.
  // Verified: ((v>>1)&0x7BEF7BEF) matches per-16-bit ((pix>>1)&0x7BEF) for both halves.
  if (is_aligned4(p) && (n % 2u) == 0u) {
    uint32_t* p32 = reinterpret_cast<uint32_t*>(p);
    const uint32_t mask = 0x7BEF7BEFu;
    const uint16_t n2 = static_cast<uint16_t>(n / 2u);
    for (uint16_t i = 0; i < n2; ++i) {
      p32[i] = (p32[i] >> 1) & mask;
    }
    return;
  }

  // Fallback
  for (uint16_t i = 0; i < n; ++i) {
    const uint16_t c = p[i];
    p[i] = static_cast<uint16_t>((c >> 1) & 0x7BEFu);
  }
}

} // namespace ui::fx::blit
