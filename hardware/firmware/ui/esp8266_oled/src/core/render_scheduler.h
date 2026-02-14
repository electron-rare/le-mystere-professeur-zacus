#pragma once

#include <Arduino.h>

#include "../apps/screen_app.h"

namespace screen_core {

class RenderScheduler {
 public:
  RenderScheduler(const screen_apps::ScreenApp* const* apps, uint8_t count);

  const screen_apps::ScreenApp* select(const screen_apps::ScreenRenderContext& ctx) const;

 private:
  const screen_apps::ScreenApp* const* apps_ = nullptr;
  uint8_t count_ = 0U;
};

}  // namespace screen_core
