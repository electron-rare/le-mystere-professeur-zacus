#include <Arduino.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <SD_MMC.h>
#include <esp_log.h>
#include <WiFi.h>
#include <mbedtls/base64.h>

#include <algorithm>

#include "audio/AudioEngine.h"
#include "audio/Es8388Driver.h"
#include "config/A252ConfigStore.h"
#include "config/a1s_board_pins.h"
#include "core/CommandDispatcher.h"
#include "core/PlatformProfile.h"
#include "props/EspNowBridge.h"
#include "slic/Ks0835SlicController.h"
#include "telephony/TelephonyService.h"
#include "web/WebServerManager.h"
#include "visual/ScopeDisplay.h"
#include "wifi/WifiManagerInstance.h"
#include "usb/UsbHostRuntime.h"
#include "usb/UsbMassStorageRuntime.h"

#ifndef UNIT_TEST
namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr int kAudioAmpEnablePin = A1S_PA_ENABLE;
constexpr bool kAudioAmpActiveHigh = true;
constexpr char kBootLogTag[] = "RTC_BOOT";
constexpr bool kPrintHelpOnBoot = false;
constexpr uint32_t kToneOffSuppressionMs = 1500U;
constexpr uint32_t kHotlineDefaultLoopPauseMs = 3000U;
constexpr uint16_t kHotlineMaxPauseMs = 10000U;
constexpr uint16_t kFsListDefaultPageSize = 100U;
constexpr uint16_t kFsListMaxPageSize = 200U;
constexpr uint32_t kFsListMaxPage = 100000U;
constexpr uint32_t kEspNowPeerDiscoveryIntervalMs = 60000U;
constexpr uint32_t kEspNowPeerDiscoveryAckWindowMs = 2500U;
constexpr char kEspNowDefaultDeviceName[] = "HOTLINE_PHONE";
constexpr char kFirmwareContractVersion[] = "A252_AUDIO_CHAIN_V4";
constexpr char kFirmwareBuildId[] = __DATE__ " " __TIME__;
constexpr char kHotlineAssetsRoot[] = "/hotline";
constexpr char kHotlineSceneVoiceSuffix[] = "__fr-fr-deniseneural.mp3";

#ifndef RTC_FIRMWARE_GIT_SHA
#define RTC_FIRMWARE_GIT_SHA "unknown"
#endif
constexpr char kFirmwareGitSha[] = RTC_FIRMWARE_GIT_SHA;
// Branch lock: API web access remains open (no Wi-Fi basic auth) for this flow.
constexpr bool kWebAuthEnabledByDefault = false;

#ifdef RTC_WEB_AUTH_DEV_DISABLE
constexpr bool kWebAuthLocalDisableEnabled = true;
#else
constexpr bool kWebAuthLocalDisableEnabled = false;
#endif

BoardProfile g_profile = detectBoardProfile();
FeatureMatrix g_features = getFeatureMatrix(g_profile);

A252PinsConfig g_pins_cfg = A252ConfigStore::defaultPins();
A252AudioConfig g_audio_cfg = A252ConfigStore::defaultAudio();
EspNowPeerStore g_peer_store;
EspNowCallMap g_espnow_call_map;
DialMediaMap g_dial_media_map;
String g_active_scene_id;
MediaRouteEntry g_pending_espnow_call_media;
bool g_pending_espnow_call = false;

Ks0835SlicController g_slic;
AudioEngine g_audio;
Es8388Driver g_codec;
TelephonyService g_telephony;
EspNowBridge g_espnow;
CommandDispatcher g_dispatcher;
ScopeDisplay g_scope_display;
String g_serial_line;
WebServerManager g_web_server;

struct HardwareInitStatus {
    bool init_ok = false;
    bool slic_ready = false;
    bool codec_ready = false;
    bool audio_ready = false;
};

HardwareInitStatus g_hw_status;

struct ConfigMigrationStatus {
    bool espnow_call_map_reset = false;
    bool dial_media_map_reset = false;
};

ConfigMigrationStatus g_config_migrations;

struct HotlineRuntimeState {
    bool active = false;
    String current_key;
    String current_digits;
    String current_source = "NONE";
    MediaRouteEntry current_route;
    bool pending_restart = false;
    uint32_t next_restart_ms = 0U;
    bool queued = false;
    String queued_key;
    String queued_digits;
    String queued_source = "NONE";
    MediaRouteEntry queued_route;
    String last_notify_event;
    bool last_notify_ok = false;
};

HotlineRuntimeState g_hotline;

struct EspNowPeerDiscoveryRuntimeState {
    bool enabled = true;
    uint32_t interval_ms = kEspNowPeerDiscoveryIntervalMs;
    uint32_t ack_window_ms = kEspNowPeerDiscoveryAckWindowMs;
    uint32_t next_probe_ms = 0U;
    bool probe_pending = false;
    String probe_msg_id;
    uint32_t probe_seq = 0U;
    uint32_t probe_deadline_ms = 0U;
    uint32_t probes_sent = 0U;
    uint32_t probe_send_fail = 0U;
    uint32_t probe_ack_seen = 0U;
    uint32_t auto_add_new_ok = 0U;
    uint32_t auto_add_fail = 0U;
    String last_mac;
    String last_device_name;
    String last_error;
};

EspNowPeerDiscoveryRuntimeState g_espnow_peer_discovery;
String g_espnow_local_mac;

void setAmpEnabled(bool enabled) {
    const bool level_high = (enabled == kAudioAmpActiveHigh);
    digitalWrite(kAudioAmpEnablePin, level_high ? HIGH : LOW);
}

bool persistA252AudioConfig(const A252AudioConfig& cfg, const char* source);
bool persistA252AudioConfigIfNeeded(const A252AudioConfig& cfg, const char* source);
String dialSourceText(bool from_pulse);
bool sendHotlineNotify(const char* state,
                       const String& digit_key,
                       const String& digits,
                       const String& source,
                       const MediaRouteEntry& route);
void clearHotlineRuntimeState();
void queueHotlineRoute(const String& digit_key,
                       const String& digits,
                       const String& source,
                       const MediaRouteEntry& route);
bool startHotlineRouteNow(const String& digit_key,
                          const String& digits,
                          const String& source,
                          const MediaRouteEntry& route);
void stopHotlineForHangup();
void tickHotlineRuntime();
void ensureEspNowDeviceName();
void initEspNowPeerDiscoveryRuntime();
void tickEspNowPeerDiscoveryRuntime();
bool maybeTrackEspNowPeerDiscoveryAck(const String& source, const JsonVariantConst& payload);

void ensureA252AudioDefaults() {
    if (g_profile != BoardProfile::ESP32_A252) {
        return;
    }

    constexpr uint8_t kA252CodecMaxVolumePercent = 100;
    bool updated = false;

    if (g_audio_cfg.volume != kA252CodecMaxVolumePercent) {
        Serial.printf("[RTC_BL_PHONE] correcting A252 audio volume %u -> %u (ES8388 max)\n",
                      static_cast<unsigned>(g_audio_cfg.volume),
                      static_cast<unsigned>(kA252CodecMaxVolumePercent));
        g_audio_cfg.volume = kA252CodecMaxVolumePercent;
        updated = true;
    }

    if (g_audio_cfg.sample_rate != 8000U) {
        Serial.printf("[RTC_BL_PHONE] correcting A252 sample_rate %u -> 8000Hz for tone-plan compatibility\n",
                      static_cast<unsigned>(g_audio_cfg.sample_rate));
        g_audio_cfg.sample_rate = 8000;
        updated = true;
    }

    if (g_audio_cfg.bits_per_sample != 16U) {
        Serial.printf("[RTC_BL_PHONE] correcting A252 bits_per_sample %u -> 16 (codec output lock)\n",
                      static_cast<unsigned>(g_audio_cfg.bits_per_sample));
        g_audio_cfg.bits_per_sample = 16;
        updated = true;
    }

    String clock_policy = g_audio_cfg.clock_policy;
    clock_policy.trim();
    clock_policy.toUpperCase();
    if (clock_policy != "HYBRID_TELCO") {
        Serial.printf("[RTC_BL_PHONE] correcting A252 clock_policy %s -> HYBRID_TELCO\n", g_audio_cfg.clock_policy.c_str());
        g_audio_cfg.clock_policy = "HYBRID_TELCO";
        updated = true;
    } else {
        g_audio_cfg.clock_policy = "HYBRID_TELCO";
    }

    String wav_policy = g_audio_cfg.wav_loudness_policy;
    wav_policy.trim();
    wav_policy.toUpperCase();
    if (wav_policy != "FIXED_GAIN_ONLY") {
        Serial.printf("[RTC_BL_PHONE] correcting wav_loudness_policy %s -> FIXED_GAIN_ONLY\n",
                      g_audio_cfg.wav_loudness_policy.c_str());
        g_audio_cfg.wav_loudness_policy = "FIXED_GAIN_ONLY";
        updated = true;
    } else {
        g_audio_cfg.wav_loudness_policy = "FIXED_GAIN_ONLY";
    }

    const int16_t prev_rms = g_audio_cfg.wav_target_rms_dbfs;
    if (g_audio_cfg.wav_target_rms_dbfs < -36) {
        g_audio_cfg.wav_target_rms_dbfs = -36;
    } else if (g_audio_cfg.wav_target_rms_dbfs > -6) {
        g_audio_cfg.wav_target_rms_dbfs = -6;
    }
    updated = updated || (prev_rms != g_audio_cfg.wav_target_rms_dbfs);

    const int16_t prev_ceiling = g_audio_cfg.wav_limiter_ceiling_dbfs;
    if (g_audio_cfg.wav_limiter_ceiling_dbfs < -12) {
        g_audio_cfg.wav_limiter_ceiling_dbfs = -12;
    } else if (g_audio_cfg.wav_limiter_ceiling_dbfs > 0) {
        g_audio_cfg.wav_limiter_ceiling_dbfs = 0;
    }
    updated = updated || (prev_ceiling != g_audio_cfg.wav_limiter_ceiling_dbfs);

    const uint16_t prev_attack = g_audio_cfg.wav_limiter_attack_ms;
    g_audio_cfg.wav_limiter_attack_ms =
        std::max<uint16_t>(1U, std::min<uint16_t>(1000U, g_audio_cfg.wav_limiter_attack_ms));
    updated = updated || (prev_attack != g_audio_cfg.wav_limiter_attack_ms);

    const uint16_t prev_release = g_audio_cfg.wav_limiter_release_ms;
    g_audio_cfg.wav_limiter_release_ms =
        std::max<uint16_t>(1U, std::min<uint16_t>(5000U, g_audio_cfg.wav_limiter_release_ms));
    updated = updated || (prev_release != g_audio_cfg.wav_limiter_release_ms);

    if (!updated) {
        return;
    }
    if (!persistA252AudioConfigIfNeeded(g_audio_cfg, "A252Defaults")) {
        Serial.println("[RTC_BL_PHONE] failed to persist corrected A252 audio config");
    }
}

bool persistA252AudioConfig(const A252AudioConfig& cfg, const char* source) {
    const uint8_t previous_volume = g_audio_cfg.volume;
    String error;
    if (!A252ConfigStore::saveAudio(cfg, &error)) {
        Serial.printf(
            "[RTC_BL_PHONE] failed to persist audio config from %s: %s\n",
            source,
            error.c_str()
        );
        return false;
    }

    g_audio_cfg = cfg;
    if (previous_volume != g_audio_cfg.volume) {
        Serial.printf("[RTC_BL_PHONE] audio volume persisted via %s: %u -> %u\n",
                      source,
                      static_cast<unsigned>(previous_volume),
                      static_cast<unsigned>(g_audio_cfg.volume));
    }
    return true;
}

bool persistA252AudioConfigIfNeeded(const A252AudioConfig& cfg, const char* source) {
    if (cfg.volume != g_audio_cfg.volume) {
        return persistA252AudioConfig(cfg, source);
    }

    String error;
    if (!A252ConfigStore::saveAudio(cfg, &error)) {
        Serial.printf("[RTC_BL_PHONE] failed to persist audio config from %s: %s\n", source, error.c_str());
        return false;
    }

    g_audio_cfg = cfg;
    return true;
}

void ensureEspNowDeviceName() {
    const String expected = A252ConfigStore::normalizeDeviceName(kEspNowDefaultDeviceName);
    if (expected.isEmpty()) {
        return;
    }

    const String current = A252ConfigStore::normalizeDeviceName(g_peer_store.device_name);
    if (!current.isEmpty()) {
        g_peer_store.device_name = current;
        Serial.printf("[RTC_BL_PHONE] espnow device_name=%s\n", current.c_str());
        return;
    }

    g_peer_store.device_name = expected;
    String error;
    if (!A252ConfigStore::saveEspNowPeers(g_peer_store, &error)) {
        Serial.printf("[RTC_BL_PHONE] failed to persist espnow device_name=%s: %s\n", expected.c_str(), error.c_str());
        return;
    }
    Serial.printf("[RTC_BL_PHONE] espnow device_name forced to %s\n", expected.c_str());
}

void initEspNowPeerDiscoveryRuntime() {
    g_espnow_peer_discovery = EspNowPeerDiscoveryRuntimeState{};
    g_espnow_local_mac = A252ConfigStore::normalizeMac(WiFi.macAddress());
    g_espnow_peer_discovery.next_probe_ms = millis() + g_espnow_peer_discovery.interval_ms;
    Serial.printf("[RTC_BL_PHONE] espnow peer discovery runtime enabled interval_ms=%lu local_mac=%s\n",
                  static_cast<unsigned long>(g_espnow_peer_discovery.interval_ms),
                  g_espnow_local_mac.c_str());
}

bool maybeTrackEspNowPeerDiscoveryAck(const String& source, const JsonVariantConst& payload) {
    if (!g_espnow_peer_discovery.enabled || !g_espnow_peer_discovery.probe_pending) {
        return false;
    }
    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    const JsonObjectConst root = payload.as<JsonObjectConst>();
    String type = root["type"] | "";
    type.toLowerCase();
    if (type != "ack") {
        return false;
    }

    const String msg_id = root["msg_id"] | "";
    const uint32_t seq = root["seq"] | 0U;
    if (msg_id != g_espnow_peer_discovery.probe_msg_id || seq != g_espnow_peer_discovery.probe_seq) {
        return false;
    }

    const String normalized_source = A252ConfigStore::normalizeMac(source);
    if (normalized_source.isEmpty()) {
        return false;
    }
    if (!g_espnow_local_mac.isEmpty() && normalized_source == g_espnow_local_mac) {
        return true;
    }

    g_espnow_peer_discovery.probe_ack_seen++;
    g_espnow_peer_discovery.last_mac = normalized_source;

    bool ack_ok = false;
    String ack_error;
    String device_name;
    if (root["payload"].is<JsonObjectConst>()) {
        const JsonObjectConst ack_payload = root["payload"].as<JsonObjectConst>();
        ack_ok = ack_payload["ok"] | false;
        ack_error = ack_payload["error"] | "";
        if (ack_payload["data"].is<JsonObjectConst>()) {
            const JsonObjectConst data = ack_payload["data"].as<JsonObjectConst>();
            if (data["device_name"].is<const char*>()) {
                device_name = A252ConfigStore::normalizeDeviceName(data["device_name"].as<const char*>());
            }
        }
    }
    g_espnow_peer_discovery.last_device_name = device_name;

    if (!ack_ok) {
        g_espnow_peer_discovery.last_error = ack_error.isEmpty() ? String("probe_ack_not_ok") : ack_error;
        return true;
    }

    const bool already_known =
        std::find(g_peer_store.peers.begin(), g_peer_store.peers.end(), normalized_source) != g_peer_store.peers.end();
    const bool add_ok = g_espnow.addPeer(normalized_source);
    g_peer_store.peers = g_espnow.peers();
    g_peer_store.device_name = g_espnow.deviceName();

    if (!add_ok) {
        g_espnow_peer_discovery.auto_add_fail++;
        g_espnow_peer_discovery.last_error = "auto_add_peer_failed";
        Serial.printf("[RTC_BL_PHONE] espnow peer discovery add failed mac=%s\n", normalized_source.c_str());
        return true;
    }

    if (!already_known) {
        g_espnow_peer_discovery.auto_add_new_ok++;
        Serial.printf("[RTC_BL_PHONE] espnow peer discovery added mac=%s name=%s\n",
                      normalized_source.c_str(),
                      device_name.c_str());
    }
    g_espnow_peer_discovery.last_error = "";
    return true;
}

void tickEspNowPeerDiscoveryRuntime() {
    if (!g_espnow_peer_discovery.enabled || !g_espnow.isReady()) {
        return;
    }

    const uint32_t now = millis();
    if (g_espnow_peer_discovery.probe_pending) {
        if (static_cast<int32_t>(now - g_espnow_peer_discovery.probe_deadline_ms) < 0) {
            return;
        }
        g_espnow_peer_discovery.probe_pending = false;
        g_espnow_peer_discovery.probe_msg_id = "";
        g_espnow_peer_discovery.probe_seq = 0U;
        g_espnow_peer_discovery.probe_deadline_ms = 0U;
        g_espnow_peer_discovery.next_probe_ms = now + g_espnow_peer_discovery.interval_ms;
    }

    if (static_cast<int32_t>(now - g_espnow_peer_discovery.next_probe_ms) < 0) {
        return;
    }

    const uint32_t probe_index = g_espnow_peer_discovery.probes_sent + 1U;
    const String msg_id = String("peerdisc-") + String(now) + "-" + String(probe_index);
    const uint32_t seq = now;

    JsonDocument doc;
    doc["msg_id"] = msg_id;
    doc["seq"] = seq;
    doc["type"] = "command";
    doc["ack"] = true;
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["cmd"] = "ESPNOW_DEVICE_NAME_GET";

    String wire;
    serializeJson(doc, wire);

    if (!g_espnow.sendJson("broadcast", wire)) {
        g_espnow_peer_discovery.probe_send_fail++;
        g_espnow_peer_discovery.last_error = "probe_send_failed";
        g_espnow_peer_discovery.next_probe_ms = now + g_espnow_peer_discovery.interval_ms;
        return;
    }

    g_espnow_peer_discovery.probes_sent = probe_index;
    g_espnow_peer_discovery.probe_pending = true;
    g_espnow_peer_discovery.probe_msg_id = msg_id;
    g_espnow_peer_discovery.probe_seq = seq;
    g_espnow_peer_discovery.probe_deadline_ms = now + g_espnow_peer_discovery.ack_window_ms;
    g_espnow_peer_discovery.next_probe_ms = now + g_espnow_peer_discovery.interval_ms;
    g_espnow_peer_discovery.last_error = "";
}

DispatchResponse makeResponse(bool ok, const String& code) {
    DispatchResponse res;
    res.ok = ok;
    res.code = code;
    return res;
}

DispatchResponse jsonResponse(JsonDocument& doc) {
    DispatchResponse res;
    res.ok = true;
    serializeJson(doc, res.json);
    return res;
}

bool splitFirstToken(const String& input, String& first, String& rest) {
    String work = input;
    work.trim();
    if (work.isEmpty()) {
        first = "";
        rest = "";
        return false;
    }

    if (work[0] == '"') {
        bool escaped = false;
        int close_index = -1;
        for (int i = 1; i < work.length(); ++i) {
            const char c = work[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                close_index = i;
                break;
            }
        }
        if (close_index < 0) {
            first = "";
            rest = "";
            return false;
        }

        String token = work.substring(1, close_index);
        token.replace("\\\"", "\"");
        token.replace("\\\\", "\\");
        first = token;
        rest = work.substring(close_index + 1);
        rest.trim();
        return true;
    }

    const int sep = work.indexOf(' ');
    if (sep < 0) {
        first = work;
        rest = "";
        return true;
    }

    first = work.substring(0, sep);
    rest = work.substring(sep + 1);
    rest.trim();
    return true;
}

bool extractBridgeCommand(JsonVariantConst payload, String& out_cmd, uint8_t depth = 0) {
    if (depth > 4U) {
        return false;
    }

    if (payload.is<const char*>()) {
        out_cmd = payload.as<const char*>();
        out_cmd.trim();
        return !out_cmd.isEmpty();
    }

    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    const char* keys[] = {"cmd", "raw", "command", "action"};
    for (const char* key : keys) {
        if (!payload[key].is<const char*>()) {
            continue;
        }
        out_cmd = payload[key].as<const char*>();
        out_cmd.trim();
        if (!out_cmd.isEmpty()) {
            return true;
        }
    }

    if (!payload["event"].isNull() && extractBridgeCommand(payload["event"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }
    if (!payload["message"].isNull() &&
        extractBridgeCommand(payload["message"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }
    if (!payload["payload"].isNull() &&
        extractBridgeCommand(payload["payload"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }

    return false;
}

struct FsListOptions {
    MediaSource source = MediaSource::SD;
    String path = "/";
    uint32_t page = 0U;
    uint16_t page_size = kFsListDefaultPageSize;
    bool recursive = true;
    bool include_dirs = true;
    bool include_files = true;
};

struct FsListEntry {
    String path;
    bool is_dir = false;
    size_t size = 0U;
};

struct FsListResult {
    MediaSource source_used = MediaSource::AUTO;
    bool has_next = false;
    std::vector<FsListEntry> entries;
};

struct FsListWalkState {
    size_t offset = 0U;
    uint16_t page_size = kFsListDefaultPageSize;
    size_t seen = 0U;
};

String sanitizeListPath(const String& raw_path) {
    String path = raw_path;
    path.trim();
    if (path.isEmpty()) {
        return "/";
    }
    if (path.length() >= 2U && path[0] == '"' && path[path.length() - 1U] == '"') {
        path = path.substring(1U, path.length() - 1U);
    }
    path.trim();
    if (path.isEmpty()) {
        return "/";
    }
    if (path.startsWith("{") || path.startsWith("[")) {
        return "";
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    while (path.indexOf("//") >= 0) {
        path.replace("//", "/");
    }
    if (path.indexOf("..") >= 0) {
        return "";
    }
    if (path.length() > 1U && path.endsWith("/")) {
        path.remove(path.length() - 1U);
    }
    return path;
}

bool parseBoolLikeString(const String& raw_token, bool& out_value) {
    String token = raw_token;
    token.trim();
    token.toLowerCase();
    if (token == "1" || token == "true" || token == "yes" || token == "on") {
        out_value = true;
        return true;
    }
    if (token == "0" || token == "false" || token == "no" || token == "off") {
        out_value = false;
        return true;
    }
    return false;
}

bool parseBoolLike(JsonVariantConst value, bool& out_value) {
    if (value.is<bool>()) {
        out_value = value.as<bool>();
        return true;
    }
    if (value.is<int>()) {
        const int parsed = value.as<int>();
        if (parsed == 0) {
            out_value = false;
            return true;
        }
        if (parsed == 1) {
            out_value = true;
            return true;
        }
        return false;
    }
    if (value.is<const char*>()) {
        return parseBoolLikeString(value.as<const char*>(), out_value);
    }
    return false;
}

bool parseUint32Token(const String& raw_token, uint32_t min_value, uint32_t max_value, uint32_t& out_value) {
    String token = raw_token;
    token.trim();
    if (token.isEmpty() || token.startsWith("-")) {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = strtoul(token.c_str(), &end, 10);
    if (end == nullptr || end == token.c_str() || *end != '\0') {
        return false;
    }
    if (parsed < min_value || parsed > max_value) {
        return false;
    }
    out_value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseUint32Field(JsonVariantConst value, uint32_t min_value, uint32_t max_value, uint32_t& out_value) {
    if (value.is<uint32_t>()) {
        const uint32_t parsed = value.as<uint32_t>();
        if (parsed < min_value || parsed > max_value) {
            return false;
        }
        out_value = parsed;
        return true;
    }
    if (value.is<int>()) {
        const int parsed = value.as<int>();
        if (parsed < 0 || static_cast<uint32_t>(parsed) < min_value || static_cast<uint32_t>(parsed) > max_value) {
            return false;
        }
        out_value = static_cast<uint32_t>(parsed);
        return true;
    }
    if (value.is<const char*>()) {
        return parseUint32Token(value.as<const char*>(), min_value, max_value, out_value);
    }
    return false;
}

String sanitizeFsPath(const String& raw_path) {
    String path = raw_path;
    path.trim();
    if (path.isEmpty()) {
        return "";
    }
    if (path.length() >= 2U && path[0] == '"' && path[path.length() - 1U] == '"') {
        path = path.substring(1U, path.length() - 1U);
    }
    path.trim();
    if (path.isEmpty() || path == "/" || path.startsWith("{") || path.startsWith("[")) {
        return "";
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    if (path.indexOf("..") >= 0) {
        return "";
    }
    return path;
}

bool ensureFfatMounted() {
    if (FFat.begin(false)) {
        return true;
    }
    return FFat.begin(true);
}

bool ensureLittleFsMountedForList() {
#ifdef USB_MSC_BOOT_ENABLE
    if (FFat.begin(false, "/usbmsc", 10, "usbmsc")) {
        return true;
    }
    return FFat.begin(true, "/usbmsc", 10, "usbmsc");
#else
    if (FFat.begin(false)) {
        return true;
    }
    return FFat.begin(true);
#endif
}

bool ensureSdMountedForList() {
    return SD_MMC.begin();
}

bool resolveFsListSource(MediaSource source_requested, fs::FS*& out_fs, MediaSource& out_source) {
    out_fs = nullptr;
    out_source = MediaSource::AUTO;

    auto use_sd = [&]() -> bool {
        if (!ensureSdMountedForList()) {
            return false;
        }
        out_fs = &SD_MMC;
        out_source = MediaSource::SD;
        return true;
    };

    auto use_littlefs = [&]() -> bool {
        if (!ensureLittleFsMountedForList()) {
            return false;
        }
        out_fs = &FFat;
        out_source = MediaSource::LITTLEFS;
        return true;
    };

    if (source_requested == MediaSource::SD) {
        return use_sd();
    }
    if (source_requested == MediaSource::LITTLEFS) {
        return use_littlefs();
    }
    if (use_sd()) {
        return true;
    }
    return use_littlefs();
}

bool ensureParentDirsOnFfat(const String& absolute_path) {
    if (!absolute_path.startsWith("/")) {
        return false;
    }
    int idx = 1;
    while (idx > 0 && idx < absolute_path.length()) {
        idx = absolute_path.indexOf('/', idx);
        if (idx < 0) {
            break;
        }
        const String dir = absolute_path.substring(0, idx);
        if (!dir.isEmpty() && !FFat.exists(dir)) {
            if (!FFat.mkdir(dir)) {
                return false;
            }
        }
        ++idx;
    }
    return true;
}

bool decodeBase64ToBytes(const String& b64, std::vector<uint8_t>& out) {
    out.clear();
    if (b64.isEmpty()) {
        return true;
    }
    size_t out_len = 0U;
    out.resize(((b64.length() + 3U) / 4U) * 3U + 4U);
    const int ret = mbedtls_base64_decode(out.data(),
                                          out.size(),
                                          &out_len,
                                          reinterpret_cast<const unsigned char*>(b64.c_str()),
                                          b64.length());
    if (ret != 0) {
        out.clear();
        return false;
    }
    out.resize(out_len);
    return true;
}

bool parseFsListOptions(const String& args, FsListOptions& out_options, String& out_error) {
    out_options = FsListOptions{};
    out_error = "";

    String work = args;
    work.trim();
    if (work.isEmpty()) {
        return true;
    }

    if (work.startsWith("{")) {
        JsonDocument doc;
        if (deserializeJson(doc, work) != DeserializationError::Ok || !doc.is<JsonObjectConst>()) {
            out_error = "invalid_args";
            return false;
        }
        JsonObjectConst obj = doc.as<JsonObjectConst>();

        if (!obj["source"].isNull()) {
            if (!obj["source"].is<const char*>()) {
                out_error = "invalid_source";
                return false;
            }
            MediaSource parsed_source = MediaSource::AUTO;
            if (!parseMediaSource(obj["source"].as<const char*>(), parsed_source)) {
                out_error = "invalid_source";
                return false;
            }
            out_options.source = parsed_source;
        }

        if (!obj["path"].isNull()) {
            if (!obj["path"].is<const char*>()) {
                out_error = "invalid_path";
                return false;
            }
            const String path = sanitizeListPath(obj["path"].as<const char*>());
            if (path.isEmpty()) {
                out_error = "invalid_path";
                return false;
            }
            out_options.path = path;
        }

        if (!obj["page"].isNull()) {
            uint32_t page = 0U;
            if (!parseUint32Field(obj["page"], 0U, kFsListMaxPage, page)) {
                out_error = "invalid_page";
                return false;
            }
            out_options.page = page;
        }

        if (!obj["page_size"].isNull()) {
            uint32_t page_size = 0U;
            if (!parseUint32Field(obj["page_size"], 1U, static_cast<uint32_t>(kFsListMaxPageSize), page_size)) {
                out_error = "invalid_page_size";
                return false;
            }
            out_options.page_size = static_cast<uint16_t>(page_size);
        }

        if (!obj["recursive"].isNull()) {
            bool recursive = false;
            if (!parseBoolLike(obj["recursive"], recursive)) {
                out_error = "invalid_args";
                return false;
            }
            out_options.recursive = recursive;
        }

        if (!obj["include_dirs"].isNull()) {
            bool include_dirs = false;
            if (!parseBoolLike(obj["include_dirs"], include_dirs)) {
                out_error = "invalid_args";
                return false;
            }
            out_options.include_dirs = include_dirs;
        }

        if (!obj["include_files"].isNull()) {
            bool include_files = false;
            if (!parseBoolLike(obj["include_files"], include_files)) {
                out_error = "invalid_args";
                return false;
            }
            out_options.include_files = include_files;
        }

        if (!out_options.include_dirs && !out_options.include_files) {
            out_error = "invalid_args";
            return false;
        }
        return true;
    }

    String source_token;
    String trailing;
    if (!splitFirstToken(work, source_token, trailing) || source_token.isEmpty() || !trailing.isEmpty()) {
        out_error = "invalid_args";
        return false;
    }

    MediaSource parsed_source = MediaSource::AUTO;
    if (!parseMediaSource(source_token, parsed_source)) {
        out_error = "invalid_source";
        return false;
    }
    out_options.source = parsed_source;
    return true;
}

String buildFsListEntryPath(const String& parent_path, const String& entry_name) {
    String name = entry_name;
    name.replace("\\", "/");
    if (name.startsWith("/")) {
        while (name.indexOf("//") >= 0) {
            name.replace("//", "/");
        }
        return name;
    }

    String path = parent_path;
    if (path.isEmpty()) {
        path = "/";
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    if (path.endsWith("/")) {
        path.remove(path.length() - 1U);
    }
    if (path.isEmpty()) {
        path = "/";
    }
    if (path == "/") {
        path += name;
    } else {
        path += "/";
        path += name;
    }
    while (path.indexOf("//") >= 0) {
        path.replace("//", "/");
    }
    return path;
}

bool walkFsListEntries(File& directory,
                       const String& current_path,
                       const FsListOptions& options,
                       FsListWalkState& state,
                       FsListResult& out_result) {
    if (!directory || !directory.isDirectory()) {
        return false;
    }

    File entry = directory.openNextFile();
    while (entry) {
        const bool is_dir = entry.isDirectory();
        const String entry_path = buildFsListEntryPath(current_path, entry.name());
        const bool include_entry = (is_dir && options.include_dirs) || (!is_dir && options.include_files);

        if (include_entry) {
            if (state.seen >= state.offset) {
                if (out_result.entries.size() < state.page_size) {
                    FsListEntry out_entry;
                    out_entry.path = entry_path;
                    out_entry.is_dir = is_dir;
                    out_entry.size = is_dir ? 0U : static_cast<size_t>(entry.size());
                    out_result.entries.push_back(out_entry);
                } else {
                    out_result.has_next = true;
                    entry.close();
                    return true;
                }
            }
            state.seen++;
        }

        if (options.recursive && is_dir) {
            if (!walkFsListEntries(entry, entry_path, options, state, out_result)) {
                entry.close();
                return false;
            }
            if (out_result.has_next) {
                entry.close();
                return true;
            }
        }

        entry.close();
        entry = directory.openNextFile();
    }
    return true;
}

DispatchResponse dispatchFsListCommand(const String& args) {
    FsListOptions options;
    String parse_error;
    if (!parseFsListOptions(args, options, parse_error)) {
        return makeResponse(false, String("FS_LIST ") + parse_error);
    }

    fs::FS* fs = nullptr;
    MediaSource source_used = MediaSource::AUTO;
    if (!resolveFsListSource(options.source, fs, source_used) || fs == nullptr) {
        return makeResponse(false, "FS_LIST mount_failed");
    }

    File directory = fs->open(options.path, FILE_READ);
    if (!directory) {
        return makeResponse(false, "FS_LIST open_failed");
    }
    if (!directory.isDirectory()) {
        directory.close();
        return makeResponse(false, "FS_LIST not_directory");
    }

    FsListResult result;
    result.source_used = source_used;
    result.entries.reserve(options.page_size);
    FsListWalkState walk_state;
    walk_state.offset = static_cast<size_t>(options.page) * static_cast<size_t>(options.page_size);
    walk_state.page_size = options.page_size;

    if (!walkFsListEntries(directory, options.path, options, walk_state, result)) {
        directory.close();
        return makeResponse(false, "FS_LIST open_failed");
    }
    directory.close();

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["source_requested"] = mediaSourceToString(options.source);
    root["source_used"] = mediaSourceToString(result.source_used);
    root["path"] = options.path;
    root["page"] = options.page;
    root["page_size"] = options.page_size;
    root["recursive"] = options.recursive;
    root["include_dirs"] = options.include_dirs;
    root["include_files"] = options.include_files;
    root["count"] = static_cast<uint32_t>(result.entries.size());
    root["has_next"] = result.has_next;

    root["next_page"] = result.has_next ? static_cast<int32_t>(options.page + 1U) : -1;

    JsonArray entries = root["entries"].to<JsonArray>();
    for (const FsListEntry& entry : result.entries) {
        JsonObject item = entries.add<JsonObject>();
        item["path"] = entry.path;
        item["type"] = entry.is_dir ? "dir" : "file";
        item["size"] = static_cast<uint32_t>(entry.size);
    }

    return jsonResponse(doc);
}

bool isDialMapNumberKey(const String& number) {
    if (number.isEmpty() || number.length() > 20U) {
        return false;
    }
    for (size_t i = 0; i < number.length(); ++i) {
        if (number[i] < '0' || number[i] > '9') {
            return false;
        }
    }
    return true;
}

bool parsePlaybackPolicyFromObject(JsonObjectConst obj, FilePlaybackPolicy& out_policy) {
    out_policy = FilePlaybackPolicy{};
    bool loop = false;
    int pause_ms = 0;

    if (obj["playback"]["loop"].is<bool>()) {
        loop = obj["playback"]["loop"].as<bool>();
    } else if (obj["loop"].is<bool>()) {
        loop = obj["loop"].as<bool>();
    }

    if (obj["playback"]["pause_ms"].is<int>()) {
        pause_ms = obj["playback"]["pause_ms"].as<int>();
    } else if (obj["pause_ms"].is<int>()) {
        pause_ms = obj["pause_ms"].as<int>();
    }

    if (pause_ms < 0) {
        return false;
    }
    if (pause_ms > static_cast<int>(kHotlineMaxPauseMs)) {
        return false;
    }

    out_policy.loop = loop;
    out_policy.pause_ms = static_cast<uint16_t>(pause_ms);
    return true;
}

MediaRouteEntry buildHotlineSdFileRoute(const String& path, bool loop = false, uint16_t pause_ms = 0U) {
    MediaRouteEntry route;
    route.kind = MediaRouteKind::FILE;
    route.path = sanitizeMediaPath(path);
    route.source = MediaSource::SD;
    route.playback.loop = loop;
    route.playback.pause_ms = pause_ms;
    return route;
}

String buildHotlineVoicePathFromStem(const String& stem) {
    String clean_stem = stem;
    clean_stem.trim();
    if (clean_stem.isEmpty()) {
        return "";
    }
    return String(kHotlineAssetsRoot) + "/" + clean_stem + kHotlineSceneVoiceSuffix;
}

MediaRouteEntry buildHotlineSdVoiceRoute(const String& stem, bool loop = false, uint16_t pause_ms = 0U) {
    return buildHotlineSdFileRoute(buildHotlineVoicePathFromStem(stem), loop, pause_ms);
}

String buildMp3FallbackWavPath(const String& path) {
    String normalized = sanitizeMediaPath(path);
    if (normalized.isEmpty()) {
        return "";
    }

    String lower = normalized;
    lower.toLowerCase();
    if (!lower.endsWith(".mp3")) {
        return "";
    }

    String voice_suffix = kHotlineSceneVoiceSuffix;
    voice_suffix.toLowerCase();
    if (lower.endsWith(voice_suffix) && normalized.length() > voice_suffix.length()) {
        return normalized.substring(0, normalized.length() - voice_suffix.length()) + ".wav";
    }
    if (normalized.length() <= 4U) {
        return "";
    }
    return normalized.substring(0, normalized.length() - 4U) + ".wav";
}

String normalizeHotlineSceneKey(const String& raw_scene_id) {
    String key = raw_scene_id;
    key.trim();
    key.toUpperCase();
    if (key.startsWith("SCENE_")) {
        key = key.substring(6);
    }
    if (key == "LOCK") {
        key = "LOCKED";
    } else if (key == "LA_DETECT") {
        key = "LA_DETECTOR";
    } else if (key == "LE_FOU_DETECTOR") {
        key = "LEFOU_DETECTOR";
    }
    return key;
}

String hotlineSceneStemFromKey(const String& scene_key) {
    if (scene_key == "READY") {
        return "scene_ready_2";
    }
    if (scene_key == "LOCKED") {
        return "scene_locked_2";
    }
    if (scene_key == "BROKEN" || scene_key == "SIGNAL_SPIKE" || scene_key == "WARNING") {
        return "scene_broken_2";
    }
    if (scene_key == "SEARCH" || scene_key == "LEFOU_DETECTOR") {
        return "scene_search_2";
    }
    if (scene_key == "LA_DETECTOR") {
        return "scene_la_detector_2";
    }
    if (scene_key == "CAMERA_SCAN" || scene_key == "QR_DETECTOR") {
        return "scene_camera_scan_2";
    }
    if (scene_key == "MEDIA_ARCHIVE") {
        return "scene_media_archive_2";
    }
    if (scene_key == "WIN" || scene_key == "REWARD" || scene_key == "WINNER" || scene_key == "FINAL_WIN" ||
        scene_key == "FIREWORKS" || scene_key == "WIN_ETAPE" || scene_key == "WIN_ETAPE1" ||
        scene_key == "WIN_ETAPE2") {
        return "scene_win_2";
    }
    if (scene_key == "U_SON_PROTO" || scene_key == "MP3_PLAYER" || scene_key == "MEDIA_MANAGER") {
        return "fiches-hotline_2";
    }
    return "";
}

bool resolveHotlineSceneRoute(const String& scene_id, MediaRouteEntry& out_route) {
    const String key = normalizeHotlineSceneKey(scene_id);
    const String stem = hotlineSceneStemFromKey(key);
    if (stem.isEmpty()) {
        out_route = MediaRouteEntry{};
        return false;
    }

    out_route = buildHotlineSdVoiceRoute(stem, false, 0U);
    return mediaRouteHasPayload(out_route);
}

void initDefaultEspNowCallMap(EspNowCallMap& out_map) {
    out_map.clear();
    EspNowCallMapEntry la_ok;
    la_ok.keyword = "LA_OK";
    la_ok.route.kind = MediaRouteKind::TONE;
    la_ok.route.tone.profile = ToneProfile::FR_FR;
    la_ok.route.tone.event = ToneEvent::DIAL;
    out_map.push_back(la_ok);

    EspNowCallMapEntry la_busy;
    la_busy.keyword = "LA_BUSY";
    la_busy.route.kind = MediaRouteKind::TONE;
    la_busy.route.tone.profile = ToneProfile::FR_FR;
    la_busy.route.tone.event = ToneEvent::BUSY;
    out_map.push_back(la_busy);
}

void initDefaultDialMediaMap(DialMediaMap& out_map) {
    out_map.clear();
    auto add_default = [&](const char* key, const char* path) {
        DialMediaMapEntry entry;
        entry.number = key;
        entry.route.kind = MediaRouteKind::FILE;
        entry.route.path = sanitizeMediaPath(path);
        entry.route.source = MediaSource::SD;
        entry.route.playback.loop = true;
        entry.route.playback.pause_ms = static_cast<uint16_t>(kHotlineDefaultLoopPauseMs);
        out_map.push_back(entry);
    };
    add_default("1", "/hotline/menu_dtmf_short.wav");
    add_default("2", "/hotline/menu_dtmf.wav");
    add_default("3", "/hotline/menu_dtmf_long.wav");
}

bool findDialMediaRoute(const String& digits, MediaRouteEntry& out_route) {
    for (const DialMediaMapEntry& entry : g_dial_media_map) {
        if (entry.number == digits && mediaRouteHasPayload(entry.route)) {
            out_route = entry.route;
            return true;
        }
    }
    return false;
}

DialRouteMatch resolveDialRouteMatch(const String& digits) {
    if (digits.isEmpty()) {
        return DialRouteMatch::NONE;
    }
    bool exact = false;
    bool longer_prefix = false;
    bool prefix_only = false;
    for (const DialMediaMapEntry& entry : g_dial_media_map) {
        if (entry.number.isEmpty() || !mediaRouteHasPayload(entry.route)) {
            continue;
        }
        if (entry.number == digits) {
            exact = true;
            continue;
        }
        if (entry.number.startsWith(digits)) {
            if (entry.number.length() > digits.length()) {
                longer_prefix = true;
            } else {
                prefix_only = true;
            }
        }
    }
    if (exact && longer_prefix) {
        return DialRouteMatch::EXACT_AND_PREFIX;
    }
    if (exact) {
        return DialRouteMatch::EXACT;
    }
    if (longer_prefix || prefix_only) {
        return DialRouteMatch::PREFIX;
    }
    return DialRouteMatch::NONE;
}

bool triggerHotlineRouteForDigits(const String& digits, bool from_pulse, String* out_state = nullptr) {
    if (out_state != nullptr) {
        *out_state = "";
    }
    if (!isDialMapNumberKey(digits)) {
        if (out_state != nullptr) {
            *out_state = "invalid_number";
        }
        return false;
    }

    MediaRouteEntry route;
    if (!findDialMediaRoute(digits, route)) {
        if (out_state != nullptr) {
            *out_state = "missing_route";
        }
        return false;
    }
    if (route.kind == MediaRouteKind::FILE) {
        // Hotline file routes are cyclic until hangup: play file, wait, replay.
        route.playback.loop = true;
        route.playback.pause_ms = static_cast<uint16_t>(kHotlineDefaultLoopPauseMs);
    }

    const String source = dialSourceText(from_pulse);
    if (g_hotline.active) {
        queueHotlineRoute(digits, digits, source, route);
        if (out_state != nullptr) {
            *out_state = "queued";
        }
        return true;
    }

    const bool ok = startHotlineRouteNow(digits, digits, source, route);
    if (out_state != nullptr) {
        *out_state = ok ? "triggered" : "play_failed";
    }
    return ok;
}

bool parseMediaRouteFromArgs(const String& args, MediaRouteEntry& out_route, bool allow_tone_route = true) {
    out_route = MediaRouteEntry{};
    out_route.kind = MediaRouteKind::FILE;
    out_route.source = MediaSource::AUTO;

    String work = args;
    work.trim();
    if (work.isEmpty()) {
        return false;
    }

    if (work.startsWith("{")) {
        JsonDocument doc;
        if (deserializeJson(doc, work) != DeserializationError::Ok || !doc.is<JsonObject>()) {
            return false;
        }
        JsonObjectConst obj = doc.as<JsonObjectConst>();
        JsonObjectConst audio = obj["audio"].is<JsonObjectConst>() ? obj["audio"].as<JsonObjectConst>() : obj;

        MediaRouteKind kind = MediaRouteKind::FILE;
        if (audio["kind"].is<const char*>()) {
            if (!parseMediaRouteKind(audio["kind"].as<const char*>(), kind)) {
                return false;
            }
        } else if (allow_tone_route && audio["profile"].is<const char*>() && audio["event"].is<const char*>()) {
            kind = MediaRouteKind::TONE;
        }

        if (kind == MediaRouteKind::TONE) {
            if (!allow_tone_route || !audio["profile"].is<const char*>() || !audio["event"].is<const char*>()) {
                return false;
            }
            out_route.kind = MediaRouteKind::TONE;
            if (!parseToneProfile(audio["profile"].as<const char*>(), out_route.tone.profile)) {
                return false;
            }
            if (!parseToneEvent(audio["event"].as<const char*>(), out_route.tone.event)) {
                return false;
            }
            return out_route.tone.profile != ToneProfile::NONE && out_route.tone.event != ToneEvent::NONE;
        }

        if (audio["path"].is<const char*>()) {
            out_route.kind = MediaRouteKind::FILE;
            out_route.path = sanitizeMediaPath(audio["path"].as<const char*>());
        }
        if (audio["source"].is<const char*>()) {
            MediaSource parsed = MediaSource::AUTO;
            if (!parseMediaSource(audio["source"].as<const char*>(), parsed)) {
                return false;
            }
            out_route.source = parsed;
        }
        if (!parsePlaybackPolicyFromObject(audio, out_route.playback)) {
            return false;
        }
        return !out_route.path.isEmpty();
    }

    String lower = work;
    lower.toLowerCase();
    if (lower.startsWith("sd:")) {
        out_route.kind = MediaRouteKind::FILE;
        out_route.source = MediaSource::SD;
        out_route.path = sanitizeMediaPath(work.substring(3));
        return !out_route.path.isEmpty();
    }
    if (lower.startsWith("littlefs:")) {
        out_route.kind = MediaRouteKind::FILE;
        out_route.source = MediaSource::LITTLEFS;
        out_route.path = sanitizeMediaPath(work.substring(9));
        return !out_route.path.isEmpty();
    }
    if (lower.startsWith("auto:")) {
        out_route.kind = MediaRouteKind::FILE;
        out_route.source = MediaSource::AUTO;
        out_route.path = sanitizeMediaPath(work.substring(5));
        return !out_route.path.isEmpty();
    }

    if (allow_tone_route && lower.startsWith("tone:")) {
        const String tone_spec = work.substring(5);
        String first;
        String rest;
        if (!splitFirstToken(tone_spec, first, rest)) {
            return false;
        }
        ToneProfile profile = ToneProfile::FR_FR;
        ToneEvent event = ToneEvent::NONE;
        if (rest.isEmpty()) {
            if (!parseToneEvent(first, event)) {
                return false;
            }
        } else {
            if (!parseToneProfile(first, profile)) {
                return false;
            }
            if (!parseToneEvent(rest, event)) {
                return false;
            }
        }
        out_route.kind = MediaRouteKind::TONE;
        out_route.tone.profile = profile;
        out_route.tone.event = event;
        return out_route.tone.profile != ToneProfile::NONE && out_route.tone.event != ToneEvent::NONE;
    }

    out_route.kind = MediaRouteKind::FILE;
    out_route.path = sanitizeMediaPath(work);
    return !out_route.path.isEmpty();
}

bool playMediaRoute(const MediaRouteEntry& route) {
    if (route.kind == MediaRouteKind::TONE) {
        return g_audio.playTone(route.tone.profile, route.tone.event);
    }
    if (route.path.isEmpty()) {
        return false;
    }
    if (isLegacyToneWavPath(route.path)) {
        Serial.printf("[RTC_BL_PHONE] rejected legacy tone wav path: %s\n", route.path.c_str());
        return false;
    }
    if (g_audio.playFileFromSource(route.path.c_str(), route.source)) {
        return true;
    }
    const String fallback_wav = buildMp3FallbackWavPath(route.path);
    if (fallback_wav.isEmpty() || fallback_wav == route.path) {
        return false;
    }
    Serial.printf("[RTC_BL_PHONE] media fallback %s -> %s\n", route.path.c_str(), fallback_wav.c_str());
    return g_audio.playFileFromSource(fallback_wav.c_str(), route.source);
}

String dialSourceText(bool from_pulse) {
    return from_pulse ? "PULSE" : "DTMF";
}

bool sendHotlineNotify(const char* state, const String& digit_key, const String& digits, const String& source, const MediaRouteEntry& route) {
    JsonDocument doc;
    doc["proto"] = "rtcbl/1";
    doc["type"] = "event";
    doc["event"] = "hotline_script";
    doc["id"] = String(millis());
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["state"] = state == nullptr ? "" : state;
    payload["digit_key"] = digit_key;
    payload["digits"] = digits;
    payload["source"] = source;
    payload["device_name"] = g_peer_store.device_name;
    JsonObject out_route = payload["route"].to<JsonObject>();
    out_route["kind"] = mediaRouteKindToString(route.kind);
    if (route.kind == MediaRouteKind::TONE) {
        out_route["profile"] = toneProfileToString(route.tone.profile);
        out_route["event"] = toneEventToString(route.tone.event);
    } else {
        out_route["path"] = route.path;
        out_route["source"] = mediaSourceToString(route.source);
        JsonObject playback = out_route["playback"].to<JsonObject>();
        playback["loop"] = route.playback.loop;
        playback["pause_ms"] = route.playback.pause_ms;
    }

    String wire;
    serializeJson(doc, wire);
    const bool ok = g_espnow.sendJson("broadcast", wire);
    g_hotline.last_notify_event = state == nullptr ? "" : state;
    g_hotline.last_notify_ok = ok;
    if (!ok) {
        Serial.printf("[Hotline] notify failed state=%s\n", state == nullptr ? "" : state);
    }
    return ok;
}

void clearHotlineRuntimeState() {
    const String last_event = g_hotline.last_notify_event;
    const bool last_ok = g_hotline.last_notify_ok;
    g_hotline = HotlineRuntimeState{};
    g_hotline.last_notify_event = last_event;
    g_hotline.last_notify_ok = last_ok;
}

void queueHotlineRoute(const String& digit_key, const String& digits, const String& source, const MediaRouteEntry& route) {
    g_hotline.queued = true;
    g_hotline.queued_key = digit_key;
    g_hotline.queued_digits = digits;
    g_hotline.queued_source = source;
    g_hotline.queued_route = route;
    sendHotlineNotify("queued", digit_key, digits, source, route);
}

bool startHotlineRouteNow(const String& digit_key, const String& digits, const String& source, const MediaRouteEntry& route) {
    const bool ok = playMediaRoute(route);
    if (!ok) {
        return false;
    }
    g_hotline.active = true;
    g_hotline.current_key = digit_key;
    g_hotline.current_digits = digits;
    g_hotline.current_source = source;
    g_hotline.current_route = route;
    g_hotline.pending_restart = false;
    g_hotline.next_restart_ms = 0U;
    sendHotlineNotify("triggered", digit_key, digits, source, route);
    return true;
}

void stopHotlineForHangup() {
    if (!g_hotline.active && !g_hotline.queued && !g_hotline.pending_restart) {
        return;
    }
    g_audio.stopPlayback();
    g_audio.stopTone();
    sendHotlineNotify(
        "stopped_hangup",
        g_hotline.current_key,
        g_hotline.current_digits,
        g_hotline.current_source,
        g_hotline.current_route);
    clearHotlineRuntimeState();
}

void tickHotlineRuntime() {
    if (!g_slic.isHookOff()) {
        stopHotlineForHangup();
        return;
    }
    if (!g_hotline.active) {
        return;
    }

    if (g_hotline.current_route.kind == MediaRouteKind::TONE) {
        if (g_hotline.queued) {
            // Tone routes can be effectively unbounded; stop to switch deterministically
            // to the queued route on next restart.
            g_audio.stopTone();
        } else if (g_audio.isToneRenderingActive()) {
            return;
        }
    } else if (g_audio.isPlaying()) {
        return;
    }

    const uint32_t now = millis();
    if (!g_hotline.pending_restart) {
        const bool should_continue = g_hotline.current_route.playback.loop || g_hotline.queued;
        if (!should_continue) {
            clearHotlineRuntimeState();
            return;
        }
        uint16_t pause_ms = g_hotline.current_route.playback.pause_ms;
        if (pause_ms == 0U) {
            pause_ms = static_cast<uint16_t>(kHotlineDefaultLoopPauseMs);
        }
        g_hotline.pending_restart = true;
        g_hotline.next_restart_ms = now + pause_ms;
        return;
    }

    if (now < g_hotline.next_restart_ms) {
        return;
    }

    if (g_hotline.queued) {
        g_hotline.current_key = g_hotline.queued_key;
        g_hotline.current_digits = g_hotline.queued_digits;
        g_hotline.current_source = g_hotline.queued_source;
        g_hotline.current_route = g_hotline.queued_route;
        g_hotline.queued = false;
        g_hotline.queued_key = "";
        g_hotline.queued_digits = "";
        g_hotline.queued_source = "NONE";
        g_hotline.queued_route = MediaRouteEntry{};
    }

    if (!playMediaRoute(g_hotline.current_route)) {
        Serial.printf("[Hotline] restart failed key=%s digits=%s\n", g_hotline.current_key.c_str(), g_hotline.current_digits.c_str());
        clearHotlineRuntimeState();
        return;
    }
    g_hotline.pending_restart = false;
    g_hotline.next_restart_ms = 0U;
}

MediaRouteEntry resolveEspNowMediaRoute(const String& message, const String& args) {
    MediaRouteEntry route;
    route.kind = MediaRouteKind::FILE;
    route.path = "";
    route.source = MediaSource::AUTO;

    String normalized_message = message;
    normalized_message.trim();
    normalized_message.toUpperCase();

    if (parseMediaRouteFromArgs(args, route, true) && mediaRouteHasPayload(route)) {
        return route;
    }

    for (const EspNowCallMapEntry& entry : g_espnow_call_map) {
        if (!entry.keyword.equalsIgnoreCase(normalized_message)) {
            continue;
        }
        if (mediaRouteHasPayload(entry.route)) {
            route = entry.route;
            return route;
        }
    }

    if (normalized_message.isEmpty()) {
        return route;
    }
    normalized_message.toLowerCase();
    route.kind = MediaRouteKind::FILE;
    route.path = "/" + normalized_message + ".wav";
    route.source = MediaSource::AUTO;
    return route;
}

DispatchResponse makeEspNowCallResponse(bool ok, const String& message, const MediaRouteEntry& route, bool pending) {
    DispatchResponse res = makeResponse(ok, ok ? (pending ? "ESPNOW_CALL_RINGING" : "ESPNOW_CALL_PLAY") : "ESPNOW_CALL_FAILED");
    JsonDocument payload;
    payload["call"] = message;
    JsonObject audio = payload["audio"].to<JsonObject>();
    audio["kind"] = mediaRouteKindToString(route.kind);
    if (route.kind == MediaRouteKind::TONE) {
        audio["profile"] = toneProfileToString(route.tone.profile);
        audio["event"] = toneEventToString(route.tone.event);
    } else {
        audio["path"] = route.path;
        audio["source"] = mediaSourceToString(route.source);
        JsonObject playback = audio["playback"].to<JsonObject>();
        playback["loop"] = route.playback.loop;
        playback["pause_ms"] = route.playback.pause_ms;
    }
    payload["pending"] = pending;
    res.json = "";
    res.raw = "";
    res.ok = ok;
    String json;
    serializeJson(payload, json);
    res.json = json;
    return res;
}

bool mapHotlineValidationToAckEvent(const String& raw_state, const char** out_event_name) {
    if (out_event_name == nullptr) {
        return false;
    }

    String state = raw_state;
    state.trim();
    state.toUpperCase();
    if (state == "WIN1" || state == "ACK_WIN1") {
        *out_event_name = "ACK_WIN1";
        return true;
    }
    if (state == "WIN2" || state == "ACK_WIN2") {
        *out_event_name = "ACK_WIN2";
        return true;
    }
    if (state == "WARNING" || state == "ACK_WARNING") {
        *out_event_name = "ACK_WARNING";
        return true;
    }
    return false;
}

bool resolveHotlineValidationCueRoute(const char* ack_event_name, MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    if (ack_event_name == nullptr || ack_event_name[0] == '\0') {
        return false;
    }
    if (!g_active_scene_id.isEmpty() && resolveHotlineSceneRoute(g_active_scene_id, out_route)) {
        return true;
    }

    const char* stem = nullptr;
    if (strcmp(ack_event_name, "ACK_WARNING") == 0) {
        stem = "scene_broken_2";
    } else if (strcmp(ack_event_name, "ACK_WIN1") == 0 || strcmp(ack_event_name, "ACK_WIN2") == 0) {
        stem = "scene_win_2";
    }
    if (stem == nullptr) {
        return false;
    }
    out_route = buildHotlineSdVoiceRoute(stem, false, 0U);
    return mediaRouteHasPayload(out_route);
}

void playHotlineValidationCue(const char* ack_event_name) {
    MediaRouteEntry route;
    if (!resolveHotlineValidationCueRoute(ack_event_name, route)) {
        return;
    }

    const bool hotline_busy = (g_telephony.state() == TelephonyState::OFF_HOOK) ||
                              (g_telephony.state() == TelephonyState::PLAYING_MESSAGE) ||
                              g_slic.isHookOff();
    if (hotline_busy) {
        g_audio.stopTone();
        g_audio.stopPlayback();
        if (!playMediaRoute(route)) {
            Serial.printf("[RTC_BL_PHONE] validation cue play_failed event=%s path=%s\n",
                          ack_event_name == nullptr ? "" : ack_event_name,
                          route.path.c_str());
        }
        return;
    }

    g_pending_espnow_call_media = route;
    g_pending_espnow_call = mediaRouteHasPayload(route);
    if (g_pending_espnow_call) {
        g_telephony.triggerIncomingRing();
    }
}

bool parseHotlineValidateAckFlag(const String& raw_token, bool& out_ack_requested) {
    String token = raw_token;
    token.trim();
    token.toUpperCase();
    if (token == "ACK" || token == "TRUE" || token == "YES" || token == "1") {
        out_ack_requested = true;
        return true;
    }
    if (token == "NOACK" || token == "FALSE" || token == "NO" || token == "0") {
        out_ack_requested = false;
        return true;
    }
    return false;
}

DispatchResponse dispatchHotlineValidateCommand(const String& args) {
    String state_token;
    String rest;
    if (!splitFirstToken(args, state_token, rest) || state_token.isEmpty()) {
        return makeResponse(false, "HOTLINE_VALIDATE invalid_args");
    }

    bool ack_requested = false;
    if (!rest.isEmpty()) {
        String ack_token;
        String trailing;
        if (!splitFirstToken(rest, ack_token, trailing) || !trailing.isEmpty()) {
            return makeResponse(false, "HOTLINE_VALIDATE invalid_args");
        }
        if (!parseHotlineValidateAckFlag(ack_token, ack_requested)) {
            return makeResponse(false, "HOTLINE_VALIDATE invalid_ack_flag");
        }
    }

    const char* ack_event_name = nullptr;
    if (!mapHotlineValidationToAckEvent(state_token, &ack_event_name)) {
        return makeResponse(false, "HOTLINE_VALIDATE invalid_state");
    }

    if (!g_espnow.isReady()) {
        return makeResponse(false, "HOTLINE_VALIDATE espnow_not_ready");
    }

    JsonDocument frame;
    frame["msg_id"] = String("hv-") + String(millis());
    frame["seq"] = static_cast<uint32_t>(millis());
    frame["type"] = "command";
    frame["ack"] = ack_requested;
    JsonObject payload = frame["payload"].to<JsonObject>();
    payload["cmd"] = "SC_EVENT";
    JsonObject event_args = payload["args"].to<JsonObject>();
    event_args["event_type"] = "espnow";
    event_args["event_name"] = ack_event_name;

    String wire;
    serializeJson(frame, wire);
    const bool sent = g_espnow.sendJson("broadcast", wire);
    g_hotline.last_notify_event = String("validate_") + ack_event_name;
    g_hotline.last_notify_ok = sent;
    if (!sent) {
        return makeResponse(false, "HOTLINE_VALIDATE send_failed");
    }
    playHotlineValidationCue(ack_event_name);
    return makeResponse(true, String("HOTLINE_VALIDATE ") + ack_event_name);
}

DispatchResponse dispatchWaitingValidationCommand() {
    if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
        return makeResponse(false, "WAITING_VALIDATION busy");
    }
    g_pending_espnow_call_media = buildHotlineSdVoiceRoute("enter_code_5", false, 0U);
    g_pending_espnow_call = mediaRouteHasPayload(g_pending_espnow_call_media);
    g_telephony.triggerIncomingRing();
    g_hotline.last_notify_event = "waiting_validation";
    g_hotline.last_notify_ok = true;
    return makeResponse(true, "WAITING_VALIDATION");
}

bool handleIncomingEspNowCallCommand(const String& command_line, DispatchResponse& out) {
    String keyword;
    String args;
    if (!splitFirstToken(command_line, keyword, args)) {
        return false;
    }

    keyword.trim();
    keyword.toUpperCase();

    if (keyword == "WAITING_VALIDATION") {
        out = dispatchWaitingValidationCommand();
        return true;
    }

    if (!keyword.startsWith("LA_")) {
        return false;
    }

    if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
        out = makeResponse(false, "ESPNOW_CALL_BUSY");
        return true;
    }

    const MediaRouteEntry route = resolveEspNowMediaRoute(keyword, args);
    if (!mediaRouteHasPayload(route)) {
        out = makeResponse(false, "ESPNOW_CALL_NO_AUDIO");
        return true;
    }

    g_pending_espnow_call_media = route;
    g_pending_espnow_call = true;
    g_telephony.triggerIncomingRing();

    out = makeEspNowCallResponse(true, keyword, route, true);
    return true;
}

bool buildEspNowEnvelopeCommand(JsonVariantConst payload,
                                String& out_cmd,
                                String& out_msg_id,
                                uint32_t& out_seq,
                                bool& out_ack_requested) {
    out_cmd = "";
    out_msg_id = "";
    out_seq = 0;
    out_ack_requested = true;

    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    JsonObjectConst obj = payload.as<JsonObjectConst>();
    if (!obj["type"].is<const char*>()) {
        return false;
    }

    String type = obj["type"] | "";
    type.toLowerCase();
    if (type != "command" && type != "request" && type != "cmd") {
        return false;
    }

    out_msg_id = obj["msg_id"] | "";
    out_seq = obj["seq"] | 0;
    out_ack_requested = obj["ack"] | true;

    JsonVariantConst body = obj["payload"];
    if (body.isNull()) {
        return false;
    }

    if (body.is<const char*>()) {
        out_cmd = body.as<const char*>();
        out_cmd.trim();
        return !out_cmd.isEmpty();
    }

    if (body.is<JsonObjectConst>()) {
        JsonObjectConst body_obj = body.as<JsonObjectConst>();
        const String cmd = body_obj["cmd"] | "";
        if (!cmd.isEmpty()) {
            out_cmd = cmd;
            out_cmd.trim();
            if (out_cmd.isEmpty()) {
                return false;
            }

            if (!body_obj["args"].isNull()) {
                String args;
                serializeJson(body_obj["args"], args);
                args.trim();
                if (!args.isEmpty() && args != "null") {
                    out_cmd += " ";
                    out_cmd += args;
                }
            }
            return true;
        }
    }

    return extractBridgeCommand(body, out_cmd);
}

bool buildRtcBlV1BridgeCommand(JsonVariantConst payload,
                               String& out_cmd,
                               String& out_request_id,
                               bool& out_is_v1) {
    out_is_v1 = false;
    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    JsonObjectConst obj = payload.as<JsonObjectConst>();
    const String proto = obj["proto"] | "";
    if (!proto.equalsIgnoreCase("rtcbl/1")) {
        return false;
    }

    const String cmd = obj["cmd"] | "";
    if (cmd.isEmpty()) {
        return false;
    }

    out_cmd = cmd;
    out_cmd.trim();
    if (out_cmd.isEmpty()) {
        return false;
    }

    out_request_id = obj["id"] | "";
    out_is_v1 = true;

    if (obj["args"].isNull()) {
        return true;
    }

    String args;
    serializeJson(obj["args"], args);
    args.trim();
    if (!args.isEmpty() && args != "null") {
        out_cmd += " ";
        out_cmd += args;
    }

    return true;
}

bool isMacAddressString(const String& value) {
    uint8_t mac[6] = {0};
    return A252ConfigStore::parseMac(value, mac);
}

bool parseSceneIdFromArgs(const String& args, String& scene_id) {
    scene_id = "";
    String normalized = args;
    normalized.trim();
    if (normalized.isEmpty()) {
        return false;
    }

    if (normalized[0] == '{') {
        JsonDocument doc;
        if (deserializeJson(doc, normalized) == DeserializationError::Ok && doc.is<JsonObject>()) {
            scene_id = doc["id"] | "";
            scene_id.trim();
            return !scene_id.isEmpty();
        }
        return false;
    }

    if (normalized[0] == '"') {
        if (normalized.length() >= 2U) {
            scene_id = normalized.substring(1, normalized.length() - 1);
            scene_id.trim();
        }
        return !scene_id.isEmpty();
    }

    String rest;
    splitFirstToken(normalized, scene_id, rest);
    scene_id.trim();
    return !scene_id.isEmpty();
}

AudioConfig buildI2sConfig(const A252PinsConfig& pins_cfg, const A252AudioConfig& audio_cfg) {
    AudioConfig cfg;
    cfg.port = I2S_NUM_0;
    cfg.sample_rate = audio_cfg.sample_rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.bck_pin = pins_cfg.i2s_bck;
    cfg.ws_pin = pins_cfg.i2s_ws;
    cfg.data_out_pin = pins_cfg.i2s_dout;
    cfg.data_in_pin = pins_cfg.i2s_din;
    cfg.capture_adc_pin = pins_cfg.slic_adc_in;
    cfg.enable_capture = audio_cfg.enable_capture;
    cfg.adc_dsp_enabled = audio_cfg.adc_dsp_enabled;
    cfg.adc_fft_enabled = audio_cfg.adc_fft_enabled;
    cfg.adc_dsp_fft_downsample = audio_cfg.adc_dsp_fft_downsample;
    cfg.adc_fft_ignore_low_bin = audio_cfg.adc_fft_ignore_low_bin;
    cfg.adc_fft_ignore_high_bin = audio_cfg.adc_fft_ignore_high_bin;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    cfg.hybrid_telco_clock_policy = true;
    // Hotline profile: hard-disable WAV auto loudness processing.
    cfg.wav_auto_normalize_limiter = false;
    cfg.wav_target_rms_dbfs = audio_cfg.wav_target_rms_dbfs;
    cfg.wav_limiter_ceiling_dbfs = audio_cfg.wav_limiter_ceiling_dbfs;
    cfg.wav_limiter_attack_ms = audio_cfg.wav_limiter_attack_ms;
    cfg.wav_limiter_release_ms = audio_cfg.wav_limiter_release_ms;
    return cfg;
}

void applyPcm5102ControlPins(const A252PinsConfig& pins_cfg) {
    auto apply = [](int pin, int value) {
        if (pin < 0) {
            return;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value);
    };

    apply(pins_cfg.pcm_flt, LOW);
    apply(pins_cfg.pcm_demp, LOW);
    apply(pins_cfg.pcm_xsmt, HIGH);
    apply(pins_cfg.pcm_fmt, LOW);
}

bool applyHardwareConfig() {
    g_hw_status = HardwareInitStatus{};
    String pin_validation_error;
    if (!A252ConfigStore::validatePins(g_pins_cfg, pin_validation_error)) {
        Serial.printf("[RTC_BL_PHONE] invalid pins configuration: %s\n", pin_validation_error.c_str());
        return false;
    }

    auto u8_pin = [](int pin) { return static_cast<uint8_t>(pin); };
    const SlicPins slic_pins = {
        .pin_rm = u8_pin(g_pins_cfg.slic_rm),
        .pin_fr = u8_pin(g_pins_cfg.slic_fr),
        .pin_shk = u8_pin(g_pins_cfg.slic_shk),
        .pin_line_enable = static_cast<int8_t>(-1),
        .pin_pd = static_cast<int8_t>(g_pins_cfg.slic_pd),
        .hook_active_high = g_pins_cfg.hook_active_high,
    };

    const bool slic_ok = g_slic.begin(slic_pins);
    g_slic.setPowerDown(false);
    g_slic.setRing(false);

    bool codec_ok = true;
    if (g_profile == BoardProfile::ESP32_A252) {
        codec_ok = g_codec.begin(g_pins_cfg.es8388_sda, g_pins_cfg.es8388_scl);
        g_codec.setVolume(g_audio_cfg.volume);
        g_codec.setMute(g_audio_cfg.mute);
        g_codec.setRoute(g_audio_cfg.route);
    }

    applyPcm5102ControlPins(g_pins_cfg);
    const AudioConfig audio = buildI2sConfig(g_pins_cfg, g_audio_cfg);
    bool audio_ok = g_audio.begin(audio);
    if (!audio_ok) {
        Serial.println("[RTC_BL_PHONE] audio init failed, retrying once");
        audio_ok = g_audio.begin(audio);
    }
    g_audio.resetMetrics();
    clearHotlineRuntimeState();

    g_telephony.begin(g_profile, g_slic, g_audio);
    g_telephony.setDialMatchCallback([](const String& digits) {
        return resolveDialRouteMatch(digits);
    });
    g_telephony.setDialCallback([](const String& number, bool from_pulse) {
        String state;
        const bool ok = triggerHotlineRouteForDigits(number, from_pulse, &state);
        Serial.printf("[Telephony] dial route number=%s source=%s state=%s ok=%s\n",
                      number.c_str(),
                      from_pulse ? "PULSE" : "DTMF",
                      state.c_str(),
                      ok ? "true" : "false");
        if (!ok) {
            const bool busy_ok = g_audio.playTone(ToneProfile::FR_FR, ToneEvent::BUSY);
            Serial.printf("[Telephony] busy tone ok=%s\n", busy_ok ? "true" : "false");
        }
        return ok;
    });
    g_telephony.setAnswerCallback([]() {
        if (!g_pending_espnow_call || !mediaRouteHasPayload(g_pending_espnow_call_media)) {
            Serial.println("[Telephony] answer callback disabled");
            return false;
        }

        const MediaRouteEntry media = g_pending_espnow_call_media;
        g_pending_espnow_call_media = MediaRouteEntry{};
        g_pending_espnow_call = false;

        const bool ok = playMediaRoute(media);
        if (media.kind == MediaRouteKind::TONE) {
            Serial.printf("[Telephony] answer callback -> play tone profile=%s event=%s ok=%s\n",
                          toneProfileToString(media.tone.profile),
                          toneEventToString(media.tone.event),
                          ok ? "true" : "false");
        } else {
            Serial.printf("[Telephony] answer callback -> play file '%s' source=%s ok=%s\n",
                          media.path.c_str(),
                          mediaSourceToString(media.source),
                          ok ? "true" : "false");
        }
        return ok;
    });

    g_hw_status.slic_ready = slic_ok;
    g_hw_status.codec_ready = codec_ok;
    g_hw_status.audio_ready = audio_ok;
    g_hw_status.init_ok = slic_ok && codec_ok && audio_ok;

    Serial.printf("[RTC_BL_PHONE] HW init slic=%s codec=%s audio=%s init=%s\n",
                  slic_ok ? "ok" : "fail",
                  codec_ok ? "ok" : "fail",
                  audio_ok ? "ok" : "fail",
                  g_hw_status.init_ok ? "ok" : "fail");

    return g_hw_status.init_ok;
}

void appendAudioMetrics(JsonObject root) {
    const AudioRuntimeMetrics metrics = g_audio.metrics();

    root["audio_frames_requested"] = metrics.frames_requested;
    root["audio_frames_read"] = metrics.frames_read;
    root["audio_drop_frames"] = metrics.drop_frames;
    root["audio_underrun_count"] = metrics.underrun_count;
    root["audio_last_latency_ms"] = metrics.last_latency_ms;
    root["audio_max_latency_ms"] = metrics.max_latency_ms;

    JsonObject audio = root["audio"].to<JsonObject>();
    audio["full_duplex"] = g_audio.supportsFullDuplex();
    audio["ready"] = g_audio.isReady();
    audio["dial_tone_active"] = g_audio.isDialToneActive();
    audio["tone_route_active"] = g_audio.isToneRouteActive();
    audio["tone_rendering"] = g_audio.isToneRenderingActive();
    audio["tone_active"] = g_audio.isToneActive();
    audio["tone_profile"] = toneProfileToString(g_audio.activeToneProfile());
    audio["tone_event"] = toneEventToString(g_audio.activeToneEvent());
    audio["tone_engine"] = g_audio.isToneRenderingActive() ? "CODE" : "NONE";
    audio["playback_input_sample_rate"] = g_audio.playbackInputSampleRate();
    audio["playback_input_bits_per_sample"] = g_audio.playbackInputBitsPerSample();
    audio["playback_input_channels"] = g_audio.playbackInputChannels();
    audio["playback_output_sample_rate"] = g_audio.playbackOutputSampleRate();
    audio["playback_output_bits_per_sample"] = g_audio.playbackOutputBitsPerSample();
    audio["playback_output_channels"] = g_audio.playbackOutputChannels();
    audio["playback_resampler_active"] = g_audio.playbackResamplerActive();
    audio["playback_channel_upmix_active"] = g_audio.playbackChannelUpmixActive();
    audio["playback_loudness_auto"] = g_audio.playbackLoudnessAuto();
    audio["playback_loudness_gain_db"] = g_audio.playbackLoudnessGainDb();
    audio["playback_limiter_active"] = g_audio.playbackLimiterActive();
    audio["playback_rate_fallback"] = g_audio.playbackRateFallback();
    audio["playback_copy_source_bytes"] = g_audio.playbackCopySourceBytes();
    audio["playback_copy_accepted_bytes"] = g_audio.playbackCopyAcceptedBytes();
    audio["playback_copy_loss_bytes"] = g_audio.playbackCopyLossBytes();
    audio["playback_copy_loss_events"] = g_audio.playbackCopyLossEvents();
    audio["playback_last_error"] = g_audio.playbackLastError();
    audio["playback_sample_rate"] = g_audio.playbackSampleRate();
    audio["playback_bits_per_sample"] = g_audio.playbackBitsPerSample();
    audio["playback_channels"] = g_audio.playbackChannels();
    audio["playback_format_overridden"] = g_audio.playbackFormatOverridden();
    audio["playing"] = g_audio.isPlaying();
    audio["sd_ready"] = g_audio.isSdReady();
    audio["littlefs_ready"] = g_audio.isLittleFsReady();
    audio["storage_default_policy"] = "SD_THEN_LITTLEFS";
    const String last_storage_path = g_audio.lastStoragePath();
    audio["storage_last_source"] = last_storage_path.isEmpty() ? "NONE" : mediaSourceToString(g_audio.lastStorageSource());
    audio["storage_last_path"] = last_storage_path;
    audio["frames"] = metrics.frames_read;
    audio["underrun"] = metrics.underrun_count;
    audio["drop"] = metrics.drop_frames;
    audio["latence_ms"] = metrics.last_latency_ms;
    audio["adc_fft_peak_bin"] = metrics.adc_fft_peak_bin;
    audio["adc_fft_peak_freq_hz"] = metrics.adc_fft_peak_freq_hz;
    audio["adc_fft_peak_mag"] = metrics.adc_fft_peak_magnitude;
    audio["tone_jitter_us_max"] = g_audio.toneJitterUsMax();
    audio["tone_write_miss_count"] = g_audio.toneWriteMissCount();
}

void fillStatusSnapshot(JsonObject root) {
    root["board_profile"] = boardProfileToString(g_profile);
    root["active_scene"] = g_active_scene_id;

    JsonObject telephony = root["telephony"].to<JsonObject>();
    telephony["state"] = telephonyStateToString(g_telephony.state());
    telephony["hook"] = g_slic.isHookOff() ? "OFF_HOOK" : "ON_HOOK";
    telephony["powered"] = g_telephony.isTelephonyPowered();
    telephony["power_probe_active"] = g_telephony.isPowerProbeActive();
    telephony["slic_power_down"] = g_slic.isPowerDownEnabled();
    telephony["dial_buffer"] = g_telephony.dialBuffer();
    telephony["dial_source"] = g_telephony.dialSource();
    telephony["dial_match_state"] = dialMatchStateToString(g_telephony.dialMatchState());
    telephony["hotline_active"] = g_hotline.active;
    telephony["hotline_current_key"] = g_hotline.current_key;
    telephony["hotline_queued_key"] = g_hotline.queued_key;
    telephony["hotline_next_restart_ms"] = g_hotline.next_restart_ms;
    telephony["pending_espnow_call"] = g_pending_espnow_call;
    telephony["pending_espnow_call_kind"] = mediaRouteKindToString(g_pending_espnow_call_media.kind);
    if (g_pending_espnow_call_media.kind == MediaRouteKind::TONE) {
        telephony["pending_espnow_call_profile"] = toneProfileToString(g_pending_espnow_call_media.tone.profile);
        telephony["pending_espnow_call_event"] = toneEventToString(g_pending_espnow_call_media.tone.event);
        telephony["pending_espnow_call_audio"] = "";
        telephony["pending_espnow_call_source"] = "AUTO";
    } else {
        telephony["pending_espnow_call_profile"] = "NONE";
        telephony["pending_espnow_call_event"] = "NONE";
        telephony["pending_espnow_call_audio"] = g_pending_espnow_call_media.path;
        telephony["pending_espnow_call_source"] = mediaSourceToString(g_pending_espnow_call_media.source);
    }

    appendAudioMetrics(root);

    JsonObject scope = root["scope_display"].to<JsonObject>();
    scope["supported"] = g_scope_display.supported();
    scope["enabled"] = g_scope_display.enabled();
    scope["frequency"] = g_scope_display.frequency();
    scope["amplitude"] = g_scope_display.amplitude();

    JsonObject espnow = root["espnow"].to<JsonObject>();
    g_espnow.statusToJson(espnow);
    espnow["hotline_notify_last_event"] = g_hotline.last_notify_event;
    espnow["hotline_notify_last_ok"] = g_hotline.last_notify_ok;
    espnow["peer_discovery_enabled"] = g_espnow_peer_discovery.enabled;
    espnow["peer_discovery_interval_ms"] = g_espnow_peer_discovery.interval_ms;
    espnow["peer_discovery_ack_window_ms"] = g_espnow_peer_discovery.ack_window_ms;
    espnow["peer_discovery_next_probe_ms"] = g_espnow_peer_discovery.next_probe_ms;
    espnow["peer_discovery_probe_pending"] = g_espnow_peer_discovery.probe_pending;
    espnow["peer_discovery_probe_msg_id"] = g_espnow_peer_discovery.probe_msg_id;
    espnow["peer_discovery_probe_seq"] = g_espnow_peer_discovery.probe_seq;
    espnow["peer_discovery_probes_sent"] = g_espnow_peer_discovery.probes_sent;
    espnow["peer_discovery_probe_send_fail"] = g_espnow_peer_discovery.probe_send_fail;
    espnow["peer_discovery_probe_ack_seen"] = g_espnow_peer_discovery.probe_ack_seen;
    espnow["peer_discovery_auto_add_new_ok"] = g_espnow_peer_discovery.auto_add_new_ok;
    espnow["peer_discovery_auto_add_fail"] = g_espnow_peer_discovery.auto_add_fail;
    espnow["peer_discovery_last_mac"] = g_espnow_peer_discovery.last_mac;
    espnow["peer_discovery_last_device_name"] = g_espnow_peer_discovery.last_device_name;
    espnow["peer_discovery_last_error"] = g_espnow_peer_discovery.last_error;

    JsonObject hw = root["hw"].to<JsonObject>();
    hw["init_ok"] = g_hw_status.init_ok;
    hw["slic_ready"] = g_hw_status.slic_ready;
    hw["codec_ready"] = g_hw_status.codec_ready;
    hw["audio_ready"] = g_hw_status.audio_ready;

    JsonObject config = root["config"].to<JsonObject>();
    A252ConfigStore::pinsToJson(g_pins_cfg, config["pins"].to<JsonObject>());
    A252ConfigStore::audioToJson(g_audio_cfg, config["audio"].to<JsonObject>());
    config["espnow_device_name"] = g_peer_store.device_name;
    A252ConfigStore::espNowCallMapToJson(g_espnow_call_map, config["espnow_call_map"].to<JsonObject>());
    A252ConfigStore::dialMediaMapToJson(g_dial_media_map, config["dial_media_map"].to<JsonObject>());
    JsonObject migrations = root["config_migrations"].to<JsonObject>();
    migrations["espnow_call_map_reset"] = g_config_migrations.espnow_call_map_reset;
    migrations["dial_media_map_reset"] = g_config_migrations.dial_media_map_reset;

    JsonObject firmware = root["firmware"].to<JsonObject>();
    firmware["build_id"] = kFirmwareBuildId;
    firmware["git_sha"] = kFirmwareGitSha;
    firmware["contract_version"] = kFirmwareContractVersion;

    JsonArray peers = config["espnow_peers"].to<JsonArray>();
    A252ConfigStore::peersToJson(g_peer_store, peers);
}

bool applyPinsPatch(JsonVariantConst patch, A252PinsConfig& target, String& error) {
    A252PinsConfig next = target;

    if (patch["i2s"]["bck"].is<int>()) {
        next.i2s_bck = patch["i2s"]["bck"].as<int>();
    }
    if (patch["i2s"]["ws"].is<int>()) {
        next.i2s_ws = patch["i2s"]["ws"].as<int>();
    }
    if (patch["i2s"]["dout"].is<int>()) {
        next.i2s_dout = patch["i2s"]["dout"].as<int>();
    }
    if (patch["i2s"]["din"].is<int>()) {
        next.i2s_din = patch["i2s"]["din"].as<int>();
    }

    if (patch["codec_i2c"]["sda"].is<int>()) {
        next.es8388_sda = patch["codec_i2c"]["sda"].as<int>();
    }
    if (patch["codec_i2c"]["scl"].is<int>()) {
        next.es8388_scl = patch["codec_i2c"]["scl"].as<int>();
    }

    if (patch["slic"]["rm"].is<int>()) {
        next.slic_rm = patch["slic"]["rm"].as<int>();
    }
    if (patch["slic"]["fr"].is<int>()) {
        next.slic_fr = patch["slic"]["fr"].as<int>();
    }
    if (patch["slic"]["shk"].is<int>()) {
        next.slic_shk = patch["slic"]["shk"].as<int>();
    }
    if (patch["slic"]["pd"].is<int>()) {
        next.slic_pd = patch["slic"]["pd"].as<int>();
    }
    if (patch["slic"]["adc_in"].is<int>()) {
        next.slic_adc_in = patch["slic"]["adc_in"].as<int>();
    }
    if (patch["slic"]["hook_active_high"].is<bool>()) {
        next.hook_active_high = patch["slic"]["hook_active_high"].as<bool>();
    }
    if (patch["pcm"]["flt"].is<int>()) {
        next.pcm_flt = patch["pcm"]["flt"].as<int>();
    }
    if (patch["pcm"]["demp"].is<int>()) {
        next.pcm_demp = patch["pcm"]["demp"].as<int>();
    }
    if (patch["pcm"]["xsmt"].is<int>()) {
        next.pcm_xsmt = patch["pcm"]["xsmt"].as<int>();
    }
    if (patch["pcm"]["fmt"].is<int>()) {
        next.pcm_fmt = patch["pcm"]["fmt"].as<int>();
    }

    if (patch["i2s_bck"].is<int>()) {
        next.i2s_bck = patch["i2s_bck"].as<int>();
    }
    if (patch["i2s_ws"].is<int>()) {
        next.i2s_ws = patch["i2s_ws"].as<int>();
    }
    if (patch["i2s_dout"].is<int>()) {
        next.i2s_dout = patch["i2s_dout"].as<int>();
    }
    if (patch["i2s_din"].is<int>()) {
        next.i2s_din = patch["i2s_din"].as<int>();
    }

    if (patch["es8388_sda"].is<int>()) {
        next.es8388_sda = patch["es8388_sda"].as<int>();
    }
    if (patch["es8388_scl"].is<int>()) {
        next.es8388_scl = patch["es8388_scl"].as<int>();
    }

    if (patch["slic_rm"].is<int>()) {
        next.slic_rm = patch["slic_rm"].as<int>();
    }
    if (patch["slic_fr"].is<int>()) {
        next.slic_fr = patch["slic_fr"].as<int>();
    }
    if (patch["slic_shk"].is<int>()) {
        next.slic_shk = patch["slic_shk"].as<int>();
    }
    if (patch["slic_pd"].is<int>()) {
        next.slic_pd = patch["slic_pd"].as<int>();
    }
    if (patch["slic_adc_in"].is<int>()) {
        next.slic_adc_in = patch["slic_adc_in"].as<int>();
    }
    if (patch["hook_active_high"].is<bool>()) {
        next.hook_active_high = patch["hook_active_high"].as<bool>();
    }
    if (patch["pcm_flt"].is<int>()) {
        next.pcm_flt = patch["pcm_flt"].as<int>();
    }
    if (patch["pcm_demp"].is<int>()) {
        next.pcm_demp = patch["pcm_demp"].as<int>();
    }
    if (patch["pcm_xsmt"].is<int>()) {
        next.pcm_xsmt = patch["pcm_xsmt"].as<int>();
    }
    if (patch["pcm_fmt"].is<int>()) {
        next.pcm_fmt = patch["pcm_fmt"].as<int>();
    }

    next.slic_line = -1;

    if (!A252ConfigStore::validatePins(next, error)) {
        return false;
    }

    target = next;
    return true;
}

bool applyAudioPatch(JsonVariantConst patch, A252AudioConfig& target, String& error) {
    A252AudioConfig next = target;

    if (patch["sample_rate"].is<uint32_t>()) {
        next.sample_rate = patch["sample_rate"].as<uint32_t>();
    }
    if (patch["bits_per_sample"].is<uint8_t>()) {
        next.bits_per_sample = patch["bits_per_sample"].as<uint8_t>();
    }
    if (patch["enable_capture"].is<bool>()) {
        next.enable_capture = patch["enable_capture"].as<bool>();
    }
    if (patch["volume"].is<uint8_t>()) {
        next.volume = patch["volume"].as<uint8_t>();
    }
    if (patch["mute"].is<bool>()) {
        next.mute = patch["mute"].as<bool>();
    }
    if (patch["adc_dsp_enabled"].is<bool>()) {
        next.adc_dsp_enabled = patch["adc_dsp_enabled"].as<bool>();
    }
    if (patch["adc_fft_enabled"].is<bool>()) {
        next.adc_fft_enabled = patch["adc_fft_enabled"].as<bool>();
    }
    if (patch["adc_dsp_fft_downsample"].is<int>()) {
        const int ds = patch["adc_dsp_fft_downsample"].as<int>();
        if (ds >= 0 && ds <= 255) {
            next.adc_dsp_fft_downsample = static_cast<uint8_t>(ds);
        }
    } else if (patch["adc_dsp_fft_downsample"].is<uint16_t>()) {
        next.adc_dsp_fft_downsample = static_cast<uint8_t>(patch["adc_dsp_fft_downsample"].as<uint16_t>());
    }
    if (patch["adc_fft_ignore_low_bin"].is<int>()) {
        const int low_bin = patch["adc_fft_ignore_low_bin"].as<int>();
        if (low_bin >= 0 && low_bin <= static_cast<int>(UINT16_MAX)) {
            next.adc_fft_ignore_low_bin = static_cast<uint16_t>(low_bin);
        }
    } else if (patch["adc_fft_ignore_low_bin"].is<uint16_t>()) {
        next.adc_fft_ignore_low_bin = patch["adc_fft_ignore_low_bin"].as<uint16_t>();
    }
    if (patch["adc_fft_ignore_high_bin"].is<int>()) {
        const int high_bin = patch["adc_fft_ignore_high_bin"].as<int>();
        if (high_bin >= 0 && high_bin <= static_cast<int>(UINT16_MAX)) {
            next.adc_fft_ignore_high_bin = static_cast<uint16_t>(high_bin);
        }
    } else if (patch["adc_fft_ignore_high_bin"].is<uint16_t>()) {
        next.adc_fft_ignore_high_bin = patch["adc_fft_ignore_high_bin"].as<uint16_t>();
    }
    if (patch["route"].is<const char*>()) {
        next.route = patch["route"].as<const char*>();
        next.route.toLowerCase();
    }
    if (patch["clock_policy"].is<const char*>()) {
        next.clock_policy = patch["clock_policy"].as<const char*>();
        next.clock_policy.trim();
        next.clock_policy.toUpperCase();
    }
    if (patch["wav_loudness_policy"].is<const char*>()) {
        next.wav_loudness_policy = patch["wav_loudness_policy"].as<const char*>();
        next.wav_loudness_policy.trim();
        next.wav_loudness_policy.toUpperCase();
    }
    if (patch["wav_target_rms_dbfs"].is<int>()) {
        next.wav_target_rms_dbfs = static_cast<int16_t>(patch["wav_target_rms_dbfs"].as<int>());
    }
    if (patch["wav_limiter_ceiling_dbfs"].is<int>()) {
        next.wav_limiter_ceiling_dbfs = static_cast<int16_t>(patch["wav_limiter_ceiling_dbfs"].as<int>());
    }
    if (patch["wav_limiter_attack_ms"].is<int>()) {
        const int attack = patch["wav_limiter_attack_ms"].as<int>();
        if (attack >= 0 && attack <= static_cast<int>(UINT16_MAX)) {
            next.wav_limiter_attack_ms = static_cast<uint16_t>(attack);
        }
    }
    if (patch["wav_limiter_release_ms"].is<int>()) {
        const int release = patch["wav_limiter_release_ms"].as<int>();
        if (release >= 0 && release <= static_cast<int>(UINT16_MAX)) {
            next.wav_limiter_release_ms = static_cast<uint16_t>(release);
        }
    }

    if (g_profile == BoardProfile::ESP32_A252) {
        next.clock_policy = "HYBRID_TELCO";
        next.sample_rate = 8000U;
        next.bits_per_sample = 16U;
        next.wav_loudness_policy = "FIXED_GAIN_ONLY";
        next.volume = 100U;
    }

    if (!A252ConfigStore::validateAudio(next, error)) {
        return false;
    }
    target = next;
    return true;
}

bool parseStrictMediaRouteFromMapEntry(JsonVariantConst value, MediaRouteEntry& out_route, String& out_error) {
    out_route = MediaRouteEntry{};
    out_error = "";

    if (value.is<const char*>()) {
        out_route.kind = MediaRouteKind::FILE;
        out_route.path = sanitizeMediaPath(value.as<const char*>());
        out_route.source = MediaSource::AUTO;
        out_route.playback.loop = false;
        out_route.playback.pause_ms = 0U;
        if (out_route.path.isEmpty()) {
            out_error = "invalid_file_path";
            return false;
        }
        if (isLegacyToneWavPath(out_route.path)) {
            out_error = "tone_wav_deprecated_use_kind_tone";
            return false;
        }
        return true;
    }

    if (!value.is<JsonObjectConst>()) {
        out_error = "invalid_route";
        return false;
    }

    JsonObjectConst obj = value.as<JsonObjectConst>();
    MediaRouteKind kind = MediaRouteKind::FILE;
    if (obj["kind"].is<const char*>()) {
        if (!parseMediaRouteKind(obj["kind"].as<const char*>(), kind)) {
            out_error = "invalid_kind";
            return false;
        }
    } else if (obj["path"].is<const char*>()) {
        kind = MediaRouteKind::FILE;
    } else {
        out_error = "missing_kind";
        return false;
    }

    out_route.kind = kind;

    if (kind == MediaRouteKind::TONE) {
        if (!obj["profile"].is<const char*>() || !obj["event"].is<const char*>()) {
            out_error = "tone_missing_profile_event";
            return false;
        }
        if (!parseToneProfile(obj["profile"].as<const char*>(), out_route.tone.profile)) {
            out_error = "invalid_tone_profile";
            return false;
        }
        if (!parseToneEvent(obj["event"].as<const char*>(), out_route.tone.event)) {
            out_error = "invalid_tone_event";
            return false;
        }
        if (out_route.tone.profile == ToneProfile::NONE || out_route.tone.event == ToneEvent::NONE) {
            out_error = "invalid_tone_route";
            return false;
        }
        return true;
    }

    if (!obj["path"].is<const char*>()) {
        out_error = "file_missing_path";
        return false;
    }
    out_route.path = sanitizeMediaPath(obj["path"].as<const char*>());
    if (out_route.path.isEmpty()) {
        out_error = "invalid_file_path";
        return false;
    }
    if (isLegacyToneWavPath(out_route.path)) {
        out_error = "tone_wav_deprecated_use_kind_tone";
        return false;
    }
    out_route.source = MediaSource::AUTO;
    if (obj["source"].is<const char*>()) {
        MediaSource parsed_source = MediaSource::AUTO;
        if (!parseMediaSource(obj["source"].as<const char*>(), parsed_source)) {
            out_error = "invalid_file_source";
            return false;
        }
        out_route.source = parsed_source;
    }
    if (!parsePlaybackPolicyFromObject(obj, out_route.playback)) {
        out_error = "invalid_playback_policy";
        return false;
    }
    return true;
}

DispatchResponse applyEspNowCallMapSetImpl(const String& args, bool persist, const char* command_name) {
    const String command = command_name == nullptr ? String("ESPNOW_CALL_MAP_SET") : String(command_name);
    if (args.isEmpty()) {
        return makeResponse(false, command + " invalid_json");
    }

    JsonDocument doc;
    if (deserializeJson(doc, args) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return makeResponse(false, command + " invalid_json");
    }

    JsonObject obj = doc.as<JsonObject>();
    EspNowCallMap next;
    for (JsonPair pair : obj) {
        String keyword = pair.key().c_str();
        keyword.trim();
        keyword.toUpperCase();
        if (keyword.isEmpty()) {
            continue;
        }
        if (!keyword.startsWith("LA_")) {
            continue;
        }

        MediaRouteEntry route;
        String route_error;
        if (!parseStrictMediaRouteFromMapEntry(pair.value().as<JsonVariantConst>(), route, route_error)) {
            return makeResponse(false, command + " " + route_error + " " + keyword);
        }

        bool updated = false;
        for (EspNowCallMapEntry& entry : next) {
            if (entry.keyword.equalsIgnoreCase(keyword)) {
                entry.route = route;
                updated = true;
                break;
            }
        }
        if (!updated) {
            EspNowCallMapEntry created;
            created.keyword = keyword;
            created.route = route;
            next.push_back(created);
        }
    }

    if (next.empty()) {
        return makeResponse(false, command + " no_valid_entries");
    }

    if (persist) {
        String save_error;
        if (!A252ConfigStore::saveEspNowCallMap(next, &save_error)) {
            return makeResponse(false, command + " save_failed" + (save_error.isEmpty() ? "" : String(" ") + save_error));
        }
    }
    g_espnow_call_map = next;
    return makeResponse(true, command);
}

DispatchResponse applyEspNowCallMapSet(const String& args) {
    return applyEspNowCallMapSetImpl(args, true, "ESPNOW_CALL_MAP_SET");
}

DispatchResponse applyDialMediaMapSetImpl(const String& args, bool persist, const char* command_name) {
    const String command = command_name == nullptr ? String("DIAL_MEDIA_MAP_SET") : String(command_name);
    if (args.isEmpty()) {
        return makeResponse(false, command + " invalid_json");
    }

    JsonDocument doc;
    if (deserializeJson(doc, args) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return makeResponse(false, command + " invalid_json");
    }

    DialMediaMap next;
    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair pair : obj) {
        String number = pair.key().c_str();
        number.trim();
        if (number.isEmpty()) {
            continue;
        }
        if (!isDialMapNumberKey(number)) {
            return makeResponse(false, command + " invalid_number " + number);
        }

        MediaRouteEntry route;
        String route_error;
        if (!parseStrictMediaRouteFromMapEntry(pair.value().as<JsonVariantConst>(), route, route_error)) {
            return makeResponse(false, command + " " + route_error + " " + number);
        }
        DialMediaMapEntry created;
        created.number = number;
        created.route = route;
        next.push_back(created);
    }

    if (next.empty()) {
        return makeResponse(false, command + " no_valid_entries");
    }

    if (persist) {
        String save_error;
        if (!A252ConfigStore::saveDialMediaMap(next, &save_error)) {
            return makeResponse(false, command + " save_failed" + (save_error.isEmpty() ? "" : String(" ") + save_error));
        }
    }
    g_dial_media_map = next;
    return makeResponse(true, command);
}

DispatchResponse applyDialMediaMapSet(const String& args) {
    return applyDialMediaMapSetImpl(args, true, "DIAL_MEDIA_MAP_SET");
}

bool espNowCallMapHasLegacyToneWav(const EspNowCallMap& map) {
    for (const EspNowCallMapEntry& entry : map) {
        if (entry.route.kind != MediaRouteKind::FILE) {
            continue;
        }
        if (isLegacyToneWavPath(entry.route.path)) {
            return true;
        }
    }
    return false;
}

bool dialMediaMapHasLegacyToneWav(const DialMediaMap& map) {
    for (const DialMediaMapEntry& entry : map) {
        if (entry.route.kind != MediaRouteKind::FILE) {
            continue;
        }
        if (isLegacyToneWavPath(entry.route.path)) {
            return true;
        }
    }
    return false;
}

DispatchResponse executeCommandLine(const String& line) {
    return g_dispatcher.dispatch(line);
}

void registerCommands() {
    g_dispatcher.registerCommand("PING", [](const String&) {
        DispatchResponse res;
        res.ok = true;
        res.raw = "PONG";
        return res;
    });

    g_dispatcher.registerCommand("HELP", [](const String&) {
        DispatchResponse res;
        res.ok = true;
        res.raw = g_dispatcher.helpText();
        return res;
    });

    g_dispatcher.registerCommand("STATUS", [](const String&) {
        JsonDocument doc;
        fillStatusSnapshot(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("CALL", [](const String&) {
        g_telephony.triggerIncomingRing();
        return makeResponse(true, "CALL");
    });

    g_dispatcher.registerCommand("RING", [](const String&) {
        g_telephony.triggerIncomingRing();
        return makeResponse(true, "RING");
    });

    g_dispatcher.registerCommand("WIFI_STATUS", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        const wl_status_t status = WiFi.status();
        const bool connected = status == WL_CONNECTED;

        root["connected"] = connected;
        root["status"] = status;
        if (connected) {
            root["ssid"] = WiFi.SSID();
            root["ip"] = WiFi.localIP().toString();
            root["rssi"] = WiFi.RSSI();
            root["channel"] = WiFi.channel();
        } else {
            root["ssid"] = "";
            root["ip"] = "";
            root["rssi"] = 0;
            root["channel"] = 0;
        }
        root["mode"] = WiFi.getMode() == WIFI_MODE_STA
                           ? "STA"
                           : WiFi.getMode() == WIFI_MODE_AP
                                 ? "AP"
                                 : WiFi.getMode() == WIFI_MODE_APSTA
                                       ? "APSTA"
                                       : "NULL";
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WIFI_CONNECT", [](const String& args) {
        String ssid;
        String rest;
        if (!splitFirstToken(args, ssid, rest)) {
            return makeResponse(false, "WIFI_CONNECT invalid_args");
        }
        if (ssid.isEmpty()) {
            return makeResponse(false, "WIFI_CONNECT invalid_ssid");
        }
        String password;
        if (!rest.isEmpty()) {
            String trailing;
            if (!splitFirstToken(rest, password, trailing) || !trailing.isEmpty()) {
                return makeResponse(false, "WIFI_CONNECT invalid_args");
            }
        }
        const bool ok = g_wifi.connect(ssid, password);
        return makeResponse(ok, ok ? "WIFI_CONNECT" : "WIFI_CONNECT failed");
    });

    g_dispatcher.registerCommand("WIFI_SCAN", [](const String&) {
        JsonDocument doc;
        JsonArray networks = doc.to<JsonArray>();
        g_wifi.scanToJson(networks, 20);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WIFI_DISCONNECT", [](const String&) {
        g_wifi.disconnect(false);
        return makeResponse(true, "WIFI_DISCONNECT");
    });

    g_dispatcher.registerCommand("WIFI_RECONNECT", [](const String&) {
        const bool ok = g_wifi.reconnect();
        return makeResponse(ok, ok ? "WIFI_RECONNECT" : "WIFI_RECONNECT no_credentials");
    });

    g_dispatcher.registerCommand("UNLOCK", [](const String&) {
        g_slic.setLineEnabled(true);
        return makeResponse(true, "UNLOCK");
    });

    g_dispatcher.registerCommand("SLIC_PD_ON", [](const String&) {
        if (g_telephony.state() != TelephonyState::IDLE) {
            return makeResponse(false, "SLIC_PD_ON telephony_active");
        }
        g_telephony.forceTelephonyPower(false);
        return makeResponse(true, "SLIC_PD_ON");
    });

    g_dispatcher.registerCommand("SLIC_PD_OFF", [](const String&) {
        g_telephony.forceTelephonyPower(true);
        return makeResponse(true, "SLIC_PD_OFF");
    });

    g_dispatcher.registerCommand("SLIC_PD_STATUS", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["power_down"] = g_slic.isPowerDownEnabled();
        root["telephony_powered"] = g_telephony.isTelephonyPowered();
        root["power_probe_active"] = g_telephony.isPowerProbeActive();
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("NEXT", [](const String&) {
        if (g_active_scene_id.isEmpty()) {
            return makeResponse(false, "scene_not_found");
        }
        g_active_scene_id = "";
        return makeResponse(true, "NEXT");
    });

    g_dispatcher.registerCommand("STORY_REFRESH_SD", [](const String&) {
        return makeResponse(g_audio.isSdReady(), "STORY_REFRESH_SD");
    });

    g_dispatcher.registerCommand("SC_EVENT", [](const String&) {
        return makeResponse(true, "SC_EVENT");
    });

    g_dispatcher.registerCommand("SCENE", [](const String& args) {
        String scene_id;
        if (!parseSceneIdFromArgs(args, scene_id)) {
            return makeResponse(false, "missing_scene_id");
        }
        g_active_scene_id = scene_id;

        MediaRouteEntry scene_route;
        const bool scene_audio_mapped = resolveHotlineSceneRoute(scene_id, scene_route);
        bool scene_audio_started = false;
        String scene_audio_state = "none";
        if (scene_audio_mapped) {
            if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
                scene_audio_state = "telephony_busy";
            } else {
                scene_audio_started = playMediaRoute(scene_route);
                scene_audio_state = scene_audio_started ? "started" : "play_failed";
            }
        }

        JsonDocument out;
        JsonObject root = out.to<JsonObject>();
        root["ok"] = true;
        root["code"] = "SCENE";
        root["scene"] = scene_id;
        root["active"] = true;
        root["audio_mapped"] = scene_audio_mapped;
        root["audio_started"] = scene_audio_started;
        root["audio_state"] = scene_audio_state;
        if (scene_audio_mapped) {
            root["audio_path"] = scene_route.path;
            root["audio_source"] = mediaSourceToString(scene_route.source);
        }
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("CAPTURE_START", [](const String&) {
        return makeResponse(g_audio.startCapture(), "CAPTURE_START");
    });

    g_dispatcher.registerCommand("CAPTURE_STOP", [](const String&) {
        g_audio.stopCapture();
        return makeResponse(true, "CAPTURE_STOP");
    });

    g_dispatcher.registerCommand("OSC_START", [](const String& args) {
        String first;
        String rest;
        uint16_t freq = 1200U;
        uint8_t amp = 48U;

        if (!args.isEmpty()) {
            if (!splitFirstToken(args, first, rest)) {
                return makeResponse(false, "OSC_START invalid_args");
            }
            const int parsed_freq = first.toInt();
            if (parsed_freq > 0) {
                freq = static_cast<uint16_t>(parsed_freq);
            }
            if (!rest.isEmpty()) {
                const int parsed_amp = rest.toInt();
                if (parsed_amp > 0) {
                    amp = static_cast<uint8_t>(parsed_amp);
                }
            }
            if (!g_scope_display.configure(freq, amp)) {
                return makeResponse(false, "OSC_START invalid_config");
            }
        }

        if (!g_scope_display.begin()) {
            return makeResponse(false, "OSC_START not_supported");
        }
        g_scope_display.enable(true);
        return makeResponse(true, "OSC_START");
    });

    g_dispatcher.registerCommand("OSC_STOP", [](const String&) {
        g_scope_display.enable(false);
        return makeResponse(true, "OSC_STOP");
    });

    g_dispatcher.registerCommand("OSC_STATUS", [](const String&) {
        JsonDocument out;
        JsonObject scope = out.to<JsonObject>();
        scope["supported"] = g_scope_display.supported();
        scope["enabled"] = g_scope_display.enabled();
        scope["frequency"] = g_scope_display.frequency();
        scope["amplitude"] = g_scope_display.amplitude();
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("PLAY", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "PLAY missing_args");
        }
        MediaRouteEntry route;
        if (!parseMediaRouteFromArgs(args, route, false) || route.kind != MediaRouteKind::FILE) {
            return makeResponse(false, "PLAY invalid_args");
        }
        if (isLegacyToneWavPath(route.path)) {
            return makeResponse(false, "PLAY tone_wav_deprecated_use_TONE_PLAY");
        }
        return makeResponse(playMediaRoute(route), "PLAY");
    });

    g_dispatcher.registerCommand("FFAT_RESET", [](const String& args) {
        const String path = sanitizeFsPath(args);
        if (path.isEmpty()) {
            return makeResponse(false, "FFAT_RESET invalid_path");
        }
        if (!ensureFfatMounted()) {
            return makeResponse(false, "FFAT_RESET mount_failed");
        }
        if (!ensureParentDirsOnFfat(path)) {
            return makeResponse(false, "FFAT_RESET mkdir_failed");
        }
        File f = FFat.open(path, FILE_WRITE);
        if (!f) {
            return makeResponse(false, "FFAT_RESET open_failed");
        }
        f.close();
        return makeResponse(true, "FFAT_RESET");
    });

    g_dispatcher.registerCommand("FFAT_APPEND_B64", [](const String& args) {
        String path;
        String b64;
        if (!splitFirstToken(args, path, b64) || path.isEmpty() || b64.isEmpty()) {
            return makeResponse(false, "FFAT_APPEND_B64 invalid_args");
        }
        path = sanitizeFsPath(path);
        if (path.isEmpty()) {
            return makeResponse(false, "FFAT_APPEND_B64 invalid_path");
        }
        if (!ensureFfatMounted()) {
            return makeResponse(false, "FFAT_APPEND_B64 mount_failed");
        }
        std::vector<uint8_t> decoded;
        if (!decodeBase64ToBytes(b64, decoded)) {
            return makeResponse(false, "FFAT_APPEND_B64 decode_failed");
        }
        if (!ensureParentDirsOnFfat(path)) {
            return makeResponse(false, "FFAT_APPEND_B64 mkdir_failed");
        }
        File f = FFat.open(path, FILE_APPEND);
        if (!f) {
            return makeResponse(false, "FFAT_APPEND_B64 open_failed");
        }
        const size_t written = f.write(decoded.data(), decoded.size());
        f.close();
        if (written != decoded.size()) {
            return makeResponse(false, "FFAT_APPEND_B64 write_failed");
        }
        return makeResponse(true, "FFAT_APPEND_B64");
    });

    g_dispatcher.registerCommand("FFAT_EXISTS", [](const String& args) {
        const String path = sanitizeFsPath(args);
        if (path.isEmpty()) {
            return makeResponse(false, "FFAT_EXISTS invalid_path");
        }
        if (!ensureFfatMounted()) {
            return makeResponse(false, "FFAT_EXISTS mount_failed");
        }
        return makeResponse(FFat.exists(path), "FFAT_EXISTS");
    });

    g_dispatcher.registerCommand("FS_LIST", [](const String& args) {
        return dispatchFsListCommand(args);
    });

    g_dispatcher.registerCommand("TONE_PLAY", [](const String& args) {
        if (!g_audio.isReady()) {
            return makeResponse(false, "TONE_PLAY audio_not_ready");
        }
        g_telephony.clearDialToneSuppression();
        String first;
        String rest;
        if (!splitFirstToken(args, first, rest) || first.isEmpty()) {
            return makeResponse(false, "TONE_PLAY invalid_args");
        }
        ToneProfile profile = ToneProfile::FR_FR;
        ToneEvent event = ToneEvent::NONE;
        if (rest.isEmpty()) {
            if (!parseToneEvent(first, event)) {
                return makeResponse(false, "TONE_PLAY invalid_event");
            }
        } else {
            String event_text;
            String trailing;
            if (!splitFirstToken(rest, event_text, trailing) || event_text.isEmpty() || !trailing.isEmpty()) {
                return makeResponse(false, "TONE_PLAY invalid_args");
            }
            if (!parseToneProfile(first, profile)) {
                return makeResponse(false, "TONE_PLAY invalid_profile");
            }
            if (!parseToneEvent(event_text, event)) {
                return makeResponse(false, "TONE_PLAY invalid_event");
            }
        }
        if (profile == ToneProfile::NONE || event == ToneEvent::NONE) {
            return makeResponse(false, "TONE_PLAY invalid_route");
        }
        const bool ok = g_audio.playTone(profile, event);
        return makeResponse(ok, ok ? "TONE_PLAY" : "TONE_PLAY failed");
    });

    g_dispatcher.registerCommand("TONE_STOP", [](const String&) {
        g_audio.stopTone();
        return makeResponse(true, "TONE_STOP");
    });

    g_dispatcher.registerCommand("VOLUME_SET", [](const String& args) {
        String value_token;
        String trailing;
        if (!splitFirstToken(args, value_token, trailing) || value_token.isEmpty() || !trailing.isEmpty()) {
            return makeResponse(false, "VOLUME_SET invalid_args");
        }

        char* end = nullptr;
        const long value = strtol(value_token.c_str(), &end, 10);
        if (end == nullptr || end == value_token.c_str() || *end != '\0' || value < 0 || value > 100) {
            return makeResponse(false, "VOLUME_SET invalid_value");
        }

        A252AudioConfig next = g_audio_cfg;
        long applied_value = value;
        if (g_profile == BoardProfile::ESP32_A252) {
            if (value != 100L) {
                Serial.printf("[RTC_BL_PHONE] forcing ES8388 volume to 100 (requested=%ld)\n", value);
            }
            applied_value = 100L;
        }
        next.volume = static_cast<uint8_t>(applied_value);

        if (!persistA252AudioConfigIfNeeded(next, "VOLUME_SET")) {
            return makeResponse(false, "VOLUME_SET persist_failed");
        }

        if (g_profile == BoardProfile::ESP32_A252) {
            g_codec.setVolume(g_audio_cfg.volume);
        }
        return makeResponse(true, "VOLUME_SET");
    });

    g_dispatcher.registerCommand("VOLUME_GET", [](const String&) {
        JsonDocument doc;
        doc["volume"] = g_audio_cfg.volume;
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("RESET_METRICS", [](const String&) {
        g_audio.resetMetrics();
        return makeResponse(true, "RESET_METRICS");
    });

    g_dispatcher.registerCommand("TONE_ON", [](const String&) {
        if (!g_audio.isReady()) {
            return makeResponse(false, "TONE_ON audio_not_ready");
        }
        g_telephony.clearDialToneSuppression();
        const bool ok = g_audio.playTone(ToneProfile::FR_FR, ToneEvent::DIAL);
        return makeResponse(ok, ok ? "TONE_ON" : "TONE_ON failed");
    });

    g_dispatcher.registerCommand("TONE_OFF", [](const String&) {
        g_telephony.suppressDialToneForMs(kToneOffSuppressionMs);
        g_audio.stopTone();
        return makeResponse(true, "TONE_OFF");
    });

    g_dispatcher.registerCommand("AMP_ON", [](const String&) {
        setAmpEnabled(true);
        return makeResponse(true, "AMP_ON");
    });

    g_dispatcher.registerCommand("AMP_OFF", [](const String&) {
        setAmpEnabled(false);
        return makeResponse(true, "AMP_OFF");
    });

    g_dispatcher.registerCommand("ESPNOW_ON", [](const String&) {
        return makeResponse(g_espnow.begin(g_peer_store), "ESPNOW_ON");
    });

    g_dispatcher.registerCommand("ESPNOW_OFF", [](const String&) {
        return makeResponse(g_espnow.stop(), "ESPNOW_OFF");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_ADD", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "ESPNOW_PEER_ADD invalid_mac");
        }
        const bool ok = g_espnow.addPeer(args);
        if (ok) {
            g_peer_store.peers = g_espnow.peers();
            g_peer_store.device_name = g_espnow.deviceName();
            A252ConfigStore::saveEspNowPeers(g_peer_store);
        }
        return makeResponse(ok, "ESPNOW_PEER_ADD");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_DEL", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "ESPNOW_PEER_DEL invalid_mac");
        }
        const bool ok = g_espnow.deletePeer(args);
        if (ok) {
            g_peer_store.peers = g_espnow.peers();
            g_peer_store.device_name = g_espnow.deviceName();
            A252ConfigStore::saveEspNowPeers(g_peer_store);
        }
        return makeResponse(ok, "ESPNOW_PEER_DEL");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_LIST", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["device_name"] = g_espnow.deviceName();
        JsonArray peers = root["peers"].to<JsonArray>();
        g_peer_store.peers = g_espnow.peers();
        g_peer_store.device_name = g_espnow.deviceName();
        A252ConfigStore::peersToJson(g_peer_store, peers);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_STATUS", [](const String&) {
        JsonDocument doc;
        g_espnow.statusToJson(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_DEVICE_NAME_GET", [](const String&) {
        JsonDocument doc;
        doc["device_name"] = g_espnow.deviceName();
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_DEVICE_NAME_SET", [](const String& args) {
        const String normalized = A252ConfigStore::normalizeDeviceName(args);
        if (normalized.isEmpty()) {
            return makeResponse(false, "ESPNOW_DEVICE_NAME_SET invalid_name");
        }
        if (!g_espnow.setDeviceName(normalized, true)) {
            return makeResponse(false, "ESPNOW_DEVICE_NAME_SET persist_failed");
        }
        g_peer_store.device_name = g_espnow.deviceName();
        g_peer_store.peers = g_espnow.peers();
        return makeResponse(true, "ESPNOW_DEVICE_NAME_SET");
    });

    g_dispatcher.registerCommand("ESPNOW_SEND", [](const String& args) {
        String target;
        String payload;
        if (!splitFirstToken(args, target, payload) || target.isEmpty() || payload.isEmpty()) {
            return makeResponse(false, "ESPNOW_SEND invalid_args");
        }
        return makeResponse(g_espnow.sendJson(target, payload), "ESPNOW_SEND");
    });

    g_dispatcher.registerCommand("ESPNOW_CALL_MAP_GET", [](const String&) {
        JsonDocument doc;
        JsonObject map = doc.to<JsonObject>();
        A252ConfigStore::espNowCallMapToJson(g_espnow_call_map, map);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_CALL_MAP_SET", [](const String& args) {
        return applyEspNowCallMapSet(args);
    });

    g_dispatcher.registerCommand("ESPNOW_CALL_MAP_SET_VOLATILE", [](const String& args) {
        return applyEspNowCallMapSetImpl(args, false, "ESPNOW_CALL_MAP_SET_VOLATILE");
    });

    g_dispatcher.registerCommand("ESPNOW_CALL_MAP_RESET", [](const String&) {
        initDefaultEspNowCallMap(g_espnow_call_map);
        if (!A252ConfigStore::saveEspNowCallMap(g_espnow_call_map)) {
            return makeResponse(false, "ESPNOW_CALL_MAP_RESET save_failed");
        }
        return makeResponse(true, "ESPNOW_CALL_MAP_RESET");
    });

    g_dispatcher.registerCommand("ESPNOW_CALL_MAP_RESET_VOLATILE", [](const String&) {
        initDefaultEspNowCallMap(g_espnow_call_map);
        return makeResponse(true, "ESPNOW_CALL_MAP_RESET_VOLATILE");
    });

    g_dispatcher.registerCommand("DIAL_MEDIA_MAP_GET", [](const String&) {
        JsonDocument doc;
        JsonObject map = doc.to<JsonObject>();
        A252ConfigStore::dialMediaMapToJson(g_dial_media_map, map);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("DIAL_MEDIA_MAP_SET", [](const String& args) {
        return applyDialMediaMapSet(args);
    });

    g_dispatcher.registerCommand("DIAL_MEDIA_MAP_SET_VOLATILE", [](const String& args) {
        return applyDialMediaMapSetImpl(args, false, "DIAL_MEDIA_MAP_SET_VOLATILE");
    });

    g_dispatcher.registerCommand("DIAL_MEDIA_MAP_RESET", [](const String&) {
        initDefaultDialMediaMap(g_dial_media_map);
        if (!A252ConfigStore::saveDialMediaMap(g_dial_media_map)) {
            return makeResponse(false, "DIAL_MEDIA_MAP_RESET save_failed");
        }
        return makeResponse(true, "DIAL_MEDIA_MAP_RESET");
    });

    g_dispatcher.registerCommand("DIAL_MEDIA_MAP_RESET_VOLATILE", [](const String&) {
        initDefaultDialMediaMap(g_dial_media_map);
        return makeResponse(true, "DIAL_MEDIA_MAP_RESET_VOLATILE");
    });

    g_dispatcher.registerCommand("HOTLINE_STATUS", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["active"] = g_hotline.active;
        root["telephony_state"] = telephonyStateToString(g_telephony.state());
        root["hook_off"] = g_slic.isHookOff();
        root["current_key"] = g_hotline.current_key;
        root["current_digits"] = g_hotline.current_digits;
        root["current_source"] = g_hotline.current_source;
        root["queued"] = g_hotline.queued;
        root["queued_key"] = g_hotline.queued_key;
        root["queued_digits"] = g_hotline.queued_digits;
        root["queued_source"] = g_hotline.queued_source;
        root["pending_restart"] = g_hotline.pending_restart;
        root["next_restart_ms"] = g_hotline.next_restart_ms;
        root["last_notify_event"] = g_hotline.last_notify_event;
        root["last_notify_ok"] = g_hotline.last_notify_ok;
        JsonObject route = root["current_route"].to<JsonObject>();
        route["kind"] = mediaRouteKindToString(g_hotline.current_route.kind);
        if (g_hotline.current_route.kind == MediaRouteKind::TONE) {
            route["profile"] = toneProfileToString(g_hotline.current_route.tone.profile);
            route["event"] = toneEventToString(g_hotline.current_route.tone.event);
        } else {
            route["path"] = g_hotline.current_route.path;
            route["source"] = mediaSourceToString(g_hotline.current_route.source);
            JsonObject playback = route["playback"].to<JsonObject>();
            playback["loop"] = g_hotline.current_route.playback.loop;
            playback["pause_ms"] = g_hotline.current_route.playback.pause_ms;
        }
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("HOTLINE_TRIGGER", [](const String& args) {
        String digits;
        String rest;
        if (!splitFirstToken(args, digits, rest) || digits.isEmpty()) {
            return makeResponse(false, "HOTLINE_TRIGGER invalid_args");
        }

        bool from_pulse = false;
        if (!rest.isEmpty()) {
            String source;
            String trailing;
            if (!splitFirstToken(rest, source, trailing) || !trailing.isEmpty()) {
                return makeResponse(false, "HOTLINE_TRIGGER invalid_args");
            }
            source.trim();
            source.toLowerCase();
            if (source == "pulse") {
                from_pulse = true;
            } else if (source == "dtmf") {
                from_pulse = false;
            } else {
                return makeResponse(false, "HOTLINE_TRIGGER invalid_source");
            }
        }

        String state;
        const bool ok = triggerHotlineRouteForDigits(digits, from_pulse, &state);
        return makeResponse(ok, ok ? String("HOTLINE_TRIGGER ") + state : String("HOTLINE_TRIGGER ") + state);
    });

    g_dispatcher.registerCommand("HOTLINE_VALIDATE", [](const String& args) {
        return dispatchHotlineValidateCommand(args);
    });

    g_dispatcher.registerCommand("HOTLINE_SCENE_PLAY", [](const String& args) {
        String scene_id;
        if (!parseSceneIdFromArgs(args, scene_id)) {
            return makeResponse(false, "HOTLINE_SCENE_PLAY missing_scene_id");
        }

        MediaRouteEntry route;
        if (!resolveHotlineSceneRoute(scene_id, route)) {
            return makeResponse(false, "HOTLINE_SCENE_PLAY missing_scene_audio");
        }
        if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
            return makeResponse(false, "HOTLINE_SCENE_PLAY telephony_busy");
        }
        if (!playMediaRoute(route)) {
            return makeResponse(false, "HOTLINE_SCENE_PLAY play_failed");
        }

        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["ok"] = true;
        root["code"] = "HOTLINE_SCENE_PLAY";
        root["scene"] = scene_id;
        root["path"] = route.path;
        root["source"] = mediaSourceToString(route.source);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WAITING_VALIDATION", [](const String&) {
        return dispatchWaitingValidationCommand();
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_GET", [](const String&) {
        JsonDocument doc;
        A252ConfigStore::pinsToJson(g_pins_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "SLIC_CONFIG_SET invalid_json");
        }

        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "SLIC_CONFIG_SET invalid_json");
        }

        A252PinsConfig next = g_pins_cfg;
        String error;
        if (!applyPinsPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "SLIC_CONFIG_SET " + error);
        }
        if (!A252ConfigStore::savePins(next, &error)) {
            return makeResponse(false, "SLIC_CONFIG_SET " + error);
        }

        const A252PinsConfig prev = g_pins_cfg;
        g_pins_cfg = next;
        if (!applyHardwareConfig()) {
            g_pins_cfg = prev;
            applyHardwareConfig();
            return makeResponse(false, "SLIC_CONFIG_SET apply_failed");
        }

        JsonDocument out;
        A252ConfigStore::pinsToJson(g_pins_cfg, out.to<JsonObject>());
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_GET", [](const String&) {
        JsonDocument doc;
        A252ConfigStore::audioToJson(g_audio_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "AUDIO_CONFIG_SET invalid_json");
        }

        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "AUDIO_CONFIG_SET invalid_json");
        }

        A252AudioConfig next = g_audio_cfg;
        String error;
        if (!applyAudioPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "AUDIO_CONFIG_SET " + error);
        }
        if (!persistA252AudioConfigIfNeeded(next, "AUDIO_CONFIG_SET")) {
            return makeResponse(false, "AUDIO_CONFIG_SET persist_failed");
        }

        if (g_profile == BoardProfile::ESP32_A252) {
            g_codec.setVolume(g_audio_cfg.volume);
            g_codec.setMute(g_audio_cfg.mute);
            g_codec.setRoute(g_audio_cfg.route);
        }
        const bool audio_ok = g_audio.begin(buildI2sConfig(g_pins_cfg, g_audio_cfg));
        g_hw_status.audio_ready = audio_ok;
        g_hw_status.init_ok = g_hw_status.slic_ready && g_hw_status.codec_ready && g_hw_status.audio_ready;
        return makeResponse(audio_ok, "AUDIO_CONFIG_SET");
    });

    g_dispatcher.registerCommand("AUDIO_POLICY_GET", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["clock_policy"] = g_audio_cfg.clock_policy;
        root["wav_loudness_policy"] = g_audio_cfg.wav_loudness_policy;
        root["wav_target_rms_dbfs"] = g_audio_cfg.wav_target_rms_dbfs;
        root["wav_limiter_ceiling_dbfs"] = g_audio_cfg.wav_limiter_ceiling_dbfs;
        root["wav_limiter_attack_ms"] = g_audio_cfg.wav_limiter_attack_ms;
        root["wav_limiter_release_ms"] = g_audio_cfg.wav_limiter_release_ms;
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("AUDIO_POLICY_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "AUDIO_POLICY_SET invalid_json");
        }

        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "AUDIO_POLICY_SET invalid_json");
        }

        A252AudioConfig next = g_audio_cfg;
        String error;
        if (!applyAudioPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "AUDIO_POLICY_SET " + error);
        }
        if (!persistA252AudioConfigIfNeeded(next, "AUDIO_POLICY_SET")) {
            return makeResponse(false, "AUDIO_POLICY_SET persist_failed");
        }

        if (g_profile == BoardProfile::ESP32_A252) {
            g_codec.setVolume(g_audio_cfg.volume);
            g_codec.setMute(g_audio_cfg.mute);
            g_codec.setRoute(g_audio_cfg.route);
        }
        const bool audio_ok = g_audio.begin(buildI2sConfig(g_pins_cfg, g_audio_cfg));
        g_hw_status.audio_ready = audio_ok;
        g_hw_status.init_ok = g_hw_status.slic_ready && g_hw_status.codec_ready && g_hw_status.audio_ready;
        return makeResponse(audio_ok, "AUDIO_POLICY_SET");
    });

    g_dispatcher.registerCommand("AUDIO_PROBE", [](const String& args) {
        MediaRouteEntry route;
        if (!parseMediaRouteFromArgs(args, route, false) || route.kind != MediaRouteKind::FILE || route.path.isEmpty()) {
            return makeResponse(false, "AUDIO_PROBE invalid_args");
        }

        AudioPlaybackProbeResult probe;
        const bool ok = g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe);
        if (!ok) {
            return makeResponse(false, "AUDIO_PROBE " + (probe.error.isEmpty() ? String("failed") : probe.error));
        }

        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["ok"] = probe.ok;
        root["path"] = probe.path;
        root["source"] = mediaSourceToString(probe.source);
        root["input_sample_rate"] = probe.input_sample_rate;
        root["input_bits_per_sample"] = probe.input_bits_per_sample;
        root["input_channels"] = probe.input_channels;
        root["output_sample_rate"] = probe.output_sample_rate;
        root["output_bits_per_sample"] = probe.output_bits_per_sample;
        root["output_channels"] = probe.output_channels;
        root["resampler_active"] = probe.resampler_active;
        root["channel_upmix_active"] = probe.channel_upmix_active;
        root["loudness_auto"] = probe.loudness_auto;
        root["loudness_gain_db"] = probe.loudness_gain_db;
        root["limiter_active"] = probe.limiter_active;
        root["rate_fallback"] = probe.rate_fallback;
        root["data_size_bytes"] = probe.data_size_bytes;
        root["duration_ms"] = probe.duration_ms;
        return jsonResponse(doc);
    });
}

void processInboundBridgeCommand(const String& source, const JsonVariantConst& payload) {
    maybeTrackEspNowPeerDiscoveryAck(source, payload);

    String cmd;
    String request_id;
    uint32_t request_seq = 0;
    bool request_ack = true;
    bool is_envelope_v2 = false;
    bool is_rtcbl_v1 = false;

    if (buildEspNowEnvelopeCommand(payload, cmd, request_id, request_seq, request_ack)) {
        is_envelope_v2 = true;
    } else if (!buildRtcBlV1BridgeCommand(payload, cmd, request_id, is_rtcbl_v1) &&
               !extractBridgeCommand(payload, cmd)) {
        return;
    }

    DispatchResponse result;
    if (handleIncomingEspNowCallCommand(cmd, result)) {
        if (is_envelope_v2 && request_ack && isMacAddressString(source)) {
            JsonDocument response;
            response["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
            response["seq"] = request_seq;
            response["type"] = "ack";
            response["ack"] = true;
            JsonObject ack_payload = response["payload"].to<JsonObject>();
            ack_payload["ok"] = result.ok;
            ack_payload["code"] = result.code;
            ack_payload["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

            if (!result.json.isEmpty()) {
                JsonDocument parsed;
                if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                    ack_payload["data"].set(parsed.as<JsonVariantConst>());
                } else {
                    ack_payload["data_raw"] = result.json;
                }
            } else if (!result.raw.isEmpty()) {
                ack_payload["data_raw"] = result.raw;
            }

            String response_payload;
            serializeJson(response, response_payload);
            g_espnow.sendJson(source, response_payload);
            return;
        }

        if (!is_rtcbl_v1 || !isMacAddressString(source)) {
            return;
        }

        JsonDocument response;
        response["proto"] = "rtcbl/1";
        response["id"] = request_id;
        response["ok"] = result.ok;
        response["code"] = result.code;
        response["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

        if (!result.json.isEmpty()) {
            JsonDocument parsed;
            if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                JsonVariant data = response["data"];
                data.set(parsed.as<JsonVariantConst>());
            } else {
                response["data_raw"] = result.json;
            }
        } else if (!result.raw.isEmpty()) {
            response["data_raw"] = result.raw;
        }

        String response_payload;
        serializeJson(response, response_payload);
        g_espnow.sendJson(source, response_payload);
        return;
    }

    result = executeCommandLine(cmd);

    if (is_envelope_v2 && request_ack && isMacAddressString(source)) {
        JsonDocument response;
        response["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
        response["seq"] = request_seq;
        response["type"] = "ack";
        response["ack"] = true;

        JsonObject ack_payload = response["payload"].to<JsonObject>();
        ack_payload["ok"] = result.ok;
        ack_payload["code"] = result.code;
        ack_payload["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

        if (!result.json.isEmpty()) {
            JsonDocument parsed;
            if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                ack_payload["data"].set(parsed.as<JsonVariantConst>());
            } else {
                ack_payload["data_raw"] = result.json;
            }
        } else if (!result.raw.isEmpty()) {
            ack_payload["data_raw"] = result.raw;
        }

        String response_payload;
        serializeJson(response, response_payload);
        g_espnow.sendJson(source, response_payload);
        return;
    }

    if (!is_rtcbl_v1 || !isMacAddressString(source)) {
        return;
    }

    JsonDocument response;
    response["proto"] = "rtcbl/1";
    response["id"] = request_id;
    response["ok"] = result.ok;
    response["code"] = result.code;
    response["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

    if (!result.json.isEmpty()) {
        JsonDocument parsed;
        if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
            JsonVariant data = response["data"];
            data.set(parsed.as<JsonVariantConst>());
        } else {
            response["data_raw"] = result.json;
        }
    } else if (!result.raw.isEmpty()) {
        response["data_raw"] = result.raw;
    }

    String response_payload;
    serializeJson(response, response_payload);
    g_espnow.sendJson(source, response_payload);
}

void printHelp() {
    Serial.println("[RTC_BL_PHONE] Commands:");
    const std::vector<String> names = g_dispatcher.commands();
    for (const String& name : names) {
        Serial.printf("  %s\n", name.c_str());
    }
}

void handleSerialCommand(const String& line) {
    const DispatchResponse res = executeCommandLine(line);

    if (!res.raw.isEmpty()) {
        Serial.println(res.raw);
        return;
    }

    if (!res.json.isEmpty()) {
        Serial.println(res.json);
        return;
    }

    Serial.printf("%s %s\n", res.ok ? "OK" : "ERR", res.code.c_str());
}

void pollSerial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (!g_serial_line.isEmpty()) {
                handleSerialCommand(g_serial_line);
                g_serial_line = "";
            }
        } else {
            g_serial_line += c;
        }
    }
}

void configureCommandServer() {
    g_web_server.setCommandExecutor(executeCommandLine);
    g_web_server.setCommandValidator([](const String& command_id) {
        return g_dispatcher.hasCommand(command_id);
    });
    g_web_server.setAuthEnabled(kWebAuthEnabledByDefault && !kWebAuthLocalDisableEnabled);
    g_web_server.setStatusCallback([](JsonObject obj) {
        fillStatusSnapshot(obj);
    });
}

}  // namespace

void setup() {
    Serial.begin(kSerialBaud);
    delay(80);

    // Warm up ESP-IDF log/stdout locks from the main task context.
    ESP_LOGI(kBootLogTag, "log lock warmup");
    printf("[RTC_BL_PHONE] stdio lock warmup\n");
    fflush(stdout);

    g_profile = detectBoardProfile();
    g_features = getFeatureMatrix(g_profile);

#ifdef USB_HOST_BOOT_ENABLE
    const bool usb_host = usb_host_runtime::enableHostPortPower();
    Serial.printf("[RTC_BL_PHONE] USB host bootstrap: %s\n", usb_host ? "ok" : "not available");
#endif

#ifdef USB_MSC_BOOT_ENABLE
    const bool usb_msc = usb_msc_runtime::beginUsbMassStorage();
    Serial.printf("[RTC_BL_PHONE] USB MSC bootstrap: %s\n", usb_msc ? "ok" : "failed");
#endif

    A252ConfigStore::loadPins(g_pins_cfg);
    g_pins_cfg.slic_line = -1;
    A252ConfigStore::loadAudio(g_audio_cfg);
    ensureA252AudioDefaults();
    A252ConfigStore::loadEspNowPeers(g_peer_store);
    ensureEspNowDeviceName();
    g_config_migrations = ConfigMigrationStatus{};
    initDefaultEspNowCallMap(g_espnow_call_map);
    if (!A252ConfigStore::loadEspNowCallMap(g_espnow_call_map)) {
        initDefaultEspNowCallMap(g_espnow_call_map);
        A252ConfigStore::saveEspNowCallMap(g_espnow_call_map);
    }
    if (espNowCallMapHasLegacyToneWav(g_espnow_call_map)) {
        Serial.println("[RTC_BL_PHONE] migration: resetting espnow_call_map legacy tone wav routes");
        initDefaultEspNowCallMap(g_espnow_call_map);
        A252ConfigStore::saveEspNowCallMap(g_espnow_call_map);
        g_config_migrations.espnow_call_map_reset = true;
    }
    initDefaultDialMediaMap(g_dial_media_map);
    A252ConfigStore::loadDialMediaMap(g_dial_media_map);
    initDefaultDialMediaMap(g_dial_media_map);
    if (!A252ConfigStore::saveDialMediaMap(g_dial_media_map)) {
        Serial.println("[RTC_BL_PHONE] failed to persist forced hotline preset 1/2/3");
    } else {
        Serial.println("[RTC_BL_PHONE] hotline preset forced 1/2/3");
    }

    pinMode(kAudioAmpEnablePin, OUTPUT);
    setAmpEnabled(true);

    const bool hw_init_ok = applyHardwareConfig();
    if (!hw_init_ok) {
        Serial.println("[RTC_BL_PHONE] hardware init failed");
    }
    registerCommands();

    g_espnow.begin(g_peer_store);
    g_peer_store.device_name = g_espnow.deviceName();
    g_peer_store.peers = g_espnow.peers();
    initEspNowPeerDiscoveryRuntime();
    g_espnow.setCommandCallback([](const String& source, const JsonVariantConst& payload) {
        processInboundBridgeCommand(source, payload);
    });
    configureCommandServer();
    g_web_server.begin();

    Serial.printf("[RTC_BL_PHONE] Boot: profile=%s full_duplex=%s\n",
                  boardProfileToString(g_profile),
                  g_features.has_full_duplex_i2s ? "true" : "false");
    if (kPrintHelpOnBoot) {
        printHelp();
    }
}

void loop() {
    g_wifi.loop();
    g_telephony.tick();
    tickHotlineRuntime();
    g_scope_display.tick();
    g_web_server.handle();
    g_espnow.tick();
    tickEspNowPeerDiscoveryRuntime();
    pollSerial();
    delay(1);
}
#endif  // UNIT_TEST
