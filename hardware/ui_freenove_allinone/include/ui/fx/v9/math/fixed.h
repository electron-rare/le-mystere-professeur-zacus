#pragma once
#include <cstdint>
#include <cmath>

namespace fx {

// Q15 fixed helpers
using q15_t = int16_t;

static inline q15_t q15_from_float(float v) {
  int32_t x = (int32_t)lrintf(v * 32767.0f);
  if (x < -32768) x = -32768;
  if (x >  32767) x =  32767;
  return (q15_t)x;
}

static inline float q15_to_float(q15_t v) {
  return (float)v / 32767.0f;
}

static inline int32_t mul_q15(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * (int64_t)b) >> 15);
}

} // namespace fx
