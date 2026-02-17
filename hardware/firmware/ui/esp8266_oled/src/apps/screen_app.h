#pragma once



#include "../core/telemetry_state.h"

namespace screen_apps {

struct UiHooks {
  void (*renderBootSplash)(uint32_t nowMs) = nullptr;
  void (*renderStartup)(uint32_t nowMs) = nullptr;
  void (*renderULock)(uint32_t nowMs) = nullptr;
  void (*renderUnlockSequence)(uint32_t nowMs) = nullptr;
  void (*renderMp3)() = nullptr;
  void (*renderLinkDown)(uint32_t nowMs) = nullptr;
  void (*renderLinkRecovering)(uint32_t nowMs) = nullptr;
  void (*renderLinkDisabled)() = nullptr;
  void (*renderFallback)() = nullptr;
};

struct ScreenRenderContext {
  uint32_t nowMs = 0U;
  bool linkEnabled = true;
  bool linkAlive = false;
  bool hasValidState = false;
  bool bootSplashActive = false;
  bool recoveringLink = false;
  const screen_core::TelemetryState* state = nullptr;
  UiHooks ui = {};
};

class ScreenApp {
 public:
  virtual ~ScreenApp() = default;
  virtual const char* id() const = 0;
  virtual bool matches(const ScreenRenderContext& ctx) const = 0;
  virtual void render(const ScreenRenderContext& ctx) const = 0;
};

}  // namespace screen_apps
