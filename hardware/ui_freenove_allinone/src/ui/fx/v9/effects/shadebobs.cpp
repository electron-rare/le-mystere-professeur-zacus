#include "ui/fx/v9/effects/shadebobs.h"
#include <algorithm>

namespace fx::effects {

ShadebobsFx::ShadebobsFx(FxServices s) : FxBase(s) {}

void ShadebobsFx::init(const FxContext& ctx)
{
  rng.seed(ctx.seed ^ 0xB0B5B0B5u);
  bob.clear();
  bob.reserve((size_t)bobs);
  for (int i = 0; i < bobs; i++) {
    Bob b;
    b.a = (uint8_t)rng.nextRange(0, 256);
    b.b = (uint8_t)rng.nextRange(0, 256);
    bob.push_back(b);
  }
}

void ShadebobsFx::update(const FxContext& /*ctx*/) {}

static inline uint8_t add_sat_u8(uint8_t a, uint8_t b)
{
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return (s > 255) ? 255 : (uint8_t)s;
}

void ShadebobsFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !svc.luts) return;

  // Optional: could decay whole buffer first if this effect owns a layer buffer.
  // Here: just draw bobs additively, leaving composition to engine.

  for (int i = 0; i < (int)bob.size(); i++) {
    uint8_t pa = (uint8_t)(bob[i].a + ctx.frame * 2);
    uint8_t pb = (uint8_t)(bob[i].b + ctx.frame * 3);

    int x = (rt.w/2) + (int)((int32_t)svc.luts->sin(pa) * (rt.w/3) / 32767);
    int y = (rt.h/2) + (int)((int32_t)svc.luts->cos(pb) * (rt.h/3) / 32767);

    // draw filled circle (cheap)
    for (int yy = -radius; yy <= radius; yy++) {
      int y2 = y + yy;
      if ((unsigned)y2 >= (unsigned)rt.h) continue;
      uint8_t* row = rt.rowPtr<uint8_t>(y2);
      int dx = radius - (yy < 0 ? -yy : yy);
      int x0 = x - dx;
      int x1 = x + dx;
      if (x0 < 0) x0 = 0;
      if (x1 >= rt.w) x1 = rt.w-1;
      uint8_t v = (uint8_t)(120 + (radius - (yy<0?-yy:yy)) * 10);
      for (int xx = x0; xx <= x1; xx++) row[xx] = add_sat_u8(row[xx], v);
    }
  }

  if (invertOnBar && ctx.barHit) {
    // simple invert (demoscene trick)
    for (int y = 0; y < rt.h; y++) {
      uint8_t* row = rt.rowPtr<uint8_t>(y);
      for (int x = 0; x < rt.w; x++) row[x] = (uint8_t)(255 - row[x]);
    }
  }
}

} // namespace fx::effects
