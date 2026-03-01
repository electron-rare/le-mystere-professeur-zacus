// la_trigger_service.h - LA detector matching and gate state updates.
#pragma once

#include <Arduino.h>

#include "hardware_manager.h"
#include "runtime/runtime_config_types.h"
#include "scenario_manager.h"

class LaTriggerService {
 public:
  struct UpdateResult {
    bool gate_active = false;
    bool timed_out = false;
    bool lock_ready = false;
  };

  static bool isTriggerStep(const ScenarioSnapshot& snapshot);
  static bool shouldEnforceMatchOnly(const RuntimeHardwareConfig& config,
                                     const ScenarioSnapshot& snapshot);
  static void resetState(LaTriggerRuntimeState* state, bool keep_cooldown = true);
  static void resetTimeout(LaTriggerRuntimeState* state, uint32_t now_ms, const char* source_tag);
  static uint8_t stablePercent(const RuntimeHardwareConfig& config,
                               const LaTriggerRuntimeState& state);
  static bool isSampleMatching(const RuntimeHardwareConfig& config,
                               const HardwareManager::Snapshot& hw,
                               bool relaxed_for_continuity);
  static UpdateResult update(const RuntimeHardwareConfig& config,
                             LaTriggerRuntimeState* state,
                             const ScenarioSnapshot& snapshot,
                             const HardwareManager::Snapshot& hw,
                             uint32_t now_ms);
};
