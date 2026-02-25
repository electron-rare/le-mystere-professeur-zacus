#pragma once
#include <cstdint>
#include <cstddef>

namespace ui::fx::blit {

// Alignment helpers
static inline bool is_aligned4(const void* p)  { return (((uintptr_t)p) & 3u) == 0u; }
static inline bool is_aligned16(const void* p) { return (((uintptr_t)p) & 15u) == 0u; }

// Duplicate a RGB565 low-res line horizontally (scale ×2).
// dst must be sized for (2*src_w) pixels.
void scale2x_rgb565_line(uint16_t* dst, const uint16_t* src, uint16_t src_w);

// Copy a full RGB565 line (w pixels).
void copy_rgb565_line(uint16_t* dst, const uint16_t* src, uint16_t w);

// Shadow-style darken (half brightness) on a span [x0, x1).
// SIMD-ready: if you later add esp_simd / S3 PIE, plug it here (when aligned16 and width multiple).
void darken_span_half_rgb565(uint16_t* line, uint16_t x0, uint16_t x1);

} // namespace ui::fx::blit
