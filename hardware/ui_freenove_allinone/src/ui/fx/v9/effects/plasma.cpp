#include "ui/fx/v9/effects/plasma.h"
#include <cmath>
#include <algorithm>

namespace fx::effects {

PlasmaFx::PlasmaFx(FxServices s) : FxBase(s) {}

void PlasmaFx::init(const FxContext& /*ctx*/) { phase = 0; }
void PlasmaFx::update(const FxContext& /*ctx*/) {}

void PlasmaFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !svc.luts) return;

  phase = (uint8_t)(phase + (uint8_t)lrintf(speed * 255.0f));

  for (int y = 0; y < rt.h; y++) {
    uint8_t* row = rt.rowPtr<uint8_t>(y);
    for (int x = 0; x < rt.w; x++) {
      // Classic plasma formula using LUTs in Q15
      uint8_t a = (uint8_t)(x * 2 + phase);
      uint8_t b = (uint8_t)(y * 3 + phase);
      uint8_t c = (uint8_t)((x + y) + phase);
      int v = (int)svc.luts->sin(a) + (int)svc.luts->sin(b) + (int)svc.luts->sin(c);
      // v in [-3*32767, 3*32767]
      float vf = (float)v / (3.0f * 32767.0f);
      vf = vf * contrast;
      int idx = (int)lrintf((vf * 0.5f + 0.5f) * 255.0f);
      if (idx < 0) idx = 0; if (idx > 255) idx = 255;
      row[x] = (uint8_t)idx;
    }
  }
}

} // namespace fx::effects
