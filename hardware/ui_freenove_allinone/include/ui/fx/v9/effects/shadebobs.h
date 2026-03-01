#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <vector>

namespace fx::effects {

class ShadebobsFx : public FxBase {
public:
  explicit ShadebobsFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  int bobs = 16;
  int radius = 8;
  float decay = 0.92f; // if you keep an internal buffer (optional)
  bool invertOnBar = false;

private:
  struct Bob { uint8_t a, b; };
  std::vector<Bob> bob;
};

} // namespace fx::effects
