// main.cpp - Freenove ESP32-S3 all-in-one runtime loop.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "audio_manager.h"
#include "button_manager.h"
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
  char ap_default_ssid[33] = "Les cils";
  char ap_default_password[65] = "mascarade";
  bool force_ap_if_not_local = false;
  uint32_t local_retry_ms = kDefaultLocalRetryMs;
  bool espnow_enabled_on_boot = true;
  bool espnow_bridge_to_story_event = true;
  uint8_t espnow_boot_peer_count = 0U;
  char espnow_boot_peers[kMaxEspNowBootPeers][18] = {};
};

AudioManager g_audio;
ScenarioManager g_scenario;
UiManager g_ui;
StorageManager g_storage;
ButtonManager g_buttons;
TouchManager g_touch;
NetworkManager g_network;
RuntimeNetworkConfig g_network_cfg;
WebServer g_web_server(80);
bool g_web_started = false;
bool g_web_disconnect_sta_pending = false;
uint32_t g_web_disconnect_sta_at_ms = 0U;
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
  copyText(g_network_cfg.ap_default_ssid, sizeof(g_network_cfg.ap_default_ssid), kDefaultWifiTestSsid);
  copyText(g_network_cfg.ap_default_password, sizeof(g_network_cfg.ap_default_password), kDefaultWifiTestPassword);
  g_network_cfg.force_ap_if_not_local = false;
  g_network_cfg.local_retry_ms = kDefaultLocalRetryMs;
  g_network_cfg.espnow_enabled_on_boot = true;
  g_network_cfg.espnow_bridge_to_story_event = true;
  clearEspNowBootPeers();
}

void loadRuntimeNetworkConfig() {
  resetRuntimeNetworkConfig();

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

  Serial.printf("[NET] cfg host=%s local=%s wifi_test=%s ap_default=%s ap_policy=%u retry_ms=%lu espnow_boot=%u bridge_story=%u peers=%u\n",
                g_network_cfg.hostname,
                g_network_cfg.local_ssid,
                g_network_cfg.wifi_test_ssid,
                g_network_cfg.ap_default_ssid,
                g_network_cfg.force_ap_if_not_local ? 1U : 0U,
                static_cast<unsigned long>(g_network_cfg.local_retry_ms),
                g_network_cfg.espnow_enabled_on_boot ? 1U : 0U,
                g_network_cfg.espnow_bridge_to_story_event ? 1U : 0U,
                g_network_cfg.espnow_boot_peer_count);
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
                "ap_ssid=%s local_target=%s local_match=%u rssi=%ld peers=%u rx=%lu tx_ok=%lu tx_fail=%lu drop=%lu\n",
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
                net.local_target[0] != '\0' ? net.local_target : "n/a",
                net.local_match ? 1U : 0U,
                static_cast<long>(net.rssi),
                net.espnow_peer_count,
                static_cast<unsigned long>(net.espnow_rx_packets),
                static_cast<unsigned long>(net.espnow_tx_ok),
                static_cast<unsigned long>(net.espnow_tx_fail),
                static_cast<unsigned long>(net.espnow_drop_packets));
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
  const char* scenario_id = scenarioIdFromSnapshot(snapshot);
  const char* step_id = stepIdFromSnapshot(snapshot);
  const char* screen_id = (snapshot.screen_scene_id != nullptr) ? snapshot.screen_scene_id : "n/a";
  const char* audio_pack = (snapshot.audio_pack_id != nullptr) ? snapshot.audio_pack_id : "n/a";
  Serial.printf("STATUS scenario=%s step=%s screen=%s pack=%s audio=%u track=%s profile=%u:%s vol=%u "
                "net=%s/%s sta=%u connecting=%u ap=%u espnow=%u peers=%u ip=%s key=%u mv=%d\n",
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
                g_buttons.lastAnalogMilliVolts());
}

bool dispatchScenarioEventByType(StoryEventType type, const char* event_name, uint32_t now_ms);
bool dispatchScenarioEventByName(const char* event_name, uint32_t now_ms);

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
    async function post(path, params) {
      const body = new URLSearchParams(params || {});
      await fetch(path, { method: "POST", body });
      await refreshStatus();
    }
    async function refreshStatus() {
      const res = await fetch("/api/status");
      const json = await res.json();
      document.getElementById("status").textContent = JSON.stringify(json, null, 2);
    }
    function unlock() { return post("/api/scenario/unlock"); }
    function nextStep() { return post("/api/scenario/next"); }
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
    setInterval(refreshStatus, 3000);
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
  out["last_rx_mac"] = net.last_rx_peer;
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
  out["ssid"] = net.sta_ssid;
  out["ip"] = net.sta_connected ? net.ip : "";
  out["rssi"] = net.rssi;
  out["state"] = net.state;
  out["ap_active"] = net.ap_enabled;
  out["ap_ssid"] = net.ap_ssid;
  out["ap_ip"] = (!net.sta_connected && net.ap_enabled) ? net.ip : "";
  out["mode"] = net.mode;
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

bool webDispatchAction(const String& action_raw) {
  String action = action_raw;
  action.trim();
  if (action.isEmpty()) {
    return false;
  }

  if (action.equalsIgnoreCase("UNLOCK")) {
    g_scenario.notifyUnlock(millis());
    return true;
  }
  if (action.equalsIgnoreCase("NEXT")) {
    g_scenario.notifyButton(5U, false, millis());
    return true;
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
    return dispatchScenarioEventByName(event_name.c_str(), millis());
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
    return dispatchScenarioEventByType(event_type, event_name.isEmpty() ? nullptr : event_name.c_str(), millis());
  }

  return false;
}

void webSendStatus() {
  const NetworkManager::Snapshot net = g_network.snapshot();
  const ScenarioSnapshot scenario = g_scenario.snapshot();

  StaticJsonDocument<1536> document;
  JsonObject network = document["network"].to<JsonObject>();
  network["state"] = net.state;
  network["mode"] = net.mode;
  network["sta_connected"] = net.sta_connected;
  network["sta_connecting"] = net.sta_connecting;
  network["fallback_ap"] = net.fallback_ap_active;
  network["sta_ssid"] = net.sta_ssid;
  network["ap_ssid"] = net.ap_ssid;
  network["local_target"] = net.local_target;
  network["local_match"] = net.local_match;
  network["ip"] = net.ip;
  network["rssi"] = net.rssi;

  JsonObject wifi = document["wifi"].to<JsonObject>();
  webFillWifiStatus(wifi, net);

  JsonObject espnow = document["espnow"].to<JsonObject>();
  webFillEspNowStatus(espnow, net);

  JsonObject story = document["story"].to<JsonObject>();
  story["scenario"] = scenarioIdFromSnapshot(scenario);
  story["step"] = stepIdFromSnapshot(scenario);
  story["screen"] = (scenario.screen_scene_id != nullptr) ? scenario.screen_scene_id : "";
  story["audio_pack"] = (scenario.audio_pack_id != nullptr) ? scenario.audio_pack_id : "";

  JsonObject audio = document["audio"].to<JsonObject>();
  audio["playing"] = g_audio.isPlaying();
  audio["track"] = g_audio.currentTrack();
  audio["volume"] = g_audio.volume();

  webSendJsonDocument(document);
}

void setupWebUi() {
  g_web_server.on("/", HTTP_GET, []() {
    g_web_server.send(200, "text/html", kWebUiIndex);
  });

  g_web_server.on("/api/status", HTTP_GET, []() {
    webSendStatus();
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
    const bool ok = webDispatchAction(action);
    StaticJsonDocument<256> response;
    response["ok"] = ok;
    response["action"] = action;
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
      refreshSceneIfNeeded(true);
      startPendingAudioIfAny();
    }
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
  g_storage.ensureDefaultScenarioFile(kDefaultScenarioFile);
  loadRuntimeNetworkConfig();
  Serial.printf("[MAIN] default scenario checksum=%lu\n",
                static_cast<unsigned long>(g_storage.checksum(kDefaultScenarioFile)));

  g_buttons.begin();
  g_touch.begin();
  g_network.begin(g_network_cfg.hostname);
  g_network.configureFallbackAp(g_network_cfg.ap_default_ssid, g_network_cfg.ap_default_password);
  g_network.configureLocalPolicy(g_network_cfg.local_ssid,
                                 g_network_cfg.local_password,
                                 g_network_cfg.force_ap_if_not_local,
                                 g_network_cfg.local_retry_ms);
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
  }

  TouchPoint touch;
  if (g_touch.poll(&touch)) {
    g_ui.handleTouch(touch.x, touch.y, touch.touched);
  } else {
    g_ui.handleTouch(0, 0, false);
  }

  g_network.update(now_ms);
  char net_payload[128] = {0};
  char net_peer[18] = {0};
  while (g_network.consumeEspNowMessage(net_payload, sizeof(net_payload), net_peer, sizeof(net_peer))) {
    if (!g_network_cfg.espnow_bridge_to_story_event) {
      Serial.printf("[NET] ESPNOW peer=%s payload=%s bridge=off\n",
                    net_peer[0] != '\0' ? net_peer : "n/a",
                    net_payload);
      continue;
    }
    char event_token[kSerialLineCapacity] = {0};
    if (!normalizeEspNowPayloadToScenarioEvent(net_payload, event_token, sizeof(event_token))) {
      Serial.printf("[NET] ESPNOW peer=%s payload=%s ignored=unsupported\n",
                    net_peer[0] != '\0' ? net_peer : "n/a",
                    net_payload);
      continue;
    }
    const ScenarioSnapshot before = g_scenario.snapshot();
    const bool dispatched = dispatchScenarioEventByName(event_token, now_ms);
    const ScenarioSnapshot after = g_scenario.snapshot();
    const bool changed = std::strcmp(stepIdFromSnapshot(before), stepIdFromSnapshot(after)) != 0;
    Serial.printf("[NET] ESPNOW peer=%s payload=%s event=%s dispatched=%u changed=%u step=%s\n",
                  net_peer[0] != '\0' ? net_peer : "n/a",
                  net_payload,
                  event_token,
                  dispatched ? 1U : 0U,
                  changed ? 1U : 0U,
                  stepIdFromSnapshot(after));
  }

  g_audio.update();
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
