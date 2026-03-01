// runtime_config_service.cpp - load APP_* runtime configs from story files.
#include "runtime/runtime_config_service.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kDefaultWifiHostname = "zacus-freenove";
constexpr const char* kDefaultWifiSsid = "";
constexpr const char* kDefaultWifiPassword = "";
constexpr uint16_t kMaxLaToleranceHz = 10U;

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

void toLowerAsciiInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  for (size_t index = 0U; text[index] != '\0'; ++index) {
    text[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[index])));
  }
}

void clearEspNowBootPeers(RuntimeNetworkConfig* network_cfg) {
  if (network_cfg == nullptr) {
    return;
  }
  network_cfg->espnow_boot_peer_count = 0U;
  for (uint8_t index = 0U; index < RuntimeNetworkConfig::kMaxEspNowBootPeers; ++index) {
    network_cfg->espnow_boot_peers[index][0] = '\0';
  }
}

void addEspNowBootPeer(RuntimeNetworkConfig* network_cfg, const char* mac_text) {
  if (network_cfg == nullptr || mac_text == nullptr || mac_text[0] == '\0') {
    return;
  }
  if (network_cfg->espnow_boot_peer_count >= RuntimeNetworkConfig::kMaxEspNowBootPeers) {
    return;
  }
  copyText(network_cfg->espnow_boot_peers[network_cfg->espnow_boot_peer_count],
           sizeof(network_cfg->espnow_boot_peers[network_cfg->espnow_boot_peer_count]),
           mac_text);
  ++network_cfg->espnow_boot_peer_count;
}

void resetRuntimeNetworkConfig(RuntimeNetworkConfig* network_cfg) {
  if (network_cfg == nullptr) {
    return;
  }
  copyText(network_cfg->hostname, sizeof(network_cfg->hostname), kDefaultWifiHostname);
  copyText(network_cfg->wifi_test_ssid, sizeof(network_cfg->wifi_test_ssid), kDefaultWifiSsid);
  copyText(network_cfg->wifi_test_password, sizeof(network_cfg->wifi_test_password), kDefaultWifiPassword);
  copyText(network_cfg->local_ssid, sizeof(network_cfg->local_ssid), kDefaultWifiSsid);
  copyText(network_cfg->local_password, sizeof(network_cfg->local_password), kDefaultWifiPassword);
  copyText(network_cfg->ap_default_ssid, sizeof(network_cfg->ap_default_ssid), "Freenove-Setup");
  copyText(network_cfg->ap_default_password, sizeof(network_cfg->ap_default_password), kDefaultWifiPassword);
  network_cfg->force_ap_if_not_local = false;
  network_cfg->pause_local_retry_when_ap_client = false;
  network_cfg->local_retry_ms = RuntimeNetworkConfig::kDefaultLocalRetryMs;
  network_cfg->espnow_enabled_on_boot = true;
  network_cfg->espnow_bridge_to_story_event = true;
  clearEspNowBootPeers(network_cfg);
}

void resetRuntimeHardwareConfig(RuntimeHardwareConfig* hardware_cfg) {
  if (hardware_cfg == nullptr) {
    return;
  }
  *hardware_cfg = RuntimeHardwareConfig();
}

void resetRuntimeCameraConfig(CameraManager::Config* camera_cfg) {
  if (camera_cfg == nullptr) {
    return;
  }
  *camera_cfg = CameraManager::Config();
}

void resetRuntimeMediaConfig(MediaManager::Config* media_cfg) {
  if (media_cfg == nullptr) {
    return;
  }
  *media_cfg = MediaManager::Config();
}

}  // namespace

void RuntimeConfigService::load(StorageManager& storage,
                                RuntimeNetworkConfig* network_cfg,
                                RuntimeHardwareConfig* hardware_cfg,
                                CameraManager::Config* camera_cfg,
                                MediaManager::Config* media_cfg) {
  resetRuntimeNetworkConfig(network_cfg);
  resetRuntimeHardwareConfig(hardware_cfg);
  resetRuntimeCameraConfig(camera_cfg);
  resetRuntimeMediaConfig(media_cfg);

  if (network_cfg == nullptr || hardware_cfg == nullptr || camera_cfg == nullptr || media_cfg == nullptr) {
    return;
  }

  const String wifi_payload = storage.loadTextFile("/story/apps/APP_WIFI.json");
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
      const uint32_t local_retry_ms = config["local_retry_ms"] | RuntimeNetworkConfig::kDefaultLocalRetryMs;
      if (hostname[0] != '\0') {
        copyText(network_cfg->hostname, sizeof(network_cfg->hostname), hostname);
      }
      if (local_ssid[0] != '\0') {
        copyText(network_cfg->local_ssid, sizeof(network_cfg->local_ssid), local_ssid);
      }
      if (local_password[0] != '\0') {
        copyText(network_cfg->local_password, sizeof(network_cfg->local_password), local_password);
      }
      if (test_ssid[0] != '\0') {
        copyText(network_cfg->wifi_test_ssid, sizeof(network_cfg->wifi_test_ssid), test_ssid);
      }
      if (test_password[0] != '\0') {
        copyText(network_cfg->wifi_test_password, sizeof(network_cfg->wifi_test_password), test_password);
      }
      if (ap_ssid[0] != '\0') {
        copyText(network_cfg->ap_default_ssid, sizeof(network_cfg->ap_default_ssid), ap_ssid);
      }
      if (ap_password[0] != '\0') {
        copyText(network_cfg->ap_default_password, sizeof(network_cfg->ap_default_password), ap_password);
      }

      if (ap_policy[0] != '\0') {
        char policy_normalized[32] = {0};
        copyText(policy_normalized, sizeof(policy_normalized), ap_policy);
        toLowerAsciiInPlace(policy_normalized);
        if (std::strcmp(policy_normalized, "force_if_not_local") == 0) {
          network_cfg->force_ap_if_not_local = true;
        } else if (std::strcmp(policy_normalized, "if_no_known_wifi") == 0) {
          network_cfg->force_ap_if_not_local = false;
        }
      } else {
        network_cfg->force_ap_if_not_local = ap_policy_bool;
      }
      network_cfg->pause_local_retry_when_ap_client = pause_retry_when_ap_client;
      if (local_retry_ms >= 1000U) {
        network_cfg->local_retry_ms = local_retry_ms;
      }

      if (test_ssid[0] == '\0' && network_cfg->local_ssid[0] != '\0') {
        copyText(network_cfg->wifi_test_ssid, sizeof(network_cfg->wifi_test_ssid), network_cfg->local_ssid);
      }
      if (test_password[0] == '\0' && network_cfg->local_password[0] != '\0') {
        copyText(network_cfg->wifi_test_password,
                 sizeof(network_cfg->wifi_test_password),
                 network_cfg->local_password);
      }
    } else {
      Serial.printf("[NET] APP_WIFI invalid json (%s)\n", error.c_str());
    }
  }

  const String espnow_payload = storage.loadTextFile("/story/apps/APP_ESPNOW.json");
  if (!espnow_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, espnow_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        network_cfg->espnow_enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      if (config["bridge_to_story_event"].is<bool>()) {
        network_cfg->espnow_bridge_to_story_event = config["bridge_to_story_event"].as<bool>();
      }
      if (config["peers"].is<JsonArrayConst>()) {
        clearEspNowBootPeers(network_cfg);
        for (JsonVariantConst peer_variant : config["peers"].as<JsonArrayConst>()) {
          const char* peer_text = peer_variant | "";
          if (peer_text[0] == '\0') {
            continue;
          }
          addEspNowBootPeer(network_cfg, peer_text);
        }
      }
    } else {
      Serial.printf("[NET] APP_ESPNOW invalid json (%s)\n", error.c_str());
    }
  }

  const String hardware_payload = storage.loadTextFile("/story/apps/APP_HARDWARE.json");
  if (!hardware_payload.isEmpty()) {
    StaticJsonDocument<1024> document;
    const DeserializationError error = deserializeJson(document, hardware_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        hardware_cfg->enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      if (config["telemetry_period_ms"].is<unsigned int>()) {
        const uint32_t telemetry = config["telemetry_period_ms"].as<unsigned int>();
        if (telemetry >= 250U) {
          hardware_cfg->telemetry_period_ms = telemetry;
        }
      }
      if (config["led_auto_from_scene"].is<bool>()) {
        hardware_cfg->led_auto_from_scene = config["led_auto_from_scene"].as<bool>();
      }
      if (config["mic_enabled"].is<bool>()) {
        hardware_cfg->mic_enabled = config["mic_enabled"].as<bool>();
      }
      if (config["mic_event_threshold_pct"].is<unsigned int>()) {
        uint8_t threshold = static_cast<uint8_t>(config["mic_event_threshold_pct"].as<unsigned int>());
        if (threshold > 100U) {
          threshold = 100U;
        }
        hardware_cfg->mic_event_threshold_pct = threshold;
      }
      const char* mic_event_name = config["mic_event_name"] | "";
      if (mic_event_name[0] != '\0') {
        copyText(hardware_cfg->mic_event_name, sizeof(hardware_cfg->mic_event_name), mic_event_name);
      }
      if (config["la_trigger_enabled"].is<bool>()) {
        hardware_cfg->mic_la_trigger_enabled = config["la_trigger_enabled"].as<bool>();
      }
      if (config["la_target_hz"].is<unsigned int>()) {
        uint16_t target = static_cast<uint16_t>(config["la_target_hz"].as<unsigned int>());
        if (target < 220U) {
          target = 220U;
        } else if (target > 880U) {
          target = 880U;
        }
        hardware_cfg->mic_la_target_hz = target;
      }
      if (config["la_tolerance_hz"].is<unsigned int>()) {
        uint16_t tolerance = static_cast<uint16_t>(config["la_tolerance_hz"].as<unsigned int>());
        if (tolerance < 2U) {
          tolerance = 2U;
        } else if (tolerance > kMaxLaToleranceHz) {
          tolerance = kMaxLaToleranceHz;
        }
        hardware_cfg->mic_la_tolerance_hz = tolerance;
      }
      if (config["la_max_abs_cents"].is<unsigned int>()) {
        uint8_t max_abs_cents = static_cast<uint8_t>(config["la_max_abs_cents"].as<unsigned int>());
        if (max_abs_cents > 120U) {
          max_abs_cents = 120U;
        }
        hardware_cfg->mic_la_max_abs_cents = max_abs_cents;
      }
      if (config["la_min_confidence"].is<unsigned int>()) {
        uint8_t min_conf = static_cast<uint8_t>(config["la_min_confidence"].as<unsigned int>());
        if (min_conf > 100U) {
          min_conf = 100U;
        }
        hardware_cfg->mic_la_min_confidence = min_conf;
      }
      if (config["la_min_level_pct"].is<unsigned int>()) {
        uint8_t min_level = static_cast<uint8_t>(config["la_min_level_pct"].as<unsigned int>());
        if (min_level > 100U) {
          min_level = 100U;
        }
        hardware_cfg->mic_la_min_level_pct = min_level;
      }
      if (config["la_stable_ms"].is<unsigned int>()) {
        uint16_t stable_ms = static_cast<uint16_t>(config["la_stable_ms"].as<unsigned int>());
        if (stable_ms < 120U) {
          stable_ms = 120U;
        } else if (stable_ms > 5000U) {
          stable_ms = 5000U;
        }
        hardware_cfg->mic_la_stable_ms = stable_ms;
      }
      if (config["la_release_ms"].is<unsigned int>()) {
        uint16_t release_ms = static_cast<uint16_t>(config["la_release_ms"].as<unsigned int>());
        if (release_ms > 2000U) {
          release_ms = 2000U;
        }
        hardware_cfg->mic_la_release_ms = release_ms;
      }
      if (config["la_cooldown_ms"].is<unsigned int>()) {
        uint16_t cooldown_ms = static_cast<uint16_t>(config["la_cooldown_ms"].as<unsigned int>());
        if (cooldown_ms < 100U) {
          cooldown_ms = 100U;
        } else if (cooldown_ms > 15000U) {
          cooldown_ms = 15000U;
        }
        hardware_cfg->mic_la_cooldown_ms = cooldown_ms;
      }
      if (config["la_timeout_ms"].is<unsigned int>()) {
        uint32_t timeout_ms = config["la_timeout_ms"].as<unsigned int>();
        if (timeout_ms > 600000U) {
          timeout_ms = 600000U;
        }
        hardware_cfg->mic_la_timeout_ms = timeout_ms;
      }
      const char* la_event_name = config["la_event_name"] | "";
      if (la_event_name[0] != '\0') {
        copyText(hardware_cfg->mic_la_event_name, sizeof(hardware_cfg->mic_la_event_name), la_event_name);
      }
      if (config["battery_enabled"].is<bool>()) {
        hardware_cfg->battery_enabled = config["battery_enabled"].as<bool>();
      }
      if (config["battery_low_pct"].is<unsigned int>()) {
        uint8_t threshold = static_cast<uint8_t>(config["battery_low_pct"].as<unsigned int>());
        if (threshold > 100U) {
          threshold = 100U;
        }
        hardware_cfg->battery_low_pct = threshold;
      }
      const char* battery_event_name = config["battery_low_event_name"] | "";
      if (battery_event_name[0] != '\0') {
        copyText(
            hardware_cfg->battery_low_event_name, sizeof(hardware_cfg->battery_low_event_name), battery_event_name);
      }
    } else {
      Serial.printf("[HW] APP_HARDWARE invalid json (%s)\n", error.c_str());
    }
  }

  const String camera_payload = storage.loadTextFile("/story/apps/APP_CAMERA.json");
  if (!camera_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, camera_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      if (config["enabled_on_boot"].is<bool>()) {
        camera_cfg->enabled_on_boot = config["enabled_on_boot"].as<bool>();
      }
      const char* frame_size = config["frame_size"] | "";
      if (frame_size[0] != '\0') {
        copyText(camera_cfg->frame_size, sizeof(camera_cfg->frame_size), frame_size);
      }
      if (config["jpeg_quality"].is<unsigned int>()) {
        camera_cfg->jpeg_quality = static_cast<uint8_t>(config["jpeg_quality"].as<unsigned int>());
      }
      if (config["fb_count"].is<unsigned int>()) {
        camera_cfg->fb_count = static_cast<uint8_t>(config["fb_count"].as<unsigned int>());
      }
      if (config["xclk_hz"].is<unsigned int>()) {
        camera_cfg->xclk_hz = config["xclk_hz"].as<unsigned int>();
      }
      const char* snapshot_dir = config["snapshot_dir"] | "";
      if (snapshot_dir[0] != '\0') {
        copyText(camera_cfg->snapshot_dir, sizeof(camera_cfg->snapshot_dir), snapshot_dir);
      }
    } else {
      Serial.printf("[CAM] APP_CAMERA invalid json (%s)\n", error.c_str());
    }
  }

  const String la_payload = storage.loadTextFile("/story/apps/APP_LA.json");
  if (!la_payload.isEmpty()) {
    StaticJsonDocument<384> document;
    const DeserializationError error = deserializeJson(document, la_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      // Keep LA timeout in sync with the scene-level APP_LA contract so trigger and hourglass share one timer.
      if (config["timeout_ms"].is<unsigned int>()) {
        uint32_t timeout_ms = config["timeout_ms"].as<unsigned int>();
        if (timeout_ms > 600000U) {
          timeout_ms = 600000U;
        }
        hardware_cfg->mic_la_timeout_ms = timeout_ms;
      }
    } else {
      Serial.printf("[HW] APP_LA invalid json (%s)\n", error.c_str());
    }
  }

  const String media_payload = storage.loadTextFile("/story/apps/APP_MEDIA.json");
  if (!media_payload.isEmpty()) {
    StaticJsonDocument<512> document;
    const DeserializationError error = deserializeJson(document, media_payload);
    if (!error) {
      JsonVariantConst config = document["config"];
      const char* music_dir = config["music_dir"] | "";
      const char* picture_dir = config["picture_dir"] | "";
      const char* record_dir = config["record_dir"] | "";
      if (music_dir[0] != '\0') {
        copyText(media_cfg->music_dir, sizeof(media_cfg->music_dir), music_dir);
      }
      if (picture_dir[0] != '\0') {
        copyText(media_cfg->picture_dir, sizeof(media_cfg->picture_dir), picture_dir);
      }
      if (record_dir[0] != '\0') {
        copyText(media_cfg->record_dir, sizeof(media_cfg->record_dir), record_dir);
      }
      if (config["record_max_seconds"].is<unsigned int>()) {
        media_cfg->record_max_seconds = static_cast<uint16_t>(config["record_max_seconds"].as<unsigned int>());
      }
      if (config["auto_stop_record_on_step_change"].is<bool>()) {
        media_cfg->auto_stop_record_on_step_change = config["auto_stop_record_on_step_change"].as<bool>();
      }
    } else {
      Serial.printf("[MEDIA] APP_MEDIA invalid json (%s)\n", error.c_str());
    }
  }

  Serial.printf(
      "[NET] cfg host=%s local=%s wifi_test=%s ap_default=%s ap_policy=%u pause_retry_on_ap_client=%u retry_ms=%lu "
      "espnow_boot=%u bridge_story=%u peers=%u\n",
      network_cfg->hostname,
      network_cfg->local_ssid,
      network_cfg->wifi_test_ssid,
      network_cfg->ap_default_ssid,
      network_cfg->force_ap_if_not_local ? 1U : 0U,
      network_cfg->pause_local_retry_when_ap_client ? 1U : 0U,
      static_cast<unsigned long>(network_cfg->local_retry_ms),
      network_cfg->espnow_enabled_on_boot ? 1U : 0U,
      network_cfg->espnow_bridge_to_story_event ? 1U : 0U,
      network_cfg->espnow_boot_peer_count);
  Serial.printf(
      "[HW] cfg boot=%u telemetry_ms=%lu led_auto=%u mic=%u threshold=%u la_trigger=%u target=%u tol=%u "
      "cents=%u conf_min=%u level_min=%u stable=%ums timeout=%lums battery=%u low_pct=%u\n",
      hardware_cfg->enabled_on_boot ? 1U : 0U,
      static_cast<unsigned long>(hardware_cfg->telemetry_period_ms),
      hardware_cfg->led_auto_from_scene ? 1U : 0U,
      hardware_cfg->mic_enabled ? 1U : 0U,
      hardware_cfg->mic_event_threshold_pct,
      hardware_cfg->mic_la_trigger_enabled ? 1U : 0U,
      static_cast<unsigned int>(hardware_cfg->mic_la_target_hz),
      static_cast<unsigned int>(hardware_cfg->mic_la_tolerance_hz),
      static_cast<unsigned int>(hardware_cfg->mic_la_max_abs_cents),
      static_cast<unsigned int>(hardware_cfg->mic_la_min_confidence),
      static_cast<unsigned int>(hardware_cfg->mic_la_min_level_pct),
      static_cast<unsigned int>(hardware_cfg->mic_la_stable_ms),
      static_cast<unsigned long>(hardware_cfg->mic_la_timeout_ms),
      hardware_cfg->battery_enabled ? 1U : 0U,
      hardware_cfg->battery_low_pct);
  Serial.printf("[CAM] cfg boot=%u frame=%s quality=%u fb=%u xclk=%lu dir=%s\n",
                camera_cfg->enabled_on_boot ? 1U : 0U,
                camera_cfg->frame_size,
                static_cast<unsigned int>(camera_cfg->jpeg_quality),
                static_cast<unsigned int>(camera_cfg->fb_count),
                static_cast<unsigned long>(camera_cfg->xclk_hz),
                camera_cfg->snapshot_dir);
  Serial.printf("[MEDIA] cfg music=%s picture=%s record=%s max_sec=%u auto_stop=%u\n",
                media_cfg->music_dir,
                media_cfg->picture_dir,
                media_cfg->record_dir,
                static_cast<unsigned int>(media_cfg->record_max_seconds),
                media_cfg->auto_stop_record_on_step_change ? 1U : 0U);
}
