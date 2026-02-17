#include "link_monitor.h"

#include <Arduino.h>

namespace screen_core {

uint32_t latestLinkTickMs(const TelemetryState& state, const LinkMonitorState& link) {
  if (state.lastRxMs > link.lastByteMs) {
    return state.lastRxMs;
  }
  return link.lastByteMs;
}

uint32_t safeAgeMs(uint32_t nowMs, uint32_t tickMs) {
  if (tickMs == 0U || nowMs < tickMs) {
    return 0U;
  }
  return nowMs - tickMs;
}

bool isPhysicalLinkAlive(const TelemetryState& state,
                         const LinkMonitorState& link,
                         uint32_t nowMs,
                         uint32_t timeoutMs) {
  if (!link.linkEnabled) {
    return false;
  }

  const uint32_t lastTickMs = latestLinkTickMs(state, link);
  if (lastTickMs == 0U) {
    return false;
  }
  if (nowMs < lastTickMs) {
    return true;
  }
  return (nowMs - lastTickMs) <= timeoutMs;
}

bool isLinkAlive(const TelemetryState& state,
                 LinkMonitorState* link,
                 uint32_t nowMs,
                 uint32_t timeoutMs,
                 uint32_t downConfirmMs) {
  if (link == nullptr || !link->linkEnabled) {
    return false;
  }

  if (latestLinkTickMs(state, *link) == 0U) {
    return false;
  }

  if (isPhysicalLinkAlive(state, *link, nowMs, timeoutMs)) {
    link->linkDownSinceMs = 0U;
    return true;
  }

  if (link->linkDownSinceMs == 0U) {
    link->linkDownSinceMs = nowMs;
    return true;
  }

  return (nowMs - link->linkDownSinceMs) < downConfirmMs;
}

bool isPeerRebootGraceActive(const LinkMonitorState& link, uint32_t nowMs) {
  return link.peerRebootUntilMs != 0U &&
         static_cast<int32_t>(nowMs - link.peerRebootUntilMs) < 0;
}


void logLinkVerdict(const TelemetryState& state, const LinkMonitorState& link, uint32_t nowMs) {
  bool connected = isPhysicalLinkAlive(state, link, nowMs, 2000U); // timeout 2s par défaut
  Serial.println("[UI_LINK] STATUS: connected=" + String(connected ? 1 : 0));
  if (isPeerRebootGraceActive(link, nowMs)) {
    Serial.println("[UI_LINK] STATUS: peer reboot grace active");
  }
  if (!connected && link.peerRebootUntilMs == 0U) {
    Serial.println("[UI_LINK] ERROR: PANIC or link lost");
  }

  // Ajout d'exemples pour screen/app screen, debug, erreur
  // Serial.println("[UI_LINK] SCREEN: " + String(currentScreenName));
  // Serial.println("[UI_LINK] DEBUG: " + String(debugMsg));
  // Serial.println("[UI_LINK] ERROR: " + String(errorMsg));
}

}  // namespace screen_core
