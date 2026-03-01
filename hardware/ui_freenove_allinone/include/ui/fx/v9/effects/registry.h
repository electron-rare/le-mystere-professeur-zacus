#pragma once
#include "ui/fx/v9/engine/engine.h"
#include "ui/fx/v9/effects/fx_base.h"

namespace fx::effects {

// Registers sample FX into the engine.
void registerAll(Engine& e, FxServices svc);

} // namespace fx::effects
