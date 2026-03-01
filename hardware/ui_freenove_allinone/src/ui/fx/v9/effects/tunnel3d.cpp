#include "ui/fx/v9/effects/tunnel3d.h"
#include <cmath>
#include <algorithm>

namespace fx::effects {

static constexpr float kTwoPi = 6.2831853071795864769f;

Tunnel3DFx::Tunnel3DFx(FxServices s) : FxBase(s) {}

void Tunnel3DFx::buildTexture_()
{
  // 256x256 procedural texture (fast to sample with (v<<8)|u).
  tex_.assign(256 * 256, 0);

  for (int v = 0; v < 256; v++) {
    for (int u = 0; u < 256; u++) {
      // Big tile checker
      uint8_t check = (uint8_t)(((u >> 5) ^ (v >> 5)) & 1);

      // High-frequency diagonal stripes
      uint8_t stripes = (uint8_t)((u * 5 + v * 3) & 255);

      // Add subtle rings using v
      uint8_t ring = (uint8_t)((v & 31) < 2 ? 64 : 0);

      uint8_t val = check ? stripes : (uint8_t)(255 - stripes);
      val = (uint8_t)std::min<int>(255, (int)val + ring);

      tex_[(v << 8) | u] = val;
    }
  }
}

void Tunnel3DFx::buildMaps_(int w, int h)
{
  w_ = w;
  h_ = h;
  uMap_.assign((size_t)w * (size_t)h, 0);
  vMap_.assign((size_t)w * (size_t)h, 0);

  const float cx = (float)(w - 1) * 0.5f;
  const float cy = (float)(h - 1) * 0.5f;

  // Depth scaling constant: tuned for low-res
  const float K = (float)w * 32.0f;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float dx = (float)x - cx;
      float dy = (float)y - cy;

      float ang = atan2f(dy, dx); // -pi..pi
      int u = (int)lrintf((ang / kTwoPi + 0.5f) * 256.0f) & 255;

      float r = sqrtf(dx * dx + dy * dy);
      int v = 0;
      if (r > 0.001f) {
        v = (int)lrintf(K / r);
      }

      size_t i = (size_t)y * (size_t)w + (size_t)x;
      uMap_[i] = (uint8_t)u;
      vMap_[i] = (uint8_t)(v & 255);
    }
  }
}

void Tunnel3DFx::init(const FxContext& ctx)
{
  // Allocate everything in init (no allocations in render).
  int w = (ctx.internalW > 0) ? ctx.internalW : 160;
  int h = (ctx.internalH > 0) ? ctx.internalH : 120;

  buildTexture_();
  buildMaps_(w, h);

  uPhase_ = 0;
  vPhase_ = 0;
  palShift_ = 0;

  rng.seed(ctx.seed ^ 0x7E11A3Du);
}

void Tunnel3DFx::update(const FxContext& ctx)
{
  // Phases in 0..255
  const float du = rotSpeed * 256.0f * ctx.dt;
  const float dv = speed * 256.0f * ctx.dt;

  uPhase_ = (uint8_t)(uPhase_ + (uint8_t)lrintf(du));
  vPhase_ = (uint8_t)(vPhase_ + (uint8_t)lrintf(dv));

  palShift_ = (uint8_t)(palShift_ + palSpeed);
  if (ctx.beatHit) palShift_ = (uint8_t)(palShift_ + beatKick);
}

void Tunnel3DFx::render(const FxContext& /*ctx*/, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !rt.pixels) return;
  if (rt.w != w_ || rt.h != h_) {
    // Size mismatch: do a safe fallback (no new allocations) by centering in the known map size.
    // Recommended: keep internal resolution fixed (ex 160x120).
    const int w = std::min(rt.w, w_);
    const int h = std::min(rt.h, h_);
    for (int y = 0; y < h; y++) {
      uint8_t* out = rt.rowPtr<uint8_t>(y);
      const uint8_t* um = &uMap_[(size_t)y * (size_t)w_];
      const uint8_t* vm = &vMap_[(size_t)y * (size_t)w_];
      for (int x = 0; x < w; x++) {
        uint8_t u = (uint8_t)(um[x] + uPhase_);
        uint8_t v = (uint8_t)(vm[x] + vPhase_);
        out[x] = (uint8_t)(tex_[(v << 8) | u] + palShift_);
      }
    }
    return;
  }

  // Fast path
  uint8_t* out = (uint8_t*)rt.pixels;
  const size_t n = (size_t)w_ * (size_t)h_;

  // Tight loop: 2 adds + 1 table fetch
  for (size_t i = 0; i < n; i++) {
    uint8_t u = (uint8_t)(uMap_[i] + uPhase_);
    uint8_t v = (uint8_t)(vMap_[i] + vPhase_);
    out[i] = (uint8_t)(tex_[(v << 8) | u] + palShift_);
  }
}

} // namespace fx::effects
