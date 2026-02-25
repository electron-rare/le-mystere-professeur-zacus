// main.cpp - Freenove ESP32-S3 all-in-one runtime loop.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

#include "audio_manager.h"
#include "button_manager.h"
#include "camera_manager.h"
#include "hardware_manager.h"
#include "media_manager.h"
#include "ui_freenove_config.h"
#include "network_manager.h"
#include "app/runtime_scene_service.h"
#include "app/runtime_serial_service.h"
#include "app/runtime_web_service.h"
#include "runtime/app_coordinator.h"
#include "runtime/la_trigger_service.h"
#include "runtime/perf/perf_monitor.h"
#include "runtime/provisioning/boot_mode_store.h"
#include "runtime/provisioning/credential_store.h"
#include "runtime/resource/resource_coordinator.h"
#include "runtime/scene_fx_orchestrator.h"
#include "runtime/simd/simd_accel.h"
#include "runtime/simd/simd_accel_bench.h"
#include "runtime/runtime_config_service.h"
#include "runtime/runtime_services.h"
#include "runtime/runtime_config_types.h"
#include "scenario_manager.h"
#include "scenarios/default_scenario_v2.h"
#include "storage_manager.h"
#include "system/boot_report.h"
#include "system/rate_limited_log.h"
#include "system/runtime_metrics.h"
#include "touch_manager.h"
#include "ui/audio_player/amiga_audio_player.h"
#include "ui/camera_capture/win311_camera_ui.h"
#include "ui_manager.h"

#ifndef ZACUS_FW_VERSION
#define ZACUS_FW_VERSION "dev"
#endif

void runRuntimeIteration(uint32_t now_ms);

namespace {

constexpr const char* kDefaultScenarioFile = "/story/scenarios/DEFAULT.json";
constexpr const char* kFirmwareName = "freenove_esp32s3";
constexpr const char* kDiagAudioFile = "/music/boot_radio.mp3";
constexpr size_t kSerialLineCapacity = 192U;
constexpr bool kBootDiagnosticTone = true;
constexpr const char* kEspNowBroadcastTarget = "broadcast";
constexpr const char* kStepWinEtape = "WIN_ETAPE1";
constexpr const char* kPackWin = "PACK_WIN";
constexpr const char* kWebAuthHeaderName = "Authorization";
constexpr const char* kWebAuthBearerPrefix = "Bearer ";
constexpr const char* kProvisionStatusPath = "/api/provision/status";
constexpr const char* kSetupWifiConnectPath = "/api/wifi/connect";
constexpr const char* kSetupNetworkWifiConnectPath = "/api/network/wifi/connect";
constexpr size_t kWebAuthTokenCapacity = 65U;
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
constexpr const char* kAmpMusicPathPrimary = "/music";
constexpr const char* kAmpMusicPathFallback1 = "/audio/music";
constexpr const char* kAmpMusicPathFallback2 = "/audio";
#endif
constexpr const char* kCameraSceneId = "SCENE_PHOTO_MANAGER";
constexpr const char* kMediaManagerSceneId = "SCENE_MEDIA_MANAGER";

AudioManager g_audio;
ScenarioManager g_scenario;
UiManager g_ui;
StorageManager g_storage;
ButtonManager g_buttons;
TouchManager g_touch;
NetworkManager g_network;
HardwareManager g_hardware;
CameraManager g_camera;
MediaManager g_media;
CredentialStore g_credential_store;
BootModeStore g_boot_mode_store;
RuntimeNetworkConfig g_network_cfg;
RuntimeHardwareConfig g_hardware_cfg;
CameraManager::Config g_camera_cfg;
MediaManager::Config g_media_cfg;
WebServer g_web_server(80);
bool g_web_started = false;
bool g_web_disconnect_sta_pending = false;
uint32_t g_web_disconnect_sta_at_ms = 0U;
bool g_hardware_started = false;
uint32_t g_next_hw_telemetry_ms = 0U;
bool g_mic_tuner_stream_enabled = false;
uint16_t g_mic_tuner_stream_period_ms = 250U;
uint32_t g_next_mic_tuner_stream_ms = 0U;
bool g_mic_event_armed = true;
bool g_battery_low_latched = false;
LaTriggerRuntimeState g_la_trigger;
bool g_la_dispatch_in_progress = false;
bool g_has_ring_sent_for_win_etape = false;
bool g_win_etape_ui_refresh_pending = false;
bool g_boot_media_manager_mode = false;
bool g_setup_mode = true;
bool g_web_auth_required = false;
bool g_resource_profile_auto = true;
char g_web_auth_token[kWebAuthTokenCapacity] = {0};
char g_last_action_step_key[72] = {0};
char g_serial_line[kSerialLineCapacity] = {0};
size_t g_serial_line_len = 0U;
RuntimeServices g_runtime_services;
AppCoordinator g_app_coordinator;
runtime::resource::ResourceCoordinator g_resource_coordinator;
SceneFxOrchestrator g_scene_fx_orchestrator;
RuntimeSerialService g_runtime_serial_service;
RuntimeSceneService g_runtime_scene_service;
RuntimeWebService g_runtime_web_service;
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
ui::audio::AmigaAudioPlayer g_amp_player;
bool g_amp_ready = false;
bool g_amp_scene_active = false;
char g_amp_base_dir[40] = "/music";
#endif
ui::camera::Win311CameraUI g_camera_player;
bool g_camera_scene_active = false;
bool g_camera_scene_ready = false;

bool dispatchScenarioEventByName(const char* event_name, uint32_t now_ms);
bool parseBoolToken(const char* text, bool* out_value);

const char* audioPackToFile(const char* pack_id) {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return nullptr;
  }
  if (std::strcmp(pack_id, "PACK_BOOT_RADIO") == 0) {
    return "/music/boot_radio.mp3";
  }
  if (std::strcmp(pack_id, "PACK_SONAR_HINT") == 0) {
    return "/music/sonar_hint.mp3";
  }
  if (std::strcmp(pack_id, "PACK_MORSE_HINT") == 0) {
    return "/music/morse_hint.mp3";
  }
  if (std::strcmp(pack_id, "PACK_WIN") == 0) {
    return "/music/win.mp3";
  }
  if (std::strcmp(pack_id, "PACK_CONFIRM_WIN_ETAPE1") == 0) {
    return "/music/confirm_win_etape1.mp3";
  }
  if (std::strcmp(pack_id, "PACK_CONFIRM_WIN_ETAPE2") == 0) {
    return "/music/confirm_win_etape2.mp3";
  }
  return "/music/placeholder.mp3";
}

const char* scenarioIdFromSnapshot(const ScenarioSnapshot& snapshot) {
  return (snapshot.scenario != nullptr && snapshot.scenario->id != nullptr) ? snapshot.scenario->id : "n/a";
}

const char* stepIdFromSnapshot(const ScenarioSnapshot& snapshot) {
  return (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
}

bool loadScenarioByIdPreferStoryFile(const char* scenario_id, String* out_source, String* out_path) {
  if (scenario_id == nullptr || scenario_id[0] == '\0') {
    return false;
  }
  String normalized_id = scenario_id;
  normalized_id.trim();
  if (normalized_id.isEmpty()) {
    return false;
  }

  String story_path = "/story/scenarios/";
  story_path += normalized_id;
  story_path += ".json";
  if (g_storage.fileExists(story_path.c_str())) {
    if (g_scenario.begin(story_path.c_str())) {
      if (out_source != nullptr) {
        *out_source = "story_file";
      }
      if (out_path != nullptr) {
        *out_path = story_path;
      }
      return true;
    }
  }

  if (g_scenario.beginById(normalized_id.c_str())) {
    if (out_source != nullptr) {
      *out_source = "builtin";
    }
    if (out_path != nullptr) {
      out_path->remove(0);
    }
    return true;
  }
  return false;
}

void toLowerAsciiInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  for (size_t index = 0; text[index] != '\0'; ++index) {
    text[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[index])));
  }
}

void toUpperAsciiInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  for (size_t index = 0; text[index] != '\0'; ++index) {
    text[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[index])));
  }
}

void trimAsciiInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  const size_t len = std::strlen(text);
  size_t begin = 0U;
  while (begin < len && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = len;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U]))) {
    --end;
  }
  const size_t out_len = (end > begin) ? (end - begin) : 0U;
  if (begin > 0U && out_len > 0U) {
    std::memmove(text, &text[begin], out_len);
  }
  text[out_len] = '\0';
}

void copyText(char* out, size_t out_size, const char* text) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (text == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, text, out_size - 1U);
  out[out_size - 1U] = '\0';
}

BootModeStore::StartupMode currentStartupMode() {
  return g_boot_media_manager_mode ? BootModeStore::StartupMode::kMediaManager : BootModeStore::StartupMode::kStory;
}

void applyStartupMode(BootModeStore::StartupMode mode) {
  g_boot_media_manager_mode = (mode == BootModeStore::StartupMode::kMediaManager);
}

void printBootModeStatus() {
  const BootModeStore::StartupMode mode = currentStartupMode();
  Serial.printf("BOOT_MODE_STATUS mode=%s media_validated=%u\n",
                BootModeStore::modeLabel(mode),
                g_boot_mode_store.isMediaValidated() ? 1U : 0U);
}

bool parseBootModeToken(const char* token, BootModeStore::StartupMode* out_mode) {
  if (token == nullptr || out_mode == nullptr) {
    return false;
  }
  if (std::strcmp(token, "STORY") == 0) {
    *out_mode = BootModeStore::StartupMode::kStory;
    return true;
  }
  if (std::strcmp(token, "MEDIA_MANAGER") == 0 || std::strcmp(token, "MEDIA") == 0) {
    *out_mode = BootModeStore::StartupMode::kMediaManager;
    return true;
  }
  return false;
}

void applySceneResourcePolicy(const ScenarioSnapshot& snapshot) {
  if (!g_resource_profile_auto) {
    return;
  }
  const char* screen_scene_id = (snapshot.screen_scene_id != nullptr) ? snapshot.screen_scene_id : "n/a";
  const bool is_win_etape_step =
      (snapshot.step != nullptr && snapshot.step->id != nullptr &&
       std::strcmp(snapshot.step->id, kStepWinEtape) == 0 &&
       snapshot.audio_pack_id != nullptr && std::strcmp(snapshot.audio_pack_id, kPackWin) == 0);
  const bool is_win_etape_scene = (screen_scene_id != nullptr && std::strcmp(screen_scene_id, "SCENE_WIN_ETAPE") == 0);
  const bool is_win_etape = is_win_etape_step || is_win_etape_scene;
  const bool is_la_scene =
      (screen_scene_id != nullptr && std::strstr(screen_scene_id, "LA") != nullptr) ||
      (snapshot.step != nullptr && snapshot.step->id != nullptr && std::strstr(snapshot.step->id, "LA") != nullptr);
  const bool requires_mic =
      g_hardware_cfg.mic_enabled &&
      (is_la_scene || g_la_trigger.gate_active || g_la_trigger.timeout_pending);
  const runtime::resource::ResourceProfile target =
      is_win_etape ? runtime::resource::ResourceProfile::kGfxFocus
                   : (requires_mic ? runtime::resource::ResourceProfile::kGfxPlusMic
                                   : runtime::resource::ResourceProfile::kGfxPlusCamSnapshot);
  if (g_resource_coordinator.profile() != target) {
    g_resource_coordinator.setProfile(target);
    Serial.printf("[RESOURCE] auto profile=%s scene=%s screen=%s pack=%s\n",
                  g_resource_coordinator.profileName(),
                  snapshot.step != nullptr && snapshot.step->id != nullptr ? snapshot.step->id : "n/a",
                  screen_scene_id,
                  snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a");
  }
}

void applyResourceProfileAutoCommand(const char* arg, bool* out_parse_ok) {
  bool enabled = false;
  bool parse_ok = parseBoolToken(arg, &enabled);
  if (out_parse_ok != nullptr) {
    *out_parse_ok = parse_ok;
  }
  if (!parse_ok) {
    return;
  }
  g_resource_profile_auto = enabled;
  if (g_resource_profile_auto) {
    applySceneResourcePolicy(g_scenario.snapshot());
  }
}

void applyMicRuntimePolicy() {
  if (!g_hardware_started) {
    return;
  }
  const bool should_run =
      g_hardware_cfg.mic_enabled &&
      (g_resource_coordinator.shouldRunMic() || g_la_trigger.gate_active || g_la_trigger.timeout_pending);
  g_hardware.setMicRuntimeEnabled(should_run);
}

void logBootMemoryProfile() {
#if defined(ARDUINO_ARCH_ESP32)
  const BootHeapSnapshot heap = bootCaptureHeapSnapshot();
  Serial.printf("[MEM] free_heap=%u min_free_heap=%u total_heap=%u\n",
                static_cast<unsigned int>(ESP.getFreeHeap()),
                static_cast<unsigned int>(ESP.getMinFreeHeap()),
                static_cast<unsigned int>(ESP.getHeapSize()));
  Serial.printf("[MEM] internal_free=%lu internal_largest=%lu\n",
                static_cast<unsigned long>(heap.heap_internal_free),
                static_cast<unsigned long>(heap.heap_internal_largest));
  Serial.printf("[MEM] psram_found=%u total_psram=%lu free_psram=%lu largest_psram=%lu\n",
                heap.psram_found ? 1U : 0U,
                static_cast<unsigned long>(heap.psram_total),
                static_cast<unsigned long>(heap.heap_psram_free),
                static_cast<unsigned long>(heap.heap_psram_largest));
  if (heap.psram_total == 0U) {
    Serial.println("[MEM] PSRAM expected by build flags but not detected");
  }
#endif
}

void logBuildMemoryPolicy() {
  Serial.printf("[CFG] UI_DRAW_BUF_IN_PSRAM=%d FREENOVE_PSRAM_UI_DRAW_BUFFER=%d UI_CAMERA_FB_IN_PSRAM=%d FREENOVE_PSRAM_CAMERA_FRAMEBUFFER=%d UI_AUDIO_RINGBUF_IN_PSRAM=%d UI_DMA_TX_IN_DRAM=%d\n",
                UI_DRAW_BUF_IN_PSRAM,
                FREENOVE_PSRAM_UI_DRAW_BUFFER,
                UI_CAMERA_FB_IN_PSRAM,
                FREENOVE_PSRAM_CAMERA_FRAMEBUFFER,
                UI_AUDIO_RINGBUF_IN_PSRAM,
                UI_DMA_TX_IN_DRAM);
#if defined(FREENOVE_PSRAM_UI_DRAW_BUFFER)
  if (UI_DRAW_BUF_IN_PSRAM != FREENOVE_PSRAM_UI_DRAW_BUFFER) {
    Serial.println("[CFG] WARN draw-buffer PSRAM flags mismatch");
  }
#endif
#if defined(FREENOVE_PSRAM_CAMERA_FRAMEBUFFER)
  if (UI_CAMERA_FB_IN_PSRAM != FREENOVE_PSRAM_CAMERA_FRAMEBUFFER) {
    Serial.println("[CFG] WARN camera framebuffer PSRAM flags mismatch");
  }
#endif
}

bool parseBoolToken(const char* text, bool* out_value) {
  if (text == nullptr || out_value == nullptr) {
    return false;
  }
  char normalized[16] = {0};
  copyText(normalized, sizeof(normalized), text);
  trimAsciiInPlace(normalized);
  toLowerAsciiInPlace(normalized);
  if (std::strcmp(normalized, "1") == 0 || std::strcmp(normalized, "true") == 0 || std::strcmp(normalized, "on") == 0 ||
      std::strcmp(normalized, "yes") == 0) {
    *out_value = true;
    return true;
  }
  if (std::strcmp(normalized, "0") == 0 || std::strcmp(normalized, "false") == 0 || std::strcmp(normalized, "off") == 0 ||
      std::strcmp(normalized, "no") == 0) {
    *out_value = false;
    return true;
  }
  return false;
}

void resetLaTriggerState(bool keep_cooldown = true) {
  LaTriggerService::resetState(&g_la_trigger, keep_cooldown);
}

bool shouldEnforceLaMatchOnly(const ScenarioSnapshot& snapshot) {
  return LaTriggerService::shouldEnforceMatchOnly(g_hardware_cfg, snapshot);
}

bool notifyScenarioButtonGuarded(uint8_t key, bool long_press, uint32_t now_ms, const char* source_tag) {
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const bool is_la_trigger_step = LaTriggerService::isTriggerStep(snapshot);
  const bool is_reset_key = (key >= 1U && key <= 5U);
  const bool enforce_la_match_only = shouldEnforceLaMatchOnly(snapshot);
  if (is_la_trigger_step && is_reset_key) {
    const char* resolved_source = (source_tag != nullptr && source_tag[0] != '\0') ? source_tag : "unknown";
    LaTriggerService::resetTimeout(&g_la_trigger, now_ms, resolved_source);
    g_scenario.notifyButton(key, long_press, now_ms);
    if (enforce_la_match_only) {
      Serial.printf("[LA_TRIGGER] reset LA timeout for key=%u long=%u source=%s while waiting LA match\n",
                    static_cast<unsigned int>(key),
                    long_press ? 1U : 0U,
                    resolved_source);
      return false;
    }
    return true;
  }
  if (enforce_la_match_only) {
    Serial.printf("[LA_TRIGGER] ignore scenario button key=%u long=%u source=%s while waiting LA match\n",
                  static_cast<unsigned int>(key),
                  long_press ? 1U : 0U,
                  (source_tag != nullptr && source_tag[0] != '\0') ? source_tag : "n/a");
    return false;
  }
  g_scenario.notifyButton(key, long_press, now_ms);
  return true;
}

uint8_t laStablePercent() {
  return LaTriggerService::stablePercent(g_hardware_cfg, g_la_trigger);
}

void startLaTimeoutRecovery(const ScenarioSnapshot& snapshot, uint32_t now_ms) {
  (void)snapshot;
  (void)now_ms;
  g_la_trigger.timeout_pending = false;
  g_la_trigger.timeout_deadline_ms = 0U;
  g_la_trigger.dispatched = false;
  g_la_trigger.locked = false;
  g_la_trigger.sample_match = false;

  g_scenario.reset();
  g_audio.stop();
  g_last_action_step_key[0] = '\0';
  if (g_hardware_started) {
    g_hardware.clearManualLed();
  }
  LaTriggerService::resetState(&g_la_trigger, false);
  Serial.println("[LA_TRIGGER] timeout -> scenario reset (SCENE_LOCKED)");
}

void updateLaGameplayTrigger(const ScenarioSnapshot& snapshot, const HardwareManager::Snapshot& hw, uint32_t now_ms) {
  const LaTriggerService::UpdateResult update =
      LaTriggerService::update(g_hardware_cfg, &g_la_trigger, snapshot, hw, now_ms);
  if (update.timed_out) {
    Serial.printf(
        "[LA_TRIGGER] timeout after %lu ms (freq=%u cents=%d conf=%u level=%u)\n",
        static_cast<unsigned long>(now_ms - g_la_trigger.gate_entered_ms),
        static_cast<unsigned int>(hw.mic_freq_hz),
        static_cast<int>(hw.mic_pitch_cents),
        static_cast<unsigned int>(hw.mic_pitch_confidence),
        static_cast<unsigned int>(hw.mic_level_percent));
    startLaTimeoutRecovery(snapshot, now_ms);
    return;
  }
  if (!update.lock_ready) {
    return;
  }

  const char* event_name =
      (g_hardware_cfg.mic_la_event_name[0] != '\0') ? g_hardware_cfg.mic_la_event_name : "SERIAL:BTN_NEXT";
  const ScenarioSnapshot before = g_scenario.snapshot();
  g_la_dispatch_in_progress = true;
  const bool dispatched = dispatchScenarioEventByName(event_name, now_ms);
  g_la_dispatch_in_progress = false;
  const ScenarioSnapshot after = g_scenario.snapshot();
  const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
  Serial.printf(
      "[LA_TRIGGER] dispatched=%u changed=%u event=%s step=%s freq=%u cents=%d conf=%u level=%u stable_ms=%lu gate=%u\n",
      dispatched ? 1U : 0U,
      changed ? 1U : 0U,
      event_name,
      stepIdFromSnapshot(after),
      static_cast<unsigned int>(hw.mic_freq_hz),
      static_cast<int>(hw.mic_pitch_cents),
      static_cast<unsigned int>(hw.mic_pitch_confidence),
      static_cast<unsigned int>(hw.mic_level_percent),
      static_cast<unsigned long>(g_la_trigger.stable_ms),
      update.gate_active ? 1U : 0U);
  if (dispatched) {
    g_la_trigger.dispatched = true;
    g_la_trigger.last_trigger_ms = now_ms;
  }
}

bool startsWithIgnoreCase(const char* text, const char* prefix) {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  size_t index = 0U;
  while (prefix[index] != '\0') {
    if (text[index] == '\0') {
      return false;
    }
    const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(text[index])));
    const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[index])));
    if (lhs != rhs) {
      return false;
    }
    ++index;
  }
  return true;
}

const char* eventTypeName(StoryEventType type) {
  switch (type) {
    case StoryEventType::kUnlock:
      return "unlock";
    case StoryEventType::kAudioDone:
      return "audio_done";
    case StoryEventType::kTimer:
      return "timer";
    case StoryEventType::kSerial:
      return "serial";
    case StoryEventType::kButton:
      return "button";
    case StoryEventType::kEspNow:
      return "espnow";
    case StoryEventType::kAction:
      return "action";
    default:
      return "none";
  }
}

bool parseEventType(const char* text, StoryEventType* out_type) {
  if (text == nullptr || out_type == nullptr) {
    return false;
  }
  char normalized[20] = {0};
  std::strncpy(normalized, text, sizeof(normalized) - 1U);
  toLowerAsciiInPlace(normalized);
  if (std::strcmp(normalized, "unlock") == 0) {
    *out_type = StoryEventType::kUnlock;
    return true;
  }
  if (std::strcmp(normalized, "audio_done") == 0) {
    *out_type = StoryEventType::kAudioDone;
    return true;
  }
  if (std::strcmp(normalized, "timer") == 0) {
    *out_type = StoryEventType::kTimer;
    return true;
  }
  if (std::strcmp(normalized, "serial") == 0) {
    *out_type = StoryEventType::kSerial;
    return true;
  }
  if (std::strcmp(normalized, "button") == 0 || std::strcmp(normalized, "btn") == 0) {
    *out_type = StoryEventType::kButton;
    return true;
  }
  if (std::strcmp(normalized, "espnow") == 0 || std::strcmp(normalized, "esp_now") == 0) {
    *out_type = StoryEventType::kEspNow;
    return true;
  }
  if (std::strcmp(normalized, "action") == 0) {
    *out_type = StoryEventType::kAction;
    return true;
  }
  return false;
}

const char* defaultEventNameForType(StoryEventType type) {
  switch (type) {
    case StoryEventType::kUnlock:
      return "UNLOCK";
    case StoryEventType::kAudioDone:
      return "AUDIO_DONE";
    case StoryEventType::kTimer:
      return "ETAPE2_DUE";
    case StoryEventType::kSerial:
      return "BTN_NEXT";
    case StoryEventType::kButton:
      return "ANY";
    case StoryEventType::kEspNow:
      return "EVENT";
    case StoryEventType::kAction:
      return "ACTION_FORCE_ETAPE2";
    default:
      return "";
  }
}

bool buildEventTokenFromTypeName(StoryEventType type,
                                 const char* event_name,
                                 char* out_event,
                                 size_t out_capacity) {
  if (out_event == nullptr || out_capacity == 0U) {
    return false;
  }
  const char* resolved_name = (event_name != nullptr && event_name[0] != '\0') ? event_name : defaultEventNameForType(type);
  char normalized_name[64] = {0};
  copyText(normalized_name, sizeof(normalized_name), resolved_name);
  trimAsciiInPlace(normalized_name);
  toUpperAsciiInPlace(normalized_name);

  switch (type) {
    case StoryEventType::kUnlock:
      if (normalized_name[0] == '\0' || std::strcmp(normalized_name, "UNLOCK") == 0) {
        copyText(out_event, out_capacity, "UNLOCK");
      } else {
        snprintf(out_event, out_capacity, "UNLOCK:%s", normalized_name);
      }
      return true;
    case StoryEventType::kAudioDone:
      copyText(out_event, out_capacity, "AUDIO_DONE");
      return true;
    case StoryEventType::kTimer:
      snprintf(out_event, out_capacity, "TIMER:%s", normalized_name[0] != '\0' ? normalized_name : "ETAPE2_DUE");
      return true;
    case StoryEventType::kSerial:
      snprintf(out_event, out_capacity, "SERIAL:%s", normalized_name[0] != '\0' ? normalized_name : "BTN_NEXT");
      return true;
    case StoryEventType::kButton:
      snprintf(out_event, out_capacity, "BUTTON:%s", normalized_name[0] != '\0' ? normalized_name : "ANY");
      return true;
    case StoryEventType::kEspNow:
      snprintf(out_event, out_capacity, "ESPNOW:%s", normalized_name[0] != '\0' ? normalized_name : "EVENT");
      return true;
    case StoryEventType::kAction:
      snprintf(out_event,
               out_capacity,
               "ACTION:%s",
               normalized_name[0] != '\0' ? normalized_name : "ACTION_FORCE_ETAPE2");
      return true;
    default:
      return false;
  }
}

bool normalizeEventTokenFromText(const char* raw_text, char* out_event, size_t out_capacity) {
  if (raw_text == nullptr || out_event == nullptr || out_capacity == 0U) {
    return false;
  }

  char event[kSerialLineCapacity] = {0};
  copyText(event, sizeof(event), raw_text);
  trimAsciiInPlace(event);
  if (event[0] == '\0') {
    return false;
  }

  if (startsWithIgnoreCase(event, "SC_EVENT_RAW ")) {
    char* payload = event + 13;
    trimAsciiInPlace(payload);
    if (payload[0] == '\0') {
      return false;
    }
    copyText(out_event, out_capacity, payload);
    return true;
  }

  if (startsWithIgnoreCase(event, "SC_EVENT ")) {
    char* args = event + 9;
    trimAsciiInPlace(args);
    if (args[0] == '\0') {
      return false;
    }
    char* type_text = args;
    char* name_text = nullptr;
    for (size_t index = 0U; args[index] != '\0'; ++index) {
      if (args[index] == ' ') {
        args[index] = '\0';
        name_text = &args[index + 1U];
        break;
      }
    }
    if (name_text != nullptr) {
      trimAsciiInPlace(name_text);
      if (name_text[0] == '\0') {
        name_text = nullptr;
      }
    }
    StoryEventType parsed_type = StoryEventType::kNone;
    if (!parseEventType(type_text, &parsed_type)) {
      return false;
    }
    return buildEventTokenFromTypeName(parsed_type, name_text, out_event, out_capacity);
  }

  if (startsWithIgnoreCase(event, "SERIAL ")) {
    char* name = event + 7;
    trimAsciiInPlace(name);
    toUpperAsciiInPlace(name);
    snprintf(out_event, out_capacity, "SERIAL:%s", name[0] != '\0' ? name : "BTN_NEXT");
    return true;
  }
  if (startsWithIgnoreCase(event, "BUTTON ")) {
    char* name = event + 7;
    trimAsciiInPlace(name);
    toUpperAsciiInPlace(name);
    snprintf(out_event, out_capacity, "BUTTON:%s", name[0] != '\0' ? name : "ANY");
    return true;
  }
  if (startsWithIgnoreCase(event, "ESPNOW ")) {
    char* name = event + 7;
    trimAsciiInPlace(name);
    toUpperAsciiInPlace(name);
    snprintf(out_event, out_capacity, "ESPNOW:%s", name[0] != '\0' ? name : "EVENT");
    return true;
  }
  if (startsWithIgnoreCase(event, "TIMER ")) {
    char* name = event + 6;
    trimAsciiInPlace(name);
    toUpperAsciiInPlace(name);
    snprintf(out_event, out_capacity, "TIMER:%s", name[0] != '\0' ? name : "ETAPE2_DUE");
    return true;
  }
  if (startsWithIgnoreCase(event, "ACTION ")) {
    char* name = event + 7;
    trimAsciiInPlace(name);
    toUpperAsciiInPlace(name);
    snprintf(out_event, out_capacity, "ACTION:%s", name[0] != '\0' ? name : "ACTION_FORCE_ETAPE2");
    return true;
  }

  toUpperAsciiInPlace(event);
  copyText(out_event, out_capacity, event);
  return true;
}

bool extractEventTokenFromJsonObject(JsonVariantConst root, char* out_event, size_t out_capacity) {
  if (!root.is<JsonObjectConst>()) {
    return false;
  }

  const char* root_type = root["event_type"] | root["type"] | "";
  const char* root_name = root["event_name"] | root["name"] | "";
  if (root_type[0] != '\0') {
    StoryEventType type = StoryEventType::kNone;
    if (parseEventType(root_type, &type) && buildEventTokenFromTypeName(type, root_name, out_event, out_capacity)) {
      return true;
    }
  }

  if (root["event"].is<const char*>()) {
    const char* text = root["event"].as<const char*>();
    if (text != nullptr && text[0] != '\0' && normalizeEventTokenFromText(text, out_event, out_capacity)) {
      return true;
    }
  }

  if (root["event"].is<JsonObjectConst>()) {
    JsonVariantConst event_obj = root["event"];
    const char* event_type = event_obj["event_type"] | event_obj["type"] | "";
    const char* event_name = event_obj["event_name"] | event_obj["name"] | "";
    StoryEventType type = StoryEventType::kNone;
    if (event_type[0] != '\0' && parseEventType(event_type, &type) &&
        buildEventTokenFromTypeName(type, event_name, out_event, out_capacity)) {
      return true;
    }
    if (event_obj["cmd"].is<const char*>()) {
      const char* cmd = event_obj["cmd"].as<const char*>();
      if (cmd != nullptr && cmd[0] != '\0' && normalizeEventTokenFromText(cmd, out_event, out_capacity)) {
        return true;
      }
    }
    if (event_obj["raw"].is<const char*>()) {
      const char* raw = event_obj["raw"].as<const char*>();
      if (raw != nullptr && raw[0] != '\0' && normalizeEventTokenFromText(raw, out_event, out_capacity)) {
        return true;
      }
    }
  }

  if (root["cmd"].is<const char*>()) {
    const char* cmd = root["cmd"].as<const char*>();
    if (cmd != nullptr && cmd[0] != '\0' && normalizeEventTokenFromText(cmd, out_event, out_capacity)) {
      return true;
    }
  }
  if (root["raw"].is<const char*>()) {
    const char* raw = root["raw"].as<const char*>();
    if (raw != nullptr && raw[0] != '\0' && normalizeEventTokenFromText(raw, out_event, out_capacity)) {
      return true;
    }
  }

  if (root["payload"].is<const char*>()) {
    const char* payload = root["payload"].as<const char*>();
    if (payload != nullptr && payload[0] != '\0' && normalizeEventTokenFromText(payload, out_event, out_capacity)) {
      return true;
    }
  }
  if (root["payload"].is<JsonObjectConst>()) {
    if (extractEventTokenFromJsonObject(root["payload"], out_event, out_capacity)) {
      return true;
    }
  }
  return false;
}

bool normalizeEspNowPayloadToScenarioEvent(const char* payload_text, char* out_event, size_t out_capacity) {
  if (payload_text == nullptr || out_event == nullptr || out_capacity == 0U) {
    return false;
  }

  char normalized[kSerialLineCapacity] = {0};
  copyText(normalized, sizeof(normalized), payload_text);
  trimAsciiInPlace(normalized);
  if (normalized[0] == '\0') {
    return false;
  }

  if (normalized[0] == '{') {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, normalized);
    if (!error) {
      if (extractEventTokenFromJsonObject(document.as<JsonVariantConst>(), out_event, out_capacity)) {
        return true;
      }
      return false;
    }
  }

  char normalized_event[kSerialLineCapacity] = {0};
  if (!normalizeEventTokenFromText(normalized, normalized_event, sizeof(normalized_event))) {
    return false;
  }
  if (std::strchr(normalized_event, ':') == nullptr &&
      std::strcmp(normalized_event, "UNLOCK") != 0 &&
      std::strcmp(normalized_event, "AUDIO_DONE") != 0) {
    std::snprintf(out_event, out_capacity, "ESPNOW:%s", normalized_event);
    return true;
  }
  copyText(out_event, out_capacity, normalized_event);
  return true;
}

bool dispatchScenarioEventByType(StoryEventType type, const char* event_name, uint32_t now_ms);
bool dispatchScenarioEventByName(const char* event_name, uint32_t now_ms);
void handleSerialCommand(const char* command_line, uint32_t now_ms);
void handleSerialCommandImpl(const char* command_line, uint32_t now_ms);
void runtimeTickBridge(uint32_t now_ms, RuntimeServices* services);
void serialDispatchBridge(const char* command_line, uint32_t now_ms, RuntimeServices* services);
void refreshSceneIfNeeded(bool force_render);
void refreshSceneIfNeededImpl(bool force_render);
void startPendingAudioIfAny();
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
bool isAmpSceneId(const char* scene_id);
bool ensureAmpInitialized();
size_t scanAmpPlaylistWithFallback();
void setAmpSceneActive(bool active);
void syncAmpSceneState(const ScenarioSnapshot& snapshot);
void printAmpStatus();
#endif
bool isCameraSceneId(const char* scene_id);
bool ensureCameraUiInitialized();
void setCameraSceneActive(bool active);
void syncCameraSceneState(const ScenarioSnapshot& snapshot);
bool dispatchCameraSceneButton(uint8_t key, bool long_press);
void printCameraRecorderStatus();
void executeStoryActionsForStep(const ScenarioSnapshot& snapshot, uint32_t now_ms);
bool executeStoryAction(const char* action_id, const ScenarioSnapshot& snapshot, uint32_t now_ms);
void webFillWifiStatus(JsonObject out, const NetworkManager::Snapshot& net);
void webFillEspNowStatus(JsonObject out, const NetworkManager::Snapshot& net);
void webFillHardwareStatus(JsonObject out);
void webFillCameraStatus(JsonObject out);
void webFillMediaStatus(JsonObject out, uint32_t now_ms);
void webSendHardwareStatus();
void webSendCameraStatus();
void webSendMediaFiles();
void webSendMediaRecordStatus();
void webSendAuthStatus();
void webSendProvisionStatus();
bool webReconnectLocalWifi();
bool refreshStoryFromSd();
bool dispatchControlAction(const String& action_raw, uint32_t now_ms, String* out_error = nullptr);
bool dispatchControlActionImpl(const String& action_raw, uint32_t now_ms, String* out_error = nullptr);
void setupWebUiImpl();
void clearRuntimeStaCredentials();
void applyRuntimeStaCredentials(const char* ssid, const char* password);
void updateAuthPolicy();
bool ensureWebToken(bool rotate_token, bool print_token, bool* out_generated = nullptr);
void loadBootProvisioningState();
bool provisionWifiCredentials(const char* ssid,
                              const char* password,
                              bool persist,
                              bool* out_connect_started = nullptr,
                              bool* out_persisted = nullptr,
                              bool* out_token_generated = nullptr);
bool forgetWifiCredentials();
bool webAuthorizeApiRequest(const char* path);
void printHardwareStatus();
void printMicTunerStatus();
void printHardwareStatusJson();
void printCameraStatus();
void printMediaStatus();
void maybeEmitHardwareEvents(uint32_t now_ms);
void maybeLogHardwareTelemetry(uint32_t now_ms);
void maybeStreamMicTunerStatus(uint32_t now_ms);

struct EspNowCommandResult {
  bool handled = false;
  bool ok = false;
  String code;
  String error;
  String data_json;
};

void appendCompactRuntimeStatus(JsonObject out) {
  const NetworkManager::Snapshot net = g_network.snapshot();
  const ScenarioSnapshot scenario = g_scenario.snapshot();
  const HardwareManager::Snapshot& hardware = g_hardware.snapshotRef();
  const CameraManager::Snapshot camera = g_camera.snapshot();
  const MediaManager::Snapshot media = g_media.snapshot();
  out["state"] = net.state;
  out["mode"] = net.mode;
  out["ip"] = net.ip;
  out["sta_connected"] = net.sta_connected;
  out["espnow_enabled"] = net.espnow_enabled;
  out["scenario"] = scenarioIdFromSnapshot(scenario);
  out["step"] = stepIdFromSnapshot(scenario);
  out["screen"] = (scenario.screen_scene_id != nullptr) ? scenario.screen_scene_id : "";
  out["audio_pack"] = (scenario.audio_pack_id != nullptr) ? scenario.audio_pack_id : "";
  out["audio_playing"] = g_audio.isPlaying();
  out["hw_ready"] = hardware.ready;
  out["cam_enabled"] = camera.enabled;
  out["cam_scene_active"] = g_camera_scene_active;
  out["cam_recorder"] = camera.recorder_session_active;
  out["media_recording"] = media.recording;
}

bool executeEspNowCommandPayload(const char* payload_text, uint32_t now_ms, EspNowCommandResult* out_result) {
  if (payload_text == nullptr || out_result == nullptr) {
    return false;
  }
  out_result->handled = false;
  out_result->ok = false;
  out_result->code.remove(0);
  out_result->error.remove(0);
  out_result->data_json.remove(0);

  String command;
  String trailing_arg;
  StaticJsonDocument<768> payload_document;
  JsonVariantConst args = JsonVariantConst();

  if (payload_text[0] == '{') {
    const DeserializationError error = deserializeJson(payload_document, payload_text);
    if (!error) {
      JsonVariantConst root = payload_document.as<JsonVariantConst>();
      const char* cmd = root["cmd"] | root["command"] | root["action"] | "";
      if ((cmd == nullptr || cmd[0] == '\0') && root["payload"].is<JsonObjectConst>()) {
        JsonVariantConst nested = root["payload"];
        cmd = nested["cmd"] | nested["command"] | nested["action"] | "";
        if (nested["args"].is<JsonVariantConst>()) {
          args = nested["args"];
        }
      }
      if (cmd != nullptr && cmd[0] != '\0') {
        command = cmd;
        if (args.isNull()) {
          if (root["args"].is<JsonVariantConst>()) {
            args = root["args"];
          } else if (root["payload"].is<JsonVariantConst>()) {
            args = root["payload"];
          }
        }
        if (args.is<const char*>()) {
          trailing_arg = args.as<const char*>();
          args = JsonVariantConst();
        }
      }
    }
  }

  if (command.isEmpty()) {
    command = payload_text;
    command.trim();
    const int sep = command.indexOf(' ');
    if (sep > 0) {
      trailing_arg = command.substring(static_cast<unsigned int>(sep + 1));
      command = command.substring(0, static_cast<unsigned int>(sep));
    }
  }

  command.trim();
  command.toUpperCase();
  trailing_arg.trim();
  if (command.isEmpty()) {
    return false;
  }

  out_result->handled = true;
  out_result->code = command;

  if (command == "STATUS") {
    StaticJsonDocument<512> response;
    appendCompactRuntimeStatus(response.to<JsonObject>());
    serializeJson(response, out_result->data_json);
    out_result->ok = true;
    return true;
  }
  if (command == "WIFI_STATUS") {
    StaticJsonDocument<384> response;
    webFillWifiStatus(response.to<JsonObject>(), g_network.snapshot());
    serializeJson(response, out_result->data_json);
    out_result->ok = true;
    return true;
  }
  if (command == "ESPNOW_STATUS") {
    StaticJsonDocument<512> response;
    webFillEspNowStatus(response.to<JsonObject>(), g_network.snapshot());
    serializeJson(response, out_result->data_json);
    out_result->ok = true;
    return true;
  }
  if (command == "UNLOCK") {
    out_result->ok = dispatchScenarioEventByName("UNLOCK", now_ms);
    return true;
  }
  if (command == "NEXT") {
    bool ok = dispatchScenarioEventByName("SERIAL:BTN_NEXT", now_ms);
    if (!ok) {
      ok = notifyScenarioButtonGuarded(5U, false, now_ms, "espnow_command");
    }
    out_result->ok = ok;
    if (!out_result->ok) {
      out_result->error = "invalid_next_event";
    }
    return true;
  }
  if (command == "WIFI_DISCONNECT") {
    g_network.disconnectSta();
    out_result->ok = true;
    return true;
  }
  if (command == "WIFI_RECONNECT") {
    out_result->ok = webReconnectLocalWifi();
    if (!out_result->ok) {
      out_result->error = "wifi_reconnect_failed";
    }
    return true;
  }
  if (command == "ESPNOW_ON") {
    out_result->ok = g_network.enableEspNow();
    if (!out_result->ok) {
      out_result->error = "espnow_enable_failed";
    }
    return true;
  }
  if (command == "ESPNOW_OFF") {
    g_network.disableEspNow();
    out_result->ok = true;
    return true;
  }
  if (command == "STORY_REFRESH_SD") {
    out_result->ok = refreshStoryFromSd();
    if (!out_result->ok) {
      out_result->error = "story_refresh_sd_failed";
    }
    return true;
  }
  if (command == "SC_EVENT") {
    bool dispatched = false;
    if (!args.isNull() && args.is<JsonObjectConst>()) {
      JsonVariantConst args_obj = args;
      const char* type_text = args_obj["event_type"] | args_obj["type"] | "";
      const char* name_text = args_obj["event_name"] | args_obj["name"] | "";
      StoryEventType event_type = StoryEventType::kNone;
      if (type_text[0] != '\0' && parseEventType(type_text, &event_type)) {
        dispatched = dispatchScenarioEventByType(event_type, name_text, now_ms);
      } else {
        char event_token[kSerialLineCapacity] = {0};
        if (extractEventTokenFromJsonObject(args_obj, event_token, sizeof(event_token))) {
          dispatched = dispatchScenarioEventByName(event_token, now_ms);
        }
      }
    }
    if (!dispatched && !trailing_arg.isEmpty()) {
      char event_token[kSerialLineCapacity] = {0};
      if (normalizeEventTokenFromText(trailing_arg.c_str(), event_token, sizeof(event_token))) {
        dispatched = dispatchScenarioEventByName(event_token, now_ms);
      }
    }
    out_result->ok = dispatched;
    if (!dispatched) {
      out_result->error = "invalid_sc_event";
    }
    return true;
  }
  if (command == "RING") {
    out_result->ok = dispatchScenarioEventByName("RING", now_ms);
    if (!out_result->ok) {
      out_result->error = "invalid_ring_event";
    }
    return true;
  }
  if (command == "SCENE") {
    String scene_id = trailing_arg;
    scene_id.trim();
    if (scene_id.isEmpty() && !args.isNull()) {
      if (args.is<const char*>()) {
        scene_id = args.as<const char*>();
      } else if (args.is<JsonObjectConst>()) {
        JsonObjectConst scene_args = args.as<JsonObjectConst>();
        const char* id = scene_args["id"] | scene_args["scenario"] | scene_args["scenario_id"] | scene_args["scene_id"] | "";
        if (id != nullptr && id[0] != '\0') {
          scene_id = id;
        } else if (scene_args["name"].is<const char*>()) {
          scene_id = scene_args["name"].as<const char*>();
        }
      } else if (args.is<int>()) {
        scene_id = String(args.as<int>());
      } else if (args.is<unsigned int>()) {
        scene_id = String(args.as<unsigned int>());
      } else if (args.is<long>()) {
        scene_id = String(args.as<long>());
      } else if (args.is<unsigned long>()) {
        scene_id = String(args.as<unsigned long>());
      }
    }

    scene_id.trim();
    if (scene_id.isEmpty()) {
      out_result->ok = false;
      out_result->error = "missing_scene_id";
      return true;
    }
    scene_id.toUpperCase();

    String load_source;
    String load_path;
    out_result->ok = loadScenarioByIdPreferStoryFile(scene_id.c_str(), &load_source, &load_path);
    if (!out_result->ok) {
      out_result->error = "scene_not_found";
      return true;
    }
    if (!load_path.isEmpty()) {
      Serial.printf("[SCENARIO] SCENE source=%s path=%s\n", load_source.c_str(), load_path.c_str());
    } else {
      Serial.printf("[SCENARIO] SCENE source=%s id=%s\n", load_source.c_str(), scene_id.c_str());
    }
    g_last_action_step_key[0] = '\0';
    refreshSceneIfNeeded(true);
    startPendingAudioIfAny();
    return true;
  }

  String control_action = command;
  if (!trailing_arg.isEmpty()) {
    control_action += " ";
    control_action += trailing_arg;
  }
  String control_error;
  const bool control_ok = dispatchControlAction(control_action, now_ms, &control_error);
  if (control_ok || control_error != "unsupported_action") {
    out_result->ok = control_ok;
    if (!control_ok) {
      out_result->error = control_error;
    }
    return true;
  }

  out_result->handled = false;
  out_result->error = "unsupported_command";
  return false;
}

void sendEspNowAck(const char* peer,
                   const char* msg_id,
                   uint32_t seq,
                   const EspNowCommandResult& result,
                   bool ack_requested) {
  if (!ack_requested || peer == nullptr || peer[0] == '\0') {
    return;
  }

  StaticJsonDocument<768> response;
  char fallback_msg_id[32] = {0};
  if (msg_id == nullptr || msg_id[0] == '\0') {
    snprintf(fallback_msg_id, sizeof(fallback_msg_id), "ack-%08lX", static_cast<unsigned long>(millis()));
    msg_id = fallback_msg_id;
  }
  response["msg_id"] = msg_id;
  response["seq"] = seq;
  response["type"] = "ack";
  response["ack"] = true;
  JsonObject payload = response["payload"].to<JsonObject>();
  payload["ok"] = result.ok;
  payload["code"] = result.code;
  payload["error"] = result.error;
  if (!result.data_json.isEmpty()) {
    StaticJsonDocument<512> data_doc;
    if (!deserializeJson(data_doc, result.data_json)) {
      payload["data"] = data_doc.as<JsonVariantConst>();
    } else {
      payload["data_raw"] = result.data_json;
    }
  }

  String frame;
  serializeJson(response, frame);
  if (!g_network.sendEspNowTarget(peer, frame.c_str())) {
    Serial.printf("[NET] ESPNOW ACK send failed peer=%s msg_id=%s code=%s\n",
                  peer,
                  msg_id,
                  result.code.c_str());
  }
}

void printScenarioList() {
  const char* default_id = storyScenarioV2IdAt(0U);
  Serial.printf("SC_LIST count=%u default=%s\n",
                storyScenarioV2Count(),
                (default_id != nullptr) ? default_id : "n/a");
  for (uint8_t index = 0U; index < storyScenarioV2Count(); ++index) {
    const char* scenario_id = storyScenarioV2IdAt(index);
    if (scenario_id == nullptr) {
      continue;
    }
    Serial.printf("SC_LIST_ITEM idx=%u id=%s\n", index, scenario_id);
  }
}

bool splitSsidPass(const char* argument, String* out_ssid, String* out_pass) {
  if (argument == nullptr || out_ssid == nullptr || out_pass == nullptr) {
    return false;
  }
  String raw = argument;
  raw.trim();
  if (raw.isEmpty()) {
    return false;
  }
  const int sep = raw.lastIndexOf(' ');
  if (sep < 0) {
    *out_ssid = raw;
    out_pass->remove(0);
    return true;
  }
  *out_ssid = raw.substring(0, static_cast<unsigned int>(sep));
  *out_pass = raw.substring(static_cast<unsigned int>(sep + 1));
  out_ssid->trim();
  out_pass->trim();
  return !out_ssid->isEmpty();
}

bool parseBoundedLongToken(const char* token, long min_value, long max_value, long* out_value) {
  if (token == nullptr || out_value == nullptr) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(token, &end, 10);
  if (errno == ERANGE || end == token || (end != nullptr && *end != '\0') || parsed < min_value || parsed > max_value) {
    return false;
  }
  *out_value = parsed;
  return true;
}

bool parseHwLedSetArgs(const char* args,
                       uint8_t* out_r,
                       uint8_t* out_g,
                       uint8_t* out_b,
                       uint8_t* out_brightness,
                       bool* out_pulse) {
  if (args == nullptr || out_r == nullptr || out_g == nullptr || out_b == nullptr || out_brightness == nullptr ||
      out_pulse == nullptr) {
    return false;
  }

  char buffer[kSerialLineCapacity] = {0};
  copyText(buffer, sizeof(buffer), args);
  trimAsciiInPlace(buffer);
  if (buffer[0] == '\0') {
    return false;
  }

  char* tokens[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
  size_t token_count = 0U;
  char* cursor = buffer;
  while (*cursor != '\0' && token_count < 5U) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    tokens[token_count++] = cursor;
    while (*cursor != '\0' && *cursor != ' ') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    *cursor++ = '\0';
  }
  if (token_count < 3U) {
    return false;
  }

  long parsed_values[5] = {0L, 0L, 0L, static_cast<long>(FREENOVE_WS2812_BRIGHTNESS), 1L};
  for (size_t index = 0U; index < token_count; ++index) {
    const long max_value = (index == 4U) ? 1L : 255L;
    if (!parseBoundedLongToken(tokens[index], 0L, max_value, &parsed_values[index])) {
      return false;
    }
  }
  *out_r = static_cast<uint8_t>(parsed_values[0]);
  *out_g = static_cast<uint8_t>(parsed_values[1]);
  *out_b = static_cast<uint8_t>(parsed_values[2]);
  *out_brightness = static_cast<uint8_t>(parsed_values[3]);
  *out_pulse = (parsed_values[4] != 0L);
  return true;
}

bool parseEspNowSendPayload(const char* argument, String& payload, bool* used_target) {
  if (argument == nullptr) {
    return false;
  }

  String args = argument;
  args.trim();
  if (args.isEmpty()) {
    return false;
  }
  if (used_target != nullptr) {
    *used_target = false;
  }

  const int separator = args.indexOf(' ');
  if (separator < 0) {
    payload = args;
    return true;
  }

  String maybe_target = args.substring(0U, static_cast<unsigned int>(separator));
  maybe_target.trim();
  String parsed_payload = args.substring(static_cast<unsigned int>(separator + 1U));
  parsed_payload.trim();
  if (parsed_payload.isEmpty()) {
    return false;
  }

  bool looks_like_target = false;
  String target_lower = maybe_target;
  target_lower.toLowerCase();
  if (target_lower == kEspNowBroadcastTarget) {
    looks_like_target = true;
  } else {
    uint8_t parsed_mac[6] = {0};
    if (g_network.parseMac(maybe_target.c_str(), parsed_mac)) {
      looks_like_target = true;
    }
  }

  if (!looks_like_target) {
    payload = args;
    return true;
  }

  if (used_target != nullptr) {
    *used_target = true;
  }
  payload = parsed_payload;
  return true;
}

void printNetworkStatus() {
  const NetworkManager::Snapshot net = g_network.snapshot();
  Serial.printf("NET_STATUS state=%s mode=%s sta=%u connecting=%u ap=%u fallback_ap=%u espnow=%u ip=%s sta_ssid=%s "
                "ap_ssid=%s ap_clients=%u local_target=%s local_match=%u local_retry_paused=%u rssi=%ld peers=%u rx=%lu "
                "tx_ok=%lu tx_fail=%lu drop=%lu last_msg=%s seq=%lu type=%s ack=%u\n",
                net.state,
                net.mode,
                net.sta_connected ? 1U : 0U,
                net.sta_connecting ? 1U : 0U,
                net.ap_enabled ? 1U : 0U,
                net.fallback_ap_active ? 1U : 0U,
                net.espnow_enabled ? 1U : 0U,
                net.ip,
                net.sta_ssid[0] != '\0' ? net.sta_ssid : "n/a",
                net.ap_ssid[0] != '\0' ? net.ap_ssid : "n/a",
                static_cast<unsigned int>(net.ap_clients),
                net.local_target[0] != '\0' ? net.local_target : "n/a",
                net.local_match ? 1U : 0U,
                net.local_retry_paused ? 1U : 0U,
                static_cast<long>(net.rssi),
                net.espnow_peer_count,
                static_cast<unsigned long>(net.espnow_rx_packets),
                static_cast<unsigned long>(net.espnow_tx_ok),
                static_cast<unsigned long>(net.espnow_tx_fail),
                static_cast<unsigned long>(net.espnow_drop_packets),
                net.last_msg_id[0] != '\0' ? net.last_msg_id : "n/a",
                static_cast<unsigned long>(net.espnow_last_seq),
                net.last_type[0] != '\0' ? net.last_type : "n/a",
                net.espnow_last_ack ? 1U : 0U);
  for (uint8_t index = 0U; index < g_network.espNowPeerCount(); ++index) {
    char peer[18] = {0};
    if (!g_network.espNowPeerAt(index, peer, sizeof(peer))) {
      continue;
    }
    Serial.printf("NET_PEER idx=%u mac=%s\n", index, peer);
  }
  if (net.last_payload[0] != '\0') {
    Serial.printf("NET_LAST peer=%s payload=%s\n",
                  net.last_peer[0] != '\0' ? net.last_peer : "n/a",
                  net.last_payload);
  }
}

void printEspNowStatusJson() {
  const NetworkManager::Snapshot net = g_network.snapshot();
  StaticJsonDocument<768> document;
  document["ready"] = net.espnow_enabled;
  document["peer_count"] = net.espnow_peer_count;
  document["tx_ok"] = net.espnow_tx_ok;
  document["tx_fail"] = net.espnow_tx_fail;
  document["rx_count"] = net.espnow_rx_packets;
  document["last_rx_mac"] = net.last_rx_peer;
  document["last_msg_id"] = net.last_msg_id;
  document["last_seq"] = net.espnow_last_seq;
  document["last_type"] = net.last_type;
  document["last_ack"] = net.espnow_last_ack;
  document["last_payload"] = net.last_payload;
  JsonArray peers = document["peers"].to<JsonArray>();
  for (uint8_t index = 0U; index < g_network.espNowPeerCount(); ++index) {
    char peer[18] = {0};
    if (!g_network.espNowPeerAt(index, peer, sizeof(peer))) {
      continue;
    }
    peers.add(peer);
  }
  serializeJson(document, Serial);
  Serial.println();
}

bool sendRingCommandToRtc() {
  StaticJsonDocument<96> payload;
  payload["cmd"] = "RING";
  String payload_text;
  serializeJson(payload, payload_text);
  const bool ok = g_network.sendEspNowTarget(kEspNowBroadcastTarget, payload_text.c_str());
  Serial.printf("[MAIN] RING send to rtc ok=%u payload=%s\n", ok ? 1U : 0U, payload_text.c_str());
  return ok;
}

#if defined(USE_AUDIO) && (USE_AUDIO != 0)
bool isAmpSceneId(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strcmp(scene_id, "SCENE_MP3_PLAYER") == 0 ||
         std::strcmp(scene_id, "SCENE_AUDIO_PLAYER") == 0 ||
         std::strcmp(scene_id, "SCENE_MP3") == 0;
}

bool beginAmpAtBase(const char* base_dir) {
  if (base_dir == nullptr || base_dir[0] == '\0') {
    return false;
  }
  ui::audio::AmigaAudioPlayer::UiConfig ui_cfg;
  ui_cfg.base_dir = base_dir;
  ui_cfg.start_visible = false;
  ui_cfg.auto_scan = false;
  ui_cfg.dim_background = true;
  ui_cfg.capture_keys_when_visible = true;
  const bool ok = g_amp_player.begin(ui_cfg, ui::audio::AudioPlayerService::Config{});
  if (ok) {
    copyText(g_amp_base_dir, sizeof(g_amp_base_dir), base_dir);
    g_amp_ready = true;
    Serial.printf("[AMP] ready base_dir=%s\n", g_amp_base_dir);
  }
  return ok;
}

bool ensureAmpInitialized() {
  if (g_amp_ready) {
    return true;
  }
  // Keep AMP backend lazy to avoid dual Audio/I2S contention with story audio.
  if (!g_amp_scene_active) {
    return false;
  }
  const char* const candidates[] = {kAmpMusicPathPrimary, kAmpMusicPathFallback1, kAmpMusicPathFallback2};
  for (const char* candidate : candidates) {
    if (beginAmpAtBase(candidate)) {
      return true;
    }
  }
  return false;
}

size_t scanAmpPlaylistWithFallback() {
  if (!ensureAmpInitialized()) {
    return 0U;
  }
  size_t count = g_amp_player.service().scanPlaylist();
  if (count > 0U) {
    return count;
  }

  const char* const candidates[] = {kAmpMusicPathPrimary, kAmpMusicPathFallback1, kAmpMusicPathFallback2};
  for (const char* candidate : candidates) {
    if (std::strcmp(candidate, g_amp_base_dir) == 0) {
      continue;
    }
    g_amp_player.end();
    g_amp_ready = false;
    if (!beginAmpAtBase(candidate)) {
      continue;
    }
    count = g_amp_player.service().scanPlaylist();
    if (count > 0U) {
      return count;
    }
  }
  return count;
}

void syncAmpSceneState(const ScenarioSnapshot& snapshot) {
  const bool want_amp_scene = isAmpSceneId(snapshot.screen_scene_id);
  setAmpSceneActive(want_amp_scene);
}

void setAmpSceneActive(bool active) {
  if (active == g_amp_scene_active) {
    return;
  }
  g_amp_scene_active = active;
  if (g_amp_scene_active) {
    g_audio.stop();
    if (ensureAmpInitialized()) {
      (void)scanAmpPlaylistWithFallback();
      g_amp_player.show();
    }
    Serial.println("[AMP] scene owner=amp");
    return;
  }
  if (g_amp_ready) {
    g_amp_player.service().stop();
    g_amp_player.hide();
  }
  Serial.println("[AMP] scene owner=story_audio");
}

void printAmpStatus() {
  if (!ensureAmpInitialized()) {
    Serial.println("AMP_STATUS ready=0");
    return;
  }
  const ui::audio::AudioPlayerService::Stats stats = g_amp_player.service().stats();
  const size_t count = g_amp_player.service().trackCount();
  const size_t index = g_amp_player.service().currentIndex();
  const char* path = g_amp_player.service().currentPath();
  Serial.printf("AMP_STATUS ready=1 visible=%u scene=%u base=%s tracks=%u idx=%u path=%s state=%u pos=%lu dur=%lu vu=%u\n",
                g_amp_player.visible() ? 1U : 0U,
                g_amp_scene_active ? 1U : 0U,
                g_amp_base_dir,
                static_cast<unsigned int>(count),
                static_cast<unsigned int>(index),
                (path != nullptr && path[0] != '\0') ? path : "n/a",
                static_cast<unsigned int>(stats.state),
                static_cast<unsigned long>(stats.position_s),
                static_cast<unsigned long>(stats.duration_s),
                static_cast<unsigned int>(stats.vu));
}
#endif

bool isCameraSceneId(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strcmp(scene_id, kCameraSceneId) == 0;
}

bool ensureCameraUiInitialized() {
  if (g_camera_scene_ready) {
    return true;
  }
  ui::camera::Win311CameraUI::UiConfig ui_cfg;
  ui_cfg.start_visible = false;
  ui_cfg.base_dir = "/picture";
  ui_cfg.camera = &g_camera;
  ui_cfg.capture_keys_when_visible = true;
  ui::camera::CameraCaptureService::Config service_cfg;
  service_cfg.camera = &g_camera;
  service_cfg.base_dir = "/picture";
  g_camera_scene_ready = g_camera_player.begin(ui_cfg, service_cfg);
  if (!g_camera_scene_ready) {
    Serial.println("[CAM_UI] init failed");
    return false;
  }
  g_camera_player.hide();
  Serial.println("[CAM_UI] ready");
  return true;
}

void syncCameraSceneState(const ScenarioSnapshot& snapshot) {
  const bool want_camera_scene = isCameraSceneId(snapshot.screen_scene_id);
  setCameraSceneActive(want_camera_scene);
}

void setCameraSceneActive(bool active) {
  if (active == g_camera_scene_active) {
    return;
  }
  g_camera_scene_active = active;
  if (g_camera_scene_active) {
    if (!ensureCameraUiInitialized()) {
      g_camera_scene_active = false;
      return;
    }
    if (!g_camera.startRecorderSession()) {
      Serial.println("[CAM_UI] recorder session start failed");
      g_camera_player.show();
      return;
    }
    g_camera_player.show();
    Serial.println("[CAM_UI] scene owner=recorder");
    return;
  }
  if (g_camera_scene_ready) {
    g_camera_player.hide();
    g_camera_player.service().discard_frozen();
  }
  g_camera.stopRecorderSession();
  Serial.println("[CAM_UI] scene owner=legacy");
}

bool dispatchCameraSceneButton(uint8_t key, bool long_press) {
  if (!g_camera_scene_active || !g_camera_scene_ready) {
    return false;
  }
  ui::camera::Win311CameraUI::InputAction action = ui::camera::Win311CameraUI::InputAction::kSnapToggle;
  switch (key) {
    case 1U:
      action = ui::camera::Win311CameraUI::InputAction::kSnapToggle;
      break;
    case 2U:
      action = ui::camera::Win311CameraUI::InputAction::kSave;
      break;
    case 3U:
      action = long_press ? ui::camera::Win311CameraUI::InputAction::kGalleryNext
                          : ui::camera::Win311CameraUI::InputAction::kGalleryToggle;
      break;
    case 4U:
      action = ui::camera::Win311CameraUI::InputAction::kDeleteSelected;
      break;
    case 5U:
      action = ui::camera::Win311CameraUI::InputAction::kClose;
      break;
    default:
      return false;
  }
  return g_camera_player.handleInputAction(action);
}

void printCameraRecorderStatus() {
  const CameraManager::Snapshot camera = g_camera.snapshot();
  Serial.printf(
      "CAM_REC_STATUS scene=%u ui_ready=%u visible=%u session=%u frozen=%u preview=%ux%u selected=%s last=%s err=%s\n",
      g_camera_scene_active ? 1U : 0U,
      g_camera_scene_ready ? 1U : 0U,
      (g_camera_scene_ready && g_camera_player.visible()) ? 1U : 0U,
      camera.recorder_session_active ? 1U : 0U,
      camera.recorder_frozen ? 1U : 0U,
      static_cast<unsigned int>(camera.recorder_preview_width),
      static_cast<unsigned int>(camera.recorder_preview_height),
      camera.recorder_selected_file[0] != '\0' ? camera.recorder_selected_file : "n/a",
      camera.last_file[0] != '\0' ? camera.last_file : "n/a",
      camera.last_error[0] != '\0' ? camera.last_error : "none");
}

void onAudioFinished(const char* track, void* ctx) {
  (void)ctx;
  Serial.printf("[MAIN] audio done: %s\n", track != nullptr ? track : "unknown");
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  if (g_amp_scene_active) {
    return;
  }
#endif
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  if (snapshot.step != nullptr &&
      snapshot.step->id != nullptr &&
      std::strcmp(snapshot.step->id, kStepWinEtape) == 0 &&
      snapshot.audio_pack_id != nullptr &&
      std::strcmp(snapshot.audio_pack_id, kPackWin) == 0 &&
      !g_has_ring_sent_for_win_etape) {
    g_has_ring_sent_for_win_etape = sendRingCommandToRtc();
  }
  if (snapshot.step != nullptr && snapshot.step->id != nullptr && std::strcmp(snapshot.step->id, kStepWinEtape) == 0 &&
      snapshot.audio_pack_id != nullptr && std::strcmp(snapshot.audio_pack_id, kPackWin) == 0) {
    g_win_etape_ui_refresh_pending = true;
  }
  g_scenario.notifyAudioDone(millis());
}

void printButtonRead() {
  Serial.printf("BTN mv=%d key=%u\n", g_buttons.lastAnalogMilliVolts(), g_buttons.currentKey());
}

void printRuntimeStatus() {
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const NetworkManager::Snapshot net = g_network.snapshot();
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  const CameraManager::Snapshot camera = g_camera.snapshot();
  const MediaManager::Snapshot media = g_media.snapshot();
  const runtime::resource::ResourceCoordinatorSnapshot resource = g_resource_coordinator.snapshot();
  const char* scenario_id = scenarioIdFromSnapshot(snapshot);
  const char* step_id = stepIdFromSnapshot(snapshot);
  const char* screen_id = (snapshot.screen_scene_id != nullptr) ? snapshot.screen_scene_id : "n/a";
  const char* audio_pack = (snapshot.audio_pack_id != nullptr) ? snapshot.audio_pack_id : "n/a";
  Serial.printf("STATUS scenario=%s step=%s screen=%s pack=%s audio=%u track=%s codec=%s bitrate=%u profile=%u:%s fx=%u:%s vol=%u "
                "net=%s/%s sta=%u connecting=%u ap=%u espnow=%u peers=%u ip=%s key=%u mv=%d "
                "hw=%u mic=%u battery=%u cam=%u media_play=%u rec=%u res=%s pressure=%u mic_should=%u cam_allow=%u\n",
                scenario_id,
                step_id,
                screen_id,
                audio_pack,
                g_audio.isPlaying() ? 1 : 0,
                g_audio.currentTrack(),
                g_audio.activeCodec(),
                g_audio.activeBitrateKbps(),
                g_audio.outputProfile(),
                g_audio.outputProfileLabel(g_audio.outputProfile()),
                g_audio.fxProfile(),
                g_audio.fxProfileLabel(g_audio.fxProfile()),
                g_audio.volume(),
                net.state,
                net.mode,
                net.sta_connected ? 1U : 0U,
                net.sta_connecting ? 1U : 0U,
                net.ap_enabled ? 1U : 0U,
                net.espnow_enabled ? 1U : 0U,
                net.espnow_peer_count,
                net.ip,
                g_buttons.currentKey(),
                g_buttons.lastAnalogMilliVolts(),
                hw.ready ? 1U : 0U,
                hw.mic_level_percent,
                hw.battery_percent,
                camera.enabled ? 1U : 0U,
                media.playing ? 1U : 0U,
                media.recording ? 1U : 0U,
                g_resource_coordinator.profileName(),
                resource.graphics_pressure ? 1U : 0U,
                resource.mic_should_run ? 1U : 0U,
                resource.allow_camera_ops ? 1U : 0U);
}

void printHardwareStatus() {
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  Serial.printf(
      "HW_STATUS ready=%u ws2812=%u mic=%u battery=%u auto=%u manual=%u led=%u,%u,%u br=%u "
      "mic_pct=%u mic_peak=%u mic_noise=%u mic_gain=%u mic_freq=%u mic_cents=%d mic_conf=%u "
      "la_gate=%u la_match=%u la_lock=%u la_pending=%u la_stable_ms=%lu la_timeout_ms=%lu "
      "battery_pct=%u battery_mv=%u charging=%u scene=%s\n",
      hw.ready ? 1U : 0U,
      hw.ws2812_ready ? 1U : 0U,
      hw.mic_ready ? 1U : 0U,
      hw.battery_ready ? 1U : 0U,
      g_hardware_cfg.led_auto_from_scene ? 1U : 0U,
      hw.led_manual ? 1U : 0U,
      hw.led_r,
      hw.led_g,
      hw.led_b,
      hw.led_brightness,
      hw.mic_level_percent,
      hw.mic_peak,
      static_cast<unsigned int>(hw.mic_noise_floor),
      static_cast<unsigned int>(hw.mic_gain_percent),
      hw.mic_freq_hz,
      static_cast<int>(hw.mic_pitch_cents),
      hw.mic_pitch_confidence,
      g_la_trigger.gate_active ? 1U : 0U,
      g_la_trigger.sample_match ? 1U : 0U,
      g_la_trigger.locked ? 1U : 0U,
      g_la_trigger.timeout_pending ? 1U : 0U,
      static_cast<unsigned long>(g_la_trigger.stable_ms),
      static_cast<unsigned long>(g_hardware_cfg.mic_la_timeout_ms),
      hw.battery_percent,
      hw.battery_cell_mv,
      hw.charging ? 1U : 0U,
      hw.scene_id);
}

void printMicTunerStatus() {
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  Serial.printf(
      "MIC_TUNER_STATUS freq=%u cents=%d conf=%u level=%u peak=%u noise=%u gain=%u scene=%s stream=%u period_ms=%u "
      "la_gate=%u la_match=%u la_lock=%u la_pending=%u la_stable_ms=%lu la_pct=%u\n",
                static_cast<unsigned int>(hw.mic_freq_hz),
                static_cast<int>(hw.mic_pitch_cents),
                static_cast<unsigned int>(hw.mic_pitch_confidence),
                static_cast<unsigned int>(hw.mic_level_percent),
                static_cast<unsigned int>(hw.mic_peak),
                static_cast<unsigned int>(hw.mic_noise_floor),
                static_cast<unsigned int>(hw.mic_gain_percent),
                hw.scene_id,
                g_mic_tuner_stream_enabled ? 1U : 0U,
                static_cast<unsigned int>(g_mic_tuner_stream_period_ms),
                g_la_trigger.gate_active ? 1U : 0U,
                g_la_trigger.sample_match ? 1U : 0U,
                g_la_trigger.locked ? 1U : 0U,
                g_la_trigger.timeout_pending ? 1U : 0U,
                static_cast<unsigned long>(g_la_trigger.stable_ms),
                static_cast<unsigned int>(laStablePercent()));
}

void printHardwareStatusJson() {
  StaticJsonDocument<1024> document;
  webFillHardwareStatus(document.to<JsonObject>());
  serializeJson(document, Serial);
  Serial.println();
}

void printCameraStatus() {
  const CameraManager::Snapshot camera = g_camera.snapshot();
  Serial.printf(
      "CAM_STATUS supported=%u enabled=%u init=%u frame=%s quality=%u fb=%u xclk=%lu captures=%lu fails=%lu "
      "rec_scene=%u rec_session=%u rec_frozen=%u preview=%ux%u last=%s err=%s\n",
                camera.supported ? 1U : 0U,
                camera.enabled ? 1U : 0U,
                camera.initialized ? 1U : 0U,
                camera.frame_size,
                static_cast<unsigned int>(camera.jpeg_quality),
                static_cast<unsigned int>(camera.fb_count),
                static_cast<unsigned long>(camera.xclk_hz),
                static_cast<unsigned long>(camera.capture_count),
                static_cast<unsigned long>(camera.fail_count),
                g_camera_scene_active ? 1U : 0U,
                camera.recorder_session_active ? 1U : 0U,
                camera.recorder_frozen ? 1U : 0U,
                static_cast<unsigned int>(camera.recorder_preview_width),
                static_cast<unsigned int>(camera.recorder_preview_height),
                camera.last_file[0] != '\0' ? camera.last_file : "n/a",
                camera.last_error[0] != '\0' ? camera.last_error : "none");
}

bool approveCameraOperation(const char* operation, String* out_error) {
  if (g_resource_coordinator.approveCameraOperation()) {
    return true;
  }
  if (out_error != nullptr) {
    *out_error = "camera_blocked_by_resource_profile";
  }
  Serial.printf("[RESOURCE] camera op blocked profile=%s op=%s\n",
                g_resource_coordinator.profileName(),
                (operation != nullptr) ? operation : "unknown");
  return false;
}

void printResourceStatus() {
  const runtime::resource::ResourceCoordinatorSnapshot snapshot = g_resource_coordinator.snapshot();
  const UiMemorySnapshot ui_mem = g_ui.memorySnapshot();
  Serial.printf(
      "RESOURCE_STATUS profile=%s profile_auto=%u pressure=%u mic_should_run=%u mic_force=%u cam_allow=%u pressure_until=%lu mic_hold_until=%lu cam_cooldown_until=%lu cam_allowed=%lu cam_blocked=%lu "
      "delta_ovf=%lu delta_block=%lu draw_avg=%lu draw_max=%lu flush_avg=%lu flush_max=%lu fx_fps=%u ui_block=%lu ui_ovf=%lu ui_stall=%lu ui_recover=%lu\n",
      g_resource_coordinator.profileName(),
      g_resource_profile_auto ? 1U : 0U,
      snapshot.graphics_pressure ? 1U : 0U,
      snapshot.mic_should_run ? 1U : 0U,
      snapshot.mic_force_on ? 1U : 0U,
      snapshot.allow_camera_ops ? 1U : 0U,
      static_cast<unsigned long>(snapshot.pressure_until_ms),
      static_cast<unsigned long>(snapshot.mic_hold_until_ms),
      static_cast<unsigned long>(snapshot.camera_cooldown_until_ms),
      static_cast<unsigned long>(snapshot.camera_allowed_ops),
      static_cast<unsigned long>(snapshot.camera_blocked_ops),
      static_cast<unsigned long>(snapshot.flush_overflow_delta),
      static_cast<unsigned long>(snapshot.flush_blocked_delta),
      static_cast<unsigned long>(snapshot.last_draw_avg_us),
      static_cast<unsigned long>(snapshot.last_draw_max_us),
      static_cast<unsigned long>(snapshot.last_flush_avg_us),
      static_cast<unsigned long>(snapshot.last_flush_max_us),
      static_cast<unsigned int>(ui_mem.fx_fps),
      static_cast<unsigned long>(ui_mem.flush_blocked),
      static_cast<unsigned long>(ui_mem.flush_overflow),
      static_cast<unsigned long>(ui_mem.flush_stall),
      static_cast<unsigned long>(ui_mem.flush_recover));
}

void printSimdStatus() {
  const runtime::simd::SimdAccelStatus& status = runtime::simd::status();
  Serial.printf("SIMD_STATUS enabled=%u esp_dsp=%u selftest_runs=%lu selftest_fail=%lu bench_runs=%lu loops=%lu pixels=%lu l8_us=%lu idx_us=%lu rgb888_us=%lu gain_us=%lu\n",
                status.simd_path_enabled ? 1U : 0U,
                status.esp_dsp_enabled ? 1U : 0U,
                static_cast<unsigned long>(status.selftest_runs),
                static_cast<unsigned long>(status.selftest_failures),
                static_cast<unsigned long>(status.bench_runs),
                static_cast<unsigned long>(status.bench_loops),
                static_cast<unsigned long>(status.bench_pixels),
                static_cast<unsigned long>(status.bench_l8_to_rgb565_us),
                static_cast<unsigned long>(status.bench_idx8_to_rgb565_us),
                static_cast<unsigned long>(status.bench_rgb888_to_rgb565_us),
                static_cast<unsigned long>(status.bench_s16_gain_q15_us));
}

void printMediaStatus() {
  const MediaManager::Snapshot media = g_media.snapshot();
  Serial.printf("REC_STATUS playing=%u recording=%u elapsed=%u/%u file=%s music_dir=%s picture_dir=%s record_dir=%s last_ok=%u err=%s\n",
                media.playing ? 1U : 0U,
                media.recording ? 1U : 0U,
                static_cast<unsigned int>(media.record_elapsed_seconds),
                static_cast<unsigned int>(media.record_limit_seconds),
                media.record_file[0] != '\0' ? media.record_file : "n/a",
                media.music_dir,
                media.picture_dir,
                media.record_dir,
                media.last_ok ? 1U : 0U,
                media.last_error[0] != '\0' ? media.last_error : "none");
}

constexpr const char* kWebUiIndex = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Zacus Freenove</title>
  <style>
    body { font-family: sans-serif; margin: 1rem; background: #111; color: #eee; }
    .card { border: 1px solid #444; border-radius: 8px; padding: 1rem; margin-bottom: 1rem; }
    button { margin: 0.25rem; padding: 0.5rem 0.8rem; }
    input { margin: 0.25rem; padding: 0.4rem; }
    pre { white-space: pre-wrap; word-break: break-word; background: #1b1b1b; padding: 0.8rem; border-radius: 6px; }
  </style>
</head>
<body>
  <h2>Zacus Freenove WebUI</h2>
  <div class="card">
    <button onclick="unlock()">UNLOCK</button>
    <button onclick="nextStep()">NEXT</button>
    <button onclick="storyRefreshSd()">STORY_REFRESH_SD</button>
    <button onclick="wifiDisc()">WIFI_DISCONNECT</button>
    <button onclick="wifiReconn()">WIFI_RECONNECT</button>
    <button onclick="refreshStatus()">Refresh</button>
  </div>
  <div class="card">
    <input id="ssid" placeholder="SSID" />
    <input id="pass" placeholder="Password" />
    <button onclick="wifiConn()">WIFI_CONNECT</button>
  </div>
  <div class="card">
    <input id="token" placeholder="Bearer token" />
    <button onclick="saveToken()">SET TOKEN</button>
  </div>
  <div class="card">
    <input id="payload" placeholder="Payload" />
    <button onclick="espnowSend()">ESPNOW_SEND</button>
    <button onclick="espnowOn()">ESPNOW_ON</button>
    <button onclick="espnowOff()">ESPNOW_OFF</button>
  </div>
  <div class="card">
    <pre id="status">loading...</pre>
  </div>
  <script>
    let stream;
    let reconnectTimer;
    function showStatus(json) {
      document.getElementById("status").textContent = JSON.stringify(json, null, 2);
    }
    const tokenStorageKey = "zacus_web_token";
    let apiToken = localStorage.getItem(tokenStorageKey) || "";
    function authHeaders() {
      if (!apiToken) {
        return {};
      }
      return { "Authorization": "Bearer " + apiToken };
    }
    function saveToken() {
      apiToken = (document.getElementById("token").value || "").trim();
      localStorage.setItem(tokenStorageKey, apiToken);
      refreshStatus();
      connectStream();
    }
    async function post(path, params) {
      const body = new URLSearchParams(params || {});
      await fetch(path, { method: "POST", body, headers: authHeaders() });
      await refreshStatus();
    }
    async function refreshStatus() {
      const res = await fetch("/api/status", { headers: authHeaders() });
      if (!res.ok) {
        document.getElementById("status").textContent = "HTTP " + res.status;
        return;
      }
      const json = await res.json();
      showStatus(json);
    }
    function connectStream() {
      if (stream) {
        stream.close();
        stream = null;
      }
      if (apiToken || typeof EventSource === "undefined") {
        return;
      }
      stream = new EventSource("/api/stream");
      stream.addEventListener("status", (evt) => {
        try {
          showStatus(JSON.parse(evt.data));
        } catch (err) {
          console.warn("status parse failed", err);
        }
      });
      stream.addEventListener("done", () => {
        stream.close();
        clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(connectStream, 400);
      });
      stream.onerror = () => {
        if (stream) {
          stream.close();
        }
        clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(connectStream, 1000);
      };
    }
    function unlock() { return post("/api/scenario/unlock"); }
    function nextStep() { return post("/api/scenario/next"); }
    function storyRefreshSd() { return post("/api/story/refresh-sd"); }
    function wifiDisc() { return post("/api/wifi/disconnect"); }
    function wifiReconn() { return post("/api/network/wifi/reconnect"); }
    function wifiConn() {
      return post("/api/wifi/connect", {
        ssid: document.getElementById("ssid").value,
        password: document.getElementById("pass").value,
        persist: 1
      });
    }
    function espnowOn() { return post("/api/network/espnow/on"); }
    function espnowOff() { return post("/api/network/espnow/off"); }
    function espnowSend() {
      return post("/api/espnow/send", {
        payload: document.getElementById("payload").value
      });
    }
    document.getElementById("token").value = apiToken;
    refreshStatus();
    setInterval(refreshStatus, 3000);
    connectStream();
  </script>
</body>
</html>
)HTML";

template <typename TDocument>
void webSendJsonDocument(const TDocument& document, int status_code = 200) {
  String payload;
  serializeJson(document, payload);
  g_web_server.send(status_code, "application/json", payload);
}

void webSendResult(const char* action, bool ok) {
  StaticJsonDocument<192> document;
  document["action"] = action;
  document["ok"] = ok;
  webSendJsonDocument(document, ok ? 200 : 400);
}

template <size_t N>
bool webParseJsonBody(StaticJsonDocument<N>* out_document) {
  if (out_document == nullptr || !g_web_server.hasArg("plain")) {
    return false;
  }
  const String body = g_web_server.arg("plain");
  if (body.isEmpty()) {
    return false;
  }
  const DeserializationError error = deserializeJson(*out_document, body);
  return !error;
}

void clearRuntimeStaCredentials() {
  g_network_cfg.local_ssid[0] = '\0';
  g_network_cfg.local_password[0] = '\0';
  g_network_cfg.wifi_test_ssid[0] = '\0';
  g_network_cfg.wifi_test_password[0] = '\0';
}

void applyRuntimeStaCredentials(const char* ssid, const char* password) {
  copyText(g_network_cfg.local_ssid, sizeof(g_network_cfg.local_ssid), (ssid != nullptr) ? ssid : "");
  copyText(g_network_cfg.local_password, sizeof(g_network_cfg.local_password), (password != nullptr) ? password : "");
  copyText(g_network_cfg.wifi_test_ssid, sizeof(g_network_cfg.wifi_test_ssid), g_network_cfg.local_ssid);
  copyText(g_network_cfg.wifi_test_password, sizeof(g_network_cfg.wifi_test_password), g_network_cfg.local_password);
}

void updateAuthPolicy() {
  g_web_auth_required = !g_setup_mode;
}

bool ensureWebToken(bool rotate_token, bool print_token, bool* out_generated) {
  if (out_generated != nullptr) {
    *out_generated = false;
  }
  if (!rotate_token && g_web_auth_token[0] != '\0') {
    return true;
  }
  if (!rotate_token && g_credential_store.loadWebToken(g_web_auth_token, sizeof(g_web_auth_token))) {
    return true;
  }
  if (!g_credential_store.generateWebToken(g_web_auth_token, sizeof(g_web_auth_token))) {
    g_web_auth_token[0] = '\0';
    return false;
  }
  if (!g_credential_store.saveWebToken(g_web_auth_token)) {
    g_web_auth_token[0] = '\0';
    return false;
  }
  if (out_generated != nullptr) {
    *out_generated = true;
  }
  if (print_token) {
    Serial.printf("[AUTH] web token=%s\n", g_web_auth_token);
  }
  return true;
}

void loadBootProvisioningState() {
  clearRuntimeStaCredentials();
  char stored_ssid[sizeof(g_network_cfg.local_ssid)] = {0};
  char stored_password[sizeof(g_network_cfg.local_password)] = {0};
  const bool has_credentials =
      g_credential_store.loadStaCredentials(stored_ssid, sizeof(stored_ssid), stored_password, sizeof(stored_password));
  if (has_credentials) {
    applyRuntimeStaCredentials(stored_ssid, stored_password);
  }
  g_setup_mode = !has_credentials;
  updateAuthPolicy();
  if (!g_setup_mode) {
    bool token_generated = false;
    if (!ensureWebToken(false, true, &token_generated)) {
      Serial.println("[AUTH] web token load/generation failed");
    }
  }
}

bool provisionWifiCredentials(const char* ssid,
                              const char* password,
                              bool persist,
                              bool* out_connect_started,
                              bool* out_persisted,
                              bool* out_token_generated) {
  if (out_connect_started != nullptr) {
    *out_connect_started = false;
  }
  if (out_persisted != nullptr) {
    *out_persisted = false;
  }
  if (out_token_generated != nullptr) {
    *out_token_generated = false;
  }
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }

  bool persisted = true;
  if (persist) {
    persisted = g_credential_store.saveStaCredentials(ssid, (password != nullptr) ? password : "");
    if (persisted) {
      applyRuntimeStaCredentials(ssid, (password != nullptr) ? password : "");
      g_network.configureLocalPolicy(g_network_cfg.local_ssid,
                                     g_network_cfg.local_password,
                                     g_network_cfg.force_ap_if_not_local,
                                     g_network_cfg.local_retry_ms,
                                     g_network_cfg.pause_local_retry_when_ap_client);
      g_setup_mode = false;
      updateAuthPolicy();
      bool token_generated = false;
      if (!ensureWebToken(false, true, &token_generated)) {
        persisted = false;
      } else if (out_token_generated != nullptr) {
        *out_token_generated = token_generated;
      }
      g_network.stopAp();
    }
  }

  const bool connect_started = g_network.connectSta(ssid, (password != nullptr) ? password : "");
  if (out_connect_started != nullptr) {
    *out_connect_started = connect_started;
  }
  if (out_persisted != nullptr) {
    *out_persisted = persisted;
  }
  return connect_started && persisted;
}

bool forgetWifiCredentials() {
  const bool cleared = g_credential_store.clearStaCredentials();
  clearRuntimeStaCredentials();
  g_network.configureLocalPolicy(g_network_cfg.local_ssid,
                                 g_network_cfg.local_password,
                                 g_network_cfg.force_ap_if_not_local,
                                 g_network_cfg.local_retry_ms,
                                 g_network_cfg.pause_local_retry_when_ap_client);
  g_network.disconnectSta();
  g_setup_mode = true;
  updateAuthPolicy();
  if (g_network_cfg.ap_default_ssid[0] != '\0') {
    g_network.startAp(g_network_cfg.ap_default_ssid, g_network_cfg.ap_default_password);
  }
  return cleared;
}

bool isSetupWhitelistApiPath(const char* path) {
  if (path == nullptr) {
    return false;
  }
  return std::strcmp(path, kProvisionStatusPath) == 0 || std::strcmp(path, kSetupWifiConnectPath) == 0 ||
         std::strcmp(path, kSetupNetworkWifiConnectPath) == 0;
}

bool hasValidBearerToken() {
  const String header = g_web_server.header(kWebAuthHeaderName);
  if (!header.startsWith(kWebAuthBearerPrefix)) {
    return false;
  }
  String token = header.substring(static_cast<unsigned int>(std::strlen(kWebAuthBearerPrefix)));
  token.trim();
  return g_web_auth_token[0] != '\0' && token == g_web_auth_token;
}

bool webAuthorizeApiRequest(const char* path) {
  if (path == nullptr || std::strncmp(path, "/api/", 5U) != 0) {
    return true;
  }
  if (g_setup_mode) {
    if (isSetupWhitelistApiPath(path)) {
      return true;
    }
    g_web_server.send(403, "application/json", "{\"ok\":false,\"error\":\"setup_mode_restricted\"}");
    return false;
  }
  if (!g_web_auth_required) {
    return true;
  }
  if (g_web_auth_token[0] == '\0') {
    g_web_server.send(503, "application/json", "{\"ok\":false,\"error\":\"auth_token_missing\"}");
    return false;
  }
  if (hasValidBearerToken()) {
    return true;
  }
  g_web_server.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
  return false;
}

template <typename Handler>
void webOnApi(const char* path, HTTPMethod method, Handler&& handler) {
  g_web_server.on(path, method, [path, handler]() {
    if (!webAuthorizeApiRequest(path)) {
      return;
    }
    handler();
  });
}

void webFillEspNowStatus(JsonObject out, const NetworkManager::Snapshot& net) {
  out["ready"] = net.espnow_enabled;
  out["peer_count"] = net.espnow_peer_count;
  out["tx_ok"] = net.espnow_tx_ok;
  out["tx_fail"] = net.espnow_tx_fail;
  out["rx_count"] = net.espnow_rx_packets;
  out["last_rx_mac"] = String(net.last_rx_peer);
  out["last_msg_id"] = String(net.last_msg_id);
  out["last_seq"] = net.espnow_last_seq;
  out["last_type"] = String(net.last_type);
  out["last_ack"] = net.espnow_last_ack;
  out["last_payload"] = String(net.last_payload);
  JsonArray peers = out["peers"].to<JsonArray>();
  for (uint8_t index = 0U; index < g_network.espNowPeerCount(); ++index) {
    char peer[18] = {0};
    if (!g_network.espNowPeerAt(index, peer, sizeof(peer))) {
      continue;
    }
    peers.add(peer);
  }
}

void webFillWifiStatus(JsonObject out, const NetworkManager::Snapshot& net) {
  out["connected"] = net.sta_connected;
  out["has_credentials"] = (g_network_cfg.local_ssid[0] != '\0');
  out["setup_mode"] = g_setup_mode;
  out["auth_required"] = g_web_auth_required;
  out["token_set"] = (g_web_auth_token[0] != '\0');
  out["ssid"] = String(net.sta_ssid);
  out["ip"] = net.sta_connected ? String(net.ip) : String("");
  out["rssi"] = net.rssi;
  out["state"] = String(net.state);
  out["ap_active"] = net.ap_enabled;
  out["ap_ssid"] = String(net.ap_ssid);
  out["ap_ip"] = (!net.sta_connected && net.ap_enabled) ? String(net.ip) : String("");
  out["ap_clients"] = net.ap_clients;
  out["local_retry_paused"] = net.local_retry_paused;
  out["mode"] = String(net.mode);
}

void webFillHardwareStatus(JsonObject out) {
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  out["ready"] = hw.ready;
  out["enabled_on_boot"] = g_hardware_cfg.enabled_on_boot;
  out["led_auto_from_scene"] = g_hardware_cfg.led_auto_from_scene;
  out["telemetry_period_ms"] = g_hardware_cfg.telemetry_period_ms;
  out["ws2812_ready"] = hw.ws2812_ready;
  out["mic_ready"] = hw.mic_ready;
  out["battery_ready"] = hw.battery_ready;
  out["led_manual"] = hw.led_manual;
  JsonObject led = out["led"].to<JsonObject>();
  led["r"] = hw.led_r;
  led["g"] = hw.led_g;
  led["b"] = hw.led_b;
  out["led_brightness"] = hw.led_brightness;
  out["mic_enabled"] = g_hardware_cfg.mic_enabled;
  out["mic_threshold_pct"] = g_hardware_cfg.mic_event_threshold_pct;
  out["mic_level_pct"] = hw.mic_level_percent;
  out["mic_peak"] = hw.mic_peak;
  out["mic_noise_floor"] = hw.mic_noise_floor;
  out["mic_gain_pct"] = hw.mic_gain_percent;
  out["mic_freq_hz"] = hw.mic_freq_hz;
  out["mic_pitch_cents"] = hw.mic_pitch_cents;
  out["mic_pitch_confidence"] = hw.mic_pitch_confidence;
  JsonObject la_trigger = out["la_trigger"].to<JsonObject>();
  la_trigger["enabled"] = g_hardware_cfg.mic_la_trigger_enabled;
  la_trigger["target_hz"] = g_hardware_cfg.mic_la_target_hz;
  la_trigger["tolerance_hz"] = g_hardware_cfg.mic_la_tolerance_hz;
  la_trigger["max_abs_cents"] = g_hardware_cfg.mic_la_max_abs_cents;
  la_trigger["min_confidence"] = g_hardware_cfg.mic_la_min_confidence;
  la_trigger["min_level_pct"] = g_hardware_cfg.mic_la_min_level_pct;
  la_trigger["stable_ms"] = g_hardware_cfg.mic_la_stable_ms;
  la_trigger["release_ms"] = g_hardware_cfg.mic_la_release_ms;
  la_trigger["cooldown_ms"] = g_hardware_cfg.mic_la_cooldown_ms;
  la_trigger["timeout_ms"] = g_hardware_cfg.mic_la_timeout_ms;
  la_trigger["event_name"] = g_hardware_cfg.mic_la_event_name;
  la_trigger["gate_active"] = g_la_trigger.gate_active;
  la_trigger["sample_match"] = g_la_trigger.sample_match;
  la_trigger["locked"] = g_la_trigger.locked;
  la_trigger["timeout_pending"] = g_la_trigger.timeout_pending;
  la_trigger["stable_now_ms"] = g_la_trigger.stable_ms;
  la_trigger["stable_pct"] = laStablePercent();
  out["battery_enabled"] = g_hardware_cfg.battery_enabled;
  out["battery_low_pct"] = g_hardware_cfg.battery_low_pct;
  out["battery_pct"] = hw.battery_percent;
  out["battery_mv"] = hw.battery_cell_mv;
  out["charging"] = hw.charging;
  out["last_button"] = hw.last_button;
  out["scene_id"] = hw.scene_id;
}

void webFillCameraStatus(JsonObject out) {
  const CameraManager::Snapshot camera = g_camera.snapshot();
  out["supported"] = camera.supported;
  out["enabled"] = camera.enabled;
  out["initialized"] = camera.initialized;
  out["enabled_on_boot"] = g_camera_cfg.enabled_on_boot;
  out["frame_size"] = camera.frame_size;
  out["jpeg_quality"] = camera.jpeg_quality;
  out["fb_count"] = camera.fb_count;
  out["xclk_hz"] = camera.xclk_hz;
  out["snapshot_dir"] = camera.snapshot_dir;
  out["capture_count"] = camera.capture_count;
  out["fail_count"] = camera.fail_count;
  out["last_capture_ms"] = camera.last_capture_ms;
  out["last_file"] = camera.last_file;
  out["last_error"] = camera.last_error;
  out["scene_active"] = g_camera_scene_active;
  out["ui_ready"] = g_camera_scene_ready;
  out["ui_visible"] = g_camera_scene_ready ? g_camera_player.visible() : false;
  out["recorder_session_active"] = camera.recorder_session_active;
  out["recorder_frozen"] = camera.recorder_frozen;
  out["recorder_preview_width"] = camera.recorder_preview_width;
  out["recorder_preview_height"] = camera.recorder_preview_height;
  out["recorder_selected_file"] = camera.recorder_selected_file;
}

void webFillMediaStatus(JsonObject out, uint32_t now_ms) {
  const MediaManager::Snapshot media = g_media.snapshot();
  out["ready"] = media.ready;
  out["playing"] = media.playing;
  out["playing_path"] = media.playing_path;
  out["recording"] = media.recording;
  out["record_limit_seconds"] = media.record_limit_seconds;
  uint16_t elapsed = media.record_elapsed_seconds;
  if (media.recording && media.record_started_ms > 0U) {
    elapsed = static_cast<uint16_t>((now_ms - media.record_started_ms) / 1000U);
  }
  out["record_elapsed_seconds"] = elapsed;
  out["record_file"] = media.record_file;
  out["record_simulated"] = media.record_simulated;
  out["music_dir"] = media.music_dir;
  out["picture_dir"] = media.picture_dir;
  out["record_dir"] = media.record_dir;
  out["last_ok"] = media.last_ok;
  out["last_error"] = media.last_error;
}

void webSendWifiStatus() {
  const NetworkManager::Snapshot net = g_network.snapshot();
  StaticJsonDocument<384> document;
  webFillWifiStatus(document.to<JsonObject>(), net);
  webSendJsonDocument(document);
}

void webSendEspNowStatus() {
  const NetworkManager::Snapshot net = g_network.snapshot();
  StaticJsonDocument<512> document;
  webFillEspNowStatus(document.to<JsonObject>(), net);
  webSendJsonDocument(document);
}

void webSendHardwareStatus() {
  StaticJsonDocument<1280> document;
  webFillHardwareStatus(document.to<JsonObject>());
  webSendJsonDocument(document);
}

void webSendCameraStatus() {
  StaticJsonDocument<768> document;
  webFillCameraStatus(document.to<JsonObject>());
  webSendJsonDocument(document);
}

void webSendMediaFiles() {
  String kind = g_web_server.arg("kind");
  if (kind.isEmpty()) {
    kind = "music";
  }
  String files_json;
  const bool ok = g_media.listFiles(kind.c_str(), &files_json);
  DynamicJsonDocument response(3072);
  response["ok"] = ok;
  response["kind"] = kind;
  if (ok) {
    DynamicJsonDocument files_doc(2048);
    if (!deserializeJson(files_doc, files_json)) {
      response["files"] = files_doc.as<JsonArrayConst>();
    } else {
      response["files_raw"] = files_json;
    }
  } else {
    response["error"] = "invalid_kind";
  }
  webSendJsonDocument(response, ok ? 200 : 400);
}

void webSendMediaRecordStatus() {
  StaticJsonDocument<768> document;
  webFillMediaStatus(document.to<JsonObject>(), millis());
  webSendJsonDocument(document);
}

void webSendAuthStatus() {
  StaticJsonDocument<256> document;
  document["setup_mode"] = g_setup_mode;
  document["auth_required"] = g_web_auth_required;
  document["token_set"] = (g_web_auth_token[0] != '\0');
  document["provisioned"] = g_credential_store.isProvisioned();
  document["has_credentials"] = (g_network_cfg.local_ssid[0] != '\0');
  webSendJsonDocument(document);
}

void webSendProvisionStatus() {
  StaticJsonDocument<384> document;
  const NetworkManager::Snapshot net = g_network.snapshot();
  document["setup_mode"] = g_setup_mode;
  document["auth_required"] = g_web_auth_required;
  document["token_set"] = (g_web_auth_token[0] != '\0');
  document["provisioned"] = g_credential_store.isProvisioned();
  document["has_credentials"] = (g_network_cfg.local_ssid[0] != '\0');
  document["sta_connected"] = net.sta_connected;
  document["sta_connecting"] = net.sta_connecting;
  document["ap_enabled"] = net.ap_enabled;
  document["sta_ssid"] = net.sta_ssid;
  document["ap_ssid"] = net.ap_ssid;
  document["ip"] = net.ip;
  webSendJsonDocument(document);
}

void webSendEspNowPeerList() {
  StaticJsonDocument<384> document;
  JsonArray peers = document.to<JsonArray>();
  for (uint8_t index = 0U; index < g_network.espNowPeerCount(); ++index) {
    char peer[18] = {0};
    if (!g_network.espNowPeerAt(index, peer, sizeof(peer))) {
      continue;
    }
    peers.add(peer);
  }
  webSendJsonDocument(document);
}

bool webReconnectLocalWifi() {
  if (g_network_cfg.local_ssid[0] == '\0') {
    return false;
  }
  return g_network.connectSta(g_network_cfg.local_ssid, g_network_cfg.local_password);
}

void webScheduleStaDisconnect() {
  g_web_disconnect_sta_pending = true;
  g_web_disconnect_sta_at_ms = millis() + 250U;
}

bool refreshStoryFromSd() {
  const bool synced_tree = g_storage.syncStoryTreeFromSd();
  const bool synced_default = g_storage.syncStoryFileFromSd(kDefaultScenarioFile);
  const bool synced = synced_tree || synced_default;
  if (!synced) {
    return false;
  }
  const bool reloaded = g_scenario.begin(kDefaultScenarioFile);
  if (reloaded) {
    g_last_action_step_key[0] = '\0';
    resetLaTriggerState(false);
    refreshSceneIfNeeded(true);
    startPendingAudioIfAny();
  }
  Serial.printf("[SCENARIO] refresh from sd synced=%u reload=%u\n", synced ? 1U : 0U, reloaded ? 1U : 0U);
  return reloaded;
}

void maybeEmitHardwareEvents(uint32_t now_ms) {
  if (!g_hardware_started) {
    return;
  }
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  const ScenarioSnapshot scenario = g_scenario.snapshot();

  if (g_hardware_cfg.mic_enabled && hw.mic_ready) {
    if (hw.mic_level_percent >= g_hardware_cfg.mic_event_threshold_pct) {
      if (g_mic_event_armed && g_hardware_cfg.mic_event_name[0] != '\0') {
        dispatchScenarioEventByName(g_hardware_cfg.mic_event_name, now_ms);
        g_mic_event_armed = false;
      }
    } else if (hw.mic_level_percent + 6U < g_hardware_cfg.mic_event_threshold_pct) {
      g_mic_event_armed = true;
    }
  }

  if (g_hardware_cfg.battery_enabled && hw.battery_ready) {
    if (!g_battery_low_latched && hw.battery_percent <= g_hardware_cfg.battery_low_pct &&
        g_hardware_cfg.battery_low_event_name[0] != '\0') {
      dispatchScenarioEventByName(g_hardware_cfg.battery_low_event_name, now_ms);
      g_battery_low_latched = true;
    } else if (g_battery_low_latched && hw.battery_percent > (g_hardware_cfg.battery_low_pct + 4U)) {
      g_battery_low_latched = false;
    }
  }

  updateLaGameplayTrigger(scenario, hw, now_ms);
}

void maybeLogHardwareTelemetry(uint32_t now_ms) {
  if (!g_hardware_started || g_hardware_cfg.telemetry_period_ms < 250U) {
    return;
  }
  if (now_ms < g_next_hw_telemetry_ms) {
    return;
  }
  g_next_hw_telemetry_ms = now_ms + g_hardware_cfg.telemetry_period_ms;
  const HardwareManager::Snapshot& hw = g_hardware.snapshotRef();
  Serial.printf("[HW] telemetry mic=%u%% peak=%u battery=%u%% (%umV) led=%u,%u,%u auto=%u\n",
                hw.mic_level_percent,
                hw.mic_peak,
                hw.battery_percent,
                hw.battery_cell_mv,
                hw.led_r,
                hw.led_g,
                hw.led_b,
                g_hardware_cfg.led_auto_from_scene ? 1U : 0U);
}

void maybeStreamMicTunerStatus(uint32_t now_ms) {
  if (!g_hardware_started || !g_mic_tuner_stream_enabled) {
    return;
  }
  if (now_ms < g_next_mic_tuner_stream_ms) {
    return;
  }
  g_next_mic_tuner_stream_ms = now_ms + g_mic_tuner_stream_period_ms;
  printMicTunerStatus();
}

bool executeStoryAction(const char* action_id, const ScenarioSnapshot& snapshot, uint32_t now_ms) {
  if (action_id == nullptr || action_id[0] == '\0') {
    return false;
  }

  if (std::strcmp(action_id, "ACTION_TRACE_STEP") == 0) {
    Serial.printf("[ACTION] TRACE scenario=%s step=%s screen=%s audio=%s\n",
                  scenarioIdFromSnapshot(snapshot),
                  stepIdFromSnapshot(snapshot),
                  snapshot.screen_scene_id != nullptr ? snapshot.screen_scene_id : "n/a",
                  snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a");
    return true;
  }

  if (std::strcmp(action_id, "ACTION_QUEUE_SONAR") == 0) {
    constexpr const char* kBuiltinSonarPath = "/music/sonar_hint.mp3";
    const bool ok = g_audio.play(kBuiltinSonarPath);
    Serial.printf("[ACTION] QUEUE_AUDIO_PACK pack=PACK_SONAR_HINT path=%s ok=%u source=builtin\n",
                  kBuiltinSonarPath,
                  ok ? 1U : 0U);
    return ok;
  }

  const String action_path = String("/story/actions/") + action_id + ".json";
  String payload = g_storage.loadTextFile(action_path.c_str());
  if (payload.isEmpty()) {
    const char* alias_id = nullptr;
    if (std::strcmp(action_id, "ACTION_QR_CODE_SCANNER_START") == 0) {
      alias_id = "ACTION_QR_SCAN_START";
    } else if (std::strcmp(action_id, "ACTION_SET_BOOT_MEDIA_MANAGER") == 0) {
      alias_id = "ACTION_BOOT_MEDIA_MGR";
    }
    if (alias_id != nullptr) {
      const String alias_path = String("/story/actions/") + alias_id + ".json";
      payload = g_storage.loadTextFile(alias_path.c_str());
      if (!payload.isEmpty()) {
        Serial.printf("[ACTION] payload alias id=%s file=%s\n", action_id, alias_id);
      }
    }
  }
  StaticJsonDocument<512> action_doc;
  if (!payload.isEmpty()) {
    deserializeJson(action_doc, payload);
  }
  const char* action_type = action_doc["type"] | "";

  if (std::strcmp(action_type, "trace_step") == 0) {
    Serial.printf("[ACTION] TRACE scenario=%s step=%s screen=%s audio=%s\n",
                  scenarioIdFromSnapshot(snapshot),
                  stepIdFromSnapshot(snapshot),
                  snapshot.screen_scene_id != nullptr ? snapshot.screen_scene_id : "n/a",
                  snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a");
    return true;
  }

  if (std::strcmp(action_id, "ACTION_REFRESH_SD") == 0 || std::strcmp(action_type, "refresh_storage") == 0) {
    const bool ok = g_storage.syncStoryTreeFromSd() || g_storage.syncStoryFileFromSd(kDefaultScenarioFile);
    Serial.printf("[ACTION] REFRESH_SD ok=%u\n", ok ? 1U : 0U);
    return ok;
  }

  if (std::strcmp(action_id, "ACTION_HW_LED_ALERT") == 0) {
    const uint8_t red = static_cast<uint8_t>(action_doc["config"]["r"] | 255U);
    const uint8_t green = static_cast<uint8_t>(action_doc["config"]["g"] | 60U);
    const uint8_t blue = static_cast<uint8_t>(action_doc["config"]["b"] | 32U);
    const uint8_t brightness = static_cast<uint8_t>(action_doc["config"]["brightness"] | 92U);
    const bool pulse = action_doc["config"]["pulse"] | true;
    return g_hardware.setManualLed(red, green, blue, brightness, pulse);
  }

  if (std::strcmp(action_id, "ACTION_HW_LED_READY") == 0) {
    const bool auto_scene = action_doc["config"]["auto_from_scene"] | true;
    g_hardware.clearManualLed();
    if (auto_scene && snapshot.screen_scene_id != nullptr && g_hardware_cfg.led_auto_from_scene) {
      g_hardware.setSceneHint(snapshot.screen_scene_id);
    }
    return true;
  }

  if (std::strcmp(action_id, "ACTION_CAMERA_SNAPSHOT") == 0) {
    const char* filename = action_doc["config"]["filename"] | "";
    const char* event_name = action_doc["config"]["event_on_success"] | "SERIAL:CAMERA_CAPTURED";
    if (!approveCameraOperation("action_camera_snapshot", nullptr)) {
      return false;
    }
    String out_path;
    const bool ok = g_camera.snapshotToFile(filename[0] != '\0' ? filename : nullptr, &out_path);
    Serial.printf("[ACTION] CAMERA_SNAPSHOT ok=%u path=%s\n", ok ? 1U : 0U, ok ? out_path.c_str() : "n/a");
    if (ok) {
      dispatchScenarioEventByName(event_name, now_ms);
    }
    return ok;
  }

  if (std::strcmp(action_type, "queue_audio_pack") == 0) {
    const char* pack_id = action_doc["config"]["pack_id"] | action_doc["config"]["pack"] | "";
    String audio_path = g_storage.resolveAudioPathByPackId(pack_id);
    if (audio_path.isEmpty()) {
      const char* fallback_file = action_doc["config"]["file"] | action_doc["config"]["path"] | "";
      if (fallback_file[0] != '\0') {
        audio_path = fallback_file;
      }
    }
    if (audio_path.isEmpty()) {
      Serial.printf("[ACTION] QUEUE_AUDIO_PACK missing path pack=%s\n", pack_id);
      return false;
    }
    const bool ok = g_audio.play(audio_path.c_str());
    Serial.printf("[ACTION] QUEUE_AUDIO_PACK pack=%s path=%s ok=%u\n",
                  pack_id[0] != '\0' ? pack_id : "n/a",
                  audio_path.c_str(),
                  ok ? 1U : 0U);
    return ok;
  }

  if (std::strcmp(action_id, "ACTION_MEDIA_PLAY_FILE") == 0) {
    const char* media_file = action_doc["config"]["file"] | action_doc["config"]["path"] | "/music/boot_radio.mp3";
    return g_media.play(media_file, &g_audio);
  }

  if (std::strcmp(action_id, "ACTION_REC_START") == 0) {
    const uint16_t seconds = static_cast<uint16_t>(action_doc["config"]["seconds"] | action_doc["config"]["duration_sec"] |
                                                   g_media_cfg.record_max_seconds);
    const char* filename = action_doc["config"]["filename"] | "";
    return g_media.startRecording(seconds, filename);
  }

  if (std::strcmp(action_id, "ACTION_REC_STOP") == 0) {
    return g_media.stopRecording();
  }

  if (std::strcmp(action_id, "ACTION_ESP_NOW_SEND_ETAPE1") == 0 ||
      std::strcmp(action_id, "ACTION_ESP_NOW_SEND_ETAPE2") == 0 ||
      std::strcmp(action_type, "espnow_send") == 0) {
    const char* target = action_doc["config"]["target"] | action_doc["config"]["peer"] | "broadcast";
    const char* payload = action_doc["config"]["payload"] | "";
    String fallback_payload;
    if (payload[0] == '\0') {
      const char* inferred = std::strstr(action_id, "ETAPE2") != nullptr ? "ACK_WIN2" : "ACK_WIN1";
      fallback_payload = inferred;
      payload = fallback_payload.c_str();
    }
    const bool ok = g_network.sendEspNowTarget(target, payload);
    Serial.printf("[ACTION] ESPNOW_SEND id=%s target=%s payload=%s ok=%u\n",
                  action_id,
                  target,
                  payload,
                  ok ? 1U : 0U);
    return ok;
  }

  if (std::strcmp(action_id, "ACTION_QR_CODE_SCANNER_START") == 0 ||
      std::strcmp(action_type, "qr_scanner_start") == 0) {
    Serial.println("[ACTION] QR scanner gate active");
    return true;
  }

  if (std::strcmp(action_id, "ACTION_WINNER") == 0 ||
      std::strcmp(action_type, "winner_fx") == 0) {
    Serial.println("[ACTION] WINNER effect armed");
    return true;
  }

  if (std::strcmp(action_id, "ACTION_SET_BOOT_MEDIA") == 0 ||
      std::strcmp(action_id, "ACTION_SET_BOOT_MEDIA_MANAGER") == 0) {
    const bool mode_ok = g_boot_mode_store.saveMode(BootModeStore::StartupMode::kMediaManager);
    const bool flag_ok = g_boot_mode_store.setMediaValidated(true);
    applyStartupMode(BootModeStore::StartupMode::kMediaManager);
    Serial.printf("[ACTION] SET_BOOT_MEDIA_MANAGER mode_ok=%u validated_ok=%u\n",
                  mode_ok ? 1U : 0U,
                  flag_ok ? 1U : 0U);
    return mode_ok && flag_ok;
  }

  return false;
}

void executeStoryActionsForStep(const ScenarioSnapshot& snapshot, uint32_t now_ms) {
  if (snapshot.step == nullptr) {
    g_has_ring_sent_for_win_etape = false;
    g_win_etape_ui_refresh_pending = false;
    return;
  }

  char step_key[sizeof(g_last_action_step_key)] = {0};
  snprintf(step_key,
           sizeof(step_key),
           "%s:%s",
           scenarioIdFromSnapshot(snapshot),
           stepIdFromSnapshot(snapshot));
  if (std::strcmp(step_key, g_last_action_step_key) == 0) {
    if (snapshot.action_ids == nullptr || snapshot.action_count == 0U) {
      return;
    }
  } else {
    copyText(g_last_action_step_key, sizeof(g_last_action_step_key), step_key);
    g_has_ring_sent_for_win_etape = std::strcmp(stepIdFromSnapshot(snapshot), kStepWinEtape) != 0;
    g_win_etape_ui_refresh_pending = false;
    if (snapshot.action_ids == nullptr || snapshot.action_count == 0U) {
      return;
    }
  }

  g_media.noteStepChange();
  for (uint8_t index = 0U; index < snapshot.action_count; ++index) {
    const char* action_id = snapshot.action_ids[index];
    if (action_id == nullptr || action_id[0] == '\0') {
      continue;
    }
    const bool ok = executeStoryAction(action_id, snapshot, now_ms);
    Serial.printf("[ACTION] id=%s ok=%u\n", action_id, ok ? 1U : 0U);
  }
}

bool dispatchControlActionImpl(const String& action_raw, uint32_t now_ms, String* out_error) {
  if (out_error != nullptr) {
    out_error->remove(0);
  }
  String action = action_raw;
  action.trim();
  if (action.isEmpty()) {
    if (out_error != nullptr) {
      *out_error = "empty_action";
    }
    return false;
  }

  auto parseRecorderFormat = [](const String& value,
                                ui::camera::CameraCaptureService::CaptureFormat* out_format) -> bool {
    if (out_format == nullptr) {
      return false;
    }
    String normalized = value;
    normalized.trim();
    normalized.toLowerCase();
    if (normalized.isEmpty() || normalized == "auto") {
      *out_format = ui::camera::CameraCaptureService::CaptureFormat::Auto;
      return true;
    }
    if (normalized == "bmp") {
      *out_format = ui::camera::CameraCaptureService::CaptureFormat::Bmp24;
      return true;
    }
    if (normalized == "jpg" || normalized == "jpeg") {
      *out_format = ui::camera::CameraCaptureService::CaptureFormat::Jpeg;
      return true;
    }
    if (normalized == "raw" || normalized == "rgb565") {
      *out_format = ui::camera::CameraCaptureService::CaptureFormat::RawRGB565;
      return true;
    }
    return false;
  };

  if (action.equalsIgnoreCase("UNLOCK")) {
    return dispatchScenarioEventByName("UNLOCK", now_ms);
  }
  if (action.equalsIgnoreCase("NEXT")) {
    return notifyScenarioButtonGuarded(5U, false, now_ms, "api_control");
  }
  if (action.equalsIgnoreCase("STORY_REFRESH_SD")) {
    return refreshStoryFromSd();
  }
  if (action.equalsIgnoreCase("WIFI_DISCONNECT")) {
    webScheduleStaDisconnect();
    return true;
  }
  if (action.equalsIgnoreCase("WIFI_FORGET")) {
    const bool ok = forgetWifiCredentials();
    if (!ok && out_error != nullptr) {
      *out_error = "wifi_forget_failed";
    }
    return ok;
  }
  if (action.equalsIgnoreCase("WIFI_RECONNECT")) {
    return webReconnectLocalWifi();
  }
  if (action.equalsIgnoreCase("ESPNOW_ON")) {
    return g_network.enableEspNow();
  }
  if (action.equalsIgnoreCase("ESPNOW_OFF")) {
    g_network.disableEspNow();
    return true;
  }
  if (action.equalsIgnoreCase("HW_STATUS")) {
    printHardwareStatus();
    return true;
  }
  if (action.equalsIgnoreCase("HW_STATUS_JSON")) {
    printHardwareStatusJson();
    return true;
  }
  if (action.equalsIgnoreCase("HW_MIC_STATUS")) {
    printHardwareStatus();
    return true;
  }
  if (action.equalsIgnoreCase("HW_BAT_STATUS")) {
    printHardwareStatus();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_STATUS")) {
    printCameraStatus();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_REC_STATUS")) {
    printCameraRecorderStatus();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_UI_SHOW")) {
    if (!g_camera_scene_active) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    if (!ensureCameraUiInitialized()) {
      if (out_error != nullptr) {
        *out_error = "camera_ui_not_ready";
      }
      return false;
    }
    g_camera_player.show();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_UI_HIDE")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    g_camera_player.hide();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_UI_TOGGLE")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    g_camera_player.toggle();
    return true;
  }
  if (action.equalsIgnoreCase("CAM_REC_SNAP")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    const bool was_frozen = g_camera_player.service().has_frozen();
    if (!g_camera_player.handleInputAction(ui::camera::Win311CameraUI::InputAction::kSnapToggle)) {
      if (out_error != nullptr) {
        *out_error = "camera_snap_failed";
      }
      return false;
    }
    const bool now_frozen = g_camera_player.service().has_frozen();
    if (!was_frozen && !now_frozen) {
      if (out_error != nullptr) {
        *out_error = "camera_snap_failed";
      }
      return false;
    }
    return true;
  }
  if (startsWithIgnoreCase(action.c_str(), "CAM_REC_SAVE")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    const size_t prefix_len = std::strlen("CAM_REC_SAVE");
    String format_arg = action.substring(static_cast<unsigned int>(prefix_len));
    ui::camera::CameraCaptureService::CaptureFormat format = ui::camera::CameraCaptureService::CaptureFormat::Auto;
    if (!parseRecorderFormat(format_arg, &format)) {
      if (out_error != nullptr) {
        *out_error = "cam_rec_save_arg";
      }
      return false;
    }
    if (!g_camera_player.service().has_frozen()) {
      if (out_error != nullptr) {
        *out_error = "camera_not_frozen";
      }
      return false;
    }
    String out_path;
    const bool ok = g_camera_player.service().save_frozen(out_path, format);
    if (!ok && out_error != nullptr) {
      *out_error = "camera_save_failed";
    }
    return ok;
  }
  if (action.equalsIgnoreCase("CAM_REC_GALLERY")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    return g_camera_player.handleInputAction(ui::camera::Win311CameraUI::InputAction::kGalleryToggle);
  }
  if (action.equalsIgnoreCase("CAM_REC_NEXT")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    return g_camera_player.handleInputAction(ui::camera::Win311CameraUI::InputAction::kGalleryNext);
  }
  if (action.equalsIgnoreCase("CAM_REC_DELETE")) {
    if (!g_camera_scene_active || !g_camera_scene_ready) {
      if (out_error != nullptr) {
        *out_error = "camera_scene_inactive";
      }
      return false;
    }
    return g_camera_player.handleInputAction(ui::camera::Win311CameraUI::InputAction::kDeleteSelected);
  }
  if (action.equalsIgnoreCase("RESOURCE_STATUS")) {
    printResourceStatus();
    return true;
  }
  if (action.equalsIgnoreCase("SIMD_STATUS")) {
    printSimdStatus();
    return true;
  }
  if (action.equalsIgnoreCase("SIMD_SELFTEST")) {
    return runtime::simd::runSelfTestCommand();
  }
  if (startsWithIgnoreCase(action.c_str(), "SIMD_BENCH")) {
    uint32_t loops = 200U;
    uint32_t pixels = 7680U;
    const size_t prefix_len = std::strlen("SIMD_BENCH");
    String args = action.substring(static_cast<unsigned int>(prefix_len));
    args.trim();
    if (!args.isEmpty()) {
      const int sep = args.indexOf(' ');
      String loops_text = (sep < 0) ? args : args.substring(0, static_cast<unsigned int>(sep));
      String pixels_text = (sep < 0) ? String("") : args.substring(static_cast<unsigned int>(sep + 1));
      loops_text.trim();
      pixels_text.trim();
      if (!loops_text.isEmpty()) {
        loops = static_cast<uint32_t>(std::strtoul(loops_text.c_str(), nullptr, 10));
      }
      if (!pixels_text.isEmpty()) {
        pixels = static_cast<uint32_t>(std::strtoul(pixels_text.c_str(), nullptr, 10));
      }
    }
    const runtime::simd::SimdBenchResult result = runtime::simd::runBenchCommand(loops, pixels);
    Serial.printf("SIMD_BENCH loops=%lu pixels=%lu l8_us=%lu idx_us=%lu rgb888_us=%lu gain_us=%lu\n",
                  static_cast<unsigned long>(result.loops),
                  static_cast<unsigned long>(result.pixels),
                  static_cast<unsigned long>(result.l8_to_rgb565_us),
                  static_cast<unsigned long>(result.idx8_to_rgb565_us),
                  static_cast<unsigned long>(result.rgb888_to_rgb565_us),
                  static_cast<unsigned long>(result.s16_gain_q15_us));
    return true;
  }
  if (startsWithIgnoreCase(action.c_str(), "RESOURCE_PROFILE_AUTO")) {
    const size_t prefix_len = std::strlen("RESOURCE_PROFILE_AUTO");
    String profile_auto = action.substring(static_cast<unsigned int>(prefix_len));
    profile_auto.trim();
    bool parse_ok = false;
    applyResourceProfileAutoCommand(profile_auto.c_str(), &parse_ok);
    if (!parse_ok) {
      if (out_error != nullptr) {
        *out_error = "resource_profile_auto_arg";
      }
      return false;
    }
    return true;
  }
  if (startsWithIgnoreCase(action.c_str(), "RESOURCE_PROFILE")) {
    const size_t prefix_len = std::strlen("RESOURCE_PROFILE");
    String profile = action.substring(static_cast<unsigned int>(prefix_len));
    profile.trim();
    if (profile.isEmpty()) {
      printResourceStatus();
      return true;
    }
    if (!g_resource_coordinator.parseAndSetProfile(profile.c_str())) {
      if (out_error != nullptr) {
        *out_error = "resource_profile_arg";
      }
      return false;
    }
    g_resource_profile_auto = false;
    return true;
  }
  if (action.equalsIgnoreCase("CAM_ON")) {
    if (g_camera_scene_active) {
      if (out_error != nullptr) {
        *out_error = "camera_busy_recorder_owner";
      }
      return false;
    }
    if (!approveCameraOperation("cam_on", out_error)) {
      return false;
    }
    return g_camera.start();
  }
  if (action.equalsIgnoreCase("CAM_OFF")) {
    if (g_camera_scene_active) {
      if (out_error != nullptr) {
        *out_error = "camera_busy_recorder_owner";
      }
      return false;
    }
    g_camera.stop();
    return true;
  }
  if (action.equalsIgnoreCase("MEDIA_STOP")) {
    return g_media.stop(&g_audio);
  }
  if (action.equalsIgnoreCase("REC_STOP")) {
    return g_media.stopRecording();
  }
  if (action.equalsIgnoreCase("REC_STATUS")) {
    printMediaStatus();
    return true;
  }
  if (action.equalsIgnoreCase("BOOT_MODE_STATUS")) {
    printBootModeStatus();
    return true;
  }
  if (action.equalsIgnoreCase("BOOT_MODE_CLEAR")) {
    const bool ok = g_boot_mode_store.clearMode();
    applyStartupMode(BootModeStore::StartupMode::kStory);
    if (!ok && out_error != nullptr) {
      *out_error = "boot_mode_clear_failed";
    }
    return ok;
  }
  if (startsWithIgnoreCase(action.c_str(), "BOOT_MODE_SET ")) {
    String mode_text = action.substring(static_cast<unsigned int>(std::strlen("BOOT_MODE_SET ")));
    mode_text.trim();
    mode_text.toUpperCase();
    BootModeStore::StartupMode mode = BootModeStore::StartupMode::kStory;
    if (!parseBootModeToken(mode_text.c_str(), &mode)) {
      if (out_error != nullptr) {
        *out_error = "boot_mode_set_arg";
      }
      return false;
    }
    const bool ok = g_boot_mode_store.saveMode(mode);
    if (!ok) {
      if (out_error != nullptr) {
        *out_error = "boot_mode_set_failed";
      }
      return false;
    }
    applyStartupMode(mode);
    (void)g_boot_mode_store.setMediaValidated(mode == BootModeStore::StartupMode::kMediaManager);
    return true;
  }
  if (startsWithIgnoreCase(action.c_str(), "QR_SIM ")) {
    String payload = action.substring(static_cast<unsigned int>(std::strlen("QR_SIM ")));
    payload.trim();
    const bool ok = !payload.isEmpty() && g_ui.simulateQrPayload(payload.c_str());
    if (!ok && out_error != nullptr) {
      *out_error = "qr_sim_arg";
    }
    return ok;
  }

  if (startsWithIgnoreCase(action.c_str(), "WIFI_CONNECT ")) {
    String ssid;
    String password;
    if (!splitSsidPass(action.c_str() + std::strlen("WIFI_CONNECT "), &ssid, &password)) {
      return false;
    }
    return g_network.connectSta(ssid.c_str(), password.c_str());
  }

  if (startsWithIgnoreCase(action.c_str(), "WIFI_PROVISION ")) {
    String ssid;
    String password;
    if (!splitSsidPass(action.c_str() + std::strlen("WIFI_PROVISION "), &ssid, &password)) {
      if (out_error != nullptr) {
        *out_error = "wifi_provision_args";
      }
      return false;
    }
    bool connect_started = false;
    bool persisted = false;
    const bool ok = provisionWifiCredentials(ssid.c_str(), password.c_str(), true, &connect_started, &persisted, nullptr);
    if (!ok && out_error != nullptr) {
      *out_error = persisted ? "wifi_connect_failed" : "wifi_persist_failed";
    }
    return ok;
  }

  if (startsWithIgnoreCase(action.c_str(), "ESPNOW_SEND ")) {
    String args = action.substring(static_cast<unsigned int>(std::strlen("ESPNOW_SEND ")));
    String payload;
    if (!parseEspNowSendPayload(args.c_str(), payload, nullptr)) {
      return false;
    }
    return g_network.sendEspNowTarget(kEspNowBroadcastTarget, payload.c_str());
  }

  if (startsWithIgnoreCase(action.c_str(), "SC_EVENT_RAW ")) {
    char event_name[kSerialLineCapacity] = {0};
    copyText(event_name, sizeof(event_name), action.c_str() + std::strlen("SC_EVENT_RAW "));
    trimAsciiInPlace(event_name);
    if (event_name[0] == '\0') {
      return false;
    }
    return dispatchScenarioEventByName(event_name, now_ms);
  }

  if (startsWithIgnoreCase(action.c_str(), "SC_EVENT ")) {
    char args[kSerialLineCapacity] = {0};
    copyText(args, sizeof(args), action.c_str() + std::strlen("SC_EVENT "));
    trimAsciiInPlace(args);
    if (args[0] == '\0') {
      return false;
    }
    char* type_text = args;
    char* event_name = nullptr;
    for (size_t index = 0U; args[index] != '\0'; ++index) {
      if (args[index] != ' ') {
        continue;
      }
      args[index] = '\0';
      event_name = &args[index + 1U];
      break;
    }
    if (event_name != nullptr) {
      trimAsciiInPlace(event_name);
      if (event_name[0] == '\0') {
        event_name = nullptr;
      }
    }
    StoryEventType event_type = StoryEventType::kNone;
    if (!parseEventType(type_text, &event_type)) {
      return false;
    }
    return dispatchScenarioEventByType(event_type, event_name, now_ms);
  }

  if (startsWithIgnoreCase(action.c_str(), "SCENE_GOTO ")) {
    String scene_id = action.substring(static_cast<unsigned int>(std::strlen("SCENE_GOTO ")));
    scene_id.trim();
    scene_id.toUpperCase();
    if (scene_id.isEmpty()) {
      if (out_error != nullptr) {
        *out_error = "scene_goto_arg";
      }
      return false;
    }
    if (scene_id == "SCENE_LOCK") {
      scene_id = "SCENE_LOCKED";
    } else if (scene_id == "LOCKED" || scene_id == "LOCK") {
      scene_id = "SCENE_LOCKED";
    }
    const bool ok = g_scenario.gotoScene(scene_id.c_str(), now_ms, "scene_goto_control");
    if (!ok) {
      if (out_error != nullptr) {
        *out_error = "scene_not_found";
      }
      return false;
    }
    g_last_action_step_key[0] = '\0';
    refreshSceneIfNeeded(true);
    startPendingAudioIfAny();
    return true;
  }
  if (action.equalsIgnoreCase("SCENE_GOTO")) {
    if (out_error != nullptr) {
      *out_error = "scene_goto_arg";
    }
    return false;
  }

  if (startsWithIgnoreCase(action.c_str(), "HW_LED_SET ")) {
    String args = action.substring(static_cast<unsigned int>(std::strlen("HW_LED_SET ")));
    args.trim();
    uint8_t red = 0U;
    uint8_t green = 0U;
    uint8_t blue = 0U;
    uint8_t brightness = static_cast<uint8_t>(FREENOVE_WS2812_BRIGHTNESS);
    bool pulse = true;
    if (!parseHwLedSetArgs(args.c_str(), &red, &green, &blue, &brightness, &pulse)) {
      if (out_error != nullptr) {
        *out_error = "hw_led_set_args";
      }
      return false;
    }
    return g_hardware.setManualLed(red, green, blue, brightness, pulse);
  }

  if (startsWithIgnoreCase(action.c_str(), "HW_LED_AUTO ")) {
    String value = action.substring(static_cast<unsigned int>(std::strlen("HW_LED_AUTO ")));
    value.trim();
    bool enabled = false;
    if (!parseBoolToken(value.c_str(), &enabled)) {
      if (out_error != nullptr) {
        *out_error = "hw_led_auto_args";
      }
      return false;
    }
    g_hardware_cfg.led_auto_from_scene = enabled;
    if (enabled) {
      g_hardware.clearManualLed();
      const ScenarioSnapshot snapshot = g_scenario.snapshot();
      if (snapshot.screen_scene_id != nullptr) {
        g_hardware.setSceneHint(snapshot.screen_scene_id);
      }
    }
    return true;
  }

  if (startsWithIgnoreCase(action.c_str(), "CAM_SNAPSHOT")) {
    if (g_camera_scene_active) {
      if (out_error != nullptr) {
        *out_error = "camera_busy_recorder_owner";
      }
      return false;
    }
    if (!approveCameraOperation("cam_snapshot", out_error)) {
      return false;
    }
    const size_t prefix_len = std::strlen("CAM_SNAPSHOT");
    String filename = action.substring(static_cast<unsigned int>(prefix_len));
    filename.trim();
    String out_path;
    const bool ok = g_camera.snapshotToFile(filename.isEmpty() ? nullptr : filename.c_str(), &out_path);
    if (ok) {
      dispatchScenarioEventByName("SERIAL:CAMERA_CAPTURED", now_ms);
    } else if (out_error != nullptr) {
      *out_error = "camera_snapshot_failed";
    }
    return ok;
  }

  if (startsWithIgnoreCase(action.c_str(), "MEDIA_PLAY ")) {
    String media_path = action.substring(static_cast<unsigned int>(std::strlen("MEDIA_PLAY ")));
    media_path.trim();
    const bool ok = !media_path.isEmpty() && g_media.play(media_path.c_str(), &g_audio);
    if (!ok && out_error != nullptr) {
      *out_error = "media_play_failed";
    }
    return ok;
  }

  if (startsWithIgnoreCase(action.c_str(), "REC_START")) {
    const size_t prefix_len = std::strlen("REC_START");
    String args = action.substring(static_cast<unsigned int>(prefix_len));
    args.trim();
    uint16_t seconds = g_media_cfg.record_max_seconds;
    String filename;
    if (!args.isEmpty()) {
      const int sep = args.indexOf(' ');
      String seconds_text = (sep < 0) ? args : args.substring(0, static_cast<unsigned int>(sep));
      filename = (sep < 0) ? String("") : args.substring(static_cast<unsigned int>(sep + 1));
      seconds_text.trim();
      filename.trim();
      if (!seconds_text.isEmpty()) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(seconds_text.c_str(), &end, 10);
        if (end != seconds_text.c_str() && (end == nullptr || *end == '\0')) {
          seconds = static_cast<uint16_t>(parsed);
        }
      }
    }
    return g_media.startRecording(seconds, filename.isEmpty() ? nullptr : filename.c_str());
  }

  if (startsWithIgnoreCase(action.c_str(), "MEDIA_LIST ")) {
    String kind = action.substring(static_cast<unsigned int>(std::strlen("MEDIA_LIST ")));
    kind.trim();
    if (kind.isEmpty()) {
      kind = "music";
    }
    String files_json;
    const bool ok = g_media.listFiles(kind.c_str(), &files_json);
    if (ok) {
      Serial.printf("MEDIA_LIST kind=%s files=%s\n", kind.c_str(), files_json.c_str());
    }
    return ok;
  }

  if (out_error != nullptr) {
    *out_error = "unsupported_action";
  }
  return false;
}

bool webDispatchAction(const String& action_raw) {
  return dispatchControlAction(action_raw, millis(), nullptr);
}

void webBuildStatusDocument(StaticJsonDocument<4096>* out_document) {
  if (out_document == nullptr) {
    return;
  }
  const NetworkManager::Snapshot net = g_network.snapshot();
  const ScenarioSnapshot scenario = g_scenario.snapshot();

  out_document->clear();
  JsonObject network = (*out_document)["network"].to<JsonObject>();
  network["state"] = String(net.state);
  network["mode"] = String(net.mode);
  network["sta_connected"] = net.sta_connected;
  network["sta_connecting"] = net.sta_connecting;
  network["fallback_ap"] = net.fallback_ap_active;
  network["sta_ssid"] = String(net.sta_ssid);
  network["ap_ssid"] = String(net.ap_ssid);
  network["local_target"] = String(net.local_target);
  network["local_match"] = net.local_match;
  network["ap_clients"] = net.ap_clients;
  network["local_retry_paused"] = net.local_retry_paused;
  network["ip"] = String(net.ip);
  network["rssi"] = net.rssi;

  JsonObject wifi = (*out_document)["wifi"].to<JsonObject>();
  webFillWifiStatus(wifi, net);

  JsonObject espnow = (*out_document)["espnow"].to<JsonObject>();
  webFillEspNowStatus(espnow, net);

  JsonObject story = (*out_document)["story"].to<JsonObject>();
  story["scenario"] = scenarioIdFromSnapshot(scenario);
  story["step"] = stepIdFromSnapshot(scenario);
  story["screen"] = (scenario.screen_scene_id != nullptr) ? scenario.screen_scene_id : "";
  story["audio_pack"] = (scenario.audio_pack_id != nullptr) ? scenario.audio_pack_id : "";

  JsonObject audio = (*out_document)["audio"].to<JsonObject>();
  audio["playing"] = g_audio.isPlaying();
  audio["track"] = g_audio.currentTrack();
  audio["codec"] = g_audio.activeCodec();
  audio["bitrate_kbps"] = g_audio.activeBitrateKbps();
  audio["fx"] = g_audio.fxProfile();
  audio["fx_label"] = g_audio.fxProfileLabel(g_audio.fxProfile());
  audio["profile"] = g_audio.outputProfile();
  audio["volume"] = g_audio.volume();

  JsonObject hardware = (*out_document)["hardware"].to<JsonObject>();
  webFillHardwareStatus(hardware);

  JsonObject camera = (*out_document)["camera"].to<JsonObject>();
  webFillCameraStatus(camera);

  JsonObject media = (*out_document)["media"].to<JsonObject>();
  webFillMediaStatus(media, millis());

  const runtime::resource::ResourceCoordinatorSnapshot resource_snapshot = g_resource_coordinator.snapshot();
  const UiMemorySnapshot ui_snapshot = g_ui.memorySnapshot();
  JsonObject resource = (*out_document)["resource"].to<JsonObject>();
  resource["profile"] = g_resource_coordinator.profileName();
  resource["profile_auto"] = g_resource_profile_auto;
  resource["graphics_pressure"] = resource_snapshot.graphics_pressure;
  resource["mic_should_run"] = resource_snapshot.mic_should_run;
  resource["mic_force_on"] = resource_snapshot.mic_force_on;
  resource["allow_camera_ops"] = resource_snapshot.allow_camera_ops;
  resource["mic_hold_until_ms"] = resource_snapshot.mic_hold_until_ms;
  resource["camera_allowed_ops"] = resource_snapshot.camera_allowed_ops;
  resource["camera_blocked_ops"] = resource_snapshot.camera_blocked_ops;
  resource["flush_overflow_delta"] = resource_snapshot.flush_overflow_delta;
  resource["flush_blocked_delta"] = resource_snapshot.flush_blocked_delta;
  resource["fx_fps"] = ui_snapshot.fx_fps;
  resource["flush_blocked"] = ui_snapshot.flush_blocked;
  resource["flush_overflow"] = ui_snapshot.flush_overflow;
  resource["flush_stall"] = ui_snapshot.flush_stall;
  resource["flush_recover"] = ui_snapshot.flush_recover;
}

void webSendStatus() {
  StaticJsonDocument<4096> document;
  webBuildStatusDocument(&document);
  webSendJsonDocument(document);
}

void webSendStatusSse() {
  StaticJsonDocument<4096> document;
  webBuildStatusDocument(&document);
  char payload[4608] = {0};
  const size_t measured_size = measureJson(document);
  if (measured_size >= sizeof(payload)) {
    g_web_server.send(500, "application/json", "{\"ok\":false,\"error\":\"status_payload_too_large\"}");
    return;
  }
  const size_t payload_size = serializeJson(document, payload, sizeof(payload));
  if (payload_size == 0U) {
    g_web_server.send(500, "application/json", "{\"ok\":false,\"error\":\"status_serialize_failed\"}");
    return;
  }

  g_web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_web_server.sendHeader("Cache-Control", "no-cache");
  g_web_server.sendHeader("Connection", "close");
  g_web_server.send(200, "text/event-stream", "");
  g_web_server.sendContent("event: status\n");
  g_web_server.sendContent("data: ");
  g_web_server.sendContent(payload, payload_size);
  g_web_server.sendContent("\n\n");
  g_web_server.sendContent("event: done\ndata: 1\n\n");
}

void setupWebUiImpl() {
  const char* auth_headers[] = {kWebAuthHeaderName};
  g_web_server.collectHeaders(auth_headers, 1);

  g_web_server.on("/", HTTP_GET, []() {
    g_web_server.send(200, "text/html", kWebUiIndex);
  });

  webOnApi(kProvisionStatusPath, HTTP_GET, []() {
    webSendProvisionStatus();
  });

  webOnApi("/api/auth/status", HTTP_GET, []() {
    webSendAuthStatus();
  });

  webOnApi("/api/status", HTTP_GET, []() {
    webSendStatus();
  });

  webOnApi("/api/stream", HTTP_GET, []() {
    webSendStatusSse();
  });

  webOnApi("/api/hardware", HTTP_GET, []() {
    webSendHardwareStatus();
  });

  webOnApi("/api/hardware/led", HTTP_POST, []() {
    int red = g_web_server.arg("r").toInt();
    int green = g_web_server.arg("g").toInt();
    int blue = g_web_server.arg("b").toInt();
    int brightness = g_web_server.hasArg("brightness") ? g_web_server.arg("brightness").toInt() : FREENOVE_WS2812_BRIGHTNESS;
    bool pulse = true;
    if (g_web_server.hasArg("pulse")) {
      pulse = (g_web_server.arg("pulse").toInt() != 0);
    }
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json)) {
      if (request_json["r"].is<int>()) {
        red = request_json["r"].as<int>();
      }
      if (request_json["g"].is<int>()) {
        green = request_json["g"].as<int>();
      }
      if (request_json["b"].is<int>()) {
        blue = request_json["b"].as<int>();
      }
      if (request_json["brightness"].is<int>()) {
        brightness = request_json["brightness"].as<int>();
      }
      if (request_json["pulse"].is<bool>()) {
        pulse = request_json["pulse"].as<bool>();
      }
    }
    if (brightness < 0) {
      brightness = 0;
    } else if (brightness > 255) {
      brightness = 255;
    }
    const bool ok = g_hardware.setManualLed(static_cast<uint8_t>(red),
                                            static_cast<uint8_t>(green),
                                            static_cast<uint8_t>(blue),
                                            static_cast<uint8_t>(brightness),
                                            pulse);
    webSendResult("HW_LED_SET", ok);
  });

  webOnApi("/api/hardware/led/auto", HTTP_POST, []() {
    bool enabled = false;
    bool parsed = false;
    if (g_web_server.hasArg("enabled")) {
      parsed = parseBoolToken(g_web_server.arg("enabled").c_str(), &enabled);
    } else if (g_web_server.hasArg("value")) {
      parsed = parseBoolToken(g_web_server.arg("value").c_str(), &enabled);
    }
    StaticJsonDocument<128> request_json;
    if (!parsed && webParseJsonBody(&request_json)) {
      if (request_json["enabled"].is<bool>()) {
        enabled = request_json["enabled"].as<bool>();
        parsed = true;
      } else if (request_json["value"].is<bool>()) {
        enabled = request_json["value"].as<bool>();
        parsed = true;
      }
    }
    if (!parsed) {
      webSendResult("HW_LED_AUTO", false);
      return;
    }
    g_hardware_cfg.led_auto_from_scene = enabled;
    if (enabled) {
      g_hardware.clearManualLed();
      const ScenarioSnapshot snapshot = g_scenario.snapshot();
      if (snapshot.screen_scene_id != nullptr) {
        g_hardware.setSceneHint(snapshot.screen_scene_id);
      }
    }
    webSendResult("HW_LED_AUTO", true);
  });

  webOnApi("/api/camera/status", HTTP_GET, []() {
    webSendCameraStatus();
  });

  webOnApi("/api/camera/on", HTTP_POST, []() {
    if (g_camera_scene_active) {
      g_web_server.send(409, "application/json", "{\"ok\":false,\"error\":\"camera_busy_recorder_owner\"}");
      return;
    }
    String err;
    if (!approveCameraOperation("web_cam_on", &err)) {
      g_web_server.send(429, "application/json", "{\"ok\":false,\"error\":\"camera_blocked_by_resource_profile\"}");
      return;
    }
    const bool ok = g_camera.start();
    webSendResult("CAM_ON", ok);
  });

  webOnApi("/api/camera/off", HTTP_POST, []() {
    if (g_camera_scene_active) {
      g_web_server.send(409, "application/json", "{\"ok\":false,\"error\":\"camera_busy_recorder_owner\"}");
      return;
    }
    g_camera.stop();
    webSendResult("CAM_OFF", true);
  });

  webOnApi("/api/camera/snapshot.jpg", HTTP_GET, []() {
    if (g_camera_scene_active) {
      g_web_server.send(409, "application/json", "{\"ok\":false,\"error\":\"camera_busy_recorder_owner\"}");
      return;
    }
    String err;
    if (!approveCameraOperation("web_cam_snapshot", &err)) {
      g_web_server.send(429, "application/json", "{\"ok\":false,\"error\":\"camera_blocked_by_resource_profile\"}");
      return;
    }
    String out_path;
    if (!g_camera.snapshotToFile(nullptr, &out_path)) {
      g_web_server.send(500, "application/json", "{\"ok\":false,\"error\":\"camera_snapshot_failed\"}");
      return;
    }
    File image = LittleFS.open(out_path.c_str(), "r");
    if (!image) {
      g_web_server.send(500, "application/json", "{\"ok\":false,\"error\":\"camera_snapshot_missing\"}");
      return;
    }
    g_web_server.streamFile(image, "image/jpeg");
    image.close();
    dispatchScenarioEventByName("SERIAL:CAMERA_CAPTURED", millis());
  });

  webOnApi("/api/media/files", HTTP_GET, []() {
    webSendMediaFiles();
  });

  webOnApi("/api/media/play", HTTP_POST, []() {
    String path = g_web_server.arg("path");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && path.isEmpty()) {
      path = request_json["path"] | request_json["file"] | "";
    }
    const bool ok = !path.isEmpty() && g_media.play(path.c_str(), &g_audio);
    webSendResult("MEDIA_PLAY", ok);
  });

  webOnApi("/api/media/stop", HTTP_POST, []() {
    const bool ok = g_media.stop(&g_audio);
    webSendResult("MEDIA_STOP", ok);
  });

  webOnApi("/api/media/record/start", HTTP_POST, []() {
    uint16_t seconds = static_cast<uint16_t>(g_web_server.arg("seconds").toInt());
    String filename = g_web_server.arg("filename");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json)) {
      if (request_json["seconds"].is<unsigned int>()) {
        seconds = static_cast<uint16_t>(request_json["seconds"].as<unsigned int>());
      }
      if (filename.isEmpty()) {
        filename = request_json["filename"] | "";
      }
    }
    const bool ok = g_media.startRecording(seconds, filename.isEmpty() ? nullptr : filename.c_str());
    webSendResult("REC_START", ok);
  });

  webOnApi("/api/media/record/stop", HTTP_POST, []() {
    const bool ok = g_media.stopRecording();
    webSendResult("REC_STOP", ok);
  });

  webOnApi("/api/media/record/status", HTTP_GET, []() {
    webSendMediaRecordStatus();
  });

  webOnApi("/api/network/wifi", HTTP_GET, []() {
    webSendWifiStatus();
  });

  webOnApi("/api/network/espnow", HTTP_GET, []() {
    webSendEspNowStatus();
  });

  webOnApi("/api/network/espnow/peer", HTTP_GET, []() {
    webSendEspNowPeerList();
  });

  webOnApi("/api/wifi/disconnect", HTTP_POST, []() {
    webScheduleStaDisconnect();
    webSendResult("WIFI_DISCONNECT", true);
  });

  webOnApi("/api/network/wifi/disconnect", HTTP_POST, []() {
    webScheduleStaDisconnect();
    webSendResult("WIFI_DISCONNECT", true);
  });

  webOnApi("/api/network/wifi/reconnect", HTTP_POST, []() {
    const bool ok = webReconnectLocalWifi();
    webSendResult("WIFI_RECONNECT", ok);
  });

  webOnApi("/api/wifi/connect", HTTP_POST, []() {
    String ssid = g_web_server.arg("ssid");
    String password = g_web_server.arg("password");
    bool persist = false;
    if (password.isEmpty()) {
      password = g_web_server.arg("pass");
    }
    StaticJsonDocument<768> request_json;
    if (g_web_server.hasArg("persist")) {
      bool parsed_persist = false;
      if (parseBoolToken(g_web_server.arg("persist").c_str(), &parsed_persist)) {
        persist = parsed_persist;
      } else {
        long persist_flag = 0L;
        if (parseBoundedLongToken(g_web_server.arg("persist").c_str(), 0L, 1L, &persist_flag)) {
          persist = (persist_flag != 0L);
        }
      }
    }
    if (webParseJsonBody(&request_json)) {
      if (ssid.isEmpty()) {
        ssid = request_json["ssid"] | "";
      }
      if (password.isEmpty()) {
        password = request_json["pass"] | request_json["password"] | "";
      }
      if (request_json["persist"].is<bool>()) {
        persist = request_json["persist"].as<bool>();
      } else if (request_json["persist"].is<int>()) {
        persist = request_json["persist"].as<int>() != 0;
      } else if (request_json["persist"].is<unsigned int>()) {
        persist = request_json["persist"].as<unsigned int>() != 0U;
      }
    }
    if (ssid.isEmpty()) {
      webSendResult("WIFI_CONNECT", false);
      return;
    }
    bool connect_started = false;
    bool persisted = false;
    bool token_generated = false;
    const bool ok = provisionWifiCredentials(ssid.c_str(),
                                             password.c_str(),
                                             persist,
                                             &connect_started,
                                             &persisted,
                                             &token_generated);
    StaticJsonDocument<320> response;
    response["ok"] = ok;
    response["action"] = "WIFI_CONNECT";
    response["persist"] = persist;
    response["connect_started"] = connect_started;
    if (persist) {
      response["persisted"] = persisted;
      if (token_generated && g_web_auth_token[0] != '\0') {
        response["token"] = g_web_auth_token;
      }
    }
    webSendJsonDocument(response, ok ? 200 : 400);
  });

  webOnApi("/api/network/wifi/connect", HTTP_POST, []() {
    String ssid = g_web_server.arg("ssid");
    String password = g_web_server.arg("password");
    bool persist = false;
    if (password.isEmpty()) {
      password = g_web_server.arg("pass");
    }
    StaticJsonDocument<768> request_json;
    if (g_web_server.hasArg("persist")) {
      bool parsed_persist = false;
      if (parseBoolToken(g_web_server.arg("persist").c_str(), &parsed_persist)) {
        persist = parsed_persist;
      } else {
        long persist_flag = 0L;
        if (parseBoundedLongToken(g_web_server.arg("persist").c_str(), 0L, 1L, &persist_flag)) {
          persist = (persist_flag != 0L);
        }
      }
    }
    if (webParseJsonBody(&request_json)) {
      if (ssid.isEmpty()) {
        ssid = request_json["ssid"] | "";
      }
      if (password.isEmpty()) {
        password = request_json["pass"] | request_json["password"] | "";
      }
      if (request_json["persist"].is<bool>()) {
        persist = request_json["persist"].as<bool>();
      } else if (request_json["persist"].is<int>()) {
        persist = request_json["persist"].as<int>() != 0;
      } else if (request_json["persist"].is<unsigned int>()) {
        persist = request_json["persist"].as<unsigned int>() != 0U;
      }
    }
    if (ssid.isEmpty()) {
      webSendResult("WIFI_CONNECT", false);
      return;
    }
    bool connect_started = false;
    bool persisted = false;
    bool token_generated = false;
    const bool ok = provisionWifiCredentials(ssid.c_str(),
                                             password.c_str(),
                                             persist,
                                             &connect_started,
                                             &persisted,
                                             &token_generated);
    StaticJsonDocument<320> response;
    response["ok"] = ok;
    response["action"] = "WIFI_CONNECT";
    response["persist"] = persist;
    response["connect_started"] = connect_started;
    if (persist) {
      response["persisted"] = persisted;
      if (token_generated && g_web_auth_token[0] != '\0') {
        response["token"] = g_web_auth_token;
      }
    }
    webSendJsonDocument(response, ok ? 200 : 400);
  });

  webOnApi("/api/espnow/send", HTTP_POST, []() {
    String payload = g_web_server.arg("payload");
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (payload.isEmpty()) {
        if (request_json["payload"].is<JsonVariantConst>()) {
          serializeJson(request_json["payload"], payload);
        } else {
          payload = request_json["payload"] | "";
        }
      }
    }
    if (payload.isEmpty()) {
      webSendResult("ESPNOW_SEND", false);
      return;
    }
    const bool ok = g_network.sendEspNowTarget(kEspNowBroadcastTarget, payload.c_str());
    webSendResult("ESPNOW_SEND", ok);
  });

  webOnApi("/api/network/espnow/send", HTTP_POST, []() {
    String payload = g_web_server.arg("payload");
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (payload.isEmpty()) {
        if (request_json["payload"].is<JsonVariantConst>()) {
          serializeJson(request_json["payload"], payload);
        } else {
          payload = request_json["payload"] | "";
        }
      }
    }
    if (payload.isEmpty()) {
      webSendResult("ESPNOW_SEND", false);
      return;
    }
    const bool ok = g_network.sendEspNowTarget(kEspNowBroadcastTarget, payload.c_str());
    webSendResult("ESPNOW_SEND", ok);
  });

  webOnApi("/api/network/espnow/on", HTTP_POST, []() {
    const bool ok = g_network.enableEspNow();
    webSendResult("ESPNOW_ON", ok);
  });

  webOnApi("/api/network/espnow/off", HTTP_POST, []() {
    g_network.disableEspNow();
    webSendResult("ESPNOW_OFF", true);
  });

  webOnApi("/api/network/espnow/peer", HTTP_POST, []() {
    String mac = g_web_server.arg("mac");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && mac.isEmpty()) {
      mac = request_json["mac"] | "";
    }
    const bool ok = !mac.isEmpty() && g_network.addEspNowPeer(mac.c_str());
    webSendResult("ESPNOW_PEER_ADD", ok);
  });

  webOnApi("/api/network/espnow/peer", HTTP_DELETE, []() {
    String mac = g_web_server.arg("mac");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && mac.isEmpty()) {
      mac = request_json["mac"] | "";
    }
    const bool ok = !mac.isEmpty() && g_network.removeEspNowPeer(mac.c_str());
    webSendResult("ESPNOW_PEER_DEL", ok);
  });

  webOnApi("/api/story/refresh-sd", HTTP_POST, []() {
    const bool ok = refreshStoryFromSd();
    webSendResult("STORY_REFRESH_SD", ok);
  });

  webOnApi("/api/scenario/unlock", HTTP_POST, []() {
    const bool ok = dispatchScenarioEventByName("UNLOCK", millis());
    webSendResult("UNLOCK", ok);
  });

  webOnApi("/api/scenario/next", HTTP_POST, []() {
    bool ok = dispatchScenarioEventByName("SERIAL:BTN_NEXT", millis());
    if (!ok) {
      ok = notifyScenarioButtonGuarded(5U, false, millis(), "api_scenario_next");
    }
    webSendResult("NEXT", ok);
  });

  webOnApi("/api/control", HTTP_POST, []() {
    String action = g_web_server.arg("action");
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json) && action.isEmpty()) {
      action = request_json["action"] | "";
    }
    String error;
    const bool ok = dispatchControlAction(action, millis(), &error);
    StaticJsonDocument<256> response;
    response["ok"] = ok;
    response["action"] = action;
    if (!ok && !error.isEmpty()) {
      response["error"] = error;
    }
    webSendJsonDocument(response, ok ? 200 : 400);
  });

  g_web_server.onNotFound([]() {
    g_web_server.send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });

  g_web_server.begin();
  g_web_started = true;
  Serial.println("[WEB] started :80");
}

void printScenarioCoverage() {
  const uint32_t mask = g_scenario.transitionEventMask();
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const uint8_t unlock = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kUnlock))) ? 1U : 0U;
  const uint8_t audio_done = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kAudioDone))) ? 1U : 0U;
  const uint8_t timer = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kTimer))) ? 1U : 0U;
  const uint8_t serial = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kSerial))) ? 1U : 0U;
  const uint8_t button = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kButton))) ? 1U : 0U;
  const uint8_t espnow = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kEspNow))) ? 1U : 0U;
  const uint8_t action = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kAction))) ? 1U : 0U;
  Serial.printf("SC_COVERAGE scenario=%s unlock=%u audio_done=%u timer=%u serial=%u button=%u espnow=%u action=%u\n",
                scenarioIdFromSnapshot(snapshot),
                unlock,
                audio_done,
                timer,
                serial,
                button,
                espnow,
                action);
}

bool dispatchScenarioEventByType(StoryEventType type, const char* event_name, uint32_t now_ms) {
  switch (type) {
    case StoryEventType::kUnlock: {
      char unlock_name[64] = {0};
      if (event_name != nullptr && event_name[0] != '\0') {
        copyText(unlock_name, sizeof(unlock_name), event_name);
        trimAsciiInPlace(unlock_name);
        toUpperAsciiInPlace(unlock_name);
      }
      const char* selected_name = (unlock_name[0] != '\0') ? unlock_name : "UNLOCK";
      const bool dispatched_named = g_scenario.notifyUnlockEvent(selected_name, now_ms);
      if (std::strcmp(selected_name, "UNLOCK") == 0) {
        // Preserve legacy compatibility: unlock command is considered accepted
        // even when no transition consumes it in the current step.
        return true;
      }
      if (dispatched_named) {
        return true;
      }
      return g_scenario.notifyUnlockEvent("UNLOCK", now_ms);
    }
    case StoryEventType::kAudioDone:
      if (event_name != nullptr && event_name[0] != '\0' && std::strcmp(event_name, "AUDIO_DONE") != 0) {
        return false;
      }
      g_scenario.notifyAudioDone(now_ms);
      return true;
    case StoryEventType::kTimer:
      return g_scenario.notifyTimerEvent(event_name, now_ms);
    case StoryEventType::kSerial:
      return g_scenario.notifySerialEvent(event_name, now_ms);
    case StoryEventType::kButton:
      return g_scenario.notifyButtonEvent(event_name, now_ms);
    case StoryEventType::kEspNow: {
      const bool dispatched_espnow = g_scenario.notifyEspNowEvent(event_name, now_ms);
      const bool dispatched_serial = g_scenario.notifySerialEvent(event_name, now_ms);
      return dispatched_espnow || dispatched_serial;
    }
    case StoryEventType::kAction:
      return g_scenario.notifyActionEvent(event_name, now_ms);
    default:
      return false;
  }
}

bool dispatchScenarioEventByName(const char* event_name, uint32_t now_ms) {
  if (event_name == nullptr || event_name[0] == '\0') {
    return false;
  }

  char normalized[kSerialLineCapacity] = {0};
  std::strncpy(normalized, event_name, sizeof(normalized) - 1U);
  toUpperAsciiInPlace(normalized);

  const ScenarioSnapshot current = g_scenario.snapshot();
  if (!g_la_dispatch_in_progress && shouldEnforceLaMatchOnly(current)) {
    if (std::strcmp(normalized, "UNLOCK") == 0 || std::strcmp(normalized, "BTN_NEXT") == 0 ||
        std::strcmp(normalized, "SERIAL:BTN_NEXT") == 0) {
      Serial.printf("[LA_TRIGGER] blocked manual event=%s while waiting LA match\n", normalized);
      return false;
    }
  }

  if (std::strcmp(normalized, "UNLOCK") == 0) {
    g_scenario.notifyUnlock(now_ms);
    return true;
  }
  if (std::strcmp(normalized, "AUDIO_DONE") == 0) {
    g_scenario.notifyAudioDone(now_ms);
    return true;
  }

  const char* separator = std::strchr(normalized, ':');
  if (separator != nullptr) {
    const size_t head_len = static_cast<size_t>(separator - normalized);
    const char* tail = separator + 1;
    if (tail[0] == '\0') {
      return false;
    }
    if (head_len == 5U && std::strncmp(normalized, "TIMER", 5U) == 0) {
      return g_scenario.notifyTimerEvent(tail, now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "UNLOCK", 6U) == 0) {
      const bool dispatched_named = g_scenario.notifyUnlockEvent(tail, now_ms);
      if (dispatched_named) {
        return true;
      }
      return g_scenario.notifyUnlockEvent("UNLOCK", now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "ACTION", 6U) == 0) {
      return g_scenario.notifyActionEvent(tail, now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "SERIAL", 6U) == 0) {
      return g_scenario.notifySerialEvent(tail, now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "BUTTON", 6U) == 0) {
      return g_scenario.notifyButtonEvent(tail, now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "ESPNOW", 6U) == 0) {
      const bool dispatched_espnow = g_scenario.notifyEspNowEvent(tail, now_ms);
      const bool dispatched_serial = g_scenario.notifySerialEvent(tail, now_ms);
      return dispatched_espnow || dispatched_serial;
    }
  }

  return g_scenario.notifySerialEvent(normalized, now_ms);
}

void runScenarioRevalidate(uint32_t now_ms) {
  struct EventProbe {
    StoryEventType type;
    const char* event_name;
  };
  struct HardwareProbe {
    uint8_t key;
    bool long_press;
    const char* label;
  };

  const EventProbe event_probes[] = {
      {StoryEventType::kUnlock, "UNLOCK"},
      {StoryEventType::kAudioDone, "AUDIO_DONE"},
      {StoryEventType::kTimer, "ETAPE2_DUE"},
      {StoryEventType::kSerial, "FORCE_DONE"},
      {StoryEventType::kButton, "ANY"},
      {StoryEventType::kEspNow, "ACK_WIN1"},
      {StoryEventType::kAction, "ACTION_FORCE_ETAPE2"},
  };
  const HardwareProbe hardware_probes[] = {
      {1U, false, "BTN1_SHORT"},
      {5U, false, "BTN5_SHORT"},
  };

  g_scenario.reset();
  Serial.println("SC_REVALIDATE_BEGIN");
  printScenarioCoverage();
  for (const EventProbe& probe : event_probes) {
    const ScenarioSnapshot before = g_scenario.snapshot();
    const bool dispatched = dispatchScenarioEventByType(probe.type, probe.event_name, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("SC_REVALIDATE event=%s name=%s dispatched=%u changed=%u step_before=%s step_after=%s screen=%s pack=%s\n",
                  eventTypeName(probe.type),
                  probe.event_name,
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(before),
                  stepIdFromSnapshot(after),
                  after.screen_scene_id != nullptr ? after.screen_scene_id : "n/a",
                  after.audio_pack_id != nullptr ? after.audio_pack_id : "n/a");
  }
  for (const HardwareProbe& probe : hardware_probes) {
    g_scenario.reset();
    const ScenarioSnapshot before = g_scenario.snapshot();
    g_scenario.notifyButton(probe.key, probe.long_press, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("SC_REVALIDATE_HW key=%u long=%u label=%s changed=%u step_before=%s step_after=%s screen=%s pack=%s\n",
                  probe.key,
                  probe.long_press ? 1U : 0U,
                  probe.label,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(before),
                  stepIdFromSnapshot(after),
                  after.screen_scene_id != nullptr ? after.screen_scene_id : "n/a",
                  after.audio_pack_id != nullptr ? after.audio_pack_id : "n/a");
  }

  auto prepareStepXProbe = [&]() {
    g_scenario.reset();
    g_scenario.notifyUnlock(now_ms);
    g_scenario.notifyAudioDone(now_ms);
    return g_scenario.snapshot();
  };

  {
    const ScenarioSnapshot before = prepareStepXProbe();
    const bool dispatched = g_scenario.notifyTimerEvent("ETAPE2_DUE", now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("SC_REVALIDATE_STEPX event=timer name=ETAPE2_DUE dispatched=%u changed=%u anchor_step=%s step_after=%s\n",
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(before),
                  stepIdFromSnapshot(after));
  }

  {
    const ScenarioSnapshot before = prepareStepXProbe();
    const bool dispatched = g_scenario.notifyActionEvent("ACTION_FORCE_ETAPE2", now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf(
        "SC_REVALIDATE_STEPX event=action name=ACTION_FORCE_ETAPE2 dispatched=%u changed=%u anchor_step=%s step_after=%s\n",
        dispatched ? 1U : 0U,
        changed ? 1U : 0U,
        stepIdFromSnapshot(before),
        stepIdFromSnapshot(after));
  }

  Serial.println("SC_REVALIDATE_END");
}

void runScenarioRevalidateAll(uint32_t now_ms) {
  const char* previous_scenario = scenarioIdFromSnapshot(g_scenario.snapshot());
  Serial.println("SC_REVALIDATE_ALL_BEGIN");
  for (uint8_t index = 0U; index < storyScenarioV2Count(); ++index) {
    const char* scenario_id = storyScenarioV2IdAt(index);
    if (scenario_id == nullptr || scenario_id[0] == '\0') {
      continue;
    }
    if (!g_scenario.beginById(scenario_id)) {
      Serial.printf("SC_REVALIDATE_ALL_SKIP id=%s reason=load_failed\n", scenario_id);
      continue;
    }
    Serial.printf("SC_REVALIDATE_ALL_SCENARIO id=%s\n", scenario_id);
    runScenarioRevalidate(now_ms);
  }
  if (previous_scenario != nullptr && previous_scenario[0] != '\0') {
    g_scenario.beginById(previous_scenario);
  } else {
    g_scenario.begin(kDefaultScenarioFile);
  }
  Serial.println("SC_REVALIDATE_ALL_END");
}

bool dispatchControlAction(const String& action_raw, uint32_t now_ms, String* out_error) {
  return g_runtime_serial_service.dispatchControlAction(action_raw, now_ms, out_error);
}

void setupWebUi() {
  g_runtime_web_service.setupWebUi();
}

void refreshSceneIfNeeded(bool force_render) {
  g_runtime_scene_service.refreshSceneIfNeeded(force_render);
}

void handleSerialCommand(const char* command_line, uint32_t now_ms) {
  g_runtime_serial_service.handleSerialCommand(command_line, now_ms);
}

void runtimeTickBridge(uint32_t now_ms, RuntimeServices* services) {
  (void)services;
  ::runRuntimeIteration(now_ms);
}

void serialDispatchBridge(const char* command_line, uint32_t now_ms, RuntimeServices* services) {
  (void)services;
  handleSerialCommand(command_line, now_ms);
}

void refreshSceneIfNeededImpl(bool force_render) {
  const bool changed = g_scenario.consumeSceneChanged();
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const SceneTransitionPlan transition =
      g_scene_fx_orchestrator.planTransition(snapshot.screen_scene_id, changed, force_render);
  if (!transition.should_apply) {
    return;
  }

  const uint32_t now_ms = millis();

  // Explicit transition ordering: pre-exit -> release old owner resources.
  if (transition.owner_changed) {
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
    if (transition.from_owner == SceneRuntimeOwner::kAmp) {
      setAmpSceneActive(false);
    }
#endif
    if (transition.from_owner == SceneRuntimeOwner::kCamera) {
      setCameraSceneActive(false);
    }
  }

  if (g_hardware_started && g_hardware_cfg.led_auto_from_scene && snapshot.screen_scene_id != nullptr) {
    g_hardware.setSceneHint(snapshot.screen_scene_id);
  }
  executeStoryActionsForStep(snapshot, now_ms);

  const char* step_id = (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
  const String screen_payload = g_storage.loadScenePayloadById(snapshot.screen_scene_id);
  if ((snapshot.screen_scene_id != nullptr) && snapshot.screen_scene_id[0] != '\0' && screen_payload.isEmpty()) {
    ZACUS_RL_LOG_MS(6000U,
                    "[UI] missing scene payload scenario=%s step=%s screen=%s\n",
                    scenarioIdFromSnapshot(snapshot),
                    step_id,
                    snapshot.screen_scene_id);
  }
  Serial.printf("[UI] render step=%s screen=%s pack=%s playing=%u\n",
                step_id,
                snapshot.screen_scene_id != nullptr ? snapshot.screen_scene_id : "n/a",
                snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a",
                g_audio.isPlaying() ? 1U : 0U);
  applySceneResourcePolicy(snapshot);
  UiSceneFrame frame = {};
  frame.scenario = snapshot.scenario;
  frame.screen_scene_id = snapshot.screen_scene_id;
  frame.step_id = step_id;
  frame.audio_pack_id = snapshot.audio_pack_id;
  frame.audio_playing = g_audio.isPlaying();
  frame.screen_payload_json = screen_payload.isEmpty() ? nullptr : screen_payload.c_str();
  g_ui.submitSceneFrame(frame);

  // Apply new owner resources after scene config is committed in UI.
  if (transition.owner_changed) {
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
    if (transition.to_owner == SceneRuntimeOwner::kAmp) {
      setAmpSceneActive(true);
    }
#endif
    if (transition.to_owner == SceneRuntimeOwner::kCamera) {
      setCameraSceneActive(true);
    }
  }
  g_scene_fx_orchestrator.applyTransition(transition);
}

void startPendingAudioIfAny() {
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  if (g_amp_scene_active) {
    String ignored_pack;
    if (g_scenario.consumeAudioRequest(&ignored_pack)) {
      Serial.printf("[MAIN] skip story audio while AMP owns scene pack=%s\n", ignored_pack.c_str());
      g_scenario.notifyAudioDone(millis());
    }
    return;
  }
#endif
  String audio_pack;
  if (!g_scenario.consumeAudioRequest(&audio_pack)) {
    return;
  }

  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const bool is_win_etape_audio = (audio_pack == kPackWin &&
                                   snapshot.step != nullptr && snapshot.step->id != nullptr &&
                                   std::strcmp(snapshot.step->id, kStepWinEtape) == 0);

  const String configured_path = g_storage.resolveAudioPathByPackId(audio_pack.c_str());
  const char* mapped_path = audioPackToFile(audio_pack.c_str());
  if (configured_path.isEmpty() && mapped_path == nullptr) {
    if (g_audio.playDiagnosticTone()) {
      Serial.printf("[MAIN] audio pack=%s has no asset mapping, fallback=builtin_tone\n", audio_pack.c_str());
      return;
    }
    Serial.printf("[MAIN] audio pack=%s has no asset mapping and no fallback tone\n", audio_pack.c_str());
    g_scenario.notifyAudioDone(millis());
    return;
  }

  if (!configured_path.isEmpty() && g_audio.play(configured_path.c_str())) {
    Serial.printf("[MAIN] audio pack=%s path=%s source=story_audio_json\n",
                  audio_pack.c_str(),
                  configured_path.c_str());
    if (is_win_etape_audio) {
      g_win_etape_ui_refresh_pending = true;
    }
    return;
  }
  if (mapped_path != nullptr && g_audio.play(mapped_path)) {
    Serial.printf("[MAIN] audio pack=%s path=%s source=pack_map\n", audio_pack.c_str(), mapped_path);
    if (is_win_etape_audio) {
      g_win_etape_ui_refresh_pending = true;
    }
    return;
  }
  if (g_audio.play(kDiagAudioFile)) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=%s\n", audio_pack.c_str(), kDiagAudioFile);
    if (is_win_etape_audio) {
      g_win_etape_ui_refresh_pending = true;
    }
    return;
  }
  if (g_audio.playDiagnosticTone()) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=builtin_tone\n", audio_pack.c_str());
    if (is_win_etape_audio) {
      g_win_etape_ui_refresh_pending = true;
    }
    return;
  }

  // If audio cannot start (missing/invalid file), unblock scenario transitions.
  Serial.printf("[MAIN] audio fallback failed for pack=%s\n", audio_pack.c_str());
  g_scenario.notifyAudioDone(millis());
}

void handleSerialCommandImpl(const char* command_line, uint32_t now_ms) {
  if (command_line == nullptr || command_line[0] == '\0') {
    return;
  }

  char command[kSerialLineCapacity] = {0};
  std::strncpy(command, command_line, kSerialLineCapacity - 1U);
  char* argument = nullptr;
  for (size_t index = 0; index < kSerialLineCapacity && command[index] != '\0'; ++index) {
    if (command[index] == ' ') {
      command[index] = '\0';
      argument = &command[index + 1U];
      break;
    }
  }
  while (argument != nullptr && *argument == ' ') {
    ++argument;
  }
  if (argument != nullptr && *argument == '\0') {
    argument = nullptr;
  }

  if (std::strcmp(command, "PING") == 0) {
    Serial.println("PONG");
    return;
  }
  if (std::strcmp(command, "HELP") == 0) {
    Serial.println(
        "CMDS PING STATUS BTN_READ NEXT UNLOCK RESET "
        "SC_LIST SC_LOAD <id> SCENE_GOTO <scene_id> SC_COVERAGE SC_REVALIDATE SC_REVALIDATE_ALL SC_EVENT <type> [name] "
        "SC_EVENT_RAW <name> "
        "STORY_REFRESH_SD STORY_SD_STATUS "
        "UI_GFX_STATUS UI_MEM_STATUS PERF_STATUS PERF_RESET RESOURCE_STATUS RESOURCE_PROFILE <gfx_focus|gfx_plus_mic|gfx_plus_cam_snapshot> RESOURCE_PROFILE_AUTO <on|off> "
        "SIMD_STATUS SIMD_SELFTEST SIMD_BENCH [loops] [pixels] "
        "HW_STATUS HW_STATUS_JSON HW_LED_SET <r> <g> <b> [brightness] [pulse] HW_LED_AUTO <ON|OFF> HW_MIC_STATUS HW_BAT_STATUS "
        "MIC_TUNER_STATUS [ON|OFF|<period_ms>] "
        "CAM_STATUS CAM_ON CAM_OFF CAM_SNAPSHOT [filename] "
        "CAM_UI_SHOW CAM_UI_HIDE CAM_UI_TOGGLE CAM_REC_SNAP CAM_REC_SAVE [auto|bmp|jpg|raw] CAM_REC_GALLERY CAM_REC_NEXT CAM_REC_DELETE CAM_REC_STATUS "
        "QR_SIM <payload> "
        "MEDIA_LIST <picture|music|recorder> MEDIA_PLAY <path> MEDIA_STOP REC_START [seconds] [filename] REC_STOP REC_STATUS "
        "BOOT_MODE_STATUS BOOT_MODE_SET <STORY|MEDIA_MANAGER> BOOT_MODE_CLEAR "
        "NET_STATUS WIFI_STATUS WIFI_TEST WIFI_STA <ssid> <pass> WIFI_CONNECT <ssid> <pass> WIFI_PROVISION <ssid> <pass> WIFI_FORGET WIFI_DISCONNECT "
        "AUTH_STATUS AUTH_TOKEN_ROTATE [token] "
        "WIFI_AP_ON [ssid] [pass] WIFI_AP_OFF "
        "ESPNOW_ON ESPNOW_OFF ESPNOW_STATUS ESPNOW_STATUS_JSON ESPNOW_PEER_ADD <mac> ESPNOW_PEER_DEL <mac> ESPNOW_PEER_LIST "
        "ESPNOW_SEND <text|json> "
        "AMP_SHOW AMP_HIDE AMP_TOGGLE AMP_SCAN AMP_PLAY <idx|path> AMP_NEXT AMP_PREV AMP_STOP AMP_STATUS "
        "AUDIO_TEST AUDIO_TEST_FS AUDIO_PROFILE <idx> AUDIO_FX <idx> AUDIO_STATUS VOL <0..21> AUDIO_STOP STOP");
    return;
  }
  if (std::strcmp(command, "STATUS") == 0) {
    printRuntimeStatus();
    return;
  }
  if (std::strcmp(command, "UI_GFX_STATUS") == 0) {
    g_ui.dumpStatus(UiStatusTopic::kGraphics);
    return;
  }
  if (std::strcmp(command, "UI_MEM_STATUS") == 0) {
    g_ui.dumpStatus(UiStatusTopic::kMemory);
    return;
  }
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  if (std::strcmp(command, "AMP_STATUS") == 0) {
    printAmpStatus();
    return;
  }
  if (std::strcmp(command, "AMP_SHOW") == 0) {
    const bool ok = ensureAmpInitialized();
    if (ok) {
      g_amp_player.show();
    }
    Serial.printf("ACK AMP_SHOW ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "AMP_HIDE") == 0) {
    const bool ok = ensureAmpInitialized();
    if (ok) {
      g_amp_player.hide();
    }
    Serial.printf("ACK AMP_HIDE ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "AMP_TOGGLE") == 0) {
    const bool ok = ensureAmpInitialized();
    if (ok) {
      g_amp_player.toggle();
    }
    Serial.printf("ACK AMP_TOGGLE ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "AMP_SCAN") == 0) {
    const size_t count = scanAmpPlaylistWithFallback();
    Serial.printf("ACK AMP_SCAN tracks=%u base=%s\n",
                  static_cast<unsigned int>(count),
                  g_amp_base_dir);
    return;
  }
  if (std::strcmp(command, "AMP_PLAY") == 0) {
    if (!ensureAmpInitialized()) {
      Serial.println("ERR AMP_NOT_READY");
      return;
    }
    if (argument == nullptr || argument[0] == '\0') {
      g_amp_player.service().playIndex(g_amp_player.service().currentIndex());
      Serial.println("ACK AMP_PLAY current");
      return;
    }
    char arg_text[kSerialLineCapacity] = {0};
    copyText(arg_text, sizeof(arg_text), argument);
    trimAsciiInPlace(arg_text);
    if (arg_text[0] == '\0') {
      g_amp_player.service().playIndex(g_amp_player.service().currentIndex());
      Serial.println("ACK AMP_PLAY current");
      return;
    }
    bool numeric = true;
    for (size_t i = 0U; arg_text[i] != '\0'; ++i) {
      if (!std::isdigit(static_cast<unsigned char>(arg_text[i]))) {
        numeric = false;
        break;
      }
    }
    if (numeric) {
      const unsigned long index = std::strtoul(arg_text, nullptr, 10);
      g_amp_player.service().playIndex(static_cast<size_t>(index));
      Serial.printf("ACK AMP_PLAY idx=%lu\n", index);
    } else {
      g_amp_player.service().playPath(arg_text);
      Serial.printf("ACK AMP_PLAY path=%s\n", arg_text);
    }
    return;
  }
  if (std::strcmp(command, "AMP_NEXT") == 0) {
    if (!ensureAmpInitialized()) {
      Serial.println("ERR AMP_NOT_READY");
      return;
    }
    g_amp_player.service().next();
    Serial.println("ACK AMP_NEXT");
    return;
  }
  if (std::strcmp(command, "AMP_PREV") == 0) {
    if (!ensureAmpInitialized()) {
      Serial.println("ERR AMP_NOT_READY");
      return;
    }
    g_amp_player.service().prev();
    Serial.println("ACK AMP_PREV");
    return;
  }
  if (std::strcmp(command, "AMP_STOP") == 0) {
    if (!ensureAmpInitialized()) {
      Serial.println("ERR AMP_NOT_READY");
      return;
    }
    g_amp_player.service().stop();
    Serial.println("ACK AMP_STOP");
    return;
  }
#endif
  if (std::strcmp(command, "PERF_STATUS") == 0) {
    perfMonitor().dumpStatus();
    return;
  }
  if (std::strcmp(command, "PERF_RESET") == 0) {
    perfMonitor().reset();
    Serial.println("ACK PERF_RESET");
    return;
  }
  if (std::strcmp(command, "RESOURCE_STATUS") == 0) {
    printResourceStatus();
    return;
  }
  if (std::strcmp(command, "RESOURCE_PROFILE") == 0) {
    if (argument == nullptr || argument[0] == '\0') {
      printResourceStatus();
      return;
    }
    char profile_arg[48] = {0};
    copyText(profile_arg, sizeof(profile_arg), argument);
    trimAsciiInPlace(profile_arg);
    if (!g_resource_coordinator.parseAndSetProfile(profile_arg)) {
      Serial.println("ERR RESOURCE_PROFILE_ARG");
      return;
    }
    g_resource_profile_auto = false;
    Serial.printf("ACK RESOURCE_PROFILE profile=%s\n", g_resource_coordinator.profileName());
    printResourceStatus();
    return;
  }
  if (std::strcmp(command, "RESOURCE_PROFILE_AUTO") == 0) {
    if (argument == nullptr || argument[0] == '\0') {
      Serial.printf("ERR RESOURCE_PROFILE_AUTO_ARG arg=%s\n", argument == nullptr ? "missing" : "empty");
      return;
    }
    char profile_auto_arg[24] = {0};
    copyText(profile_auto_arg, sizeof(profile_auto_arg), argument);
    trimAsciiInPlace(profile_auto_arg);
    bool parse_ok = false;
    applyResourceProfileAutoCommand(profile_auto_arg, &parse_ok);
    if (!parse_ok) {
      Serial.println("ERR RESOURCE_PROFILE_AUTO_ARG");
      return;
    }
    Serial.printf("ACK RESOURCE_PROFILE_AUTO profile=%s auto=%u\n",
                  g_resource_coordinator.profileName(),
                  g_resource_profile_auto ? 1U : 0U);
    printResourceStatus();
    return;
  }
  if (std::strcmp(command, "SIMD_STATUS") == 0) {
    printSimdStatus();
    return;
  }
  if (std::strcmp(command, "SIMD_SELFTEST") == 0) {
    const bool ok = runtime::simd::runSelfTestCommand();
    Serial.printf("ACK SIMD_SELFTEST ok=%u\n", ok ? 1U : 0U);
    printSimdStatus();
    return;
  }
  if (std::strcmp(command, "SIMD_BENCH") == 0) {
    uint32_t loops = 200U;
    uint32_t pixels = 7680U;
    if (argument != nullptr && argument[0] != '\0') {
      char args[40] = {0};
      copyText(args, sizeof(args), argument);
      trimAsciiInPlace(args);
      char* second = std::strchr(args, ' ');
      if (second != nullptr) {
        *second = '\0';
        ++second;
        while (*second == ' ') {
          ++second;
        }
      }
      if (args[0] != '\0') {
        loops = static_cast<uint32_t>(std::strtoul(args, nullptr, 10));
      }
      if (second != nullptr && second[0] != '\0') {
        pixels = static_cast<uint32_t>(std::strtoul(second, nullptr, 10));
      }
    }
    const runtime::simd::SimdBenchResult result = runtime::simd::runBenchCommand(loops, pixels);
    Serial.printf("SIMD_BENCH loops=%lu pixels=%lu l8_us=%lu idx_us=%lu rgb888_us=%lu gain_us=%lu\n",
                  static_cast<unsigned long>(result.loops),
                  static_cast<unsigned long>(result.pixels),
                  static_cast<unsigned long>(result.l8_to_rgb565_us),
                  static_cast<unsigned long>(result.idx8_to_rgb565_us),
                  static_cast<unsigned long>(result.rgb888_to_rgb565_us),
                  static_cast<unsigned long>(result.s16_gain_q15_us));
    printSimdStatus();
    return;
  }
  if (std::strcmp(command, "BTN_READ") == 0) {
    printButtonRead();
    return;
  }
  if (std::strcmp(command, "NEXT") == 0) {
    const bool ok = notifyScenarioButtonGuarded(5U, false, now_ms, "serial_next");
    Serial.printf("ACK NEXT ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "UNLOCK") == 0) {
    const bool ok = dispatchScenarioEventByName("UNLOCK", now_ms);
    Serial.printf("ACK UNLOCK ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "RESET") == 0) {
    g_audio.stop();
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
    if (g_amp_ready) {
      g_amp_player.service().stop();
    }
#endif
    (void)g_media.stop(&g_audio);
    g_scenario.reset();
    if (g_boot_media_manager_mode) {
      (void)g_scenario.gotoScene(kMediaManagerSceneId, now_ms, "boot_mode_media_manager_reset");
    }
    g_last_action_step_key[0] = '\0';
    refreshSceneIfNeeded(true);
    startPendingAudioIfAny();
    Serial.println("ACK RESET");
    return;
  }
  if (std::strcmp(command, "SC_LIST") == 0) {
    printScenarioList();
    return;
  }
  if (std::strcmp(command, "SC_LOAD") == 0) {
    if (argument == nullptr) {
      Serial.println("ERR SC_LOAD_ARG");
      return;
    }
    char scenario_id[kSerialLineCapacity] = {0};
    std::strncpy(scenario_id, argument, sizeof(scenario_id) - 1U);
    toUpperAsciiInPlace(scenario_id);
    String load_source;
    String load_path;
    const bool ok = loadScenarioByIdPreferStoryFile(scenario_id, &load_source, &load_path);
    Serial.printf("ACK SC_LOAD id=%s ok=%u\n", scenario_id, ok ? 1U : 0U);
    if (ok) {
      if (!load_path.isEmpty()) {
        Serial.printf("[SCENARIO] load source=%s path=%s\n", load_source.c_str(), load_path.c_str());
      } else {
        Serial.printf("[SCENARIO] load source=%s id=%s\n", load_source.c_str(), scenario_id);
      }
    }
    if (ok) {
      g_last_action_step_key[0] = '\0';
      refreshSceneIfNeeded(true);
      startPendingAudioIfAny();
    }
    return;
  }
  if (std::strcmp(command, "STORY_REFRESH_SD") == 0) {
    const bool ok = refreshStoryFromSd();
    Serial.printf("ACK STORY_REFRESH_SD ok=%u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "STORY_SD_STATUS") == 0) {
    Serial.printf("STORY_SD_STATUS ready=%u\n", g_storage.hasSdCard() ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "HW_STATUS") == 0 || std::strcmp(command, "HW_MIC_STATUS") == 0 ||
      std::strcmp(command, "HW_BAT_STATUS") == 0) {
    printHardwareStatus();
    return;
  }
  if (std::strcmp(command, "HW_STATUS_JSON") == 0) {
    printHardwareStatusJson();
    return;
  }
  if (std::strcmp(command, "MIC_TUNER_STATUS") == 0) {
    if (argument == nullptr) {
      printMicTunerStatus();
      return;
    }

    char arg_copy[40] = {0};
    copyText(arg_copy, sizeof(arg_copy), argument);
    trimAsciiInPlace(arg_copy);
    if (arg_copy[0] == '\0') {
      printMicTunerStatus();
      return;
    }

    char* extra = std::strchr(arg_copy, ' ');
    if (extra != nullptr) {
      *extra = '\0';
      ++extra;
      while (*extra == ' ') {
        ++extra;
      }
    }

    bool stream_value = false;
    if (parseBoolToken(arg_copy, &stream_value)) {
      g_mic_tuner_stream_enabled = stream_value;
      if (extra != nullptr && extra[0] != '\0') {
        const long period_ms = std::strtol(extra, nullptr, 10);
        if (period_ms >= 50L && period_ms <= 5000L) {
          g_mic_tuner_stream_period_ms = static_cast<uint16_t>(period_ms);
        }
      }
      g_next_mic_tuner_stream_ms = now_ms + 20U;
      Serial.printf("ACK MIC_TUNER_STATUS stream=%u period_ms=%u\n",
                    g_mic_tuner_stream_enabled ? 1U : 0U,
                    static_cast<unsigned int>(g_mic_tuner_stream_period_ms));
      if (!g_mic_tuner_stream_enabled) {
        printMicTunerStatus();
      }
      return;
    }

    const long period_ms = std::strtol(arg_copy, nullptr, 10);
    if (period_ms >= 50L && period_ms <= 5000L) {
      g_mic_tuner_stream_enabled = true;
      g_mic_tuner_stream_period_ms = static_cast<uint16_t>(period_ms);
      g_next_mic_tuner_stream_ms = now_ms + 20U;
      Serial.printf("ACK MIC_TUNER_STATUS stream=1 period_ms=%u\n", static_cast<unsigned int>(g_mic_tuner_stream_period_ms));
      return;
    }

    Serial.println("ERR MIC_TUNER_STATUS_ARG");
    return;
  }
  if (std::strcmp(command, "CAM_STATUS") == 0) {
    printCameraStatus();
    return;
  }
  if (std::strcmp(command, "CAM_REC_STATUS") == 0) {
    printCameraRecorderStatus();
    return;
  }
  if (std::strcmp(command, "REC_STATUS") == 0) {
    printMediaStatus();
    return;
  }
  if (std::strcmp(command, "HW_LED_SET") == 0 || std::strcmp(command, "HW_LED_AUTO") == 0 ||
      std::strcmp(command, "CAM_ON") == 0 || std::strcmp(command, "CAM_OFF") == 0 ||
      std::strcmp(command, "CAM_SNAPSHOT") == 0 || std::strcmp(command, "CAM_UI_SHOW") == 0 ||
      std::strcmp(command, "CAM_UI_HIDE") == 0 || std::strcmp(command, "CAM_UI_TOGGLE") == 0 ||
      std::strcmp(command, "CAM_REC_SNAP") == 0 || std::strcmp(command, "CAM_REC_SAVE") == 0 ||
      std::strcmp(command, "CAM_REC_GALLERY") == 0 || std::strcmp(command, "CAM_REC_NEXT") == 0 ||
      std::strcmp(command, "CAM_REC_DELETE") == 0 || std::strcmp(command, "MEDIA_LIST") == 0 ||
      std::strcmp(command, "MEDIA_PLAY") == 0 || std::strcmp(command, "MEDIA_STOP") == 0 ||
      std::strcmp(command, "REC_START") == 0 || std::strcmp(command, "REC_STOP") == 0 ||
      std::strcmp(command, "SCENE_GOTO") == 0 || std::strcmp(command, "QR_SIM") == 0 ||
      std::strcmp(command, "BOOT_MODE_STATUS") == 0 || std::strcmp(command, "BOOT_MODE_SET") == 0 ||
      std::strcmp(command, "BOOT_MODE_CLEAR") == 0) {
    String action = command;
    if (argument != nullptr && argument[0] != '\0') {
      action += " ";
      action += argument;
    }
    String error;
    const bool ok = dispatchControlAction(action, now_ms, &error);
    Serial.printf("ACK %s ok=%u%s%s\n",
                  command,
                  ok ? 1U : 0U,
                  error.isEmpty() ? "" : " err=",
                  error.isEmpty() ? "" : error.c_str());
    return;
  }
  if (std::strcmp(command, "SC_COVERAGE") == 0) {
    printScenarioCoverage();
    return;
  }
  if (std::strcmp(command, "SC_REVALIDATE") == 0) {
    runScenarioRevalidate(now_ms);
    return;
  }
  if (std::strcmp(command, "SC_REVALIDATE_ALL") == 0) {
    runScenarioRevalidateAll(now_ms);
    return;
  }
  if (std::strcmp(command, "SC_EVENT") == 0) {
    if (argument == nullptr) {
      Serial.println("ERR SC_EVENT_USAGE");
      return;
    }
    char event_args[kSerialLineCapacity] = {0};
    std::strncpy(event_args, argument, kSerialLineCapacity - 1U);
    char* event_type_text = event_args;
    char* event_name_raw = nullptr;
    for (size_t index = 0; index < kSerialLineCapacity && event_args[index] != '\0'; ++index) {
      if (event_args[index] == ' ') {
        event_args[index] = '\0';
        event_name_raw = &event_args[index + 1U];
        break;
      }
    }
    while (event_name_raw != nullptr && *event_name_raw == ' ') {
      ++event_name_raw;
    }
    StoryEventType event_type = StoryEventType::kNone;
    if (!parseEventType(event_type_text, &event_type)) {
      Serial.println("ERR SC_EVENT_TYPE");
      return;
    }
    const char* event_name = event_name_raw;
    if (event_name == nullptr || event_name[0] == '\0') {
      event_name = defaultEventNameForType(event_type);
    }
    const ScenarioSnapshot before = g_scenario.snapshot();
    const bool dispatched = dispatchScenarioEventByType(event_type, event_name, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    if (dispatched && changed) {
      g_last_action_step_key[0] = '\0';
      refreshSceneIfNeeded(true);
      startPendingAudioIfAny();
    }
    Serial.printf("ACK SC_EVENT type=%s name=%s dispatched=%u changed=%u step=%s\n",
                  eventTypeName(event_type),
                  event_name,
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(after));
    return;
  }
  if (std::strcmp(command, "SC_EVENT_RAW") == 0) {
    if (argument == nullptr || argument[0] == '\0') {
      Serial.println("ERR SC_EVENT_RAW_ARG");
      return;
    }
    const ScenarioSnapshot before = g_scenario.snapshot();
    const bool dispatched = dispatchScenarioEventByName(argument, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    if (dispatched && changed) {
      g_last_action_step_key[0] = '\0';
      refreshSceneIfNeeded(true);
      startPendingAudioIfAny();
    }
    Serial.printf("ACK SC_EVENT_RAW name=%s dispatched=%u changed=%u step=%s\n",
                  argument,
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(after));
    return;
  }
  if (std::strcmp(command, "NET_STATUS") == 0 || std::strcmp(command, "WIFI_STATUS") == 0 ||
      std::strcmp(command, "ESPNOW_STATUS") == 0) {
    printNetworkStatus();
    return;
  }
  if (std::strcmp(command, "AUTH_STATUS") == 0) {
    Serial.printf("AUTH_STATUS setup_mode=%u auth_required=%u token_set=%u provisioned=%u\n",
                  g_setup_mode ? 1U : 0U,
                  g_web_auth_required ? 1U : 0U,
                  g_web_auth_token[0] != '\0' ? 1U : 0U,
                  g_credential_store.isProvisioned() ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "AUTH_TOKEN_ROTATE") == 0) {
    bool ok = false;
    if (argument != nullptr && argument[0] != '\0') {
      char token[kWebAuthTokenCapacity] = {0};
      copyText(token, sizeof(token), argument);
      trimAsciiInPlace(token);
      ok = token[0] != '\0' && g_credential_store.saveWebToken(token);
      if (ok) {
        copyText(g_web_auth_token, sizeof(g_web_auth_token), token);
      }
    } else {
      ok = ensureWebToken(true, false, nullptr);
    }
    Serial.printf("ACK AUTH_TOKEN_ROTATE ok=%u%s%s\n",
                  ok ? 1U : 0U,
                  ok ? " token=" : "",
                  ok ? g_web_auth_token : "");
    return;
  }
  if (std::strcmp(command, "ESPNOW_STATUS_JSON") == 0) {
    printEspNowStatusJson();
    return;
  }
  if (std::strcmp(command, "WIFI_TEST") == 0) {
    if (g_network_cfg.wifi_test_ssid[0] == '\0') {
      Serial.println("ERR WIFI_TEST_NO_CREDENTIALS");
      return;
    }
    const bool ok = g_network.connectSta(g_network_cfg.wifi_test_ssid, g_network_cfg.wifi_test_password);
    Serial.printf("ACK WIFI_TEST ssid=%s ok=%u\n", g_network_cfg.wifi_test_ssid, ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "WIFI_PROVISION") == 0) {
    if (argument == nullptr) {
      Serial.println("ERR WIFI_PROVISION_ARG");
      return;
    }
    String ssid;
    String pass;
    if (!splitSsidPass(argument, &ssid, &pass) || ssid.isEmpty()) {
      Serial.println("ERR WIFI_PROVISION_ARG");
      return;
    }
    bool connect_started = false;
    bool persisted = false;
    bool token_generated = false;
    const bool ok = provisionWifiCredentials(
        ssid.c_str(), pass.c_str(), true, &connect_started, &persisted, &token_generated);
    Serial.printf("ACK WIFI_PROVISION ssid=%s ok=%u persisted=%u connect_started=%u setup_mode=%u token_set=%u\n",
                  ssid.c_str(),
                  ok ? 1U : 0U,
                  persisted ? 1U : 0U,
                  connect_started ? 1U : 0U,
                  g_setup_mode ? 1U : 0U,
                  g_web_auth_token[0] != '\0' ? 1U : 0U);
    if (token_generated && g_web_auth_token[0] != '\0') {
      Serial.printf("AUTH_TOKEN %s\n", g_web_auth_token);
    }
    return;
  }
  if (std::strcmp(command, "WIFI_STA") == 0 || std::strcmp(command, "WIFI_CONNECT") == 0) {
    if (argument == nullptr) {
      Serial.println("ERR WIFI_STA_ARG");
      return;
    }
    String ssid;
    String pass;
    if (!splitSsidPass(argument, &ssid, &pass) || ssid.isEmpty()) {
      Serial.println("ERR WIFI_STA_ARG");
      return;
    }
    const bool ok = g_network.connectSta(ssid.c_str(), pass.c_str());
    Serial.printf("ACK WIFI_STA ssid=%s ok=%u\n", ssid.c_str(), ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "WIFI_FORGET") == 0) {
    const bool ok = forgetWifiCredentials();
    Serial.printf("ACK WIFI_FORGET ok=%u setup_mode=%u\n", ok ? 1U : 0U, g_setup_mode ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "WIFI_DISCONNECT") == 0) {
    g_network.disconnectSta();
    Serial.println("ACK WIFI_DISCONNECT");
    return;
  }
  if (std::strcmp(command, "WIFI_AP_ON") == 0) {
    String ssid = g_network_cfg.ap_default_ssid;
    String pass = g_network_cfg.ap_default_password;
    if (argument != nullptr) {
      String parsed_ssid;
      String parsed_pass;
      if (splitSsidPass(argument, &parsed_ssid, &parsed_pass) && !parsed_ssid.isEmpty()) {
        ssid = parsed_ssid;
        if (!parsed_pass.isEmpty()) {
          pass = parsed_pass;
        }
      } else if (std::strlen(argument) > 0U) {
        ssid = argument;
      }
    }
    const bool ok = g_network.startAp(ssid.c_str(), pass.c_str());
    Serial.printf("ACK WIFI_AP_ON ssid=%s ok=%u\n", ssid.c_str(), ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "WIFI_AP_OFF") == 0) {
    g_network.stopAp();
    Serial.println("ACK WIFI_AP_OFF");
    return;
  }
  if (std::strcmp(command, "ESPNOW_ON") == 0) {
    const bool ok = g_network.enableEspNow();
    Serial.printf("ACK ESPNOW_ON %u\n", ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "ESPNOW_OFF") == 0) {
    g_network.disableEspNow();
    Serial.println("ACK ESPNOW_OFF");
    return;
  }
  if (std::strcmp(command, "ESPNOW_PEER_ADD") == 0) {
    if (argument == nullptr || argument[0] == '\0') {
      Serial.println("ERR ESPNOW_PEER_ADD_ARG");
      return;
    }
    const bool ok = g_network.addEspNowPeer(argument);
    Serial.printf("ACK ESPNOW_PEER_ADD mac=%s ok=%u\n", argument, ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "ESPNOW_PEER_DEL") == 0) {
    if (argument == nullptr || argument[0] == '\0') {
      Serial.println("ERR ESPNOW_PEER_DEL_ARG");
      return;
    }
    const bool ok = g_network.removeEspNowPeer(argument);
    Serial.printf("ACK ESPNOW_PEER_DEL mac=%s ok=%u\n", argument, ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "ESPNOW_PEER_LIST") == 0) {
    const uint8_t count = g_network.espNowPeerCount();
    Serial.printf("ESPNOW_PEER_LIST count=%u\n", count);
    for (uint8_t index = 0U; index < count; ++index) {
      char peer[18] = {0};
      if (!g_network.espNowPeerAt(index, peer, sizeof(peer))) {
        continue;
      }
      Serial.printf("ESPNOW_PEER idx=%u mac=%s\n", index, peer);
    }
    return;
  }
  if (std::strcmp(command, "ESPNOW_SEND") == 0) {
    if (argument == nullptr) {
      Serial.println("ERR ESPNOW_SEND_ARG");
      return;
    }
    String payload;
    if (!parseEspNowSendPayload(argument, payload, nullptr)) {
      Serial.println("ERR ESPNOW_SEND_ARG");
      return;
    }
    const bool ok = g_network.sendEspNowTarget(kEspNowBroadcastTarget, payload.c_str());
    Serial.printf("ACK ESPNOW_SEND target=%s ok=%u\n", kEspNowBroadcastTarget, ok ? 1U : 0U);
    return;
  }
  if (std::strcmp(command, "AUDIO_TEST") == 0) {
    g_audio.stop();
    const bool ok = g_audio.playDiagnosticTone();
    Serial.printf("ACK AUDIO_TEST %u\n", ok ? 1 : 0);
    return;
  }
  if (std::strcmp(command, "AUDIO_TEST_FS") == 0) {
    g_audio.stop();
    const bool ok = g_audio.play(kDiagAudioFile);
    Serial.printf("ACK AUDIO_TEST_FS %u\n", ok ? 1 : 0);
    return;
  }
  if (std::strcmp(command, "AUDIO_PROFILE") == 0) {
    if (argument == nullptr) {
      Serial.printf("AUDIO_PROFILE current=%u label=%s count=%u\n",
                    g_audio.outputProfile(),
                    g_audio.outputProfileLabel(g_audio.outputProfile()),
                    g_audio.outputProfileCount());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed > 255UL) {
      Serial.println("ERR AUDIO_PROFILE_ARG");
      return;
    }
    const uint8_t profile = static_cast<uint8_t>(parsed);
    const bool ok = g_audio.setOutputProfile(profile);
    Serial.printf("ACK AUDIO_PROFILE %u %u %s\n",
                  profile,
                  ok ? 1U : 0U,
                  ok ? g_audio.outputProfileLabel(profile) : "invalid");
    return;
  }
  if (std::strcmp(command, "AUDIO_STATUS") == 0) {
    Serial.printf("AUDIO_STATUS playing=%u track=%s codec=%s bitrate=%u profile=%u:%s fx=%u:%s vol=%u\n",
                  g_audio.isPlaying() ? 1U : 0U,
                  g_audio.currentTrack(),
                  g_audio.activeCodec(),
                  g_audio.activeBitrateKbps(),
                  g_audio.outputProfile(),
                  g_audio.outputProfileLabel(g_audio.outputProfile()),
                  g_audio.fxProfile(),
                  g_audio.fxProfileLabel(g_audio.fxProfile()),
                  g_audio.volume());
    return;
  }
  if (std::strcmp(command, "AUDIO_FX") == 0) {
    if (argument == nullptr) {
      Serial.printf("AUDIO_FX current=%u label=%s count=%u\n",
                    g_audio.fxProfile(),
                    g_audio.fxProfileLabel(g_audio.fxProfile()),
                    g_audio.fxProfileCount());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed >= g_audio.fxProfileCount() || parsed > 255UL) {
      Serial.println("ERR AUDIO_FX_ARG");
      return;
    }
    const uint8_t fx = static_cast<uint8_t>(parsed);
    const bool ok = g_audio.setFxProfile(fx);
    Serial.printf("ACK AUDIO_FX %u %u %s\n",
                  fx,
                  ok ? 1U : 0U,
                  ok ? g_audio.fxProfileLabel(fx) : "invalid");
    return;
  }
  if (std::strcmp(command, "VOL") == 0) {
    if (argument == nullptr) {
      Serial.printf("VOL %u\n", g_audio.volume());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed > static_cast<unsigned long>(FREENOVE_AUDIO_MAX_VOLUME)) {
      Serial.println("ERR VOL_ARG");
      return;
    }
    g_audio.setVolume(static_cast<uint8_t>(parsed));
    Serial.printf("ACK VOL %u\n", g_audio.volume());
    return;
  }
  if (std::strcmp(command, "AUDIO_STOP") == 0) {
    g_audio.stop();
    Serial.println("ACK AUDIO_STOP");
    return;
  }
  if (std::strcmp(command, "STOP") == 0) {
    g_audio.stop();
    Serial.println("ACK STOP");
    return;
  }
  Serial.printf("UNKNOWN %s\n", command_line);
}

void pollSerialCommands(uint32_t now_ms) {
  while (Serial.available() > 0) {
    const int raw = Serial.read();
    if (raw < 0) {
      break;
    }
    const char ch = static_cast<char>(raw);
    if (ch == '\r' || ch == '\n') {
      if (g_serial_line_len == 0U) {
        continue;
      }
      g_serial_line[g_serial_line_len] = '\0';
      g_app_coordinator.onSerialLine(g_serial_line, now_ms);
      g_serial_line_len = 0U;
      continue;
    }
    if (g_serial_line_len + 1U >= kSerialLineCapacity) {
      g_serial_line_len = 0U;
      Serial.println("ERR CMD_TOO_LONG");
      continue;
    }
    g_serial_line[g_serial_line_len++] = ch;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  RuntimeMetrics::instance().reset(bootResetReasonCode());
  Serial.println("[MAIN] Freenove all-in-one boot");
  bootPrintReport(kFirmwareName, ZACUS_FW_VERSION);
  logBuildMemoryPolicy();
  logBootMemoryProfile();
  g_runtime_serial_service.configure(handleSerialCommandImpl, dispatchControlActionImpl);
  g_runtime_scene_service.configure(refreshSceneIfNeededImpl, startPendingAudioIfAny);
  g_runtime_web_service.configure(setupWebUiImpl);

  if (!g_storage.begin()) {
    Serial.println("[MAIN] storage init failed");
  }
  g_storage.ensurePath("/data");
  g_storage.ensurePath("/scenarios");
  g_storage.ensurePath("/scenarios/data");
  g_storage.ensurePath("/screens");
  g_storage.ensurePath("/story");
  g_storage.ensurePath("/story/scenarios");
  g_storage.ensurePath("/story/screens");
  g_storage.ensurePath("/story/audio");
  g_storage.ensurePath("/story/apps");
  g_storage.ensurePath("/story/actions");
  g_storage.ensurePath("/picture");
  g_storage.ensurePath("/music");
  g_storage.ensurePath("/audio");
  g_storage.ensurePath("/recorder");
  g_storage.ensureDefaultStoryBundle();
  if (g_storage.hasSdCard()) {
    g_storage.syncStoryTreeFromSd();
  }
  g_storage.ensureDefaultScenarioFile(kDefaultScenarioFile);
  if (g_storage.hasSdCard()) {
    g_storage.syncStoryFileFromSd(kDefaultScenarioFile);
  }
  RuntimeConfigService::load(g_storage, &g_network_cfg, &g_hardware_cfg, &g_camera_cfg, &g_media_cfg);
  loadBootProvisioningState();
  {
    BootModeStore::StartupMode startup_mode = BootModeStore::StartupMode::kStory;
    if (g_boot_mode_store.loadMode(&startup_mode)) {
      applyStartupMode(startup_mode);
    } else {
      applyStartupMode(BootModeStore::StartupMode::kStory);
    }
    Serial.printf("[BOOT] startup_mode=%s media_validated=%u\n",
                  BootModeStore::modeLabel(currentStartupMode()),
                  g_boot_mode_store.isMediaValidated() ? 1U : 0U);
  }
  g_resource_coordinator.begin();
  Serial.printf("[MAIN] default scenario checksum=%lu\n",
                static_cast<unsigned long>(g_storage.checksum(kDefaultScenarioFile)));
  Serial.printf("[MAIN] story storage sd=%u\n", g_storage.hasSdCard() ? 1U : 0U);
  Serial.printf("[AUTH] setup_mode=%u auth_required=%u token_set=%u\n",
                g_setup_mode ? 1U : 0U,
                g_web_auth_required ? 1U : 0U,
                g_web_auth_token[0] != '\0' ? 1U : 0U);

  g_media.begin(g_media_cfg);
  g_camera.begin(g_camera_cfg);
  if (g_camera_cfg.enabled_on_boot) {
    String cam_error;
    if (approveCameraOperation("boot_cam_on", &cam_error)) {
      const bool cam_ok = g_camera.start();
      Serial.printf("[CAM] boot start=%u\n", cam_ok ? 1U : 0U);
    } else {
      Serial.printf("[CAM] boot start blocked profile=%s\n", g_resource_coordinator.profileName());
    }
  }
  if (g_hardware_cfg.enabled_on_boot) {
    g_hardware_started = g_hardware.begin();
    g_next_hw_telemetry_ms = millis() + g_hardware_cfg.telemetry_period_ms;
    g_mic_event_armed = true;
    g_battery_low_latched = false;
    resetLaTriggerState(false);
  } else {
    g_hardware_started = false;
    Serial.println("[HW] disabled by APP_HARDWARE config");
  }

  g_buttons.begin();
  g_touch.begin();
  g_network.begin(g_network_cfg.hostname);
  g_network.configureFallbackAp(g_network_cfg.ap_default_ssid, g_network_cfg.ap_default_password);
  g_network.configureLocalPolicy(g_network_cfg.local_ssid,
                                 g_network_cfg.local_password,
                                 g_network_cfg.force_ap_if_not_local,
                                 g_network_cfg.local_retry_ms,
                                 g_network_cfg.pause_local_retry_when_ap_client);
  if (g_setup_mode && g_network_cfg.ap_default_ssid[0] != '\0') {
    g_network.startAp(g_network_cfg.ap_default_ssid, g_network_cfg.ap_default_password);
  }
  if (g_network_cfg.local_ssid[0] != '\0') {
    const bool connect_started = g_network.connectSta(g_network_cfg.local_ssid, g_network_cfg.local_password);
    Serial.printf("[NET] boot wifi target=%s started=%u\n",
                  g_network_cfg.local_ssid,
                  connect_started ? 1U : 0U);
  }
  if (g_network_cfg.espnow_enabled_on_boot) {
    if (g_network.enableEspNow()) {
      for (uint8_t index = 0U; index < g_network_cfg.espnow_boot_peer_count; ++index) {
        const char* peer = g_network_cfg.espnow_boot_peers[index];
        if (peer == nullptr || peer[0] == '\0') {
          continue;
        }
        const bool ok = g_network.addEspNowPeer(peer);
        Serial.printf("[NET] boot peer add mac=%s ok=%u\n", peer, ok ? 1U : 0U);
      }
    }
  } else {
    Serial.println("[NET] ESP-NOW boot disabled by APP_ESPNOW config");
  }
  setupWebUi();
  g_audio.begin();
  Serial.printf("[MAIN] audio profile=%u:%s count=%u\n",
                g_audio.outputProfile(),
                g_audio.outputProfileLabel(g_audio.outputProfile()),
                g_audio.outputProfileCount());
  g_audio.setAudioDoneCallback(onAudioFinished, nullptr);
  if (kBootDiagnosticTone) {
    g_audio.playDiagnosticTone();
  }

  if (!g_scenario.begin(kDefaultScenarioFile)) {
    Serial.println("[MAIN] scenario init failed");
  }
  if (g_boot_media_manager_mode) {
    const bool routed = g_scenario.gotoScene(kMediaManagerSceneId, millis(), "boot_mode_media_manager");
    Serial.printf("[BOOT] route media_manager scene=%s ok=%u\n", kMediaManagerSceneId, routed ? 1U : 0U);
  }
  g_last_action_step_key[0] = '\0';

  g_ui.begin();
  g_ui.setHardwareController(&g_hardware);
  UiLaMetrics boot_la_metrics = {};
  boot_la_metrics.locked = false;
  boot_la_metrics.stability_pct = 0U;
  boot_la_metrics.stable_ms = 0U;
  boot_la_metrics.stable_target_ms = g_hardware_cfg.mic_la_stable_ms;
  boot_la_metrics.gate_elapsed_ms = 0U;
  boot_la_metrics.gate_timeout_ms = g_hardware_cfg.mic_la_timeout_ms;
  g_ui.setLaMetrics(boot_la_metrics);
  g_ui.setHardwareSnapshotRef(&g_hardware.snapshotRef());
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  g_amp_ready = false;
  g_amp_scene_active = false;
  copyText(g_amp_base_dir, sizeof(g_amp_base_dir), kAmpMusicPathPrimary);
  Serial.println("[AMP] lazy init (on SCENE_MP3_PLAYER)");
#endif
  g_camera_scene_active = false;
  g_camera_scene_ready = ensureCameraUiInitialized();
  refreshSceneIfNeeded(true);
  startPendingAudioIfAny();

  g_runtime_services.audio = &g_audio;
  g_runtime_services.scenario = &g_scenario;
  g_runtime_services.ui = &g_ui;
  g_runtime_services.storage = &g_storage;
  g_runtime_services.buttons = &g_buttons;
  g_runtime_services.touch = &g_touch;
  g_runtime_services.network = &g_network;
  g_runtime_services.hardware = &g_hardware;
  g_runtime_services.camera = &g_camera;
  g_runtime_services.media = &g_media;
  g_runtime_services.resource_coordinator = &g_resource_coordinator;
  g_runtime_services.network_cfg = &g_network_cfg;
  g_runtime_services.hardware_cfg = &g_hardware_cfg;
  g_runtime_services.camera_cfg = &g_camera_cfg;
  g_runtime_services.media_cfg = &g_media_cfg;
  g_runtime_services.tick_runtime = runtimeTickBridge;
  g_runtime_services.dispatch_serial = serialDispatchBridge;
  g_app_coordinator.begin(&g_runtime_services);
}

void runRuntimeIteration(uint32_t now_ms) {
  ButtonEvent event;
  while (g_buttons.pollEvent(&event)) {
    const uint32_t event_ms = (event.ms != 0U) ? event.ms : now_ms;
    ZACUS_RL_LOG_MS(250U,
                    "[MAIN] button key=%u long=%u ms=%lu\n",
                    event.key,
                    event.long_press ? 1U : 0U,
                    static_cast<unsigned long>(event_ms));
    UiInputEvent ui_event = {};
    ui_event.type = UiInputEventType::kButton;
    ui_event.key = event.key;
    ui_event.long_press = event.long_press;
    g_ui.submitInputEvent(ui_event);
    if (g_camera_scene_active) {
      (void)dispatchCameraSceneButton(event.key, event.long_press);
    }
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
    if (!g_amp_scene_active && !g_camera_scene_active) {
      notifyScenarioButtonGuarded(event.key, event.long_press, event_ms, "physical_button");
    }
#else
    if (!g_camera_scene_active) {
      notifyScenarioButtonGuarded(event.key, event.long_press, event_ms, "physical_button");
    }
#endif
    if (g_hardware_started) {
      g_hardware.noteButton(event.key, event.long_press, event_ms);
    }
  }

  TouchPoint touch;
  if (g_touch.poll(&touch)) {
    UiInputEvent ui_event = {};
    ui_event.type = UiInputEventType::kTouch;
    ui_event.touch_x = touch.x;
    ui_event.touch_y = touch.y;
    ui_event.touch_pressed = touch.touched;
    g_ui.submitInputEvent(ui_event);
  } else {
    UiInputEvent ui_event = {};
    ui_event.type = UiInputEventType::kTouch;
    ui_event.touch_x = 0;
    ui_event.touch_y = 0;
    ui_event.touch_pressed = false;
    g_ui.submitInputEvent(ui_event);
  }

  const uint32_t network_started_us = perfMonitor().beginSample();
  g_network.update(now_ms);
  perfMonitor().endSample(PerfSection::kNetworkUpdate, network_started_us);
  if (g_hardware_started) {
    applyMicRuntimePolicy();
    g_hardware.update(now_ms);
    maybeEmitHardwareEvents(now_ms);
    maybeLogHardwareTelemetry(now_ms);
    maybeStreamMicTunerStatus(now_ms);
  }
  char net_payload[192] = {0};
  char net_peer[18] = {0};
  char net_msg_id[32] = {0};
  char net_type[24] = {0};
  uint32_t net_seq = 0U;
  bool net_ack_requested = false;
  while (g_network.consumeEspNowMessage(net_payload,
                                        sizeof(net_payload),
                                        net_peer,
                                        sizeof(net_peer),
                                        net_msg_id,
                                        sizeof(net_msg_id),
                                        &net_seq,
                                        net_type,
                                        sizeof(net_type),
                                        &net_ack_requested)) {
    EspNowCommandResult command_result;
    bool handled_as_command = false;
    if (net_type[0] != '\0' && std::strcmp(net_type, "command") == 0) {
      handled_as_command = executeEspNowCommandPayload(net_payload, now_ms, &command_result);
      if (!command_result.handled) {
        command_result.handled = true;
        command_result.ok = false;
        command_result.code = "command";
        command_result.error = "unsupported_command";
      }
      sendEspNowAck(net_peer, net_msg_id, net_seq, command_result, net_ack_requested);
      Serial.printf("[NET] ESPNOW command peer=%s msg_id=%s seq=%lu ok=%u code=%s err=%s\n",
                    net_peer[0] != '\0' ? net_peer : "n/a",
                    net_msg_id[0] != '\0' ? net_msg_id : "n/a",
                    static_cast<unsigned long>(net_seq),
                    command_result.ok ? 1U : 0U,
                    command_result.code.c_str(),
                    command_result.error.c_str());
      if (handled_as_command) {
        continue;
      }
    }
    if (!g_network_cfg.espnow_bridge_to_story_event) {
      Serial.printf("[NET] ESPNOW peer=%s payload=%s type=%s bridge=off\n",
                    net_peer[0] != '\0' ? net_peer : "n/a",
                    net_payload,
                    net_type[0] != '\0' ? net_type : "legacy");
      continue;
    }
    char event_token[kSerialLineCapacity] = {0};
    if (!normalizeEspNowPayloadToScenarioEvent(net_payload, event_token, sizeof(event_token))) {
      Serial.printf("[NET] ESPNOW peer=%s payload=%s type=%s ignored=unsupported\n",
                    net_peer[0] != '\0' ? net_peer : "n/a",
                    net_payload,
                    net_type[0] != '\0' ? net_type : "legacy");
      continue;
    }
    const ScenarioSnapshot before = g_scenario.snapshot();
    const bool dispatched = dispatchScenarioEventByName(event_token, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("[NET] ESPNOW peer=%s payload=%s type=%s event=%s dispatched=%u changed=%u step=%s\n",
                  net_peer[0] != '\0' ? net_peer : "n/a",
                  net_payload,
                  net_type[0] != '\0' ? net_type : "legacy",
                  event_token,
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(after));
  }

  const uint32_t audio_started_us = perfMonitor().beginSample();
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  if (!g_amp_scene_active) {
    g_audio.update();
  }
#else
  g_audio.update();
#endif
  perfMonitor().endSample(PerfSection::kAudioUpdate, audio_started_us);
  g_media.update(now_ms, &g_audio);
  const uint32_t scenario_started_us = perfMonitor().beginSample();
  g_scenario.tick(now_ms);
  perfMonitor().endSample(PerfSection::kScenarioTick, scenario_started_us);
  startPendingAudioIfAny();
  if (g_win_etape_ui_refresh_pending) {
    g_win_etape_ui_refresh_pending = false;
    refreshSceneIfNeeded(true);
  }
  uint32_t la_gate_elapsed_ms = 0U;
  if (g_la_trigger.gate_active && g_la_trigger.gate_entered_ms > 0U) {
    la_gate_elapsed_ms = now_ms - g_la_trigger.gate_entered_ms;
  }
  UiLaMetrics la_metrics = {};
  la_metrics.locked = g_la_trigger.locked;
  la_metrics.stability_pct = laStablePercent();
  la_metrics.stable_ms = g_la_trigger.stable_ms;
  la_metrics.stable_target_ms = g_hardware_cfg.mic_la_stable_ms;
  la_metrics.gate_elapsed_ms = la_gate_elapsed_ms;
  la_metrics.gate_timeout_ms = g_hardware_cfg.mic_la_timeout_ms;
  g_ui.setLaMetrics(la_metrics);
  refreshSceneIfNeeded(false);
  const uint32_t ui_started_us = perfMonitor().beginSample();
  g_ui.tick(now_ms);
  char runtime_event[24] = {0};
  while (g_ui.consumeRuntimeEvent(runtime_event, sizeof(runtime_event))) {
    char event_token[40] = "SERIAL:";
    std::strncat(event_token, runtime_event, sizeof(event_token) - std::strlen(event_token) - 1U);
    bool dispatched = dispatchScenarioEventByName(event_token, now_ms);
    if (!dispatched) {
      dispatched = dispatchScenarioEventByName(runtime_event, now_ms);
      if (dispatched) {
        std::strncpy(event_token, runtime_event, sizeof(event_token) - 1U);
        event_token[sizeof(event_token) - 1U] = '\0';
      }
    }
    Serial.printf("[UI_EVENT] event=%s dispatched=%u\n", event_token, dispatched ? 1U : 0U);
    if (dispatched) {
      refreshSceneIfNeeded(true);
    }
    runtime_event[0] = '\0';
  }
#if defined(USE_AUDIO) && (USE_AUDIO != 0)
  if (g_amp_ready) {
    g_amp_player.tick(now_ms);
  }
#endif
  g_resource_coordinator.update(g_ui.memorySnapshot(), now_ms);
  applyMicRuntimePolicy();
  RuntimeMetrics::instance().noteUiFrame(now_ms);
  perfMonitor().endSample(PerfSection::kUiTick, ui_started_us);
  RuntimeMetrics::instance().logPeriodic(now_ms);
  if (g_web_started) {
    g_web_server.handleClient();
    if (g_web_disconnect_sta_pending &&
        static_cast<int32_t>(now_ms - g_web_disconnect_sta_at_ms) >= 0) {
      g_web_disconnect_sta_pending = false;
      g_network.disconnectSta();
    }
  }
  yield();
}

void loop() {
  const uint32_t now_ms = millis();
  pollSerialCommands(now_ms);
  g_app_coordinator.tick(now_ms);
}
