#pragma once

#include <Arduino.h>

#include "telemetry_state.h"
#include "ui_link_v2.h"

namespace screen_core {

bool parseStatFrame(const UiLinkFrame& frame, TelemetryState* out, uint32_t nowMs);

}  // namespace screen_core
