#include "render_scheduler.h"

namespace screen_core {

RenderScheduler::RenderScheduler(const screen_apps::ScreenApp* const* apps, uint8_t count)
    : apps_(apps), count_(count) {}

const screen_apps::ScreenApp* RenderScheduler::select(const screen_apps::ScreenRenderContext& ctx) const {
  if (apps_ == nullptr || count_ == 0U) {
    return nullptr;
  }
  for (uint8_t i = 0U; i < count_; ++i) {
    const screen_apps::ScreenApp* app = apps_[i];
    if (app != nullptr && app->matches(ctx)) {
      return app;
    }
  }
  return nullptr;
}

}  // namespace screen_core
