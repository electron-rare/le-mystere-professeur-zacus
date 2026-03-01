#pragma once
#include <cstdint>
#include <array>
#include <cmath>

namespace fx {

// 256-entry sin/cos LUT in Q15.
struct SinCosLUT {
  std::array<int16_t, 256> sinQ15{};

  void init() {
    for (int i = 0; i < 256; i++) {
      float a = (float)i * (2.0f * 3.14159265358979323846f / 256.0f);
      sinQ15[(size_t)i] = (int16_t)lrintf(sinf(a) * 32767.0f);
    }
  }
  int16_t sin(uint8_t a) const { return sinQ15[a]; }
  int16_t cos(uint8_t a) const { return sinQ15[(uint8_t)(a + 64)]; }
};

} // namespace fx
