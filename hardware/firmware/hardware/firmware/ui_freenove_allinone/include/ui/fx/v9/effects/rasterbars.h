#pragma once
#include "ui/fx/v9/effects/fx_base.h"

namespace fx::effects {

class RasterbarsFx : public FxBase {
public:
  explicit RasterbarsFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  int bars = 6;
  int thickness = 18;
  float amp = 36.0f;
  float speed = 0.045f;
  int gradientSteps = 8;

private:
  float ph = 0.0f;
};

} // namespace fx::effects
