// resource_coordinator.cpp - adaptive gating for graphics/camera/microphone coex.
#include "runtime/resource/resource_coordinator.h"

#include <cctype>
#include <cstring>

#include "ui_manager.h"

namespace runtime::resource {

namespace {

void normalizeToken(const char* text, char* out, size_t out_size) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == nullptr) {
    return;
  }
  size_t write = 0U;
  for (size_t i = 0U; text[i] != '\0' && write + 1U < out_size; ++i) {
    const char ch = text[i];
    if (ch == ' ' || ch == '-' || ch == '.') {
      out[write++] = '_';
    } else {
      out[write++] = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
  }
  out[write] = '\0';
}

uint32_t safeDelta(uint32_t now_value, uint32_t prev_value) {
  if (now_value >= prev_value) {
    return now_value - prev_value;
  }
  return now_value;
}

}  // namespace

void ResourceCoordinator::begin(const ResourceCoordinatorConfig& config) {
  config_ = config;
  snapshot_ = {};
  snapshot_.profile = ResourceProfile::kGfxFocus;
  prev_flush_overflow_ = 0U;
  prev_flush_blocked_ = 0U;
}

void ResourceCoordinator::setProfile(ResourceProfile profile) {
  snapshot_.profile = profile;
  snapshot_.allow_camera_ops =
      (profile == ResourceProfile::kGfxPlusCamSnapshot) &&
      !snapshot_.graphics_pressure &&
      (static_cast<int32_t>(snapshot_.now_ms - snapshot_.camera_cooldown_until_ms) >= 0);
}

ResourceProfile ResourceCoordinator::profile() const {
  return snapshot_.profile;
}

const char* ResourceCoordinator::profileName() const {
  return profileName(snapshot_.profile);
}

bool ResourceCoordinator::parseAndSetProfile(const char* token) {
  ResourceProfile parsed = snapshot_.profile;
  if (!parseProfile(token, &parsed)) {
    return false;
  }
  setProfile(parsed);
  return true;
}

const char* ResourceCoordinator::profileName(ResourceProfile profile) {
  switch (profile) {
    case ResourceProfile::kGfxPlusMic:
      return "gfx_plus_mic";
    case ResourceProfile::kGfxPlusCamSnapshot:
      return "gfx_plus_cam_snapshot";
    case ResourceProfile::kGfxFocus:
    default:
      return "gfx_focus";
  }
}

bool ResourceCoordinator::parseProfile(const char* token, ResourceProfile* out_profile) {
  if (out_profile == nullptr || token == nullptr || token[0] == '\0') {
    return false;
  }
  char normalized[40] = {0};
  normalizeToken(token, normalized, sizeof(normalized));
  if (std::strcmp(normalized, "gfx_focus") == 0 ||
      std::strcmp(normalized, "focus") == 0) {
    *out_profile = ResourceProfile::kGfxFocus;
    return true;
  }
  if (std::strcmp(normalized, "gfx_plus_mic") == 0 ||
      std::strcmp(normalized, "gfx_mic") == 0 ||
      std::strcmp(normalized, "mic") == 0) {
    *out_profile = ResourceProfile::kGfxPlusMic;
    return true;
  }
  if (std::strcmp(normalized, "gfx_plus_cam_snapshot") == 0 ||
      std::strcmp(normalized, "gfx_cam") == 0 ||
      std::strcmp(normalized, "cam") == 0) {
    *out_profile = ResourceProfile::kGfxPlusCamSnapshot;
    return true;
  }
  return false;
}

void ResourceCoordinator::update(const UiMemorySnapshot& ui_snapshot, uint32_t now_ms) {
  const uint32_t overflow_delta = safeDelta(ui_snapshot.flush_overflow, prev_flush_overflow_);
  const uint32_t blocked_delta = safeDelta(ui_snapshot.flush_blocked, prev_flush_blocked_);
  prev_flush_overflow_ = ui_snapshot.flush_overflow;
  prev_flush_blocked_ = ui_snapshot.flush_blocked;

  snapshot_.now_ms = now_ms;
  snapshot_.flush_overflow_delta = overflow_delta;
  snapshot_.flush_blocked_delta = blocked_delta;
  snapshot_.last_draw_avg_us = ui_snapshot.draw_time_avg_us;
  snapshot_.last_draw_max_us = ui_snapshot.draw_time_max_us;
  snapshot_.last_flush_avg_us = ui_snapshot.flush_time_avg_us;
  snapshot_.last_flush_max_us = ui_snapshot.flush_time_max_us;

  const bool pressure_event =
      (overflow_delta >= config_.flush_overflow_delta_threshold) ||
      (blocked_delta >= config_.flush_blocked_delta_threshold) ||
      (ui_snapshot.draw_time_max_us >= config_.draw_max_us_threshold) ||
      (ui_snapshot.flush_time_max_us >= config_.flush_max_us_threshold);
  if (pressure_event) {
    snapshot_.pressure_until_ms = now_ms + config_.pressure_hold_ms;
  }

  snapshot_.graphics_pressure =
      (snapshot_.pressure_until_ms != 0U) &&
      (static_cast<int32_t>(snapshot_.pressure_until_ms - now_ms) > 0);
  const bool mic_target = (snapshot_.profile == ResourceProfile::kGfxPlusMic);
  if (mic_target) {
    snapshot_.mic_should_run = true;
    snapshot_.mic_hold_until_ms = now_ms + config_.mic_hold_ms;
  } else if (snapshot_.mic_should_run &&
             static_cast<int32_t>(now_ms - snapshot_.mic_hold_until_ms) >= 0) {
    snapshot_.mic_should_run = false;
  }
  snapshot_.mic_force_on = snapshot_.mic_should_run;
  snapshot_.allow_camera_ops =
      (snapshot_.profile == ResourceProfile::kGfxPlusCamSnapshot) &&
      !snapshot_.graphics_pressure &&
      (static_cast<int32_t>(now_ms - snapshot_.camera_cooldown_until_ms) >= 0);
}

bool ResourceCoordinator::shouldRunMic() const {
  return snapshot_.mic_should_run;
}

bool ResourceCoordinator::shouldForceMicOn() const {
  return shouldRunMic();
}

bool ResourceCoordinator::allowsCameraWork() const {
  return snapshot_.allow_camera_ops;
}

bool ResourceCoordinator::approveCameraOperation() {
  const bool allowed = allowsCameraWork();
  if (allowed) {
    snapshot_.camera_allowed_ops += 1U;
    snapshot_.camera_cooldown_until_ms = snapshot_.now_ms + config_.camera_cooldown_ms;
    snapshot_.allow_camera_ops = false;
  } else {
    snapshot_.camera_blocked_ops += 1U;
  }
  return allowed;
}

ResourceCoordinatorSnapshot ResourceCoordinator::snapshot() const {
  return snapshot_;
}

}  // namespace runtime::resource
