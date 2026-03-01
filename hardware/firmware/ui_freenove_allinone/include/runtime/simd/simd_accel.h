// simd_accel.h - conversion and DSP helpers with safe scalar fallback.
#pragma once

#include <cstddef>
#include <cstdint>

namespace runtime::simd {

struct SimdAccelStatus {
  bool simd_path_enabled = false;
  bool esp_dsp_enabled = false;
  uint32_t selftest_runs = 0U;
  uint32_t selftest_failures = 0U;
  uint32_t bench_runs = 0U;
  uint32_t bench_loops = 0U;
  uint32_t bench_pixels = 0U;
  uint32_t bench_l8_to_rgb565_us = 0U;
  uint32_t bench_idx8_to_rgb565_us = 0U;
  uint32_t bench_rgb888_to_rgb565_us = 0U;
  uint32_t bench_s16_gain_q15_us = 0U;
};

struct SimdBenchResult {
  uint32_t loops = 0U;
  uint32_t pixels = 0U;
  uint32_t l8_to_rgb565_us = 0U;
  uint32_t idx8_to_rgb565_us = 0U;
  uint32_t rgb888_to_rgb565_us = 0U;
  uint32_t s16_gain_q15_us = 0U;
};

const SimdAccelStatus& status();
void resetBenchStatus();

void simd_rgb565_copy(uint16_t* dst, const uint16_t* src, size_t n_px);
void simd_rgb565_fill(uint16_t* dst, uint16_t color565, size_t n_px);
void simd_rgb565_bswap_copy(uint16_t* dst, const uint16_t* src, size_t n_px);

void simd_l8_to_rgb565(uint16_t* dst565, const uint8_t* src_l8, size_t n_px);
void simd_index8_to_rgb565(uint16_t* dst565,
                           const uint8_t* idx8,
                           const uint16_t* pal565_256,
                           size_t n_px);
void simd_rgb888_to_rgb565(uint16_t* dst565, const uint8_t* src_rgb888, size_t n_px);
void simd_yuv422_to_rgb565(uint16_t* dst565, const uint8_t* src_yuv422, size_t n_px);

void simd_s16_gain_q15(int16_t* dst, const int16_t* src, int16_t gain_q15, size_t n);
void simd_s16_mix2_q15(int16_t* dst,
                       const int16_t* a,
                       const int16_t* b,
                       int16_t ga_q15,
                       int16_t gb_q15,
                       size_t n);

bool selfTest();
SimdBenchResult runBench(uint32_t loops, uint32_t pixels);

}  // namespace runtime::simd

