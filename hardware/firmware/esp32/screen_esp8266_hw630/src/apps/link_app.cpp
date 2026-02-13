#include "link_app.h"

namespace screen_apps {

const char* LinkApp::id() const {
  return "LINK";
}

bool LinkApp::matches(const ScreenRenderContext& ctx) const {
  return !ctx.linkEnabled || !ctx.linkAlive;
}

void LinkApp::render(const ScreenRenderContext& ctx) const {
  if (!ctx.linkEnabled) {
    if (ctx.ui.renderLinkDisabled != nullptr) {
      ctx.ui.renderLinkDisabled();
    }
    return;
  }
  if (ctx.recoveringLink) {
    if (ctx.ui.renderLinkRecovering != nullptr) {
      ctx.ui.renderLinkRecovering(ctx.nowMs);
    }
    return;
  }
  if (ctx.ui.renderLinkDown != nullptr) {
    ctx.ui.renderLinkDown(ctx.nowMs);
  }
}

}  // namespace screen_apps
