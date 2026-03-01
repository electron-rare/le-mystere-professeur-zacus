#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <vector>

namespace fx::effects {

class StarfieldFx : public FxBase {
public:
  explicit StarfieldFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  // Params (can be overridden via clip params/mods)
  int layers = 3;
  int stars = 160;
  float speedNear = 2.2f;
  float driftAmp = 2.0f;

private:
  struct Star { int x, y; int z; }; // z: depth bucket
  std::vector<Star> st;
};

} // namespace fx::effects
