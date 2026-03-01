// la_trigger_service.cpp - LA detector matching and gate state updates.
#include "runtime/la_trigger_service.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "resources/screen_scene_registry.h"

namespace {

constexpr uint16_t kLaDetectionToleranceHz = 25U;
constexpr uint16_t kLaMatchLevelDenom = 7000U;
constexpr uint8_t kLaMatchLevelFloorPct = 20U;
constexpr uint8_t kLaMatchConfidenceBoostMax = 12U;
constexpr uint8_t kLaMatchConfidenceBoostScale = 4U;
constexpr uint8_t kLaMatchConfidenceFloor = 6U;
constexpr uint8_t kLaMatchRelaxedBonus = 6U;
constexpr uint8_t kLaMatchRelaxedConfidenceFloor = 10U;
constexpr uint8_t kLaMatchRelaxedConfidencePenalty = 10U;
constexpr uint8_t kLaMatchConsecutiveFrames = 2U;

uint8_t estimatedLevelPct(const HardwareManager::Snapshot& hw) {
  if (hw.mic_level_percent > 0U) {
    return hw.mic_level_percent;
  }
  const uint16_t noise_floor = hw.mic_noise_floor;
  const uint16_t effective_peak =
      (hw.mic_peak > noise_floor) ? static_cast<uint16_t>(hw.mic_peak - noise_floor) : 0U;
  return static_cast<uint8_t>(
      std::min<uint16_t>(100U, static_cast<uint16_t>((effective_peak * 100U) / kLaMatchLevelDenom)));
}

bool isSceneId(const ScenarioSnapshot& snapshot, const char* expected) {
  if (snapshot.screen_scene_id == nullptr || expected == nullptr) {
    return false;
  }
  const char* normalized_scene_id = storyNormalizeScreenSceneId(snapshot.screen_scene_id);
  if (normalized_scene_id == nullptr) {
    return false;
  }
  return std::strcmp(normalized_scene_id, expected) == 0;
}

bool isLaDetectorScene(const ScenarioSnapshot& snapshot) {
  return isSceneId(snapshot, "SCENE_LA_DETECTOR");
}

bool isLefouDetectorScene(const ScenarioSnapshot& snapshot) {
  return isSceneId(snapshot, "SCENE_LEFOU_DETECTOR");
}

uint16_t toleranceForTarget(const RuntimeHardwareConfig& config) {
  const uint16_t configured = config.mic_la_tolerance_hz;
  if (configured == 0U) {
    return 0U;
  }
  return (configured > kLaDetectionToleranceHz) ? kLaDetectionToleranceHz : configured;
}

uint8_t centsLimitForTarget(const RuntimeHardwareConfig& config,
                            uint16_t target_hz,
                            uint16_t effective_tolerance_hz) {
  uint8_t limit = config.mic_la_max_abs_cents;
  if (target_hz > 0U && effective_tolerance_hz > 0U && target_hz >= effective_tolerance_hz) {
    const float target_hz_f = static_cast<float>(target_hz);
    const float upper_hz = target_hz_f + static_cast<float>(effective_tolerance_hz);
    const float tolerance_cents_f = 1200.0f * std::log2(upper_hz / target_hz_f);
    if (std::isfinite(tolerance_cents_f) && tolerance_cents_f > 0.0f) {
      const uint8_t tolerance_cents =
          static_cast<uint8_t>(std::min<float>(120.0f, std::ceil(tolerance_cents_f)));
      if (tolerance_cents > limit) {
        limit = tolerance_cents;
      }
    }
  }
  return limit;
}

uint8_t dynamicConfidenceFloor(uint8_t base_confidence,
                               uint8_t level_pct,
                               bool relaxed_for_continuity) {
  uint8_t dynamic_boost = 0U;
  if (level_pct > kLaMatchLevelFloorPct) {
    const uint8_t raw_boost =
        static_cast<uint8_t>((level_pct - kLaMatchLevelFloorPct) / kLaMatchConfidenceBoostScale);
    dynamic_boost = (raw_boost > kLaMatchConfidenceBoostMax) ? kLaMatchConfidenceBoostMax : raw_boost;
  }
  if (relaxed_for_continuity && dynamic_boost >= kLaMatchRelaxedBonus) {
    dynamic_boost -= kLaMatchRelaxedBonus;
  }
  if (base_confidence <= dynamic_boost) {
    return kLaMatchConfidenceFloor;
  }
  const uint16_t raw_floor = base_confidence - dynamic_boost;
  return (raw_floor < kLaMatchConfidenceFloor) ? kLaMatchConfidenceFloor
                                                : static_cast<uint8_t>(raw_floor);
}

}  // namespace

bool LaTriggerService::isTriggerStep(const ScenarioSnapshot& snapshot) {
  if (snapshot.step != nullptr && snapshot.step->id != nullptr &&
      std::strcmp(snapshot.step->id, "STEP_WAIT_ETAPE2") == 0) {
    return true;
  }
  return isLaDetectorScene(snapshot) || isLefouDetectorScene(snapshot);
}

bool LaTriggerService::shouldEnforceMatchOnly(const RuntimeHardwareConfig& config,
                                              const ScenarioSnapshot& snapshot) {
  return config.mic_la_trigger_enabled && isTriggerStep(snapshot);
}

void LaTriggerService::resetState(LaTriggerRuntimeState* state, bool keep_cooldown) {
  if (state == nullptr) {
    return;
  }
  const uint32_t last_trigger_ms = state->last_trigger_ms;
  *state = LaTriggerRuntimeState();
  if (keep_cooldown) {
    state->last_trigger_ms = last_trigger_ms;
  }
}

void LaTriggerService::resetTimeout(LaTriggerRuntimeState* state,
                                    uint32_t now_ms,
                                    const char* source_tag) {
  if (state == nullptr || !state->gate_active) {
    return;
  }
  state->gate_entered_ms = now_ms;
  state->timeout_pending = false;
  state->timeout_deadline_ms = 0U;
  Serial.printf("[LA_TRIGGER] timer reset by %s at %lu ms\n",
                (source_tag != nullptr && source_tag[0] != '\0') ? source_tag : "unknown",
                static_cast<unsigned long>(now_ms));
}

uint8_t LaTriggerService::stablePercent(const RuntimeHardwareConfig& config,
                                        const LaTriggerRuntimeState& state) {
  if (!config.mic_la_trigger_enabled) {
    return 0U;
  }
  if (config.mic_la_stable_ms == 0U) {
    return state.locked ? 100U : 0U;
  }
  uint32_t percent = (state.stable_ms * 100U) / config.mic_la_stable_ms;
  if (percent > 100U) {
    percent = 100U;
  }
  return static_cast<uint8_t>(percent);
}

bool isSampleMatchingForTarget(const RuntimeHardwareConfig& config,
                               const HardwareManager::Snapshot& hw,
                               bool relaxed_for_continuity,
                               uint16_t target_hz) {
  if (target_hz == 0U) {
    return false;
  }
  if (!hw.mic_ready || hw.mic_freq_hz == 0U) {
    return false;
  }

  const uint8_t detected_level = estimatedLevelPct(hw);
  if (relaxed_for_continuity) {
    if (detected_level == 0U && hw.mic_pitch_confidence < kLaMatchRelaxedConfidenceFloor) {
      return false;
    }
  } else if (detected_level < config.mic_la_min_level_pct) {
    return false;
  }

  const uint8_t dynamic_min_confidence =
      dynamicConfidenceFloor(config.mic_la_min_confidence, detected_level, relaxed_for_continuity);
  uint8_t required_confidence = dynamic_min_confidence;
  if (relaxed_for_continuity && required_confidence > kLaMatchRelaxedConfidencePenalty) {
    required_confidence -= kLaMatchRelaxedConfidencePenalty;
  }
  if (required_confidence < kLaMatchRelaxedConfidenceFloor) {
    required_confidence = kLaMatchRelaxedConfidenceFloor;
  }
  if (hw.mic_pitch_confidence < required_confidence) {
    return false;
  }

  int16_t abs_cents = hw.mic_pitch_cents;
  if (abs_cents < 0) {
    abs_cents = static_cast<int16_t>(-abs_cents);
  }
  const uint16_t effective_tolerance_hz = toleranceForTarget(config);
  uint8_t cents_limit = centsLimitForTarget(config, target_hz, effective_tolerance_hz);
  if (relaxed_for_continuity && cents_limit < 120U) {
    cents_limit = static_cast<uint8_t>(std::min<uint16_t>(120U, static_cast<uint16_t>(cents_limit + 4U)));
  }
  if (static_cast<uint8_t>(abs_cents) > cents_limit) {
    return false;
  }
  const uint16_t tolerance_hz =
      static_cast<uint16_t>(effective_tolerance_hz + (relaxed_for_continuity ? 2U : 0U));
  const int32_t delta_hz = static_cast<int32_t>(hw.mic_freq_hz) - static_cast<int32_t>(target_hz);
  const int32_t abs_delta_hz = (delta_hz < 0) ? -delta_hz : delta_hz;
  return abs_delta_hz <= static_cast<int32_t>(tolerance_hz);
}

bool LaTriggerService::isSampleMatching(const RuntimeHardwareConfig& config,
                                        const HardwareManager::Snapshot& hw,
                                        bool relaxed_for_continuity) {
  return isSampleMatchingForTarget(config, hw, relaxed_for_continuity, config.mic_la_target_hz);
}

LaTriggerService::UpdateResult LaTriggerService::update(const RuntimeHardwareConfig& config,
                                                        LaTriggerRuntimeState* state,
                                                        const ScenarioSnapshot& snapshot,
                                                        const HardwareManager::Snapshot& hw,
                                                        uint32_t now_ms) {
  UpdateResult result;
  if (state == nullptr) {
    return result;
  }

  state->last_freq_hz = hw.mic_freq_hz;
  state->last_cents = hw.mic_pitch_cents;
  state->last_confidence = hw.mic_pitch_confidence;
  state->last_level_pct = hw.mic_level_percent;
  const uint32_t match_continuity_window_ms = static_cast<uint32_t>(config.mic_la_release_ms);

  const bool gate_was_active = state->gate_active;
  const bool gate_active = config.mic_enabled && config.mic_la_trigger_enabled && isTriggerStep(snapshot);
  state->gate_active = gate_active;
  result.gate_active = gate_active;
  if (!gate_active) {
    resetState(state);
    state->gate_active = false;
    return result;
  }
  if (!gate_was_active) {
    state->gate_entered_ms = now_ms;
    state->timeout_pending = false;
    state->timeout_deadline_ms = 0U;
  }
  if (state->timeout_pending) {
    return result;
  }

  const bool sequence_mode =
      config.mic_la_sequence_enabled &&
      isLefouDetectorScene(snapshot) &&
      (config.mic_la_sequence_count > 0U);
  if (sequence_mode) {
    const uint8_t max_notes = RuntimeHardwareConfig::kLaSequenceMaxNotes;
    uint8_t sequence_count = config.mic_la_sequence_count;
    if (sequence_count > max_notes) {
      sequence_count = max_notes;
    }
    if (sequence_count == 0U) {
      return result;
    }
    if (state->sequence_index >= sequence_count) {
      state->sequence_index = sequence_count;
      state->locked = true;
      state->stable_ms = config.mic_la_stable_ms;
    } else {
      const uint16_t note_hold_ms =
          (config.mic_la_sequence_note_hold_ms < 100U) ? 100U : config.mic_la_sequence_note_hold_ms;
      const uint16_t target_hz = config.mic_la_sequence_hz[state->sequence_index];
      state->sequence_target_hz = target_hz;
      const bool strict_match = isSampleMatchingForTarget(config, hw, false, target_hz);
      const bool relaxed_match = isSampleMatchingForTarget(config, hw, true, target_hz);
      state->sample_match = strict_match;

      const uint32_t continuity_ms = std::max<uint32_t>(1U, static_cast<uint32_t>(config.mic_la_release_ms));
      const uint32_t dt_ms = (state->last_match_ms == 0U) ? 0U : (now_ms - state->last_match_ms);
      const bool continuity_ok = (dt_ms == 0U) || (dt_ms <= continuity_ms);

      if (strict_match || (relaxed_match && continuity_ok)) {
        if (state->sequence_note_started_ms == 0U) {
          state->sequence_note_started_ms = now_ms;
          state->stable_since_ms = now_ms;
        }
        if (strict_match) {
          state->last_match_ms = now_ms;
        }
        const uint32_t held_ms = now_ms - state->sequence_note_started_ms;
        if (held_ms >= note_hold_ms) {
          if (state->sequence_index < sequence_count) {
            ++state->sequence_index;
          }
          state->sequence_note_started_ms = 0U;
          state->la_consecutive_match_count = 0U;
          state->la_match_start_ms = now_ms;
          state->last_match_ms = now_ms;
          if (state->sequence_index >= sequence_count) {
            state->locked = true;
            state->stable_ms = config.mic_la_stable_ms;
          }
        }
      } else {
        const bool expired = (state->last_match_ms != 0U) && (dt_ms > continuity_ms);
        if (expired) {
          state->last_match_ms = 0U;
          state->sequence_note_started_ms = 0U;
          state->la_consecutive_match_count = 0U;
        }
      }

      if (!state->locked) {
        uint32_t progress_units = static_cast<uint32_t>(state->sequence_index) * 100U;
        if (state->sequence_index < sequence_count && state->sequence_note_started_ms > 0U) {
          const uint32_t held_ms = now_ms - state->sequence_note_started_ms;
          const uint32_t note_pct = std::min<uint32_t>(100U, (held_ms * 100U) / note_hold_ms);
          progress_units += note_pct;
        }
        const uint32_t progress_pct = std::min<uint32_t>(
            100U, progress_units / std::max<uint8_t>(1U, sequence_count));
        state->stable_ms = (config.mic_la_stable_ms > 0U)
                               ? (config.mic_la_stable_ms * progress_pct) / 100U
                               : progress_pct;
      }
    }

    if (!state->locked && config.mic_la_timeout_ms > 0U && state->gate_entered_ms > 0U &&
        (now_ms - state->gate_entered_ms) >= config.mic_la_timeout_ms) {
      result.timed_out = true;
      return result;
    }
    if (!state->locked || state->dispatched) {
      return result;
    }
    if (state->last_trigger_ms > 0U && (now_ms - state->last_trigger_ms) < config.mic_la_cooldown_ms) {
      return result;
    }
    result.lock_ready = true;
    return result;
  }
  state->sequence_index = 0U;
  state->sequence_note_started_ms = 0U;
  state->sequence_target_hz = 0U;

  const uint32_t effective_window_ms = (match_continuity_window_ms == 0U) ? 1U : match_continuity_window_ms;
  const bool sample_match = isSampleMatching(config, hw, false);
  const bool continuity_match = isSampleMatching(config, hw, true);
  const uint32_t dt_ms = (state->last_match_ms == 0U) ? 0U : (now_ms - state->last_match_ms);
  const bool match_window_expired = (dt_ms > 0U) && (dt_ms > effective_window_ms);
  const bool seeded_by_strict_match =
      (!match_window_expired) && (state->la_consecutive_match_count > 0U);
  const bool has_match_for_progress = sample_match || (seeded_by_strict_match && continuity_match);
  state->sample_match = sample_match;
  bool has_stable_candidate = false;

  if (has_match_for_progress) {
    const bool starts_new_window = (state->last_match_ms == 0U) || match_window_expired;
    if (starts_new_window) {
      state->la_match_start_ms = now_ms;
      state->la_consecutive_match_count = sample_match ? 1U : 0U;
      state->stable_since_ms = now_ms;
    } else if (sample_match && state->la_consecutive_match_count < 255U) {
      ++state->la_consecutive_match_count;
    }
    const bool strict_stability_ready = (state->la_consecutive_match_count >= kLaMatchConsecutiveFrames);

    const bool can_progress_stable =
        (sample_match && (strict_stability_ready || state->stable_ms > 0U)) ||
        ((state->stable_ms > 0U || state->la_consecutive_match_count > 0U) && continuity_match);
    if (can_progress_stable && dt_ms > 0U &&
        (state->stable_ms < config.mic_la_stable_ms)) {
      const uint32_t stable_gain_ms = std::min<uint32_t>(dt_ms, effective_window_ms);
      const uint32_t next_stable_ms = state->stable_ms + stable_gain_ms;
      state->stable_ms =
          (next_stable_ms >= config.mic_la_stable_ms) ? config.mic_la_stable_ms : next_stable_ms;
    }

    state->last_match_ms = now_ms;
    has_stable_candidate = true;
  } else if (state->last_match_ms != 0U && dt_ms > effective_window_ms) {
    state->last_match_ms = 0U;
    state->la_match_start_ms = 0U;
    state->la_consecutive_match_count = 0U;
  }

  if (!has_stable_candidate && state->stable_ms == 0U) {
    state->stable_since_ms = 0U;
  }

  state->locked = state->stable_ms >= config.mic_la_stable_ms;
  if (!state->locked && config.mic_la_timeout_ms > 0U && state->gate_entered_ms > 0U &&
      (now_ms - state->gate_entered_ms) >= config.mic_la_timeout_ms) {
    result.timed_out = true;
    return result;
  }
  if (!state->locked || state->dispatched) {
    return result;
  }
  if (state->last_trigger_ms > 0U && (now_ms - state->last_trigger_ms) < config.mic_la_cooldown_ms) {
    return result;
  }
  result.lock_ready = true;
  return result;
}
