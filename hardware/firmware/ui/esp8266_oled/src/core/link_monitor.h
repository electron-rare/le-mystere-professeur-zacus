#pragma once

#include <Arduino.h>

#include "telemetry_state.h"

namespace screen_core {

struct LinkMonitorState {
  bool linkEnabled = true;
  uint32_t lastByteMs = 0U;
  uint32_t linkDownSinceMs = 0U;
  uint32_t linkLostSinceMs = 0U;
  uint32_t peerRebootUntilMs = 0U;
};

uint32_t latestLinkTickMs(const TelemetryState& state, const LinkMonitorState& link);
uint32_t safeAgeMs(uint32_t nowMs, uint32_t tickMs);
bool isPhysicalLinkAlive(const TelemetryState& state,
                         const LinkMonitorState& link,
                         uint32_t nowMs,
                         uint32_t timeoutMs);
bool isLinkAlive(const TelemetryState& state,
                 LinkMonitorState* link,
                 uint32_t nowMs,
                 uint32_t timeoutMs,
                 uint32_t downConfirmMs);
bool isPeerRebootGraceActive(const LinkMonitorState& link, uint32_t nowMs);

}  // namespace screen_core
