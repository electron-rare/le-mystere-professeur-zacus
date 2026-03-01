#pragma once
#include "ui/fx/v9/effects/fx_base.h"

namespace fx::effects {

class TransitionFlashFx : public FxBase {
public:
  explicit TransitionFlashFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  int flashFrames = 2;
  float fadeOut = 1.2f;

private:
  int startFrame = 0;
};

} // namespace fx::effects
