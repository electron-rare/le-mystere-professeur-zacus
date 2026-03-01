#include "ui/fx/v9/effects/wirecube.h"
#include <cmath>
#include <algorithm>

namespace fx::effects {

static constexpr float kTwoPi = 6.2831853071795864769f;

WireCubeFx::WireCubeFx(FxServices s) : FxBase(s) {}

void WireCubeFx::init(const FxContext& ctx)
{
  ax_ = ay_ = az_ = 0.0f;
  pulse_ = 0.0f;
  rng.seed(ctx.seed ^ 0xC0B3C0B3u);
}

void WireCubeFx::update(const FxContext& ctx)
{
  ax_ += rotX * kTwoPi * ctx.dt;
  ay_ += rotY * kTwoPi * ctx.dt;
  az_ += rotZ * kTwoPi * ctx.dt;

  // Keep angles small
  if (ax_ > kTwoPi) ax_ -= kTwoPi;
  if (ay_ > kTwoPi) ay_ -= kTwoPi;
  if (az_ > kTwoPi) az_ -= kTwoPi;

  if (beatPulse) {
    if (ctx.beatHit) pulse_ = 1.0f;
    // exponential decay
    pulse_ *= 0.90f;
  }
}

void WireCubeFx::plot_(RenderTarget& rt, int x, int y, uint8_t v)
{
  if ((unsigned)x >= (unsigned)rt.w || (unsigned)y >= (unsigned)rt.h) return;
  uint8_t* row = rt.rowPtr<uint8_t>(y);
  row[x] = std::max(row[x], v);
}

void WireCubeFx::line_(RenderTarget& rt, int x0, int y0, int x1, int y1, uint8_t v)
{
  int dx = std::abs(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  while (true) {
    plot_(rt, x0, y0, v);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err << 1;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void WireCubeFx::render(const FxContext& /*ctx*/, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !rt.pixels) return;

  const int w = rt.w;
  const int h = rt.h;
  const int cx = w / 2;
  const int cy = h / 2;

  // cube vertices in [-1,1]
  static const float V[8][3] = {
    {-1,-1,-1}, {+1,-1,-1}, {+1,+1,-1}, {-1,+1,-1},
    {-1,-1,+1}, {+1,-1,+1}, {+1,+1,+1}, {-1,+1,+1},
  };
  static const uint8_t E[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
  };

  const float sx = sinf(ax_), cxr = cosf(ax_);
  const float sy = sinf(ay_), cyr = cosf(ay_);
  const float sz = sinf(az_), czr = cosf(az_);

  float scale = (float)std::min(w, h) * 0.22f;
  // Beat pulse slightly increases size
  scale *= (1.0f + 0.25f * pulse_);

  // Transform and project
  int px[8], py[8];
  for (int i = 0; i < 8; i++) {
    float x = V[i][0];
    float y = V[i][1];
    float z = V[i][2];

    // Rotate Y
    float x1 = x * cyr + z * sy;
    float z1 = -x * sy + z * cyr;

    // Rotate X
    float y2 = y * cxr - z1 * sx;
    float z2 = y * sx + z1 * cxr;

    // Rotate Z
    float x3 = x1 * czr - y2 * sz;
    float y3 = x1 * sz + y2 * czr;

    float zz = z2 + zOffset;
    if (zz < 0.3f) zz = 0.3f;

    float inv = fov / zz;

    px[i] = cx + (int)lrintf(x3 * inv * scale);
    py[i] = cy + (int)lrintf(y3 * inv * scale);
  }

  uint8_t v = intensity;
  if (pulse_ > 0.0f) v = (uint8_t)std::min<int>(255, (int)intensity + (int)lrintf(35.0f * pulse_));

  for (int e = 0; e < 12; e++) {
    int a = E[e][0];
    int b = E[e][1];
    line_(rt, px[a], py[a], px[b], py[b], v);
  }
}

} // namespace fx::effects
