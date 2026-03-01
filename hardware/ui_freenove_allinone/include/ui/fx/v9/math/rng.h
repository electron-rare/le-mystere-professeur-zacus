#pragma once
#include <cstdint>

namespace fx {

// Simple deterministic RNG: xorshift32
struct Rng32 {
  uint32_t s = 0x12345678u;

  void seed(uint32_t v) { s = (v ? v : 0x12345678u); }

  uint32_t nextU32() {
    uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
  }

  uint32_t nextRange(uint32_t lo, uint32_t hi) {
    if (hi <= lo) return lo;
    return lo + (nextU32() % (hi - lo));
  }

  float next01() {
    // 24-bit mantissa
    return (float)(nextU32() & 0x00FFFFFFu) / (float)0x01000000u;
  }
};

} // namespace fx
