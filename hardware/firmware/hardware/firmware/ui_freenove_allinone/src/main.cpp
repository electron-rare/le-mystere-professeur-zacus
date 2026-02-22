// main.cpp - Freenove ESP32-S3 all-in-one runtime loop.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio_manager.h"
#include "button_manager.h"
#include "camera_manager.h"
#include "hardware_manager.h"
#include "media_manager.h"
#include "network_manager.h"
#include "scenario_manager.h"
#include "scenarios/default_scenario_v2.h"
#include "storage_manager.h"
#include "touch_manager.h"
#include "ui_manager.h"

namespace {

constexpr const char* kDefaultScenarioFile = "/story/scenarios/DEFAULT.json";
constexpr const char* kDiagAudioFile = "/music/boot_radio.mp3";
constexpr const char* kDefaultWifiHostname = "zacus-freenove";
constexpr const char* kDefaultWifiTestSsid = "Les cils";
constexpr const char* kDefaultWifiTestPassword = "mascarade";
constexpr uint32_t kDefaultLocalRetryMs = 15000U;
constexpr size_t kSerialLineCapacity = 192U;
constexpr uint8_t kMaxEspNowBootPeers = 10U;
constexpr bool kBootDiagnosticTone = true;

struct RuntimeNetworkConfig {
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
  bool battery_enabled = true;
  uint8_t battery_low_pct = 20U;
  char battery_low_event_name[32] = "SERIAL:BATTERY_LOW";
};

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
bool g_mic_event_armed = true;
bool g_battery_low_latched = false;
char g_last_action_step_key[72] = {0};
char g_serial_line[kSerialLineCapacity] = {0};
size_t g_serial_line_len = 0U;

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
  return "/music/placeholder.mp3";
}

const char* scenarioIdFromSnapshot(const ScenarioSnapshot& snapshot) {
  return (snapshot.scenario != nullptr && snapshot.scenario->id != nullptr) ? snapshot.scenario->id : "n/a";
}

const char* stepIdFromSnapshot(const ScenarioSnapshot& snapshot) {
  return (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
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

void clearEspNowBootPeers() {
  g_network_cfg.espnow_boot_peer_count = 0U;
  for (uint8_t index = 0U; index < kMaxEspNowBootPeers; ++index) {
    g_network_cfg.espnow_boot_peers[index][0] = '\0';
  }
}

void addEspNowBootPeer(const char* mac_text) {
  if (mac_text == nullptr || mac_text[0] == '\0' || g_network_cfg.espnow_boot_peer_count >= kMaxEspNowBootPeers) {
    return;
  }

  for (uint8_t index = 0U; index < g_network_cfg.espnow_boot_peer_count; ++index) {
    if (std::strcmp(g_network_cfg.espnow_boot_peers[index], mac_text) == 0) {
      return;
    }
  }

  copyText(g_network_cfg.espnow_boot_peers[g_network_cfg.espnow_boot_peer_count],
           sizeof(g_network_cfg.espnow_boot_peers[g_network_cfg.espnow_boot_peer_count]),
           mac_text);
  ++g_network_cfg.espnow_boot_peer_count;
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
    case StoryEventType::kAction:
      return "ACTION_FORCE_ETAPE2";
    default:
      return "";
  }
}

void resetRuntimeNetworkConfig() {
  copyText(g_network_cfg.hostname, sizeof(g_network_cfg.hostname), kDefaultWifiHostname);
  copyText(g_network_cfg.wifi_test_ssid, sizeof(g_network_cfg.wifi_test_ssid), kDefaultWifiTestSsid);
  copyText(g_network_cfg.wifi_test_password, sizeof(g_network_cfg.wifi_test_password), kDefaultWifiTestPassword);
  copyText(g_network_cfg.local_ssid, sizeof(g_network_cfg.local_ssid), kDefaultWifiTestSsid);
  copyText(g_network_cfg.local_password, sizeof(g_network_cfg.local_password), kDefaultWifiTestPassword);
  copyText(g_network_cfg.ap_default_ssid, sizeof(g_network_cfg.ap_default_ssid), "Freenove-Setup");
  copyText(g_network_cfg.ap_default_password, sizeof(g_network_cfg.ap_default_password), kDefaultWifiTestPassword);
  g_network_cfg.force_ap_if_not_local = false;
  g_network_cfg.pause_local_retry_when_ap_client = false;
  g_network_cfg.local_retry_ms = kDefaultLocalRetryMs;
  g_network_cfg.espnow_enabled_on_boot = true;
  g_network_cfg.espnow_bridge_to_story_event = true;
  clearEspNowBootPeers();
}

void resetRuntimeHardwareConfig() {
  g_hardware_cfg = RuntimeHardwareConfig();
}

void resetRuntimeCameraConfig() {
  g_camera_cfg = CameraManager::Config();
}

void resetRuntimeMediaConfig() {
  g_media_cfg = MediaManager::Config();
}

void loadRuntimeNetworkConfig() {
  resetRuntimeNetworkConfig();
  resetRuntimeHardwareConfig();
  resetRuntimeCameraConfig();
  resetRuntimeMediaConfig();

  const String wifi_payload = g_storage.loadTextFile("/story/apps/APP_WIFI.json");
  if (!wifi_payload.isEmpty()) {
    StaticJsonDocument<768> document;
    const DeserializationError error = deserializeJson(document, wifi_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      const char* hostname = config["hostname"] | "";
      const char* local_ssid = config["local_ssid"] | config["test_ssid"] | config["ssid"] | "";
      const char* local_password = config["local_password"] | config["test_password"] | config["password"] | "";
      const char* test_ssid = config["test_ssid"] | config["ssid"] | "";
      const char* test_password = config["test_password"] | config["password"] | "";
      const char* ap_ssid = config["ap_default_ssid"] | config["ap_ssid"] | "";
      const char* ap_password = config["ap_default_password"] | config["ap_password"] | "";
      const char* ap_policy = config["ap_policy"] | "";
      const bool ap_policy_bool = config["ap_policy_force_if_not_local"] | false;
      const bool pause_retry_when_ap_client = config["pause_local_retry_when_ap_client"] | false;
      const uint32_t local_retry_ms = config["local_retry_ms"] | kDefaultLocalRetryMs;
      if (hostname[0] != '\0') {
        copyText(g_network_cfg.hostname, sizeof(g_network_cfg.hostname), hostname);
      }
      if (local_ssid[0] != '\0') {
        copyText(g_network_cfg.local_ssid, sizeof(g_network_cfg.local_ssid), local_ssid);
      }
      if (local_password[0] != '\0') {
        copyText(g_network_cfg.local_password, sizeof(g_network_cfg.local_password), local_password);
      }
      if (test_ssid[0] != '\0') {
        copyText(g_network_cfg.wifi_test_ssid, sizeof(g_network_cfg.wifi_test_ssid), test_ssid);
      }
      if (test_password[0] != '\0') {
        copyText(g_network_cfg.wifi_test_password, sizeof(g_network_cfg.wifi_test_password), test_password);
      }
      if (ap_ssid[0] != '\0') {
        copyText(g_network_cfg.ap_default_ssid, sizeof(g_network_cfg.ap_default_ssid), ap_ssid);
      }
      if (ap_password[0] != '\0') {
        copyText(g_network_cfg.ap_default_password, sizeof(g_network_cfg.ap_default_password), ap_password);
      }

      if (ap_policy[0] != '\0') {
        char policy_normalized[32] = {0};
        copyText(policy_normalized, sizeof(policy_normalized), ap_policy);
        toLowerAsciiInPlace(policy_normalized);
        if (std::strcmp(policy_normalized, "force_if_not_local") == 0) {
          g_network_cfg.force_ap_if_not_local = true;
        } else if (std::strcmp(policy_normalized, "if_no_known_wifi") == 0) {
          g_network_cfg.force_ap_if_not_local = false;
        }
      } else {
        g_network_cfg.force_ap_if_not_local = ap_policy_bool;
      }
      g_network_cfg.pause_local_retry_when_ap_client = pause_retry_when_ap_client;
      if (local_retry_ms >= 1000U) {
        g_network_cfg.local_retry_ms = local_retry_ms;
      }

      // Backward compatibility: if legacy test fields are absent, keep them aligned with local WiFi target.
      if (test_ssid[0] == '\0' && g_network_cfg.local_ssid[0] != '\0') {
        copyText(g_network_cfg.wifi_test_ssid, sizeof(g_network_cfg.wifi_test_ssid), g_network_cfg.local_ssid);
      }
      if (test_password[0] == '\0' && g_network_cfg.local_password[0] != '\0') {
        copyText(g_network_cfg.wifi_test_password, sizeof(g_network_cfg.wifi_test_password), g_network_cfg.local_password);
      }
    } else {
      Serial.printf("[NET] APP_WIFI invalid json (%s)\n", error.c_str());
    }
  }

  const String espnow_payload = g_storage.loadTextFile("/story/apps/APP_ESPNOW.json");
  if (!espnow_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, espnow_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        g_network_cfg.espnow_enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      if (config["bridge_to_story_event"].is<bool>()) {
        g_network_cfg.espnow_bridge_to_story_event = config["bridge_to_story_event"].as<bool>();
      }
      if (config["peers"].is<JsonArrayConst>()) {
        clearEspNowBootPeers();
        for (JsonVariantConst peer_variant : config["peers"].as<JsonArrayConst>()) {
          const char* peer_text = peer_variant | "";
          if (peer_text[0] == '\0') {
            continue;
          }
          addEspNowBootPeer(peer_text);
        }
      }
    } else {
      Serial.printf("[NET] APP_ESPNOW invalid json (%s)\n", error.c_str());
    }
  }

  const String hardware_payload = g_storage.loadTextFile("/story/apps/APP_HARDWARE.json");
  if (!hardware_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, hardware_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        g_hardware_cfg.enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      if (config["telemetry_period_ms"].is<unsigned int>()) {
        const uint32_t telemetry = config["telemetry_period_ms"].as<unsigned int>();
        if (telemetry >= 250U) {
          g_hardware_cfg.telemetry_period_ms = telemetry;
        }
      }
      if (config["led_auto_from_scene"].is<bool>()) {
        g_hardware_cfg.led_auto_from_scene = config["led_auto_from_scene"].as<bool>();
      }
      if (config["mic_enabled"].is<bool>()) {
        g_hardware_cfg.mic_enabled = config["mic_enabled"].as<bool>();
      }
      if (config["mic_event_threshold_pct"].is<unsigned int>()) {
        uint8_t threshold = static_cast<uint8_t>(config["mic_event_threshold_pct"].as<unsigned int>());
        if (threshold > 100U) {
          threshold = 100U;
        }
        g_hardware_cfg.mic_event_threshold_pct = threshold;
      }
      const char* mic_event_name = config["mic_event_name"] | "";
      if (mic_event_name[0] != '\0') {
        copyText(g_hardware_cfg.mic_event_name, sizeof(g_hardware_cfg.mic_event_name), mic_event_name);
      }
      if (config["battery_enabled"].is<bool>()) {
        g_hardware_cfg.battery_enabled = config["battery_enabled"].as<bool>();
      }
      if (config["battery_low_pct"].is<unsigned int>()) {
        uint8_t threshold = static_cast<uint8_t>(config["battery_low_pct"].as<unsigned int>());
        if (threshold > 100U) {
          threshold = 100U;
        }
        g_hardware_cfg.battery_low_pct = threshold;
      }
      const char* battery_event_name = config["battery_low_event_name"] | "";
      if (battery_event_name[0] != '\0') {
        copyText(g_hardware_cfg.battery_low_event_name,
                 sizeof(g_hardware_cfg.battery_low_event_name),
                 battery_event_name);
      }
    } else {
      Serial.printf("[HW] APP_HARDWARE invalid json (%s)\n", error.c_str());
    }
  }

  const String camera_payload = g_storage.loadTextFile("/story/apps/APP_CAMERA.json");
  if (!camera_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, camera_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        g_camera_cfg.enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      const char* frame_size = config["frame_size"] | "";
      if (frame_size[0] != '\0') {
        copyText(g_camera_cfg.frame_size, sizeof(g_camera_cfg.frame_size), frame_size);
      }
      if (config["jpeg_quality"].is<unsigned int>()) {
        g_camera_cfg.jpeg_quality = static_cast<uint8_t>(config["jpeg_quality"].as<unsigned int>());
      }
      if (config["fb_count"].is<unsigned int>()) {
        g_camera_cfg.fb_count = static_cast<uint8_t>(config["fb_count"].as<unsigned int>());
      }
      if (config["xclk_hz"].is<unsigned int>()) {
        g_camera_cfg.xclk_hz = config["xclk_hz"].as<unsigned int>();
      }
      const char* snapshot_dir = config["snapshot_dir"] | "";
      if (snapshot_dir[0] != '\0') {
        copyText(g_camera_cfg.snapshot_dir, sizeof(g_camera_cfg.snapshot_dir), snapshot_dir);
      }
    } else {
      Serial.printf("[CAM] APP_CAMERA invalid json (%s)\n", error.c_str());
    }
  }

  const String media_payload = g_storage.loadTextFile("/story/apps/APP_MEDIA.json");
  if (!media_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, media_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      const char* music_dir = config["music_dir"] | "";
      const char* picture_dir = config["picture_dir"] | "";
      const char* record_dir = config["record_dir"] | "";
      if (music_dir[0] != '\0') {
        copyText(g_media_cfg.music_dir, sizeof(g_media_cfg.music_dir), music_dir);
      }
      if (picture_dir[0] != '\0') {
        copyText(g_media_cfg.picture_dir, sizeof(g_media_cfg.picture_dir), picture_dir);
      }
      if (record_dir[0] != '\0') {
        copyText(g_media_cfg.record_dir, sizeof(g_media_cfg.record_dir), record_dir);
      }
      if (config["record_max_seconds"].is<unsigned int>()) {
        g_media_cfg.record_max_seconds = static_cast<uint16_t>(config["record_max_seconds"].as<unsigned int>());
      }
      if (config["auto_stop_record_on_step_change"].is<bool>()) {
        g_media_cfg.auto_stop_record_on_step_change = config["auto_stop_record_on_step_change"].as<bool>();
      }
    } else {
      Serial.printf("[MEDIA] APP_MEDIA invalid json (%s)\n", error.c_str());
    }
  }

  Serial.printf(
      "[NET] cfg host=%s local=%s wifi_test=%s ap_default=%s ap_policy=%u pause_retry_on_ap_client=%u retry_ms=%lu "
      "espnow_boot=%u bridge_story=%u peers=%u\n",
                g_network_cfg.hostname,
                g_network_cfg.local_ssid,
                g_network_cfg.wifi_test_ssid,
                g_network_cfg.ap_default_ssid,
                g_network_cfg.force_ap_if_not_local ? 1U : 0U,
                g_network_cfg.pause_local_retry_when_ap_client ? 1U : 0U,
                static_cast<unsigned long>(g_network_cfg.local_retry_ms),
                g_network_cfg.espnow_enabled_on_boot ? 1U : 0U,
                g_network_cfg.espnow_bridge_to_story_event ? 1U : 0U,
                g_network_cfg.espnow_boot_peer_count);
  Serial.printf(
      "[HW] cfg boot=%u telemetry_ms=%lu led_auto=%u mic=%u threshold=%u battery=%u low_pct=%u\n",
      g_hardware_cfg.enabled_on_boot ? 1U : 0U,
      static_cast<unsigned long>(g_hardware_cfg.telemetry_period_ms),
      g_hardware_cfg.led_auto_from_scene ? 1U : 0U,
      g_hardware_cfg.mic_enabled ? 1U : 0U,
      g_hardware_cfg.mic_event_threshold_pct,
      g_hardware_cfg.battery_enabled ? 1U : 0U,
      g_hardware_cfg.battery_low_pct);
  Serial.printf("[CAM] cfg boot=%u frame=%s quality=%u fb=%u xclk=%lu dir=%s\n",
                g_camera_cfg.enabled_on_boot ? 1U : 0U,
                g_camera_cfg.frame_size,
                static_cast<unsigned int>(g_camera_cfg.jpeg_quality),
                static_cast<unsigned int>(g_camera_cfg.fb_count),
                static_cast<unsigned long>(g_camera_cfg.xclk_hz),
                g_camera_cfg.snapshot_dir);
  Serial.printf("[MEDIA] cfg music=%s picture=%s record=%s max_sec=%u auto_stop=%u\n",
                g_media_cfg.music_dir,
                g_media_cfg.picture_dir,
                g_media_cfg.record_dir,
                static_cast<unsigned int>(g_media_cfg.record_max_seconds),
                g_media_cfg.auto_stop_record_on_step_change ? 1U : 0U);
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
      copyText(out_event, out_capacity, "UNLOCK");
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

  return normalizeEventTokenFromText(normalized, out_event, out_capacity);
}

bool dispatchScenarioEventByType(StoryEventType type, const char* event_name, uint32_t now_ms);
bool dispatchScenarioEventByName(const char* event_name, uint32_t now_ms);
void refreshSceneIfNeeded(bool force_render);
void startPendingAudioIfAny();
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
bool webReconnectLocalWifi();
bool refreshStoryFromSd();
bool dispatchControlAction(const String& action_raw, uint32_t now_ms, String* out_error = nullptr);
void printHardwareStatus();
void printHardwareStatusJson();
void printCameraStatus();
void printMediaStatus();
void maybeEmitHardwareEvents(uint32_t now_ms);
void maybeLogHardwareTelemetry(uint32_t now_ms);

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
  const HardwareManager::Snapshot hardware = g_hardware.snapshot();
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
    g_scenario.notifyUnlock(now_ms);
    out_result->ok = true;
    return true;
  }
  if (command == "NEXT") {
    g_scenario.notifyButton(5U, false, now_ms);
    out_result->ok = true;
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

void onAudioFinished(const char* track, void* ctx) {
  (void)ctx;
  Serial.printf("[MAIN] audio done: %s\n", track != nullptr ? track : "unknown");
  g_scenario.notifyAudioDone(millis());
}

void printButtonRead() {
  Serial.printf("BTN mv=%d key=%u\n", g_buttons.lastAnalogMilliVolts(), g_buttons.currentKey());
}

void printRuntimeStatus() {
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const NetworkManager::Snapshot net = g_network.snapshot();
  const HardwareManager::Snapshot hw = g_hardware.snapshot();
  const CameraManager::Snapshot camera = g_camera.snapshot();
  const MediaManager::Snapshot media = g_media.snapshot();
  const char* scenario_id = scenarioIdFromSnapshot(snapshot);
  const char* step_id = stepIdFromSnapshot(snapshot);
  const char* screen_id = (snapshot.screen_scene_id != nullptr) ? snapshot.screen_scene_id : "n/a";
  const char* audio_pack = (snapshot.audio_pack_id != nullptr) ? snapshot.audio_pack_id : "n/a";
  Serial.printf("STATUS scenario=%s step=%s screen=%s pack=%s audio=%u track=%s profile=%u:%s vol=%u "
                "net=%s/%s sta=%u connecting=%u ap=%u espnow=%u peers=%u ip=%s key=%u mv=%d "
                "hw=%u mic=%u battery=%u cam=%u media_play=%u rec=%u\n",
                scenario_id,
                step_id,
                screen_id,
                audio_pack,
                g_audio.isPlaying() ? 1 : 0,
                g_audio.currentTrack(),
                g_audio.outputProfile(),
                g_audio.outputProfileLabel(g_audio.outputProfile()),
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
                media.recording ? 1U : 0U);
}

void printHardwareStatus() {
  const HardwareManager::Snapshot hw = g_hardware.snapshot();
  Serial.printf(
      "HW_STATUS ready=%u ws2812=%u mic=%u battery=%u auto=%u manual=%u led=%u,%u,%u br=%u "
      "mic_pct=%u mic_peak=%u battery_pct=%u battery_mv=%u charging=%u scene=%s\n",
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
      hw.battery_percent,
      hw.battery_cell_mv,
      hw.charging ? 1U : 0U,
      hw.scene_id);
}

void printHardwareStatusJson() {
  StaticJsonDocument<768> document;
  webFillHardwareStatus(document.to<JsonObject>());
  serializeJson(document, Serial);
  Serial.println();
}

void printCameraStatus() {
  const CameraManager::Snapshot camera = g_camera.snapshot();
  Serial.printf("CAM_STATUS supported=%u enabled=%u init=%u frame=%s quality=%u fb=%u xclk=%lu captures=%lu fails=%lu last=%s err=%s\n",
                camera.supported ? 1U : 0U,
                camera.enabled ? 1U : 0U,
                camera.initialized ? 1U : 0U,
                camera.frame_size,
                static_cast<unsigned int>(camera.jpeg_quality),
                static_cast<unsigned int>(camera.fb_count),
                static_cast<unsigned long>(camera.xclk_hz),
                static_cast<unsigned long>(camera.capture_count),
                static_cast<unsigned long>(camera.fail_count),
                camera.last_file[0] != '\0' ? camera.last_file : "n/a",
                camera.last_error[0] != '\0' ? camera.last_error : "none");
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
    <input id="target" placeholder="ESP-NOW target (mac|broadcast)" />
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
    async function post(path, params) {
      const body = new URLSearchParams(params || {});
      await fetch(path, { method: "POST", body });
      await refreshStatus();
    }
    async function refreshStatus() {
      const res = await fetch("/api/status");
      const json = await res.json();
      showStatus(json);
    }
    function connectStream() {
      if (typeof EventSource === "undefined") {
        setInterval(refreshStatus, 3000);
        return;
      }
      if (stream) {
        stream.close();
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
        password: document.getElementById("pass").value
      });
    }
    function espnowOn() { return post("/api/network/espnow/on"); }
    function espnowOff() { return post("/api/network/espnow/off"); }
    function espnowSend() {
      return post("/api/espnow/send", {
        target: document.getElementById("target").value,
        payload: document.getElementById("payload").value
      });
    }
    refreshStatus();
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
  const HardwareManager::Snapshot hw = g_hardware.snapshot();
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
  StaticJsonDocument<768> document;
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
  const HardwareManager::Snapshot hw = g_hardware.snapshot();

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
}

void maybeLogHardwareTelemetry(uint32_t now_ms) {
  if (!g_hardware_started || g_hardware_cfg.telemetry_period_ms < 250U) {
    return;
  }
  if (now_ms < g_next_hw_telemetry_ms) {
    return;
  }
  g_next_hw_telemetry_ms = now_ms + g_hardware_cfg.telemetry_period_ms;
  const HardwareManager::Snapshot hw = g_hardware.snapshot();
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

bool executeStoryAction(const char* action_id, const ScenarioSnapshot& snapshot, uint32_t now_ms) {
  if (action_id == nullptr || action_id[0] == '\0') {
    return false;
  }
  const String action_path = String("/story/actions/") + action_id + ".json";
  const String payload = g_storage.loadTextFile(action_path.c_str());
  StaticJsonDocument<512> action_doc;
  if (!payload.isEmpty()) {
    deserializeJson(action_doc, payload);
  }
  const char* action_type = action_doc["type"] | "";

  if (std::strcmp(action_id, "ACTION_TRACE_STEP") == 0 || std::strcmp(action_type, "trace_step") == 0) {
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
    String out_path;
    const bool ok = g_camera.snapshotToFile(filename[0] != '\0' ? filename : nullptr, &out_path);
    Serial.printf("[ACTION] CAMERA_SNAPSHOT ok=%u path=%s\n", ok ? 1U : 0U, ok ? out_path.c_str() : "n/a");
    if (ok) {
      dispatchScenarioEventByName(event_name, now_ms);
    }
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

  return false;
}

void executeStoryActionsForStep(const ScenarioSnapshot& snapshot, uint32_t now_ms) {
  if (snapshot.step == nullptr) {
    return;
  }
  if (snapshot.action_ids == nullptr || snapshot.action_count == 0U) {
    return;
  }

  char step_key[sizeof(g_last_action_step_key)] = {0};
  snprintf(step_key,
           sizeof(step_key),
           "%s:%s",
           scenarioIdFromSnapshot(snapshot),
           stepIdFromSnapshot(snapshot));
  if (std::strcmp(step_key, g_last_action_step_key) == 0) {
    return;
  }
  copyText(g_last_action_step_key, sizeof(g_last_action_step_key), step_key);

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

bool dispatchControlAction(const String& action_raw, uint32_t now_ms, String* out_error) {
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

  if (action.equalsIgnoreCase("UNLOCK")) {
    g_scenario.notifyUnlock(now_ms);
    return true;
  }
  if (action.equalsIgnoreCase("NEXT")) {
    g_scenario.notifyButton(5U, false, now_ms);
    return true;
  }
  if (action.equalsIgnoreCase("STORY_REFRESH_SD")) {
    return refreshStoryFromSd();
  }
  if (action.equalsIgnoreCase("WIFI_DISCONNECT")) {
    webScheduleStaDisconnect();
    return true;
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
  if (action.equalsIgnoreCase("CAM_ON")) {
    return g_camera.start();
  }
  if (action.equalsIgnoreCase("CAM_OFF")) {
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

  if (startsWithIgnoreCase(action.c_str(), "WIFI_CONNECT ")) {
    String ssid;
    String password;
    if (!splitSsidPass(action.c_str() + std::strlen("WIFI_CONNECT "), &ssid, &password)) {
      return false;
    }
    return g_network.connectSta(ssid.c_str(), password.c_str());
  }

  if (startsWithIgnoreCase(action.c_str(), "ESPNOW_SEND ")) {
    String args = action.substring(static_cast<unsigned int>(std::strlen("ESPNOW_SEND ")));
    args.trim();
    const int sep = args.indexOf(' ');
    if (sep <= 0) {
      return false;
    }
    String target = args.substring(0, static_cast<unsigned int>(sep));
    String payload = args.substring(static_cast<unsigned int>(sep + 1));
    target.trim();
    payload.trim();
    if (target.isEmpty() || payload.isEmpty()) {
      return false;
    }
    return g_network.sendEspNowTarget(target.c_str(), payload.c_str());
  }

  if (startsWithIgnoreCase(action.c_str(), "SC_EVENT_RAW ")) {
    String event_name = action.substring(static_cast<unsigned int>(std::strlen("SC_EVENT_RAW ")));
    event_name.trim();
    if (event_name.isEmpty()) {
      return false;
    }
    return dispatchScenarioEventByName(event_name.c_str(), now_ms);
  }

  if (startsWithIgnoreCase(action.c_str(), "SC_EVENT ")) {
    String args = action.substring(static_cast<unsigned int>(std::strlen("SC_EVENT ")));
    args.trim();
    if (args.isEmpty()) {
      return false;
    }
    const int sep = args.indexOf(' ');
    String type_text = (sep < 0) ? args : args.substring(0, static_cast<unsigned int>(sep));
    String event_name = (sep < 0) ? String("") : args.substring(static_cast<unsigned int>(sep + 1));
    type_text.trim();
    event_name.trim();
    StoryEventType event_type = StoryEventType::kNone;
    if (!parseEventType(type_text.c_str(), &event_type)) {
      return false;
    }
    return dispatchScenarioEventByType(event_type, event_name.isEmpty() ? nullptr : event_name.c_str(), now_ms);
  }

  if (startsWithIgnoreCase(action.c_str(), "HW_LED_SET ")) {
    String args = action.substring(static_cast<unsigned int>(std::strlen("HW_LED_SET ")));
    args.trim();
    int r = 0;
    int g = 0;
    int b = 0;
    int brightness = FREENOVE_WS2812_BRIGHTNESS;
    int pulse = 1;
    const int count = std::sscanf(args.c_str(), "%d %d %d %d %d", &r, &g, &b, &brightness, &pulse);
    if (count < 3) {
      if (out_error != nullptr) {
        *out_error = "hw_led_set_args";
      }
      return false;
    }
    if (brightness < 0) {
      brightness = 0;
    } else if (brightness > 255) {
      brightness = 255;
    }
    if (r < 0) {
      r = 0;
    } else if (r > 255) {
      r = 255;
    }
    if (g < 0) {
      g = 0;
    } else if (g > 255) {
      g = 255;
    }
    if (b < 0) {
      b = 0;
    } else if (b > 255) {
      b = 255;
    }
    return g_hardware.setManualLed(static_cast<uint8_t>(r),
                                   static_cast<uint8_t>(g),
                                   static_cast<uint8_t>(b),
                                   static_cast<uint8_t>(brightness),
                                   pulse != 0);
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

void webBuildStatusDocument(StaticJsonDocument<3072>* out_document) {
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
  audio["volume"] = g_audio.volume();

  JsonObject hardware = (*out_document)["hardware"].to<JsonObject>();
  webFillHardwareStatus(hardware);

  JsonObject camera = (*out_document)["camera"].to<JsonObject>();
  webFillCameraStatus(camera);

  JsonObject media = (*out_document)["media"].to<JsonObject>();
  webFillMediaStatus(media, millis());
}

void webSendStatus() {
  StaticJsonDocument<3072> document;
  webBuildStatusDocument(&document);
  webSendJsonDocument(document);
}

void webSendStatusSse() {
  StaticJsonDocument<3072> document;
  webBuildStatusDocument(&document);
  String payload;
  serializeJson(document, payload);

  g_web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_web_server.sendHeader("Cache-Control", "no-cache");
  g_web_server.sendHeader("Connection", "close");
  g_web_server.send(200, "text/event-stream", "");
  g_web_server.sendContent("event: status\n");
  g_web_server.sendContent("data: ");
  g_web_server.sendContent(payload);
  g_web_server.sendContent("\n\n");
  g_web_server.sendContent("event: done\ndata: 1\n\n");
}

void setupWebUi() {
  g_web_server.on("/", HTTP_GET, []() {
    g_web_server.send(200, "text/html", kWebUiIndex);
  });

  g_web_server.on("/api/status", HTTP_GET, []() {
    webSendStatus();
  });

  g_web_server.on("/api/stream", HTTP_GET, []() {
    webSendStatusSse();
  });

  g_web_server.on("/api/hardware", HTTP_GET, []() {
    webSendHardwareStatus();
  });

  g_web_server.on("/api/hardware/led", HTTP_POST, []() {
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

  g_web_server.on("/api/hardware/led/auto", HTTP_POST, []() {
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

  g_web_server.on("/api/camera/status", HTTP_GET, []() {
    webSendCameraStatus();
  });

  g_web_server.on("/api/camera/on", HTTP_POST, []() {
    const bool ok = g_camera.start();
    webSendResult("CAM_ON", ok);
  });

  g_web_server.on("/api/camera/off", HTTP_POST, []() {
    g_camera.stop();
    webSendResult("CAM_OFF", true);
  });

  g_web_server.on("/api/camera/snapshot.jpg", HTTP_GET, []() {
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

  g_web_server.on("/api/media/files", HTTP_GET, []() {
    webSendMediaFiles();
  });

  g_web_server.on("/api/media/play", HTTP_POST, []() {
    String path = g_web_server.arg("path");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && path.isEmpty()) {
      path = request_json["path"] | request_json["file"] | "";
    }
    const bool ok = !path.isEmpty() && g_media.play(path.c_str(), &g_audio);
    webSendResult("MEDIA_PLAY", ok);
  });

  g_web_server.on("/api/media/stop", HTTP_POST, []() {
    const bool ok = g_media.stop(&g_audio);
    webSendResult("MEDIA_STOP", ok);
  });

  g_web_server.on("/api/media/record/start", HTTP_POST, []() {
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

  g_web_server.on("/api/media/record/stop", HTTP_POST, []() {
    const bool ok = g_media.stopRecording();
    webSendResult("REC_STOP", ok);
  });

  g_web_server.on("/api/media/record/status", HTTP_GET, []() {
    webSendMediaRecordStatus();
  });

  g_web_server.on("/api/network/wifi", HTTP_GET, []() {
    webSendWifiStatus();
  });

  g_web_server.on("/api/network/espnow", HTTP_GET, []() {
    webSendEspNowStatus();
  });

  g_web_server.on("/api/network/espnow/peer", HTTP_GET, []() {
    webSendEspNowPeerList();
  });

  g_web_server.on("/api/wifi/disconnect", HTTP_POST, []() {
    webScheduleStaDisconnect();
    webSendResult("WIFI_DISCONNECT", true);
  });

  g_web_server.on("/api/network/wifi/disconnect", HTTP_POST, []() {
    webScheduleStaDisconnect();
    webSendResult("WIFI_DISCONNECT", true);
  });

  g_web_server.on("/api/network/wifi/reconnect", HTTP_POST, []() {
    const bool ok = webReconnectLocalWifi();
    webSendResult("WIFI_RECONNECT", ok);
  });

  g_web_server.on("/api/wifi/connect", HTTP_POST, []() {
    String ssid = g_web_server.arg("ssid");
    String password = g_web_server.arg("password");
    if (password.isEmpty()) {
      password = g_web_server.arg("pass");
    }
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (ssid.isEmpty()) {
        ssid = request_json["ssid"] | "";
      }
      if (password.isEmpty()) {
        password = request_json["pass"] | request_json["password"] | "";
      }
    }
    if (ssid.isEmpty()) {
      webSendResult("WIFI_CONNECT", false);
      return;
    }
    const bool ok = g_network.connectSta(ssid.c_str(), password.c_str());
    webSendResult("WIFI_CONNECT", ok);
  });

  g_web_server.on("/api/network/wifi/connect", HTTP_POST, []() {
    String ssid = g_web_server.arg("ssid");
    String password = g_web_server.arg("password");
    if (password.isEmpty()) {
      password = g_web_server.arg("pass");
    }
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (ssid.isEmpty()) {
        ssid = request_json["ssid"] | "";
      }
      if (password.isEmpty()) {
        password = request_json["pass"] | request_json["password"] | "";
      }
    }
    if (ssid.isEmpty()) {
      webSendResult("WIFI_CONNECT", false);
      return;
    }
    const bool ok = g_network.connectSta(ssid.c_str(), password.c_str());
    webSendResult("WIFI_CONNECT", ok);
  });

  g_web_server.on("/api/espnow/send", HTTP_POST, []() {
    String target = g_web_server.arg("target");
    String payload = g_web_server.arg("payload");
    if (target.isEmpty()) {
      target = g_web_server.arg("mac");
    }
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (target.isEmpty()) {
        target = request_json["target"] | request_json["mac"] | "broadcast";
      }
      if (payload.isEmpty()) {
        if (request_json["payload"].is<JsonVariantConst>()) {
          serializeJson(request_json["payload"], payload);
        } else {
          payload = request_json["payload"] | "";
        }
      }
    }
    if (target.isEmpty()) {
      target = "broadcast";
    }
    if (payload.isEmpty()) {
      webSendResult("ESPNOW_SEND", false);
      return;
    }
    const bool ok = g_network.sendEspNowTarget(target.c_str(), payload.c_str());
    webSendResult("ESPNOW_SEND", ok);
  });

  g_web_server.on("/api/network/espnow/send", HTTP_POST, []() {
    String target = g_web_server.arg("target");
    String payload = g_web_server.arg("payload");
    if (target.isEmpty()) {
      target = g_web_server.arg("mac");
    }
    StaticJsonDocument<768> request_json;
    if (webParseJsonBody(&request_json)) {
      if (target.isEmpty()) {
        target = request_json["target"] | request_json["mac"] | "broadcast";
      }
      if (payload.isEmpty()) {
        if (request_json["payload"].is<JsonVariantConst>()) {
          serializeJson(request_json["payload"], payload);
        } else {
          payload = request_json["payload"] | "";
        }
      }
    }
    if (target.isEmpty()) {
      target = "broadcast";
    }
    if (payload.isEmpty()) {
      webSendResult("ESPNOW_SEND", false);
      return;
    }
    const bool ok = g_network.sendEspNowTarget(target.c_str(), payload.c_str());
    webSendResult("ESPNOW_SEND", ok);
  });

  g_web_server.on("/api/network/espnow/on", HTTP_POST, []() {
    const bool ok = g_network.enableEspNow();
    webSendResult("ESPNOW_ON", ok);
  });

  g_web_server.on("/api/network/espnow/off", HTTP_POST, []() {
    g_network.disableEspNow();
    webSendResult("ESPNOW_OFF", true);
  });

  g_web_server.on("/api/network/espnow/peer", HTTP_POST, []() {
    String mac = g_web_server.arg("mac");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && mac.isEmpty()) {
      mac = request_json["mac"] | "";
    }
    const bool ok = !mac.isEmpty() && g_network.addEspNowPeer(mac.c_str());
    webSendResult("ESPNOW_PEER_ADD", ok);
  });

  g_web_server.on("/api/network/espnow/peer", HTTP_DELETE, []() {
    String mac = g_web_server.arg("mac");
    StaticJsonDocument<256> request_json;
    if (webParseJsonBody(&request_json) && mac.isEmpty()) {
      mac = request_json["mac"] | "";
    }
    const bool ok = !mac.isEmpty() && g_network.removeEspNowPeer(mac.c_str());
    webSendResult("ESPNOW_PEER_DEL", ok);
  });

  g_web_server.on("/api/story/refresh-sd", HTTP_POST, []() {
    const bool ok = refreshStoryFromSd();
    webSendResult("STORY_REFRESH_SD", ok);
  });

  g_web_server.on("/api/scenario/unlock", HTTP_POST, []() {
    g_scenario.notifyUnlock(millis());
    webSendResult("UNLOCK", true);
  });

  g_web_server.on("/api/scenario/next", HTTP_POST, []() {
    g_scenario.notifyButton(5U, false, millis());
    webSendResult("NEXT", true);
  });

  g_web_server.on("/api/control", HTTP_POST, []() {
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
  const uint8_t action = (mask & (1UL << static_cast<uint8_t>(StoryEventType::kAction))) ? 1U : 0U;
  Serial.printf("SC_COVERAGE scenario=%s unlock=%u audio_done=%u timer=%u serial=%u action=%u\n",
                scenarioIdFromSnapshot(snapshot),
                unlock,
                audio_done,
                timer,
                serial,
                action);
}

bool dispatchScenarioEventByType(StoryEventType type, const char* event_name, uint32_t now_ms) {
  switch (type) {
    case StoryEventType::kUnlock:
      if (event_name != nullptr && event_name[0] != '\0' && std::strcmp(event_name, "UNLOCK") != 0) {
        return false;
      }
      g_scenario.notifyUnlock(now_ms);
      return true;
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
    if (head_len == 6U && std::strncmp(normalized, "ACTION", 6U) == 0) {
      return g_scenario.notifyActionEvent(tail, now_ms);
    }
    if (head_len == 6U && std::strncmp(normalized, "SERIAL", 6U) == 0) {
      return g_scenario.notifySerialEvent(tail, now_ms);
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
      {StoryEventType::kAction, "ACTION_FORCE_ETAPE2"},
  };
  const HardwareProbe hardware_probes[] = {
      {1U, false, "BTN1_SHORT"},
      {3U, true, "BTN3_LONG"},
      {4U, true, "BTN4_LONG"},
      {5U, false, "BTN5_SHORT"},
      {5U, true, "BTN5_LONG"},
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

  {
    const ScenarioSnapshot before = prepareStepXProbe();
    g_scenario.notifyButton(5U, false, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("SC_REVALIDATE_STEPX event=button label=BTN5_SHORT changed=%u anchor_step=%s step_after=%s\n",
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

void refreshSceneIfNeeded(bool force_render) {
  const bool changed = g_scenario.consumeSceneChanged();
  if (!force_render && !changed) {
    return;
  }

  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const uint32_t now_ms = millis();
  if (g_hardware_started && g_hardware_cfg.led_auto_from_scene && snapshot.screen_scene_id != nullptr) {
    g_hardware.setSceneHint(snapshot.screen_scene_id);
  }
  executeStoryActionsForStep(snapshot, now_ms);

  const char* step_id = (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
  const String screen_payload = g_storage.loadScenePayloadById(snapshot.screen_scene_id);
  Serial.printf("[UI] render step=%s screen=%s pack=%s playing=%u\n",
                step_id,
                snapshot.screen_scene_id != nullptr ? snapshot.screen_scene_id : "n/a",
                snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a",
                g_audio.isPlaying() ? 1U : 0U);
  g_ui.renderScene(snapshot.scenario,
                   snapshot.screen_scene_id,
                   step_id,
                   snapshot.audio_pack_id,
                   g_audio.isPlaying(),
                   screen_payload.isEmpty() ? nullptr : screen_payload.c_str());
}

void startPendingAudioIfAny() {
  String audio_pack;
  if (!g_scenario.consumeAudioRequest(&audio_pack)) {
    return;
  }

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
    return;
  }
  if (mapped_path != nullptr && g_audio.play(mapped_path)) {
    Serial.printf("[MAIN] audio pack=%s path=%s source=pack_map\n", audio_pack.c_str(), mapped_path);
    return;
  }
  if (g_audio.play(kDiagAudioFile)) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=%s\n", audio_pack.c_str(), kDiagAudioFile);
    return;
  }
  if (g_audio.playDiagnosticTone()) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=builtin_tone\n", audio_pack.c_str());
    return;
  }

  // If audio cannot start (missing/invalid file), unblock scenario transitions.
  Serial.printf("[MAIN] audio fallback failed for pack=%s\n", audio_pack.c_str());
  g_scenario.notifyAudioDone(millis());
}

void handleSerialCommand(const char* command_line, uint32_t now_ms) {
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
        "SC_LIST SC_LOAD <id> SC_COVERAGE SC_REVALIDATE SC_REVALIDATE_ALL SC_EVENT <type> [name] SC_EVENT_RAW <name> "
        "STORY_REFRESH_SD STORY_SD_STATUS "
        "HW_STATUS HW_STATUS_JSON HW_LED_SET <r> <g> <b> [brightness] [pulse] HW_LED_AUTO <ON|OFF> HW_MIC_STATUS HW_BAT_STATUS "
        "CAM_STATUS CAM_ON CAM_OFF CAM_SNAPSHOT [filename] "
        "MEDIA_LIST <picture|music|recorder> MEDIA_PLAY <path> MEDIA_STOP REC_START [seconds] [filename] REC_STOP REC_STATUS "
        "NET_STATUS WIFI_STATUS WIFI_TEST WIFI_STA <ssid> <pass> WIFI_CONNECT <ssid> <pass> WIFI_DISCONNECT "
        "WIFI_AP_ON [ssid] [pass] WIFI_AP_OFF "
        "ESPNOW_ON ESPNOW_OFF ESPNOW_STATUS ESPNOW_STATUS_JSON ESPNOW_PEER_ADD <mac> ESPNOW_PEER_DEL <mac> ESPNOW_PEER_LIST "
        "ESPNOW_SEND <mac|broadcast> <text|json> "
        "AUDIO_TEST AUDIO_TEST_FS AUDIO_PROFILE <idx> AUDIO_STATUS VOL <0..21> AUDIO_STOP STOP");
    return;
  }
  if (std::strcmp(command, "STATUS") == 0) {
    printRuntimeStatus();
    return;
  }
  if (std::strcmp(command, "BTN_READ") == 0) {
    printButtonRead();
    return;
  }
  if (std::strcmp(command, "NEXT") == 0) {
    g_scenario.notifyButton(5, false, now_ms);
    Serial.println("ACK NEXT");
    return;
  }
  if (std::strcmp(command, "UNLOCK") == 0) {
    g_scenario.notifyUnlock(now_ms);
    Serial.println("ACK UNLOCK");
    return;
  }
  if (std::strcmp(command, "RESET") == 0) {
    g_scenario.reset();
    g_last_action_step_key[0] = '\0';
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
    const bool ok = g_scenario.beginById(scenario_id);
    Serial.printf("ACK SC_LOAD id=%s ok=%u\n", scenario_id, ok ? 1U : 0U);
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
  if (std::strcmp(command, "CAM_STATUS") == 0) {
    printCameraStatus();
    return;
  }
  if (std::strcmp(command, "REC_STATUS") == 0) {
    printMediaStatus();
    return;
  }
  if (std::strcmp(command, "HW_LED_SET") == 0 || std::strcmp(command, "HW_LED_AUTO") == 0 ||
      std::strcmp(command, "CAM_ON") == 0 || std::strcmp(command, "CAM_OFF") == 0 ||
      std::strcmp(command, "CAM_SNAPSHOT") == 0 || std::strcmp(command, "MEDIA_LIST") == 0 ||
      std::strcmp(command, "MEDIA_PLAY") == 0 || std::strcmp(command, "MEDIA_STOP") == 0 ||
      std::strcmp(command, "REC_START") == 0 || std::strcmp(command, "REC_STOP") == 0) {
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
  if (std::strcmp(command, "ESPNOW_STATUS_JSON") == 0) {
    printEspNowStatusJson();
    return;
  }
  if (std::strcmp(command, "WIFI_TEST") == 0) {
    const bool ok = g_network.connectSta(g_network_cfg.wifi_test_ssid, g_network_cfg.wifi_test_password);
    Serial.printf("ACK WIFI_TEST ssid=%s ok=%u\n", g_network_cfg.wifi_test_ssid, ok ? 1U : 0U);
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
    char args[kSerialLineCapacity] = {0};
    std::strncpy(args, argument, sizeof(args) - 1U);
    char* target = args;
    char* payload = nullptr;
    for (size_t index = 0U; index < sizeof(args) && args[index] != '\0'; ++index) {
      if (args[index] == ' ') {
        args[index] = '\0';
        payload = &args[index + 1U];
        break;
      }
    }
    while (payload != nullptr && *payload == ' ') {
      ++payload;
    }
    if (target[0] == '\0' || payload == nullptr || payload[0] == '\0') {
      Serial.println("ERR ESPNOW_SEND_ARG");
      return;
    }
    const bool ok = g_network.sendEspNowTarget(target, payload);
    Serial.printf("ACK ESPNOW_SEND target=%s ok=%u\n", target, ok ? 1U : 0U);
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
    Serial.printf("AUDIO_STATUS playing=%u track=%s profile=%u:%s vol=%u\n",
                  g_audio.isPlaying() ? 1U : 0U,
                  g_audio.currentTrack(),
                  g_audio.outputProfile(),
                  g_audio.outputProfileLabel(g_audio.outputProfile()),
                  g_audio.volume());
    return;
  }
  if (std::strcmp(command, "VOL") == 0) {
    if (argument == nullptr) {
      Serial.printf("VOL %u\n", g_audio.volume());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed > 21UL) {
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
      handleSerialCommand(g_serial_line, now_ms);
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
  Serial.println("[MAIN] Freenove all-in-one boot");

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
  loadRuntimeNetworkConfig();
  Serial.printf("[MAIN] default scenario checksum=%lu\n",
                static_cast<unsigned long>(g_storage.checksum(kDefaultScenarioFile)));
  Serial.printf("[MAIN] story storage sd=%u\n", g_storage.hasSdCard() ? 1U : 0U);

  g_media.begin(g_media_cfg);
  g_camera.begin(g_camera_cfg);
  if (g_camera_cfg.enabled_on_boot) {
    const bool cam_ok = g_camera.start();
    Serial.printf("[CAM] boot start=%u\n", cam_ok ? 1U : 0U);
  }
  if (g_hardware_cfg.enabled_on_boot) {
    g_hardware_started = g_hardware.begin();
    g_next_hw_telemetry_ms = millis() + g_hardware_cfg.telemetry_period_ms;
    g_mic_event_armed = true;
    g_battery_low_latched = false;
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
  g_last_action_step_key[0] = '\0';

  g_ui.begin();
  refreshSceneIfNeeded(true);
  startPendingAudioIfAny();
}

void loop() {
  const uint32_t now_ms = millis();
  pollSerialCommands(now_ms);

  ButtonEvent event;
  while (g_buttons.pollEvent(&event)) {
    Serial.printf("[MAIN] button key=%u long=%u\n", event.key, event.long_press ? 1 : 0);
    g_ui.handleButton(event.key, event.long_press);
    g_scenario.notifyButton(event.key, event.long_press, now_ms);
    if (g_hardware_started) {
      g_hardware.noteButton(event.key, event.long_press, now_ms);
    }
  }

  TouchPoint touch;
  if (g_touch.poll(&touch)) {
    g_ui.handleTouch(touch.x, touch.y, touch.touched);
  } else {
    g_ui.handleTouch(0, 0, false);
  }

  g_network.update(now_ms);
  if (g_hardware_started) {
    g_hardware.update(now_ms);
    maybeEmitHardwareEvents(now_ms);
    maybeLogHardwareTelemetry(now_ms);
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

  g_audio.update();
  g_media.update(now_ms, &g_audio);
  g_scenario.tick(now_ms);
  startPendingAudioIfAny();
  refreshSceneIfNeeded(false);
  g_ui.update();
  if (g_web_started) {
    g_web_server.handleClient();
    if (g_web_disconnect_sta_pending &&
        static_cast<int32_t>(now_ms - g_web_disconnect_sta_at_ms) >= 0) {
      g_web_disconnect_sta_pending = false;
      g_network.disconnectSta();
    }
  }
  delay(5);
}
