#include "ulock_app.h"

namespace screen_apps {

const char* ULockApp::id() const {
  return "ULOCK";
}

bool ULockApp::matches(const ScreenRenderContext& ctx) const {
  if (!ctx.linkEnabled || !ctx.linkAlive || ctx.state == nullptr) {
    return false;
  }
  return ctx.state->appStage == screen_core::kAppStageULockWaiting ||
         ctx.state->appStage == screen_core::kAppStageULockListening ||
         ctx.state->appStage == screen_core::kAppStageUSonFunctional;
}

void ULockApp::render(const ScreenRenderContext& ctx) const {
  if (ctx.state == nullptr) {
    return;
  }
  if (ctx.state->appStage == screen_core::kAppStageUSonFunctional) {
    if (ctx.ui.renderUnlockSequence != nullptr) {
      ctx.ui.renderUnlockSequence(ctx.nowMs);
    }
    return;
  }
  if (ctx.ui.renderULock != nullptr) {
    ctx.ui.renderULock(ctx.nowMs);
  }
}

}  // namespace screen_apps
