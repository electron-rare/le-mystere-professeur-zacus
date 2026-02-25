#pragma once
#include "ui/fx/v9/engine/types.h"
#include "ui/fx/v9/math/rng.h"
#include "ui/fx/v9/math/lut.h"
#include "ui/fx/v9/assets/assets.h"

namespace fx::effects {

struct FxServices {
  // Optional: provide asset manager, luts, rng to effects.
  assets::IAssetManager* assets = nullptr;
  const SinCosLUT* luts = nullptr;
};

class FxBase : public IFx {
public:
  explicit FxBase(FxServices s) : svc(s) {}
  virtual ~FxBase() = default;

protected:
  FxServices svc{};
  Rng32 rng{};
};

} // namespace fx::effects
