#pragma once

#include "ui/fx/v9/effects/fx_base.h"

namespace fx::effects {

class HourglassFx : public FxBase {
 public:
  explicit HourglassFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  float speed = 0.22f;
  float glitch = 0.08f;

 private:
  uint32_t start_frame_ = 0U;
};

}  // namespace fx::effects
