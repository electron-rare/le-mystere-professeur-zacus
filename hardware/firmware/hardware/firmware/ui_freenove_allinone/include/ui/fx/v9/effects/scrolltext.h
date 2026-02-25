#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <string>

namespace fx::effects {

class ScrolltextFx : public FxBase {
public:
  explicit ScrolltextFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  const char* textId = "greetz_01";
  float speed = 1.2f;
  int waveAmp = 14;
  int wavePeriod = 160;
  int y = 90;
  bool shadow = true;
  bool highlight = true;

private:
  float xoff = 0.0f;
};

} // namespace fx::effects
