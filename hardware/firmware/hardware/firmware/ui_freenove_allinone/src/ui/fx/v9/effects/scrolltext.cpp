#include "ui/fx/v9/effects/scrolltext.h"
#include <algorithm>

namespace fx::effects {

ScrolltextFx::ScrolltextFx(FxServices s) : FxBase(s) {}

void ScrolltextFx::init(const FxContext& ctx)
{
  (void)ctx;
  xoff = 0.0f;
}

void ScrolltextFx::update(const FxContext& ctx)
{
  xoff += speed * (float)ctx.dt * 60.0f; // tuned for 50/60 fps
}

static inline void putpix_i8(RenderTarget& rt, int x, int y, uint8_t v)
{
  if ((unsigned)x >= (unsigned)rt.w || (unsigned)y >= (unsigned)rt.h) return;
  rt.rowPtr<uint8_t>(y)[x] = std::max(rt.rowPtr<uint8_t>(y)[x], v);
}

void ScrolltextFx::render(const FxContext& ctx, RenderTarget& rt)
{
  if (rt.fmt != PixelFormat::I8 || !svc.luts) return;

  // Placeholder: draw a moving “text stripe” with sine-wave baseline
  int baseY = y;
  for (int x = 0; x < rt.w; x++) {
    uint8_t p = (uint8_t)((x * 256 / std::max(1, wavePeriod)) + (int)xoff);
    int dy = (int)((int32_t)svc.luts->sin(p) * waveAmp / 32767);
    int yy = baseY + dy;

    // shadow/highlight
    if (shadow)   putpix_i8(rt, x, yy+1, 30);
    if (highlight)putpix_i8(rt, x, yy-1, 220);
    putpix_i8(rt, x, yy, 180);
  }

  (void)ctx;
  (void)textId; // real text rendering would use assets->getText(textId) + font
}

} // namespace fx::effects
