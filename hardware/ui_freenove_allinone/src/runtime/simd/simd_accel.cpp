// simd_accel.cpp - safe conversion and DSP kernels with optional ESP-DSP acceleration.
#include "runtime/simd/simd_accel.h"

#include <Arduino.h>

#include <cstring>

#include "runtime/memory/caps_allocator.h"

#ifndef UI_ENABLE_SIMD_PATH
#define UI_ENABLE_SIMD_PATH 0
#endif

#ifndef UI_SIMD_USE_ESP_DSP
#define UI_SIMD_USE_ESP_DSP 1
#endif

#if (UI_SIMD_USE_ESP_DSP != 0) && __has_include(<dsps_mul.h>)
#include <dsps_mul.h>
#define UI_SIMD_HAS_ESP_DSP 1
#else
#define UI_SIMD_HAS_ESP_DSP 0
#endif

namespace runtime::simd {

namespace {

constexpr bool kSimdPathEnabled = (UI_ENABLE_SIMD_PATH != 0);
constexpr size_t kAudioChunk = 128U;
constexpr uint32_t kBenchMinPixels = 64U;
constexpr uint32_t kBenchMaxPixels = 8192U;
constexpr uint32_t kBenchMinLoops = 1U;
constexpr uint32_t kBenchMaxLoops = 5000U;

uint16_t g_gray8_to_rgb565[256] = {};
bool g_gray_lut_ready = false;
SimdAccelStatus g_status = {
    };
bool g_status_initialized = false;

void initStatusDefaults() {
  g_status.simd_path_enabled = kSimdPathEnabled;
  g_status.esp_dsp_enabled = (UI_SIMD_HAS_ESP_DSP != 0);
}

void ensureStatusInitialized() {
  if (g_status_initialized) {
    return;
  }
  initStatusDefaults();
  g_status_initialized = true;
}

inline uint8_t clampU8(int32_t value) {
  if (value < 0) {
    return 0U;
  }
  if (value > 255) {
    return 255U;
  }
  return static_cast<uint8_t>(value);
}

inline int16_t clampS16(int32_t value) {
  if (value < -32768) {
    return -32768;
  }
  if (value > 32767) {
    return 32767;
  }
  return static_cast<int16_t>(value);
}

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t red = static_cast<uint16_t>((r & 0xF8U) << 8U);
  const uint16_t green = static_cast<uint16_t>((g & 0xFCU) << 3U);
  const uint16_t blue = static_cast<uint16_t>(b >> 3U);
  return static_cast<uint16_t>(red | green | blue);
}

void ensureGrayLut() {
  if (g_gray_lut_ready) {
    return;
  }
  for (uint16_t g = 0U; g < 256U; ++g) {
    g_gray8_to_rgb565[g] = rgb565(static_cast<uint8_t>(g), static_cast<uint8_t>(g), static_cast<uint8_t>(g));
  }
  g_gray_lut_ready = true;
}

void gainQ15Scalar(int16_t* dst, const int16_t* src, int16_t gain_q15, size_t n) {
  if (dst == nullptr || src == nullptr || n == 0U) {
    return;
  }
  for (size_t i = 0U; i < n; ++i) {
    const int32_t mul = static_cast<int32_t>(src[i]) * static_cast<int32_t>(gain_q15);
    const int32_t rounded = (mul >= 0) ? (mul + (1 << 14)) : (mul - (1 << 14));
    dst[i] = clampS16(rounded >> 15);
  }
}

void mixQ15Scalar(int16_t* dst,
                  const int16_t* a,
                  const int16_t* b,
                  int16_t ga_q15,
                  int16_t gb_q15,
                  size_t n) {
  if (dst == nullptr || a == nullptr || b == nullptr || n == 0U) {
    return;
  }
  for (size_t i = 0U; i < n; ++i) {
    const int32_t acc = static_cast<int32_t>(a[i]) * static_cast<int32_t>(ga_q15) +
                        static_cast<int32_t>(b[i]) * static_cast<int32_t>(gb_q15);
    const int32_t rounded = (acc >= 0) ? (acc + (1 << 14)) : (acc - (1 << 14));
    dst[i] = clampS16(rounded >> 15);
  }
}

void gainQ15EspDsp(int16_t* dst, const int16_t* src, int16_t gain_q15, size_t n) {
#if UI_SIMD_HAS_ESP_DSP
  int16_t gain_buf[kAudioChunk] = {};
  size_t offset = 0U;
  while (offset < n) {
    size_t chunk = n - offset;
    if (chunk > kAudioChunk) {
      chunk = kAudioChunk;
    }
    for (size_t i = 0U; i < chunk; ++i) {
      gain_buf[i] = gain_q15;
    }
    const esp_err_t rc = dsps_mul_s16(src + offset,
                                      gain_buf,
                                      dst + offset,
                                      static_cast<int>(chunk),
                                      1,
                                      1,
                                      1,
                                      15);
    if (rc != ESP_OK) {
      gainQ15Scalar(dst + offset, src + offset, gain_q15, n - offset);
      return;
    }
    offset += chunk;
  }
#else
  (void)dst;
  (void)src;
  (void)gain_q15;
  (void)n;
#endif
}

template <typename T>
bool arraysEqual(const T* a, const T* b, size_t n) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  for (size_t i = 0U; i < n; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

const SimdAccelStatus& status() {
  ensureStatusInitialized();
  return g_status;
}

void resetBenchStatus() {
  ensureStatusInitialized();
  g_status.bench_runs = 0U;
  g_status.bench_loops = 0U;
  g_status.bench_pixels = 0U;
  g_status.bench_l8_to_rgb565_us = 0U;
  g_status.bench_idx8_to_rgb565_us = 0U;
  g_status.bench_rgb888_to_rgb565_us = 0U;
  g_status.bench_s16_gain_q15_us = 0U;
}

void simd_rgb565_copy(uint16_t* dst, const uint16_t* src, size_t n_px) {
  if (dst == nullptr || src == nullptr || n_px == 0U) {
    return;
  }
  std::memcpy(dst, src, n_px * sizeof(uint16_t));
}

void simd_rgb565_fill(uint16_t* dst, uint16_t color565, size_t n_px) {
  if (dst == nullptr || n_px == 0U) {
    return;
  }
  if ((reinterpret_cast<uintptr_t>(dst) & 0x3U) == 0U) {
    uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst);
    const uint32_t packed = (static_cast<uint32_t>(color565) << 16U) | color565;
    size_t i = 0U;
    for (; i + 1U < n_px; i += 2U) {
      dst32[i >> 1U] = packed;
    }
    if (i < n_px) {
      dst[i] = color565;
    }
    return;
  }
  for (size_t i = 0U; i < n_px; ++i) {
    dst[i] = color565;
  }
}

void simd_rgb565_bswap_copy(uint16_t* dst, const uint16_t* src, size_t n_px) {
  if (dst == nullptr || src == nullptr || n_px == 0U) {
    return;
  }
  for (size_t i = 0U; i < n_px; ++i) {
    const uint16_t value = src[i];
    dst[i] = static_cast<uint16_t>((value << 8U) | (value >> 8U));
  }
}

void simd_l8_to_rgb565(uint16_t* dst565, const uint8_t* src_l8, size_t n_px) {
  if (dst565 == nullptr || src_l8 == nullptr || n_px == 0U) {
    return;
  }
  ensureGrayLut();
  if ((reinterpret_cast<uintptr_t>(dst565) & 0x3U) == 0U) {
    uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst565);
    size_t i = 0U;
    for (; i + 1U < n_px; i += 2U) {
      const uint16_t c0 = g_gray8_to_rgb565[src_l8[i]];
      const uint16_t c1 = g_gray8_to_rgb565[src_l8[i + 1U]];
      dst32[i >> 1U] = (static_cast<uint32_t>(c1) << 16U) | c0;
    }
    if (i < n_px) {
      dst565[i] = g_gray8_to_rgb565[src_l8[i]];
    }
    return;
  }
  for (size_t i = 0U; i < n_px; ++i) {
    dst565[i] = g_gray8_to_rgb565[src_l8[i]];
  }
}

void simd_index8_to_rgb565(uint16_t* dst565,
                           const uint8_t* idx8,
                           const uint16_t* pal565_256,
                           size_t n_px) {
  if (dst565 == nullptr || idx8 == nullptr || pal565_256 == nullptr || n_px == 0U) {
    return;
  }
  if ((reinterpret_cast<uintptr_t>(dst565) & 0x3U) == 0U) {
    uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst565);
    size_t i = 0U;
    for (; i + 1U < n_px; i += 2U) {
      const uint16_t c0 = pal565_256[idx8[i]];
      const uint16_t c1 = pal565_256[idx8[i + 1U]];
      dst32[i >> 1U] = (static_cast<uint32_t>(c1) << 16U) | c0;
    }
    if (i < n_px) {
      dst565[i] = pal565_256[idx8[i]];
    }
    return;
  }
  for (size_t i = 0U; i < n_px; ++i) {
    dst565[i] = pal565_256[idx8[i]];
  }
}

void simd_rgb888_to_rgb565(uint16_t* dst565, const uint8_t* src_rgb888, size_t n_px) {
  if (dst565 == nullptr || src_rgb888 == nullptr || n_px == 0U) {
    return;
  }
  for (size_t i = 0U; i < n_px; ++i) {
    const uint8_t r = src_rgb888[(i * 3U) + 0U];
    const uint8_t g = src_rgb888[(i * 3U) + 1U];
    const uint8_t b = src_rgb888[(i * 3U) + 2U];
    dst565[i] = rgb565(r, g, b);
  }
}

void simd_yuv422_to_rgb565(uint16_t* dst565, const uint8_t* src_yuv422, size_t n_px) {
  if (dst565 == nullptr || src_yuv422 == nullptr || n_px == 0U) {
    return;
  }
  size_t i = 0U;
  for (; i + 1U < n_px; i += 2U) {
    const uint8_t y0 = src_yuv422[(i * 2U) + 0U];
    const uint8_t u = src_yuv422[(i * 2U) + 1U];
    const uint8_t y1 = src_yuv422[(i * 2U) + 2U];
    const uint8_t v = src_yuv422[(i * 2U) + 3U];

    const int32_t c0 = static_cast<int32_t>(y0) - 16;
    const int32_t c1 = static_cast<int32_t>(y1) - 16;
    const int32_t d = static_cast<int32_t>(u) - 128;
    const int32_t e = static_cast<int32_t>(v) - 128;

    const uint8_t r0 = clampU8((298 * c0 + 409 * e + 128) >> 8);
    const uint8_t g0 = clampU8((298 * c0 - 100 * d - 208 * e + 128) >> 8);
    const uint8_t b0 = clampU8((298 * c0 + 516 * d + 128) >> 8);
    const uint8_t r1 = clampU8((298 * c1 + 409 * e + 128) >> 8);
    const uint8_t g1 = clampU8((298 * c1 - 100 * d - 208 * e + 128) >> 8);
    const uint8_t b1 = clampU8((298 * c1 + 516 * d + 128) >> 8);

    dst565[i] = rgb565(r0, g0, b0);
    dst565[i + 1U] = rgb565(r1, g1, b1);
  }
  if (i < n_px) {
    const uint8_t y = src_yuv422[(i * 2U)];
    dst565[i] = rgb565(y, y, y);
  }
}

void simd_s16_gain_q15(int16_t* dst, const int16_t* src, int16_t gain_q15, size_t n) {
  if (dst == nullptr || src == nullptr || n == 0U) {
    return;
  }
#if UI_SIMD_HAS_ESP_DSP
  gainQ15EspDsp(dst, src, gain_q15, n);
#else
  gainQ15Scalar(dst, src, gain_q15, n);
#endif
}

void simd_s16_mix2_q15(int16_t* dst,
                       const int16_t* a,
                       const int16_t* b,
                       int16_t ga_q15,
                       int16_t gb_q15,
                       size_t n) {
  mixQ15Scalar(dst, a, b, ga_q15, gb_q15, n);
}

bool selfTest() {
  ensureStatusInitialized();
  g_status.selftest_runs += 1U;

  constexpr size_t kN = 257U;
  uint8_t l8[kN] = {};
  uint8_t idx[kN] = {};
  uint16_t pal[256] = {};
  uint16_t out_a[kN] = {};
  uint16_t out_b[kN] = {};
  uint8_t rgb888[kN * 3U] = {};
  uint8_t yuv422[(kN + 1U) * 2U] = {};
  int16_t s16_a[kN] = {};
  int16_t s16_b[kN] = {};
  int16_t s16_out[kN] = {};
  int16_t s16_ref[kN] = {};

  for (size_t i = 0U; i < kN; ++i) {
    l8[i] = static_cast<uint8_t>((i * 31U + 17U) & 0xFFU);
    idx[i] = static_cast<uint8_t>((i * 19U + 7U) & 0xFFU);
    rgb888[(i * 3U) + 0U] = static_cast<uint8_t>((i * 11U) & 0xFFU);
    rgb888[(i * 3U) + 1U] = static_cast<uint8_t>((i * 13U + 3U) & 0xFFU);
    rgb888[(i * 3U) + 2U] = static_cast<uint8_t>((i * 17U + 9U) & 0xFFU);
    yuv422[(i * 2U)] = static_cast<uint8_t>((i * 5U + 40U) & 0xFFU);
    yuv422[(i * 2U) + 1U] = static_cast<uint8_t>((i * 7U + 80U) & 0xFFU);
    s16_a[i] = static_cast<int16_t>((static_cast<int32_t>(i) * 97) - 12000);
    s16_b[i] = static_cast<int16_t>((static_cast<int32_t>(i) * 53) - 9000);
  }
  for (uint16_t i = 0U; i < 256U; ++i) {
    pal[i] = rgb565(static_cast<uint8_t>(i), static_cast<uint8_t>(255U - i), static_cast<uint8_t>(i ^ 0x5AU));
  }

  bool ok = true;

  simd_l8_to_rgb565(out_a, l8, kN);
  for (size_t i = 0U; i < kN; ++i) {
    out_b[i] = rgb565(l8[i], l8[i], l8[i]);
  }
  ok = ok && arraysEqual(out_a, out_b, kN);

  simd_index8_to_rgb565(out_a, idx, pal, kN);
  for (size_t i = 0U; i < kN; ++i) {
    out_b[i] = pal[idx[i]];
  }
  ok = ok && arraysEqual(out_a, out_b, kN);

  simd_rgb888_to_rgb565(out_a, rgb888, kN);
  for (size_t i = 0U; i < kN; ++i) {
    out_b[i] = rgb565(rgb888[(i * 3U) + 0U], rgb888[(i * 3U) + 1U], rgb888[(i * 3U) + 2U]);
  }
  ok = ok && arraysEqual(out_a, out_b, kN);

  simd_yuv422_to_rgb565(out_a, yuv422, kN - 1U);
  simd_yuv422_to_rgb565(out_b, yuv422, kN - 1U);
  ok = ok && arraysEqual(out_a, out_b, kN - 1U);

  simd_s16_gain_q15(s16_out, s16_a, static_cast<int16_t>(16384), kN);
  gainQ15Scalar(s16_ref, s16_a, static_cast<int16_t>(16384), kN);
  ok = ok && arraysEqual(s16_out, s16_ref, kN);

  simd_s16_mix2_q15(s16_out, s16_a, s16_b, static_cast<int16_t>(16384), static_cast<int16_t>(8192), kN);
  mixQ15Scalar(s16_ref, s16_a, s16_b, static_cast<int16_t>(16384), static_cast<int16_t>(8192), kN);
  ok = ok && arraysEqual(s16_out, s16_ref, kN);

  if (!ok) {
    g_status.selftest_failures += 1U;
  }
  return ok;
}

SimdBenchResult runBench(uint32_t loops, uint32_t pixels) {
  ensureStatusInitialized();
  if (loops < kBenchMinLoops) {
    loops = kBenchMinLoops;
  } else if (loops > kBenchMaxLoops) {
    loops = kBenchMaxLoops;
  }
  if (pixels < kBenchMinPixels) {
    pixels = kBenchMinPixels;
  } else if (pixels > kBenchMaxPixels) {
    pixels = kBenchMaxPixels;
  }

  SimdBenchResult result = {};
  result.loops = loops;
  result.pixels = pixels;

  uint8_t* l8 = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(pixels, "simd.bench.l8"));
  uint8_t* idx = static_cast<uint8_t*>(runtime::memory::CapsAllocator::allocPsram(pixels, "simd.bench.idx"));
  uint16_t* pal = static_cast<uint16_t*>(runtime::memory::CapsAllocator::allocInternalDma(sizeof(uint16_t) * 256U, "simd.bench.pal"));
  uint16_t* dst565 = static_cast<uint16_t*>(
      runtime::memory::CapsAllocator::allocPsram(static_cast<size_t>(pixels) * sizeof(uint16_t), "simd.bench.dst"));
  uint8_t* rgb888 = static_cast<uint8_t*>(
      runtime::memory::CapsAllocator::allocPsram(static_cast<size_t>(pixels) * 3U, "simd.bench.rgb888"));
  int16_t* s16_a = static_cast<int16_t*>(
      runtime::memory::CapsAllocator::allocPsram(static_cast<size_t>(pixels) * sizeof(int16_t), "simd.bench.s16a"));
  int16_t* s16_out = static_cast<int16_t*>(
      runtime::memory::CapsAllocator::allocPsram(static_cast<size_t>(pixels) * sizeof(int16_t), "simd.bench.s16out"));

  if (l8 == nullptr || idx == nullptr || pal == nullptr || dst565 == nullptr ||
      rgb888 == nullptr || s16_a == nullptr || s16_out == nullptr) {
    runtime::memory::CapsAllocator::release(l8);
    runtime::memory::CapsAllocator::release(idx);
    runtime::memory::CapsAllocator::release(pal);
    runtime::memory::CapsAllocator::release(dst565);
    runtime::memory::CapsAllocator::release(rgb888);
    runtime::memory::CapsAllocator::release(s16_a);
    runtime::memory::CapsAllocator::release(s16_out);
    return result;
  }

  for (uint32_t i = 0U; i < pixels; ++i) {
    l8[i] = static_cast<uint8_t>((i * 37U + 11U) & 0xFFU);
    idx[i] = static_cast<uint8_t>((i * 29U + 3U) & 0xFFU);
    rgb888[(i * 3U) + 0U] = static_cast<uint8_t>((i * 9U) & 0xFFU);
    rgb888[(i * 3U) + 1U] = static_cast<uint8_t>((i * 13U + 7U) & 0xFFU);
    rgb888[(i * 3U) + 2U] = static_cast<uint8_t>((i * 17U + 5U) & 0xFFU);
    s16_a[i] = static_cast<int16_t>((static_cast<int32_t>(i) * 23) - 12000);
  }
  for (uint16_t i = 0U; i < 256U; ++i) {
    pal[i] = rgb565(static_cast<uint8_t>(i), static_cast<uint8_t>(255U - i), static_cast<uint8_t>(i));
  }

  uint32_t started_us = micros();
  for (uint32_t loop = 0U; loop < loops; ++loop) {
    simd_l8_to_rgb565(dst565, l8, pixels);
  }
  result.l8_to_rgb565_us = micros() - started_us;

  started_us = micros();
  for (uint32_t loop = 0U; loop < loops; ++loop) {
    simd_index8_to_rgb565(dst565, idx, pal, pixels);
  }
  result.idx8_to_rgb565_us = micros() - started_us;

  started_us = micros();
  for (uint32_t loop = 0U; loop < loops; ++loop) {
    simd_rgb888_to_rgb565(dst565, rgb888, pixels);
  }
  result.rgb888_to_rgb565_us = micros() - started_us;

  started_us = micros();
  for (uint32_t loop = 0U; loop < loops; ++loop) {
    simd_s16_gain_q15(s16_out, s16_a, static_cast<int16_t>(21845), pixels);
  }
  result.s16_gain_q15_us = micros() - started_us;

  g_status.bench_runs += 1U;
  g_status.bench_loops = result.loops;
  g_status.bench_pixels = result.pixels;
  g_status.bench_l8_to_rgb565_us = result.l8_to_rgb565_us;
  g_status.bench_idx8_to_rgb565_us = result.idx8_to_rgb565_us;
  g_status.bench_rgb888_to_rgb565_us = result.rgb888_to_rgb565_us;
  g_status.bench_s16_gain_q15_us = result.s16_gain_q15_us;
  runtime::memory::CapsAllocator::release(l8);
  runtime::memory::CapsAllocator::release(idx);
  runtime::memory::CapsAllocator::release(pal);
  runtime::memory::CapsAllocator::release(dst565);
  runtime::memory::CapsAllocator::release(rgb888);
  runtime::memory::CapsAllocator::release(s16_a);
  runtime::memory::CapsAllocator::release(s16_out);
  return result;
}

}  // namespace runtime::simd
