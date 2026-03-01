#pragma once
#include <cstdint>
#include <cstddef>
#include <array>

namespace fx {

enum class PixelFormat : uint8_t { I8, RGB565 };

enum class BlendMode : uint8_t {
  REPLACE,
  ADD_CLAMP,
  DARKEN_HALF, // shadow style: (dst>>1)&mask
  ALPHA_MASK   // uses a per-pixel 0..255 mask (optional)
};

struct Palette565 {
  const uint16_t* data = nullptr; // 256 entries
};

struct RenderTarget {
  void* pixels = nullptr;
  int   w = 0;
  int   h = 0;
  int   strideBytes = 0;
  PixelFormat fmt = PixelFormat::I8;

  // If fmt==I8, palette565 maps index->RGB565 for final output or debug preview.
  const uint16_t* palette565 = nullptr;

  // Hint for SIMD: pixels and stride meet 16-byte alignment constraints.
  bool aligned16 = false;

  template<typename T>
  T* rowPtr(int y) const {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(pixels) + (size_t)y * (size_t)strideBytes);
  }
};

struct FxContext {
  uint32_t frame = 0;

  float demoTime = 0.0f;  // seconds since demo start
  float t = 0.0f;         // seconds since current clip start
  float dt = 0.0f;        // seconds per frame

  float bpm = 120.0f;
  uint32_t beat = 0;      // beat index since demo start
  uint32_t bar = 0;       // bar index (4 beats) since demo start
  float beatPhase = 0.0f; // 0..1 within current beat

  uint32_t seed = 0;      // global seed ^ clip seed
  bool beatHit = false;   // true on frame of beat boundary
  bool barHit  = false;   // true on frame of bar boundary

  // Internal render target info (used by 3D FX to pre-allocate maps/buffers).
  int internalW = 0;
  int internalH = 0;
  PixelFormat internalFmt = PixelFormat::I8;
};

// Base interface: no allocations in render()
struct IFx {
  virtual ~IFx() = default;
  virtual void init(const FxContext& ctx) = 0;
  virtual void update(const FxContext& ctx) = 0;
  virtual void render(const FxContext& ctx, RenderTarget& rt) = 0;
};

} // namespace fx
