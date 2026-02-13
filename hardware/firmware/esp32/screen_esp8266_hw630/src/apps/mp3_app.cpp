#include "mp3_app.h"

namespace screen_apps {

const char* Mp3App::id() const {
  return "MP3";
}

bool Mp3App::matches(const ScreenRenderContext& ctx) const {
  return ctx.linkEnabled && ctx.linkAlive && ctx.state != nullptr &&
         ctx.state->appStage == screen_core::kAppStageMp3;
}

void Mp3App::render(const ScreenRenderContext& ctx) const {
  if (ctx.ui.renderMp3 != nullptr) {
    ctx.ui.renderMp3();
  }
}

}  // namespace screen_apps
