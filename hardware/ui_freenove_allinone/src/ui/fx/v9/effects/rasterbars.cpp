#include "ui/fx/v9/effects/rasterbars.h"
#include <algorithm>
#include <cmath>

namespace fx::effects {

RasterbarsFx::RasterbarsFx(FxServices s) : FxBase(s) {}

void RasterbarsFx::init(const FxContext& /*ctx*/) { ph = 0.0f; }
void RasterbarsFx::update(const FxContext& /*ctx*/) {}

static inline uint8_t add_sat_u8(uint8_t a, uint8_t b)
{
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return (s > 255) ? 255 : (uint8_t)s;
}

void RasterbarsFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !svc.luts) return;

  ph += speed * (float)rt.w;
  uint8_t basePhase = (uint8_t)lrintf(ph);

  for (int b = 0; b < bars; b++) {
    // center y for each bar
    uint8_t p = (uint8_t)(basePhase + b * (256 / bars));
    float s = (float)svc.luts->sin(p) / 32767.0f;
    int cy = (rt.h/2) + (int)lrintf(s * amp);

    int y0 = cy - thickness/2;
    int y1 = y0 + thickness;

    for (int y = y0; y < y1; y++) {
      if ((unsigned)y >= (unsigned)rt.h) continue;
      uint8_t* row = rt.rowPtr<uint8_t>(y);

      // gradient across thickness
      int dy = y - y0;
      int g = (dy * gradientSteps) / std::max(1, thickness);
      uint8_t v = (uint8_t)(80 + g * (170 / std::max(1, gradientSteps-1)));

      // fill whole row with additive contribution (cheap)
      for (int x = 0; x < rt.w; x++) row[x] = add_sat_u8(row[x], v);
    }
  }
}

} // namespace fx::effects
