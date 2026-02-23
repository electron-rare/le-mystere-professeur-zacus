// runtime_config_types.h - shared runtime config and LA trigger state.
#pragma once

#include <Arduino.h>

struct RuntimeNetworkConfig {
  static constexpr uint32_t kDefaultLocalRetryMs = 15000U;
  static constexpr uint8_t kMaxEspNowBootPeers = 10U;

  char hostname[33] = "zacus-freenove";
  char wifi_test_ssid[33] = "Les cils";
  char wifi_test_password[65] = "mascarade";
  char local_ssid[33] = "Les cils";
  char local_password[65] = "mascarade";
  char ap_default_ssid[33] = "Freenove-Setup";
  char ap_default_password[65] = "mascarade";
  bool force_ap_if_not_local = false;
  bool pause_local_retry_when_ap_client = false;
  uint32_t local_retry_ms = kDefaultLocalRetryMs;
  bool espnow_enabled_on_boot = true;
  bool espnow_bridge_to_story_event = true;
  uint8_t espnow_boot_peer_count = 0U;
  char espnow_boot_peers[kMaxEspNowBootPeers][18] = {};
};

struct RuntimeHardwareConfig {
  bool enabled_on_boot = true;
  uint32_t telemetry_period_ms = 2500U;
  bool led_auto_from_scene = true;
  bool mic_enabled = true;
  uint8_t mic_event_threshold_pct = 72U;
  char mic_event_name[32] = "SERIAL:MIC_SPIKE";
  bool mic_la_trigger_enabled = true;
  uint16_t mic_la_target_hz = 440U;
  uint16_t mic_la_tolerance_hz = 10U;
  uint8_t mic_la_max_abs_cents = 42U;
  uint8_t mic_la_min_confidence = 28U;
  uint8_t mic_la_min_level_pct = 8U;
  uint16_t mic_la_stable_ms = 3000U;
  uint16_t mic_la_release_ms = 50U;
  uint16_t mic_la_cooldown_ms = 1400U;
  uint32_t mic_la_timeout_ms = 60000U;
  char mic_la_event_name[32] = "SERIAL:BTN_NEXT";
  bool battery_enabled = true;
  uint8_t battery_low_pct = 20U;
  char battery_low_event_name[32] = "SERIAL:BATTERY_LOW";
};

struct LaTriggerRuntimeState {
  bool gate_active = false;
  bool sample_match = false;
  bool locked = false;
  bool dispatched = false;
  bool timeout_pending = false;
  uint32_t gate_entered_ms = 0U;
  uint32_t timeout_deadline_ms = 0U;
  uint32_t stable_since_ms = 0U;
  uint32_t last_match_ms = 0U;
  uint32_t stable_ms = 0U;
  uint32_t last_trigger_ms = 0U;
  uint16_t last_freq_hz = 0U;
  int16_t last_cents = 0;
  uint8_t last_confidence = 0U;
  uint8_t last_level_pct = 0U;
};
