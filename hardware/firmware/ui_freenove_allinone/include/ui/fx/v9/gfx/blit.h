#pragma once
#include "ui/fx/v9/engine/types.h"
#include <cstdint>

namespace fx::gfx {

// Fill I8 target with value
void fill_i8(RenderTarget& rt, uint8_t v);

// Fill RGB565 target with color
void fill_rgb565(RenderTarget& rt, uint16_t c);

// Nearest upscale I8 -> RGB565 using palette (2x for 160x120->320x240 typical)
void upscale_nearest_i8_to_rgb565(const RenderTarget& srcI8, RenderTarget& dst565);

// Blend I8 source onto I8 destination (REPLACE / ADD_CLAMP)
void blend_i8(RenderTarget& dst, const RenderTarget& src, BlendMode mode);

// Darken a horizontal span in RGB565 (shadow). SIMD fast path can be plugged here.
void darken_span_rgb565_half(uint16_t* line, int x0, int x1, bool aligned16);

} // namespace fx::gfx
