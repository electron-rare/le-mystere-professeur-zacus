#pragma once

#include <Arduino.h>

#include "telemetry_state.h"

namespace screen_core {

bool parseStatFrame(const char* frame,
                    TelemetryState* out,
                    uint32_t nowMs,
                    uint32_t* crcErrorCount);

}  // namespace screen_core
