#include "ui/fx/v9/effects/registry.h"
#include "ui/fx/v9/effects/starfield.h"
#include "ui/fx/v9/effects/scrolltext.h"
#include "ui/fx/v9/effects/rasterbars.h"
#include "ui/fx/v9/effects/shadebobs.h"
#include "ui/fx/v9/effects/plasma.h"
#include "ui/fx/v9/effects/transition_flash.h"
#include "ui/fx/v9/effects/tunnel3d.h"
#include "ui/fx/v9/effects/rotozoom.h"
#include "ui/fx/v9/effects/wirecube.h"

namespace fx::effects {

void registerAll(Engine& e, FxServices svc)
{
  e.registerFx("starfield", [svc] {
    return std::unique_ptr<IFx>(new StarfieldFx(svc));
  });
  e.registerFx("scrolltext", [svc] {
    return std::unique_ptr<IFx>(new ScrolltextFx(svc));
  });
  e.registerFx("rasterbars", [svc] {
    return std::unique_ptr<IFx>(new RasterbarsFx(svc));
  });
  e.registerFx("shadebobs", [svc] {
    return std::unique_ptr<IFx>(new ShadebobsFx(svc));
  });
  e.registerFx("plasma", [svc] {
    return std::unique_ptr<IFx>(new PlasmaFx(svc));
  });
  e.registerFx("transition_flash", [svc] {
    return std::unique_ptr<IFx>(new TransitionFlashFx(svc));
  });
  e.registerFx("tunnel3d", [svc] {
    return std::unique_ptr<IFx>(new Tunnel3DFx(svc));
  });
  e.registerFx("rotozoom", [svc] {
    return std::unique_ptr<IFx>(new RotozoomFx(svc));
  });
  e.registerFx("wirecube", [svc] {
    return std::unique_ptr<IFx>(new WireCubeFx(svc));
  });
}

} // namespace fx::effects
