#pragma once

#include "screen_app.h"

namespace screen_apps {

class Mp3App : public ScreenApp {
 public:
  const char* id() const override;
  bool matches(const ScreenRenderContext& ctx) const override;
  void render(const ScreenRenderContext& ctx) const override;
};

}  // namespace screen_apps
