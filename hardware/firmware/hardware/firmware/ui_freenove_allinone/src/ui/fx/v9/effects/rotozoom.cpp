#include "ui/fx/v9/effects/rotozoom.h"
#include <cmath>
#include <algorithm>

namespace fx::effects {

static constexpr float kTwoPi = 6.2831853071795864769f;

RotozoomFx::RotozoomFx(FxServices s) : FxBase(s) {}

void RotozoomFx::buildTexture_()
{
  tex_.assign(256 * 256, 0);

  for (int v = 0; v < 256; v++) {
    for (int u = 0; u < 256; u++) {
      // Tile checker with gradient
      uint8_t c = (uint8_t)(((u >> 4) ^ (v >> 4)) & 1);
      uint8_t g = (uint8_t)((u + v) & 255);
      uint8_t val = c ? g : (uint8_t)(255 - g);

      // Add a "grid" highlight
      if ((u & 31) == 0 || (v & 31) == 0) val = (uint8_t)std::min<int>(255, (int)val + 60);

      tex_[(v << 8) | u] = val;
    }
  }
}

void RotozoomFx::init(const FxContext& ctx)
{
  w_ = (ctx.internalW > 0) ? ctx.internalW : 160;
  h_ = (ctx.internalH > 0) ? ctx.internalH : 120;

  buildTexture_();

  uOff_ = 0;
  vOff_ = 0;
  palShift_ = 0;

  rng.seed(ctx.seed ^ 0x70702001u);
}

void RotozoomFx::update(const FxContext& ctx)
{
  // Scroll offsets in texture space (cycles/sec -> 256 units/sec -> 16.16)
  const float du = scrollU * 256.0f * ctx.dt;
  const float dv = scrollV * 256.0f * ctx.dt;
  uOff_ += (int32_t)lrintf(du * 65536.0f);
  vOff_ += (int32_t)lrintf(dv * 65536.0f);

  palShift_ = (uint8_t)(palShift_ + palSpeed);
  if (ctx.beatHit) palShift_ = (uint8_t)(palShift_ + beatKick);
}

void RotozoomFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !rt.pixels) return;

  const int w = rt.w;
  const int h = rt.h;

  // Angle and zoom per frame (float math once per frame)
  const float a = ctx.demoTime * rotSpeed * kTwoPi;
  const float z = zoomBase + zoomAmp * sinf(ctx.demoTime * zoomFreq * kTwoPi);

  const float ca = cosf(a) * z;
  const float sa = sinf(a) * z;

  // Fixed-point increments (16.16)
  const int32_t du_dx = (int32_t)lrintf(ca * 65536.0f);
  const int32_t dv_dx = (int32_t)lrintf(sa * 65536.0f);
  const int32_t du_dy = (int32_t)lrintf(-sa * 65536.0f);
  const int32_t dv_dy = (int32_t)lrintf(ca * 65536.0f);

  const int32_t cx = (w / 2) << 16;
  const int32_t cy = (h / 2) << 16;

  // Start UV at top-left
  int32_t u0 = uOff_ - ((cx * du_dx + cy * du_dy) >> 16);
  int32_t v0 = vOff_ - ((cx * dv_dx + cy * dv_dy) >> 16);

  for (int y = 0; y < h; y++) {
    uint8_t* out = rt.rowPtr<uint8_t>(y);

    int32_t u = u0;
    int32_t v = v0;

    for (int x = 0; x < w; x++) {
      uint8_t uu = (uint8_t)((u >> 16) & 255);
      uint8_t vv = (uint8_t)((v >> 16) & 255);
      out[x] = (uint8_t)(tex_[(vv << 8) | uu] + palShift_);
      u += du_dx;
      v += dv_dx;
    }

    u0 += du_dy;
    v0 += dv_dy;
  }
}

} // namespace fx::effects
