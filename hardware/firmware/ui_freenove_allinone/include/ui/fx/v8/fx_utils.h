#pragma once
#include <stdint.h>

static inline uint16_t fx_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}
static inline uint16_t fx_rgb565_add(uint16_t a, uint16_t b) {
  uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint16_t r = (ar + br > 31 ? 31 : ar + br);
  uint16_t g = (ag + bg > 63 ? 63 : ag + bg);
  uint16_t bl= (ab + bb > 31 ? 31 : ab + bb);
  return (uint16_t)((r<<11)|(g<<5)|bl);
}
static inline uint16_t fx_rgb565_scale(uint16_t c, uint8_t k) {
  uint16_t r = (c >> 11) & 0x1F;
  uint16_t g = (c >> 5) & 0x3F;
  uint16_t b = c & 0x1F;
  r = (r * k) >> 8;
  g = (g * k) >> 8;
  b = (b * k) >> 8;
  return (uint16_t)((r<<11)|(g<<5)|b);
}
static inline uint16_t fx_rgb565_half(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEFu); }
