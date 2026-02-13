#include "boot_app.h"

namespace screen_apps {

const char* BootApp::id() const {
  return "BOOT";
}

bool BootApp::matches(const ScreenRenderContext& ctx) const {
  if (ctx.bootSplashActive || !ctx.hasValidState || ctx.state == nullptr) {
    return true;
  }
  return ctx.state->startupStage == screen_core::kStartupStageBootValidation;
}

void BootApp::render(const ScreenRenderContext& ctx) const {
  if (ctx.bootSplashActive && ctx.ui.renderBootSplash != nullptr) {
    ctx.ui.renderBootSplash(ctx.nowMs);
    return;
  }
  if (ctx.ui.renderStartup != nullptr) {
    ctx.ui.renderStartup(ctx.nowMs);
  }
}

}  // namespace screen_apps
