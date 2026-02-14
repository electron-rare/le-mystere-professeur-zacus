#pragma once

#include <Arduino.h>

#include "../../core/telemetry_state.h"
#include "../../core/text_slots.h"
#include "../display_backend.h"

namespace screen_gfx {

struct SceneRenderContext {
  DisplayBackend* display = nullptr;
  const screen_core::TelemetryState* state = nullptr;
  const screen_core::TextSlots* text = nullptr;
  uint32_t nowMs = 0U;
};

void renderMp3LectureScene(const SceneRenderContext& ctx);
void renderMp3ListeScene(const SceneRenderContext& ctx);
void renderMp3ReglagesScene(const SceneRenderContext& ctx);
void renderMp3SceneV3(const SceneRenderContext& ctx);

}  // namespace screen_gfx
