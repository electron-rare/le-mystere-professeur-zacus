#include "ui/fx/v9/effects/transition_flash.h"
#include <cmath>

namespace fx::effects {

TransitionFlashFx::TransitionFlashFx(FxServices s) : FxBase(s) {}

void TransitionFlashFx::init(const FxContext& ctx)
{
  startFrame = (int)ctx.frame;
}

void TransitionFlashFx::update(const FxContext& /*ctx*/) {}

void TransitionFlashFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8) return;

  int f = (int)ctx.frame - startFrame;
  if (f < flashFrames) {
    // full white
    for (int y = 0; y < rt.h; y++) {
      uint8_t* row = rt.rowPtr<uint8_t>(y);
      for (int x = 0; x < rt.w; x++) row[x] = 255;
    }
    return;
  }

  float t = ctx.t;
  float a = std::max(0.0f, 1.0f - (t / std::max(0.001f, fadeOut)));
  int vv = (int)lrintf(a * 255.0f);
  if (vv < 0) vv = 0;
  if (vv > 255) vv = 255;
  uint8_t v = (uint8_t)vv;

  // draw a fade overlay (additive)
  for (int y = 0; y < rt.h; y++) {
    uint8_t* row = rt.rowPtr<uint8_t>(y);
    for (int x = 0; x < rt.w; x++) {
      uint16_t s = (uint16_t)row[x] + v;
      row[x] = (s > 255) ? 255 : (uint8_t)s;
    }
  }
}

} // namespace fx::effects
