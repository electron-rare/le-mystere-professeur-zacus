#pragma once
#include "ui/fx/v9/effects/fx_base.h"

namespace fx::effects {

class PlasmaFx : public FxBase {
public:
  explicit PlasmaFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  float speed = 0.035f;
  float contrast = 0.80f;

private:
  uint8_t phase = 0;
};

} // namespace fx::effects
