#include "ui/fx/v9/effects/starfield.h"
#include "ui/fx/v9/gfx/blit.h"
#include <algorithm>

namespace fx::effects {

StarfieldFx::StarfieldFx(FxServices s) : FxBase(s) {}

void StarfieldFx::init(const FxContext& ctx)
{
  rng.seed(ctx.seed ^ 0xA53C9E1u);
  st.clear();
  st.reserve((size_t)stars);
  for (int i = 0; i < stars; i++) {
    Star s;
    s.x = (int)rng.nextRange(0, 160);
    s.y = (int)rng.nextRange(0, 120);
    s.z = (int)rng.nextRange(0, 3); // 0 near, 1 mid, 2 far
    st.push_back(s);
  }
}

void StarfieldFx::update(const FxContext& /*ctx*/) {}

void StarfieldFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8) return;

  // Render stars as brightness indexes (0..255). Keep background untouched: additive blend done by engine.
  uint8_t* p = (uint8_t*)rt.pixels;

  // Add drift
  int driftX = (int)(driftAmp * (float)svc.luts->sin((uint8_t)(ctx.frame)) / 32767.0f);
  int driftY = (int)(driftAmp * (float)svc.luts->cos((uint8_t)(ctx.frame)) / 32767.0f);

  for (Star& s : st) {
    float sp = (s.z == 0) ? speedNear : (s.z == 1 ? speedNear * 0.6f : speedNear * 0.35f);

    int x = (s.x + driftX);
    int y = (s.y + driftY);

    if ((unsigned)x < (unsigned)rt.w && (unsigned)y < (unsigned)rt.h) {
      uint8_t v = (s.z == 0) ? 220 : (s.z == 1 ? 160 : 110);
      p[y * rt.strideBytes + x] = std::min<uint8_t>(255, (uint8_t)(p[y * rt.strideBytes + x] + v));
    }

    // Move down for parallax
    s.y += (int)sp;
    if (s.y >= rt.h) {
      s.y -= rt.h;
      s.x = (int)rng.nextRange(0, (uint32_t)rt.w);
      s.z = (int)rng.nextRange(0, 3);
    }
  }
}

} // namespace fx::effects
