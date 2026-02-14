#include "link_monitor.h"

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

}  // namespace screen_core
