#include <Arduino.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <esp_log.h>
#include <WiFi.h>
#include <mbedtls/base64.h>

#include <algorithm>
#include <vector>

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
constexpr uint32_t kHotlineRingbackMinMs = 2000U;
constexpr uint32_t kHotlineRingbackMaxMs = 10000U;
constexpr uint32_t kOffHookAutoRandomDelayMs = 2000U;
constexpr uint16_t kFsListDefaultPageSize = 100U;
constexpr uint16_t kFsListMaxPageSize = 200U;
constexpr uint32_t kFsListMaxPage = 100000U;
constexpr uint32_t kEspNowPeerDiscoveryIntervalMs = 30000U;
constexpr uint32_t kEspNowPeerDiscoveryAckWindowMs = 2500U;
constexpr uint32_t kEspNowSceneSyncIntervalMs = 30000U;
constexpr uint32_t kEspNowSceneSyncAckWindowMs = 2500U;
constexpr char kEspNowDefaultDeviceName[] = "HOTLINE_PHONE";
constexpr char kFirmwareContractVersion[] = "A252_AUDIO_CHAIN_V4";
constexpr char kFirmwareBuildId[] = __DATE__ " " __TIME__;
constexpr char kHotlineAssetsRoot[] = "/hotline";
constexpr char kHotlineTtsAssetsRoot[] = "/hotline_tts";
constexpr char kHotlineTtsNestedAssetsRoot[] = "/hotline/hotline_tts";
constexpr char kInterludeTtsAssetsRoot[] = "/interlude_tts";
constexpr char kHotlineDefaultVoiceSuffix[] = "__fr-fr-deniseneural.wav";
constexpr char kHotlineDefaultVoiceSuffixLegacyMp3[] = "__fr-fr-deniseneural.mp3";
constexpr char kHotlineWaitingPromptStem[] = "enter_code_5";
constexpr char kHotlineLogPath[] = "/hotline/log.txt";
constexpr uint32_t kInterludeMinDelayMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kInterludeMaxDelayMs = 30UL * 60UL * 1000UL;
constexpr uint32_t kInterludeRetryDelayMs = 120000UL;
constexpr uint32_t kWarningSirenBeatTimeoutMs = 6000UL;
constexpr uint8_t kA252CodecMaxVolumePercent = 60U;  // Reduced from 100% to prevent audio saturation

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

enum class HotlineValidationState : uint8_t {
    kNone = 0,
    kWaiting,
    kGranted,
    kRefused,
};

A252PinsConfig g_pins_cfg = A252ConfigStore::defaultPins();
A252AudioConfig g_audio_cfg = A252ConfigStore::defaultAudio();
EspNowPeerStore g_peer_store;
EspNowCallMap g_espnow_call_map;
DialMediaMap g_dial_media_map;
String g_active_scene_id;
String g_active_step_id;
HotlineValidationState g_hotline_validation_state = HotlineValidationState::kNone;
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
    String last_route_lookup_key;
    String last_route_resolution;
    String last_route_target;
    bool ringback_active = false;
    uint32_t ringback_until_ms = 0U;
    ToneProfile ringback_profile = ToneProfile::NONE;
    MediaRouteEntry post_ringback_route;
    bool post_ringback_valid = false;
};

HotlineRuntimeState g_hotline;

struct WarningSirenRuntimeState {
    bool enabled = false;
    bool tone_owned = false;
    uint8_t phase = 0U;
    uint8_t strength = 220U;
    ToneProfile profile = ToneProfile::FR_FR;
    ToneEvent event = ToneEvent::RINGBACK;
    uint32_t started_ms = 0U;
    uint32_t last_control_ms = 0U;
    uint32_t next_toggle_ms = 0U;
    uint32_t toggle_period_ms = 560U;
    String last_error;
};

WarningSirenRuntimeState g_warning_siren;

struct HotlineInterludeRuntimeState {
    bool enabled = true;
    uint32_t next_due_ms = 0U;
    uint32_t last_trigger_ms = 0U;
    String last_file;
    String last_error;
};

HotlineInterludeRuntimeState g_hotline_interlude;

struct OffHookAutoRandomPlaybackState {
    bool armed = false;
    uint32_t play_after_ms = 0U;
    MediaRouteEntry route;
    String selected_path;
    String last_error;
};

OffHookAutoRandomPlaybackState g_offhook_autoplay;


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

struct EspNowSceneSyncRuntimeState {
    bool enabled = true;
    uint32_t interval_ms = kEspNowSceneSyncIntervalMs;
    uint32_t ack_window_ms = kEspNowSceneSyncAckWindowMs;
    uint32_t next_sync_ms = 0U;
    bool request_pending = false;
    String request_msg_id;
    uint32_t request_seq = 0U;
    uint32_t request_deadline_ms = 0U;
    uint32_t requests_sent = 0U;
    uint32_t request_send_fail = 0U;
    uint32_t request_ack_ok = 0U;
    uint32_t request_ack_fail = 0U;
    String last_error;
    String last_source;
    uint32_t last_update_ms = 0U;
};

EspNowSceneSyncRuntimeState g_espnow_scene_sync;
std::vector<String> g_hotline_voice_suffix_catalog;
bool g_hotline_voice_catalog_scanned = false;
bool g_hotline_voice_catalog_sd_scanned = false;
uint32_t g_hotline_log_counter = 0U;
bool g_busy_tone_after_media_pending = false;
bool g_win_etape_validation_after_media_pending = false;
bool g_prev_audio_playing = false;

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
void clearPendingEspNowCallRoute(const char* reason = nullptr);
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
void tickHotlineInterludeRuntime();
void tickOffHookAutoRandomPlayback(uint32_t now_ms);
void armOffHookAutoRandomPlayback(uint32_t now_ms);
void clearOffHookAutoRandomPlayback();
void tickPlaybackCompletionBusyTone();
void ensureEspNowDeviceName();
void initEspNowPeerDiscoveryRuntime();
void tickEspNowPeerDiscoveryRuntime();
bool maybeTrackEspNowPeerDiscoveryAck(const String& source, const JsonVariantConst& payload);
void initEspNowSceneSyncRuntime();
void tickEspNowSceneSyncRuntime();
bool requestSceneSyncFromFreenove(const char* reason, bool force_now = false);
bool maybeTrackEspNowSceneSyncAck(const String& source, const JsonVariantConst& payload);
DispatchResponse dispatchWarningSirenCommand(const String& args);
String normalizeHotlineSceneKey(const String& raw_scene_id);
HotlineValidationState inferHotlineValidationStateFromSceneKey(const String& scene_key);
bool resolveHotlineSceneDirectoryRoute(const String& scene_key,
                                       HotlineValidationState state,
                                       const String& digit_key,
                                       MediaRouteEntry& out_route,
                                       String* out_matched_file = nullptr);
bool resolveHotlineVoiceRouteFromStemCandidates(const String* stems, size_t stem_count, MediaRouteEntry& out_route);
void refreshHotlineVoiceSuffixCatalog();

bool isHybridTelcoClockPolicy(const String& raw_policy) {
    String policy = raw_policy;
    policy.trim();
    policy.toUpperCase();
    return policy == "HYBRID_TELCO";
}

void ensureA252AudioDefaults() {
  if (g_profile != BoardProfile::ESP32_A252) {
    return;
  }

  bool updated = false;

  if (g_audio_cfg.volume != kA252CodecMaxVolumePercent) {
    Serial.printf("[RTC_BL_PHONE] correcting A252 audio volume %u -> %u (optimized tel level)\n",
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

    if (g_audio_cfg.enable_capture) {
        Serial.println("[RTC_BL_PHONE] correcting A252 enable_capture true -> false (tx-only mode)");
        g_audio_cfg.enable_capture = false;
        updated = true;
    }

    if (g_audio_cfg.adc_dsp_enabled) {
        Serial.println("[RTC_BL_PHONE] correcting A252 adc_dsp_enabled true -> false (not required for hotline playback)");
        g_audio_cfg.adc_dsp_enabled = false;
        updated = true;
    }

    if (g_audio_cfg.adc_fft_enabled) {
        Serial.println("[RTC_BL_PHONE] correcting A252 adc_fft_enabled true -> false (not required for hotline playback)");
        g_audio_cfg.adc_fft_enabled = false;
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

uint32_t nextHotlineRandom32() {
    static uint32_t state = 0x5A17C3E1u;
    state ^= micros() + (millis() << 10U);
    state ^= (state << 13U);
    state ^= (state >> 17U);
    state ^= (state << 5U);
    return state;
}

ToneProfile pickRandomToneProfile() {
    static constexpr ToneProfile kProfiles[] = {
        ToneProfile::FR_FR,
        ToneProfile::ETSI_EU,
        ToneProfile::UK_GB,
        ToneProfile::NA_US,
    };
    const uint32_t index = nextHotlineRandom32() % static_cast<uint32_t>(sizeof(kProfiles) / sizeof(kProfiles[0]));
    return kProfiles[index];
}

uint32_t pickRandomRingbackDurationMs() {
    const uint32_t span = (kHotlineRingbackMaxMs - kHotlineRingbackMinMs) + 1U;
    return kHotlineRingbackMinMs + (nextHotlineRandom32() % span);
}

uint32_t pickRandomInterludeDelayMs() {
    const uint32_t span = (kInterludeMaxDelayMs - kInterludeMinDelayMs) + 1U;
    return kInterludeMinDelayMs + (nextHotlineRandom32() % span);
}

String normalizeHotlineVoiceSuffix(const String& raw_suffix) {
    String suffix = raw_suffix;
    suffix.trim();
    if (suffix.isEmpty()) {
        return "";
    }

    String lower = suffix;
    lower.toLowerCase();
    const bool supported_ext = lower.endsWith(".mp3") || lower.endsWith(".wav");
    if (!supported_ext) {
        return "";
    }
    const int marker = suffix.indexOf("__");
    if (marker < 0 || static_cast<size_t>(marker) >= suffix.length()) {
        return "";
    }
    return suffix.substring(static_cast<unsigned int>(marker));
}

void appendHotlineVoiceSuffixCatalog(const String& raw_suffix) {
    const String normalized = normalizeHotlineVoiceSuffix(raw_suffix);
    if (normalized.isEmpty()) {
        return;
    }
    for (const String& existing : g_hotline_voice_suffix_catalog) {
        if (existing.equalsIgnoreCase(normalized)) {
            return;
        }
    }
    g_hotline_voice_suffix_catalog.push_back(normalized);
}

bool ensureHotlineSdMounted(fs::FS*& out_fs) {
    out_fs = nullptr;
    if (SD_MMC.begin()) {
        out_fs = &SD_MMC;
        return true;
    }

    static bool sd_spi_bus_started = false;
    if (!sd_spi_bus_started) {
        SPI.begin(A1S_SD_SCK, A1S_SD_MISO, A1S_SD_MOSI, A1S_SD_CS);
        sd_spi_bus_started = true;
    }
    if (SD.begin(A1S_SD_CS, SPI, 10000000U)) {
        out_fs = &SD;
        return true;
    }
    return false;
}

void appendHotlineLogLine(const char* event, const String& details = String()) {
    if (event == nullptr || event[0] == '\0') {
        return;
    }
    fs::FS* sd_fs = nullptr;
    if (!ensureHotlineSdMounted(sd_fs) || sd_fs == nullptr) {
        return;
    }
    if (!sd_fs->exists(kHotlineAssetsRoot)) {
        if (!sd_fs->mkdir(kHotlineAssetsRoot)) {
            Serial.printf("[RTC_BL_PHONE] hotline log mkdir failed path=%s\n", kHotlineAssetsRoot);
            return;
        }
    }
    File log_file = sd_fs->open(kHotlineLogPath, FILE_APPEND);
    if (!log_file) {
        return;
    }
    ++g_hotline_log_counter;
    log_file.printf("%lu;%lu;%s", static_cast<unsigned long>(g_hotline_log_counter), static_cast<unsigned long>(millis()), event);
    if (!details.isEmpty()) {
        log_file.print(";");
        log_file.print(details);
    }
    log_file.println();
    log_file.close();
}

void refreshHotlineVoiceSuffixCatalog() {
    g_hotline_voice_suffix_catalog.clear();
    appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffix);
    appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffixLegacyMp3);
    g_hotline_voice_catalog_sd_scanned = false;

    if (!g_audio.isSdReady()) {
        g_hotline_voice_catalog_scanned = true;
        Serial.printf("[RTC_BL_PHONE] hotline voice catalog suffix_count=%u (sd_not_ready)\n",
                      static_cast<unsigned>(g_hotline_voice_suffix_catalog.size()));
        return;
    }

    fs::FS* sd_fs = nullptr;
    if (!ensureHotlineSdMounted(sd_fs) || sd_fs == nullptr) {
        g_hotline_voice_catalog_sd_scanned = true;
        g_hotline_voice_catalog_scanned = true;
        Serial.printf("[RTC_BL_PHONE] hotline voice catalog suffix_count=%u (sd_mount_failed)\n",
                      static_cast<unsigned>(g_hotline_voice_suffix_catalog.size()));
        return;
    }

    File dir = sd_fs->open(kHotlineAssetsRoot);
    if (!dir || !dir.isDirectory()) {
        g_hotline_voice_catalog_sd_scanned = true;
        g_hotline_voice_catalog_scanned = true;
        Serial.printf("[RTC_BL_PHONE] hotline voice catalog suffix_count=%u (dir_missing)\n",
                      static_cast<unsigned>(g_hotline_voice_suffix_catalog.size()));
        if (dir) {
            dir.close();
        }
        return;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }

        String name = entry.name();
        entry.close();
        name = sanitizeMediaPath(name);
        if (name.isEmpty()) {
            continue;
        }
        const int slash = name.lastIndexOf('/');
        if (slash >= 0 && static_cast<size_t>(slash + 1) < name.length()) {
            name = name.substring(static_cast<unsigned int>(slash + 1));
        }
        appendHotlineVoiceSuffixCatalog(name);
    }
    dir.close();

    g_hotline_voice_catalog_sd_scanned = true;
    g_hotline_voice_catalog_scanned = true;
    Serial.printf("[RTC_BL_PHONE] hotline voice catalog suffix_count=%u\n",
                  static_cast<unsigned>(g_hotline_voice_suffix_catalog.size()));
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

    DynamicJsonDocument doc(1024);
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

void initEspNowSceneSyncRuntime() {
    g_espnow_scene_sync = EspNowSceneSyncRuntimeState{};
    g_espnow_scene_sync.next_sync_ms = millis() + g_espnow_scene_sync.interval_ms;
    Serial.printf("[RTC_BL_PHONE] espnow scene sync runtime enabled interval_ms=%lu\n",
                  static_cast<unsigned long>(g_espnow_scene_sync.interval_ms));
}

bool requestSceneSyncFromFreenove(const char* reason, bool force_now) {
    if (!g_espnow_scene_sync.enabled || !g_espnow.isReady()) {
        g_espnow_scene_sync.last_error = "scene_sync_espnow_not_ready";
        return false;
    }

    if (g_espnow_scene_sync.request_pending && !force_now) {
        return false;
    }

    const uint32_t now = millis();
    if (force_now && g_espnow_scene_sync.request_pending) {
        g_espnow_scene_sync.request_pending = false;
        g_espnow_scene_sync.request_msg_id = "";
        g_espnow_scene_sync.request_seq = 0U;
        g_espnow_scene_sync.request_deadline_ms = 0U;
    }

    const uint32_t request_index = g_espnow_scene_sync.requests_sent + 1U;
    const String msg_id = String("scene-sync-") + String(now) + "-" + String(request_index);
    const uint32_t seq = now;

    DynamicJsonDocument doc(1024);
    doc["msg_id"] = msg_id;
    doc["seq"] = seq;
    doc["type"] = "command";
    doc["ack"] = true;
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["cmd"] = "UI_SCENE_STATUS";

    String wire;
    serializeJson(doc, wire);
    if (!g_espnow.sendJson("broadcast", wire)) {
        g_espnow_scene_sync.request_send_fail++;
        g_espnow_scene_sync.last_error = "scene_sync_send_failed";
        appendHotlineLogLine("SCENE_SYNC_SEND_FAIL", String("reason=") + (reason == nullptr ? "" : reason));
        g_espnow_scene_sync.next_sync_ms = now + g_espnow_scene_sync.interval_ms;
        return false;
    }

    g_espnow_scene_sync.requests_sent = request_index;
    g_espnow_scene_sync.request_pending = true;
    g_espnow_scene_sync.request_msg_id = msg_id;
    g_espnow_scene_sync.request_seq = seq;
    g_espnow_scene_sync.request_deadline_ms = now + g_espnow_scene_sync.ack_window_ms;
    g_espnow_scene_sync.next_sync_ms = now + g_espnow_scene_sync.interval_ms;
    g_espnow_scene_sync.last_error = "";
    if (reason != nullptr && reason[0] != '\0') {
        Serial.printf("[RTC_BL_PHONE] scene sync request reason=%s msg_id=%s\n", reason, msg_id.c_str());
    }
    appendHotlineLogLine("SCENE_SYNC_REQ", String("reason=") + (reason == nullptr ? "" : reason) + " msg_id=" + msg_id);
    return true;
}

bool maybeTrackEspNowSceneSyncAck(const String& source, const JsonVariantConst& payload) {
    if (!g_espnow_scene_sync.enabled || !g_espnow_scene_sync.request_pending) {
        return false;
    }
    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    JsonObjectConst root = payload.as<JsonObjectConst>();
    String type = root["type"] | "";
    type.toLowerCase();
    if (type != "ack") {
        return false;
    }

    const String msg_id = root["msg_id"] | "";
    const uint32_t seq = root["seq"] | 0U;
    if (msg_id != g_espnow_scene_sync.request_msg_id || seq != g_espnow_scene_sync.request_seq) {
        return false;
    }

    g_espnow_scene_sync.request_pending = false;
    g_espnow_scene_sync.request_msg_id = "";
    g_espnow_scene_sync.request_seq = 0U;
    g_espnow_scene_sync.request_deadline_ms = 0U;
    g_espnow_scene_sync.last_source = A252ConfigStore::normalizeMac(source);

    if (!root["payload"].is<JsonObjectConst>()) {
        g_espnow_scene_sync.request_ack_fail++;
        g_espnow_scene_sync.last_error = "scene_sync_ack_missing_payload";
        return true;
    }

    JsonObjectConst ack_payload = root["payload"].as<JsonObjectConst>();
    const bool ok = ack_payload["ok"] | false;
    if (!ok) {
        g_espnow_scene_sync.request_ack_fail++;
        const String error_text = ack_payload["error"] | "";
        g_espnow_scene_sync.last_error = error_text.isEmpty() ? String("scene_sync_ack_not_ok") : error_text;
        appendHotlineLogLine("SCENE_SYNC_ACK_FAIL", g_espnow_scene_sync.last_error);
        return true;
    }

    if (!ack_payload["data"].is<JsonObjectConst>()) {
        g_espnow_scene_sync.request_ack_fail++;
        g_espnow_scene_sync.last_error = "scene_sync_ack_missing_data";
        appendHotlineLogLine("SCENE_SYNC_ACK_FAIL", g_espnow_scene_sync.last_error);
        return true;
    }

    JsonObjectConst scene_data = ack_payload["data"].as<JsonObjectConst>();
    String scene_id = scene_data["scene_id"] | scene_data["scene"] | "";
    scene_id.trim();
    String step_id = scene_data["step_id"] | scene_data["step"] | "";
    step_id.trim();
    if (!scene_id.isEmpty()) {
        g_active_scene_id = scene_id;
        g_hotline_validation_state = inferHotlineValidationStateFromSceneKey(normalizeHotlineSceneKey(scene_id));
    }
    if (!step_id.isEmpty()) {
        g_active_step_id = step_id;
    }

    g_espnow_scene_sync.request_ack_ok++;
    g_espnow_scene_sync.last_update_ms = millis();
    g_espnow_scene_sync.last_error = "";
    Serial.printf("[RTC_BL_PHONE] scene sync ack scene=%s step=%s\n",
                  g_active_scene_id.c_str(),
                  g_active_step_id.c_str());
    appendHotlineLogLine("SCENE_SYNC_ACK", String("scene=") + g_active_scene_id + " step=" + g_active_step_id);
    return true;
}

void tickEspNowSceneSyncRuntime() {
    if (!g_espnow_scene_sync.enabled || !g_espnow.isReady()) {
        return;
    }

    const uint32_t now = millis();
    if (g_espnow_scene_sync.request_pending) {
        if (static_cast<int32_t>(now - g_espnow_scene_sync.request_deadline_ms) >= 0) {
            g_espnow_scene_sync.request_pending = false;
            g_espnow_scene_sync.request_msg_id = "";
            g_espnow_scene_sync.request_seq = 0U;
            g_espnow_scene_sync.request_deadline_ms = 0U;
            g_espnow_scene_sync.request_ack_fail++;
            g_espnow_scene_sync.last_error = "scene_sync_timeout";
            appendHotlineLogLine("SCENE_SYNC_TIMEOUT", "");
        } else {
            return;
        }
    }

    if (static_cast<int32_t>(now - g_espnow_scene_sync.next_sync_ms) >= 0) {
        requestSceneSyncFromFreenove("periodic", false);
    }
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

bool ensureSdMountedForList(fs::FS*& out_fs) {
    out_fs = nullptr;
    if (SD_MMC.begin()) {
        out_fs = &SD_MMC;
        return true;
    }

    static bool sd_spi_bus_started = false;
    if (!sd_spi_bus_started) {
        SPI.begin(A1S_SD_SCK, A1S_SD_MISO, A1S_SD_MOSI, A1S_SD_CS);
        sd_spi_bus_started = true;
    }
    if (SD.begin(A1S_SD_CS, SPI, 10000000U)) {
        out_fs = &SD;
        return true;
    }
    return false;
}

bool resolveFsListSource(MediaSource source_requested, fs::FS*& out_fs, MediaSource& out_source) {
    out_fs = nullptr;
    out_source = MediaSource::AUTO;

    auto use_sd = [&]() -> bool {
        fs::FS* sd_fs = nullptr;
        if (!ensureSdMountedForList(sd_fs) || sd_fs == nullptr) {
            return false;
        }
        out_fs = sd_fs;
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
        DynamicJsonDocument doc(1024);
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

    DynamicJsonDocument doc(1024);
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
        JsonObject item = entries.createNestedObject();
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
    return String(kHotlineAssetsRoot) + "/" + clean_stem + kHotlineDefaultVoiceSuffix;
}

String buildHotlineVoicePathFromStemWithSuffix(const String& stem, const String& raw_suffix) {
    String clean_stem = stem;
    clean_stem.trim();
    if (clean_stem.isEmpty()) {
        return "";
    }
    const String suffix = normalizeHotlineVoiceSuffix(raw_suffix);
    if (suffix.isEmpty()) {
        return "";
    }
    return String(kHotlineAssetsRoot) + "/" + clean_stem + suffix;
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

    if (normalized.length() <= 4U) {
        return "";
    }

    String base = normalized.substring(0, normalized.length() - 4U);
    const int slash = base.lastIndexOf('/');
    const int marker = base.indexOf("__", (slash < 0) ? 0U : static_cast<unsigned int>(slash + 1));
    if (marker >= 0) {
        base = base.substring(0, static_cast<unsigned int>(marker));
    }
    return base + ".wav";
}

bool isMp3MediaPath(const String& path) {
    String normalized = sanitizeMediaPath(path);
    if (normalized.isEmpty()) {
        return false;
    }
    normalized.toLowerCase();
    return normalized.endsWith(".mp3");
}

bool isWavMediaPath(const String& path) {
    String normalized = sanitizeMediaPath(path);
    if (normalized.isEmpty()) {
        return false;
    }
    normalized.toLowerCase();
    return normalized.endsWith(".wav");
}

bool isPlayableMediaPath(const String& path) {
    return isMp3MediaPath(path) || isWavMediaPath(path);
}

bool mediaPathExistsForProbe(const String& path, MediaSource source) {
    const String normalized = sanitizeMediaPath(path);
    if (normalized.isEmpty()) {
        return false;
    }

    if (source != MediaSource::SD) {
        // Keep non-SD sources untouched by this fast-path guard.
        return true;
    }

    fs::FS* sd_fs = nullptr;
    if (!ensureHotlineSdMounted(sd_fs) || sd_fs == nullptr) {
        return false;
    }
    return sd_fs->exists(normalized.c_str());
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

struct HotlineSceneStemEntry {
    const char* scene_key;
    const char* stem;
};

constexpr HotlineSceneStemEntry kHotlineSceneStemTable[] = {
    {"U_SON_PROTO", "fiches-hotline_2"},
    {"LA_DETECTOR", "scene_la_detector_2"},
    {"WIN_ETAPE", "scene_win_2"},
    {"WARNING", "scene_broken_2"},
    {"CREDITS", "scene_win_2"},
    {"WIN_ETAPE1", "scene_win_2"},
    {"WIN_ETAPE2", "scene_win_2"},
    {"QR_DETECTOR", "scene_camera_scan_2"},
    {"LEFOU_DETECTOR", "scene_search_2"},
    {"POLICE_CHASE_ARCADE", "scene_search_2"},
};

const char* hotlineLookupSceneStem(const String& scene_key) {
    for (const auto& entry : kHotlineSceneStemTable) {
        if (scene_key.equals(entry.scene_key)) {
            return entry.stem;
        }
    }
    return nullptr;
}

String hotlineSceneStemFromKey(const String& scene_key) {
    if (const char* explicit_stem = hotlineLookupSceneStem(scene_key)) {
        return String(explicit_stem);
    }

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
    if (scene_key == "POLICE_CHASE_ARCADE") {
        return "scene_search_2";
    }
    if (scene_key == "MEDIA_ARCHIVE") {
        return "scene_media_archive_2";
    }
    if (scene_key == "CREDITS") {
        return "scene_win_2";
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

const char* hotlineValidationStateToString(HotlineValidationState state) {
    switch (state) {
        case HotlineValidationState::kWaiting:
            return "waiting";
        case HotlineValidationState::kGranted:
            return "granted";
        case HotlineValidationState::kRefused:
            return "refused";
        case HotlineValidationState::kNone:
        default:
            return "none";
    }
}

struct HotlineExplicitRouteEntry {
    const char* scene_key;   // "*" matches any scene key.
    HotlineValidationState state;
    const char* digit_key;   // "none" for state cue, "1|2|3" for hint route.
    const char* stem_suffix; // suffix appended to scene stem.
};

constexpr HotlineExplicitRouteEntry kHotlineExplicitRouteTable[] = {
    {"*", HotlineValidationState::kWaiting, "none", "waiting_validation"},
    {"*", HotlineValidationState::kWaiting, "none", "validation_waiting"},
    {"*", HotlineValidationState::kGranted, "none", "validation_granted"},
    {"*", HotlineValidationState::kGranted, "none", "validation_ok"},
    {"*", HotlineValidationState::kRefused, "none", "validation_refused"},
    {"*", HotlineValidationState::kRefused, "none", "validation_warning"},
    {"*", HotlineValidationState::kRefused, "none", "warning"},
    {"*", HotlineValidationState::kWaiting, "1", "hint_1_waiting"},
    {"*", HotlineValidationState::kWaiting, "2", "hint_2_waiting"},
    {"*", HotlineValidationState::kWaiting, "3", "hint_3_waiting"},
    {"*", HotlineValidationState::kGranted, "1", "hint_1_granted"},
    {"*", HotlineValidationState::kGranted, "2", "hint_2_granted"},
    {"*", HotlineValidationState::kGranted, "3", "hint_3_granted"},
    {"*", HotlineValidationState::kRefused, "1", "hint_1_refused"},
    {"*", HotlineValidationState::kRefused, "2", "hint_2_refused"},
    {"*", HotlineValidationState::kRefused, "3", "hint_3_refused"},
    {"*", HotlineValidationState::kNone, "1", "hint_1"},
    {"*", HotlineValidationState::kNone, "2", "hint_2"},
    {"*", HotlineValidationState::kNone, "3", "hint_3"},
};

String normalizeHotlineDigitKey(const String& raw_digit) {
    String digit = raw_digit;
    digit.trim();
    if (digit.isEmpty()) {
        return "none";
    }
    return digit;
}

String buildHotlineLookupKey(const String& scene_key, HotlineValidationState state, const String& digit_key) {
    String key = scene_key;
    key.trim();
    if (key.isEmpty()) {
        key = "NONE";
    }
    String lookup = key;
    lookup += "|";
    lookup += hotlineValidationStateToString(state);
    lookup += "|";
    lookup += normalizeHotlineDigitKey(digit_key);
    return lookup;
}

String describeMediaRouteTarget(const MediaRouteEntry& route) {
    if (route.kind == MediaRouteKind::FILE) {
        return route.path;
    }
    String target = "tone:";
    target += toneProfileToString(route.tone.profile);
    target += ":";
    target += toneEventToString(route.tone.event);
    return target;
}

void noteHotlineRouteResolution(const String& lookup_key, const String& method, const MediaRouteEntry& route) {
    g_hotline.last_route_lookup_key = lookup_key;
    g_hotline.last_route_resolution = method;
    g_hotline.last_route_target = describeMediaRouteTarget(route);
    Serial.printf("[HotlineRoute] key=%s method=%s target=%s\n",
                  g_hotline.last_route_lookup_key.c_str(),
                  g_hotline.last_route_resolution.c_str(),
                  g_hotline.last_route_target.c_str());
}

bool parseHotlineValidationStateToken(const String& raw_token, HotlineValidationState* out_state) {
    if (out_state == nullptr) {
        return false;
    }
    String token = raw_token;
    token.trim();
    token.toUpperCase();
    if (token.isEmpty()) {
        return false;
    }
    if (token == "NONE" || token == "IDLE") {
        *out_state = HotlineValidationState::kNone;
        return true;
    }
    if (token == "WAITING" || token == "WAIT" || token == "PENDING") {
        *out_state = HotlineValidationState::kWaiting;
        return true;
    }
    if (token == "GRANTED" || token == "WIN" || token == "OK" || token == "SUCCESS") {
        *out_state = HotlineValidationState::kGranted;
        return true;
    }
    if (token == "REFUSED" || token == "DENIED" || token == "WARNING" || token == "KO" || token == "FAIL") {
        *out_state = HotlineValidationState::kRefused;
        return true;
    }
    return false;
}

HotlineValidationState inferHotlineValidationStateFromSceneKey(const String& scene_key) {
    if (scene_key == "WIN_ETAPE" || scene_key == "WIN_ETAPE2") {
        return HotlineValidationState::kWaiting;
    }
    if (scene_key == "BROKEN" || scene_key == "SIGNAL_SPIKE" || scene_key == "WARNING") {
        return HotlineValidationState::kRefused;
    }
    if (scene_key == "WIN" || scene_key == "REWARD" || scene_key == "WINNER" || scene_key == "FINAL_WIN" ||
        scene_key == "FIREWORKS" || scene_key == "WIN_ETAPE1") {
        return HotlineValidationState::kGranted;
    }
    return HotlineValidationState::kNone;
}

HotlineValidationState inferHotlineValidationStateFromStepId(const String& raw_step_id) {
    String step_id = raw_step_id;
    step_id.trim();
    step_id.toUpperCase();
    if (step_id.isEmpty()) {
        return HotlineValidationState::kNone;
    }
    if (step_id.indexOf("RTC_ESP_ETAPE") >= 0 || step_id.indexOf("WAITING") >= 0 || step_id.indexOf("PENDING") >= 0) {
        return HotlineValidationState::kWaiting;
    }
    if (step_id.indexOf("WARNING") >= 0 || step_id.indexOf("REFUS") >= 0 || step_id.indexOf("BROKEN") >= 0) {
        return HotlineValidationState::kRefused;
    }
    if (step_id.indexOf("WIN") >= 0 || step_id.indexOf("FINAL") >= 0 || step_id.indexOf("REWARD") >= 0) {
        return HotlineValidationState::kGranted;
    }
    return HotlineValidationState::kNone;
}

String stripHotlineStemTierSuffix(const String& stem) {
    if (stem.length() > 2U && stem.endsWith("_2")) {
        return stem.substring(0, stem.length() - 2U);
    }
    return stem;
}

void appendHotlineStemCandidate(const String& candidate, String* out_candidates, size_t capacity, size_t* inout_count) {
    if (out_candidates == nullptr || inout_count == nullptr || *inout_count >= capacity) {
        return;
    }
    String clean = candidate;
    clean.trim();
    if (clean.isEmpty()) {
        return;
    }
    for (size_t index = 0; index < *inout_count; ++index) {
        if (out_candidates[index] == clean) {
            return;
        }
    }
    out_candidates[*inout_count] = clean;
    ++(*inout_count);
}

void appendHotlineStemVariants(const String& scene_stem,
                               const String& variant,
                               String* out_candidates,
                               size_t capacity,
                               size_t* inout_count) {
    String clean_stem = scene_stem;
    clean_stem.trim();
    String clean_variant = variant;
    clean_variant.trim();
    if (clean_stem.isEmpty() || clean_variant.isEmpty()) {
        return;
    }

    appendHotlineStemCandidate(clean_stem + "_" + clean_variant, out_candidates, capacity, inout_count);

    const String stem_without_tier = stripHotlineStemTierSuffix(clean_stem);
    if (!stem_without_tier.isEmpty() && stem_without_tier != clean_stem) {
        appendHotlineStemCandidate(stem_without_tier + "_" + clean_variant + "_2",
                                   out_candidates,
                                   capacity,
                                   inout_count);
        appendHotlineStemCandidate(stem_without_tier + "_" + clean_variant, out_candidates, capacity, inout_count);
    }
}

bool resolveHotlineSceneDirectoryVariantRoute(const String& candidate_path,
                                              MediaRouteEntry& out_route,
                                              String* out_matched_file) {
    out_route = MediaRouteEntry{};
    if (candidate_path.isEmpty()) {
        return false;
    }

    const int slash = candidate_path.lastIndexOf('/');
    if (slash <= 0 || static_cast<size_t>(slash + 1) >= candidate_path.length()) {
        return false;
    }
    const String dir_path = candidate_path.substring(0U, static_cast<unsigned int>(slash));
    const String base_name = candidate_path.substring(static_cast<unsigned int>(slash + 1));

    const int dot = base_name.lastIndexOf('.');
    if (dot <= 0 || static_cast<size_t>(dot) >= base_name.length()) {
        return false;
    }
    const String base_stem = base_name.substring(0U, static_cast<unsigned int>(dot));
    String base_ext = base_name.substring(static_cast<unsigned int>(dot));
    base_ext.toLowerCase();
    if (base_stem.isEmpty() || base_ext.isEmpty()) {
        return false;
    }

    fs::FS* sd_fs = nullptr;
    if (!ensureSdMountedForList(sd_fs) || sd_fs == nullptr) {
        return false;
    }
    if (!sd_fs->exists(dir_path.c_str())) {
        return false;
    }
    File directory = sd_fs->open(dir_path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return false;
    }

    String match_paths[32];
    size_t match_count = 0U;
    File entry = directory.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int entry_slash = name.lastIndexOf('/');
            if (entry_slash >= 0 && static_cast<size_t>(entry_slash + 1) < name.length()) {
                name = name.substring(static_cast<unsigned int>(entry_slash + 1));
            }
            const int entry_dot = name.lastIndexOf('.');
            if (entry_dot > 0 && static_cast<size_t>(entry_dot) < name.length()) {
                String entry_ext = name.substring(static_cast<unsigned int>(entry_dot));
                entry_ext.toLowerCase();
                if (entry_ext == base_ext) {
                    const String entry_stem = name.substring(0U, static_cast<unsigned int>(entry_dot));
                    if (entry_stem.equalsIgnoreCase(base_stem) ||
                        entry_stem.startsWith(base_stem + "_")) {
                        appendHotlineStemCandidate(buildFsListEntryPath(dir_path, name),
                                                   match_paths,
                                                   32U,
                                                   &match_count);
                    }
                }
            }
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();

    if (match_count == 0U) {
        return false;
    }

    AudioPlaybackProbeResult probe;
    const size_t start = static_cast<size_t>(nextHotlineRandom32() % static_cast<uint32_t>(match_count));
    for (size_t pass = 0U; pass < match_count; ++pass) {
        const size_t index = (start + pass) % match_count;
        MediaRouteEntry route = buildHotlineSdFileRoute(match_paths[index], false, 0U);
        if (!mediaRouteHasPayload(route)) {
            continue;
        }
        if (!mediaPathExistsForProbe(route.path, route.source)) {
            continue;
        }
        if (g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe)) {
            out_route = route;
            if (out_matched_file != nullptr) {
                *out_matched_file = route.path;
            }
            return true;
        }
    }

    return false;
}

bool sceneKeyPrefersWarningStem(const String& scene_key) {
    return scene_key == "U_SON_PROTO" || scene_key == "POLICE_CHASE_ARCADE";
}

bool sceneKeyPrefersIndiceStem(const String& scene_key) {
    return scene_key == "LA_DETECTOR" || scene_key == "WARNING" ||
           scene_key == "QR_DETECTOR" || scene_key == "LEFOU_DETECTOR";
}

bool sceneKeyPrefersBravoStem(const String& scene_key) {
    return scene_key == "CREDITS" || scene_key == "WIN_ETAPE1" || scene_key == "WIN_ETAPE2";
}

bool resolveHotlineSceneStemRoute(const String& raw_scene_key,
                                  const String& raw_stem,
                                  MediaRouteEntry& out_route,
                                  String* out_matched_file) {
    out_route = MediaRouteEntry{};
    if (out_matched_file != nullptr) {
        *out_matched_file = "";
    }

    String scene_key = normalizeHotlineSceneKey(raw_scene_key);
    if (scene_key.isEmpty()) {
        return false;
    }

    String stem = raw_stem;
    stem.trim();
    stem.toLowerCase();
    if (stem.isEmpty()) {
        return false;
    }

    fs::FS* sd_fs = nullptr;
    if (!ensureHotlineSdMounted(sd_fs) || sd_fs == nullptr) {
        return false;
    }

    const String scene_dir = String("SCENE_") + scene_key;
    const String roots[2] = {
        String(kHotlineTtsAssetsRoot) + "/" + scene_dir,
        String(kHotlineTtsNestedAssetsRoot) + "/" + scene_dir,
    };

    String wav_paths[48];
    size_t wav_count = 0U;
    String mp3_paths[48];
    size_t mp3_count = 0U;

    auto append_path = [](String* bucket, size_t capacity, size_t* inout_count, const String& candidate) {
        if (bucket == nullptr || inout_count == nullptr || *inout_count >= capacity || candidate.isEmpty()) {
            return;
        }
        for (size_t idx = 0U; idx < *inout_count; ++idx) {
            if (bucket[idx].equalsIgnoreCase(candidate)) {
                return;
            }
        }
        bucket[*inout_count] = candidate;
        ++(*inout_count);
    };

    for (const String& root : roots) {
        if (!sd_fs->exists(root.c_str())) {
            continue;
        }
        File directory = sd_fs->open(root.c_str(), FILE_READ);
        if (!directory || !directory.isDirectory()) {
            if (directory) {
                directory.close();
            }
            continue;
        }

        File entry = directory.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                String name = entry.name();
                const int entry_slash = name.lastIndexOf('/');
                if (entry_slash >= 0 && static_cast<size_t>(entry_slash + 1) < name.length()) {
                    name = name.substring(static_cast<unsigned int>(entry_slash + 1));
                }
                const int entry_dot = name.lastIndexOf('.');
                if (entry_dot > 0 && static_cast<size_t>(entry_dot) < name.length()) {
                    String entry_stem = name.substring(0U, static_cast<unsigned int>(entry_dot));
                    String entry_stem_lower = entry_stem;
                    entry_stem_lower.toLowerCase();

                    String entry_ext = name.substring(static_cast<unsigned int>(entry_dot));
                    entry_ext.toLowerCase();

                    bool stem_match = entry_stem_lower == stem;
                    if (!stem_match && entry_stem_lower.startsWith(stem)) {
                        const size_t stem_len = stem.length();
                        if (stem_len < entry_stem_lower.length()) {
                            const char next = entry_stem_lower.charAt(static_cast<unsigned int>(stem_len));
                            stem_match = (next == '_' || next == '-' || next == '.' ||
                                          (next >= 'a' && next <= 'z'));
                        }
                    }
                    if (stem_match) {
                        const String full_path = buildFsListEntryPath(root, name);
                        if (entry_ext == ".wav") {
                            append_path(wav_paths, 48U, &wav_count, full_path);
                        } else if (entry_ext == ".mp3") {
                            append_path(mp3_paths, 48U, &mp3_count, full_path);
                        }
                    }
                }
            }
            entry.close();
            entry = directory.openNextFile();
        }
        directory.close();
    }

    AudioPlaybackProbeResult probe;
    auto try_bucket = [&](String* bucket, size_t count) -> bool {
        if (bucket == nullptr || count == 0U) {
            return false;
        }
        const size_t start = static_cast<size_t>(nextHotlineRandom32() % static_cast<uint32_t>(count));
        for (size_t pass = 0U; pass < count; ++pass) {
            const size_t idx = (start + pass) % count;
            const String& candidate = bucket[idx];
            MediaRouteEntry route = buildHotlineSdFileRoute(candidate, false, 0U);
            if (!mediaRouteHasPayload(route)) {
                continue;
            }
            if (!mediaPathExistsForProbe(route.path, route.source)) {
                continue;
            }
            if (g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe)) {
                out_route = route;
                if (out_matched_file != nullptr) {
                    *out_matched_file = route.path;
                }
                return true;
            }
        }
        return false;
    };

    if (try_bucket(wav_paths, wav_count)) {
        return true;
    }
    return try_bucket(mp3_paths, mp3_count);
}

bool resolveHotlineSceneDirectoryRoute(const String& raw_scene_key,
                                       HotlineValidationState state,
                                       const String& digit_key,
                                       MediaRouteEntry& out_route,
                                       String* out_matched_file) {
    out_route = MediaRouteEntry{};
    if (out_matched_file != nullptr) {
        *out_matched_file = "";
    }

    String scene_key = normalizeHotlineSceneKey(raw_scene_key);
    if (scene_key.isEmpty()) {
        scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    }
    if (scene_key.isEmpty()) {
        scene_key = "U_SON_PROTO";
    }

    String scene_dir = "SCENE_";
    scene_dir += scene_key;

    const String normalized_digit = normalizeHotlineDigitKey(digit_key);
    const bool has_hint_digit =
        normalized_digit.length() == 1U && normalized_digit[0] >= '1' && normalized_digit[0] <= '3';
    const bool scene_is_win =
        scene_key == "WIN_ETAPE" || scene_key == "WIN_ETAPE1" || scene_key == "WIN_ETAPE2" || scene_key == "CREDITS";
    const bool scene_is_warning = scene_key == "WARNING";
    const bool scene_prefers_warning_hint =
        scene_key == "U_SON_PROTO" || scene_key == "POLICE_CHASE_ARCADE";
    const bool scene_prefers_indice_hint =
        scene_key == "LA_DETECTOR" || scene_key == "WARNING" || scene_key == "QR_DETECTOR" ||
        scene_key == "LEFOU_DETECTOR";
    const bool scene_prefers_bravo_hint = scene_is_win;

    auto append_scene_file = [&](const String& file_name, String* out_paths, size_t capacity, size_t* inout_count) {
        String clean_file = file_name;
        clean_file.trim();
        if (clean_file.isEmpty()) {
            return;
        }

        auto append_for_file = [&](const String& candidate_file) {
            String path_primary = String(kHotlineTtsAssetsRoot) + "/" + scene_dir + "/" + candidate_file;
            appendHotlineStemCandidate(path_primary, out_paths, capacity, inout_count);

            String path_nested = String(kHotlineTtsNestedAssetsRoot) + "/" + scene_dir + "/" + candidate_file;
            appendHotlineStemCandidate(path_nested, out_paths, capacity, inout_count);
        };

        String lower = clean_file;
        lower.toLowerCase();
        if (lower.endsWith(".mp3")) {
            String wav_file = clean_file.substring(0U, clean_file.length() - 4U) + ".wav";
            append_for_file(wav_file);
            append_for_file(clean_file);
            return;
        }
        if (lower.endsWith(".wav")) {
            append_for_file(clean_file);
            String mp3_file = clean_file.substring(0U, clean_file.length() - 4U) + ".mp3";
            append_for_file(mp3_file);
            return;
        }

        append_for_file(clean_file + ".wav");
        append_for_file(clean_file + ".mp3");
    };

    String paths[32];
    size_t path_count = 0U;

    if (has_hint_digit) {
        if (scene_prefers_warning_hint) {
            append_scene_file(String("warning_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        }
        if (scene_prefers_bravo_hint) {
            append_scene_file(String("bravo_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        }
        if (scene_prefers_indice_hint || (!scene_prefers_warning_hint && !scene_prefers_bravo_hint)) {
            append_scene_file(String("indice_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        }
        append_scene_file(String("hint_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        if (scene_is_warning || state == HotlineValidationState::kRefused) {
            append_scene_file(String("warning_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        }
        if (scene_prefers_bravo_hint || state == HotlineValidationState::kGranted) {
            append_scene_file(String("bravo_") + normalized_digit + ".mp3", paths, 32U, &path_count);
        }
    }

    if (!has_hint_digit || normalized_digit == "none") {
        switch (state) {
            case HotlineValidationState::kWaiting:
                append_scene_file("attente_validation.mp3", paths, 32U, &path_count);
                append_scene_file("waiting_validation.mp3", paths, 32U, &path_count);
                append_scene_file("validation_waiting.mp3", paths, 32U, &path_count);
                break;
            case HotlineValidationState::kGranted:
                append_scene_file("validation_ok.mp3", paths, 32U, &path_count);
                append_scene_file("validation_granted.mp3", paths, 32U, &path_count);
                append_scene_file("bravo_1.mp3", paths, 32U, &path_count);
                break;
            case HotlineValidationState::kRefused:
                if (scene_prefers_indice_hint) {
                    append_scene_file("indice_1.mp3", paths, 32U, &path_count);
                }
                if (scene_prefers_warning_hint) {
                    append_scene_file("warning_1.mp3", paths, 32U, &path_count);
                }
                append_scene_file("validation_ko.mp3", paths, 32U, &path_count);
                append_scene_file("validation_refused.mp3", paths, 32U, &path_count);
                append_scene_file("validation_warning.mp3", paths, 32U, &path_count);
                append_scene_file("warning_1.mp3", paths, 32U, &path_count);
                break;
            case HotlineValidationState::kNone:
            default:
                if (scene_prefers_warning_hint) {
                    append_scene_file("warning_1.mp3", paths, 32U, &path_count);
                }
                if (scene_prefers_indice_hint) {
                    append_scene_file("indice_1.mp3", paths, 32U, &path_count);
                }
                if (scene_prefers_bravo_hint) {
                    append_scene_file("bravo_1.mp3", paths, 32U, &path_count);
                    append_scene_file("attente_validation.mp3", paths, 32U, &path_count);
                }
                if (!scene_prefers_warning_hint && !scene_prefers_indice_hint && !scene_prefers_bravo_hint) {
                    append_scene_file("indice_1.mp3", paths, 32U, &path_count);
                    append_scene_file("attente_validation.mp3", paths, 32U, &path_count);
                }
                break;
        }
    }

    // Last-resort filenames accepted by the hotline_tts tree.
    if (path_count == 0U) {
        append_scene_file("indice_1.mp3", paths, 32U, &path_count);
        append_scene_file("attente_validation.mp3", paths, 32U, &path_count);
    }

    AudioPlaybackProbeResult probe;
    for (size_t index = 0U; index < path_count; ++index) {
        if (paths[index].isEmpty()) {
            continue;
        }
        MediaRouteEntry route = buildHotlineSdFileRoute(paths[index], false, 0U);
        if (!mediaRouteHasPayload(route)) {
            continue;
        }
        if (!mediaPathExistsForProbe(route.path, route.source)) {
            continue;
        }
        if (g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe)) {
            out_route = route;
            if (out_matched_file != nullptr) {
                *out_matched_file = route.path;
            }
            return true;
        }

        MediaRouteEntry variant_route;
        if (resolveHotlineSceneDirectoryVariantRoute(route.path, variant_route, out_matched_file)) {
            out_route = variant_route;
            return true;
        }

        const String fallback_wav = buildMp3FallbackWavPath(route.path);
        if (!fallback_wav.isEmpty() && mediaPathExistsForProbe(fallback_wav, route.source) &&
            g_audio.probePlaybackFileFromSource(fallback_wav.c_str(), route.source, probe)) {
            out_route = buildHotlineSdFileRoute(fallback_wav, false, 0U);
            if (out_matched_file != nullptr) {
                *out_matched_file = fallback_wav;
            }
            return true;
        }

        MediaRouteEntry wav_variant_route;
        if (!fallback_wav.isEmpty() &&
            resolveHotlineSceneDirectoryVariantRoute(fallback_wav, wav_variant_route, out_matched_file)) {
            out_route = wav_variant_route;
            return true;
        }
    }

    return false;
}

bool resolveHotlineExplicitRoute(const String& scene_key,
                                 HotlineValidationState state,
                                 const String& digit_key,
                                 MediaRouteEntry& out_route,
                                 String* out_lookup_key = nullptr,
                                 String* out_matched_suffix = nullptr) {
    out_route = MediaRouteEntry{};
    const String normalized_digit = normalizeHotlineDigitKey(digit_key);
    const String lookup_key = buildHotlineLookupKey(scene_key, state, normalized_digit);
    if (out_lookup_key != nullptr) {
        *out_lookup_key = lookup_key;
    }

    String matched_scene_file;
    if (resolveHotlineSceneDirectoryRoute(scene_key, state, normalized_digit, out_route, &matched_scene_file)) {
        if (out_matched_suffix != nullptr) {
            *out_matched_suffix = String("scene_tts_dir:") + matched_scene_file;
        }
        return true;
    }

    if (normalized_digit == "none") {
        String state_stem;
        if (state == HotlineValidationState::kWaiting) {
            state_stem = "attente_validation";
        } else if (state == HotlineValidationState::kGranted) {
            state_stem = "validation_ok";
        } else if (state == HotlineValidationState::kRefused) {
            state_stem = "validation_ko";
        } else if (sceneKeyPrefersWarningStem(scene_key)) {
            state_stem = "warning_1";
        } else if (sceneKeyPrefersIndiceStem(scene_key)) {
            state_stem = "indice_1";
        } else if (sceneKeyPrefersBravoStem(scene_key)) {
            state_stem = "bravo_1";
        }

        if (!state_stem.isEmpty() &&
            resolveHotlineSceneStemRoute(scene_key, state_stem, out_route, &matched_scene_file)) {
            if (out_matched_suffix != nullptr) {
                *out_matched_suffix = String("scene_tts_stem:") + matched_scene_file;
            }
            return true;
        }
    } else if (normalized_digit.length() == 1U &&
               normalized_digit[0] >= '1' &&
               normalized_digit[0] <= '3') {
        String hint_stem;
        if (sceneKeyPrefersWarningStem(scene_key)) {
            hint_stem = String("warning_") + normalized_digit;
        } else if (sceneKeyPrefersIndiceStem(scene_key)) {
            hint_stem = String("indice_") + normalized_digit;
        } else if (sceneKeyPrefersBravoStem(scene_key)) {
            hint_stem = String("bravo_") + normalized_digit;
        }

        if (!hint_stem.isEmpty() &&
            resolveHotlineSceneStemRoute(scene_key, hint_stem, out_route, &matched_scene_file)) {
            if (out_matched_suffix != nullptr) {
                *out_matched_suffix = String("scene_tts_stem:") + matched_scene_file;
            }
            return true;
        }
    }

    const String scene_stem = hotlineSceneStemFromKey(scene_key);
    if (scene_stem.isEmpty()) {
        return false;
    }

    for (const HotlineExplicitRouteEntry& entry : kHotlineExplicitRouteTable) {
        const bool scene_match = (strcmp(entry.scene_key, "*") == 0) || scene_key.equalsIgnoreCase(entry.scene_key);
        if (!scene_match || entry.state != state || !normalized_digit.equalsIgnoreCase(entry.digit_key)) {
            continue;
        }
        String stems[12];
        size_t stem_count = 0U;
        appendHotlineStemVariants(scene_stem, entry.stem_suffix, stems, 12U, &stem_count);
        appendHotlineStemCandidate(String("hotline_") + entry.stem_suffix, stems, 12U, &stem_count);
        if (!resolveHotlineVoiceRouteFromStemCandidates(stems, stem_count, out_route)) {
            continue;
        }
        if (out_matched_suffix != nullptr) {
            *out_matched_suffix = entry.stem_suffix;
        }
        return true;
    }
    return false;
}

bool resolveHotlineVoiceRouteFromStemCandidates(const String* stems, size_t stem_count, MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    if (stems == nullptr || stem_count == 0U) {
        return false;
    }

    if (!g_hotline_voice_catalog_scanned) {
        refreshHotlineVoiceSuffixCatalog();
    }
    if (g_hotline_voice_suffix_catalog.empty()) {
        appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffix);
        appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffixLegacyMp3);
    }

    auto try_resolve_from_catalog = [&](MediaRouteEntry& resolved_route) -> bool {
        AudioPlaybackProbeResult probe;
        for (size_t index = 0; index < stem_count; ++index) {
            if (stems[index].isEmpty()) {
                continue;
            }

            const size_t suffix_count = g_hotline_voice_suffix_catalog.size();
            if (suffix_count == 0U) {
                continue;
            }
            const size_t start = static_cast<size_t>(nextHotlineRandom32() % static_cast<uint32_t>(suffix_count));
            for (size_t pass = 0; pass < suffix_count; ++pass) {
                const size_t suffix_index = (start + pass) % suffix_count;
                const String voice_path = buildHotlineVoicePathFromStemWithSuffix(
                    stems[index], g_hotline_voice_suffix_catalog[suffix_index]);
                MediaRouteEntry route = buildHotlineSdFileRoute(voice_path, false, 0U);
                if (!mediaRouteHasPayload(route)) {
                    continue;
                }
                if (!mediaPathExistsForProbe(route.path, route.source)) {
                    continue;
                }
                if (g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe)) {
                    resolved_route = route;
                    return true;
                }

                const String fallback_wav = buildMp3FallbackWavPath(route.path);
                if (!fallback_wav.isEmpty() && mediaPathExistsForProbe(fallback_wav, route.source) &&
                    g_audio.probePlaybackFileFromSource(fallback_wav.c_str(), route.source, probe)) {
                    resolved_route = buildHotlineSdFileRoute(fallback_wav, false, 0U);
                    return true;
                }
            }
        }
        return false;
    };

    if (try_resolve_from_catalog(out_route)) {
        return true;
    }

    if (!g_hotline_voice_catalog_sd_scanned && g_audio.isSdReady()) {
        refreshHotlineVoiceSuffixCatalog();
        if (g_hotline_voice_suffix_catalog.empty()) {
            appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffix);
            appendHotlineVoiceSuffixCatalog(kHotlineDefaultVoiceSuffixLegacyMp3);
        }
        return try_resolve_from_catalog(out_route);
    }

    return false;
}

bool resolveHotlineWaitingPromptRoute(MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    String stems[16];
    size_t stem_count = 0U;
    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    if (resolveHotlineSceneDirectoryRoute(scene_key, HotlineValidationState::kWaiting, "none", out_route)) {
        return true;
    }
    const String scene_stem = hotlineSceneStemFromKey(scene_key);
    if (!scene_stem.isEmpty()) {
        appendHotlineStemVariants(scene_stem, "waiting_validation", stems, 16U, &stem_count);
        appendHotlineStemVariants(scene_stem, "validation_waiting", stems, 16U, &stem_count);
        appendHotlineStemVariants(scene_stem, "waiting", stems, 16U, &stem_count);
    }
    appendHotlineStemCandidate(kHotlineWaitingPromptStem, stems, 16U, &stem_count);
    appendHotlineStemCandidate("waiting_validation_2", stems, 16U, &stem_count);
    appendHotlineStemCandidate("waiting_validation", stems, 16U, &stem_count);

    return resolveHotlineVoiceRouteFromStemCandidates(stems, stem_count, out_route);
}

bool resolveHotlineHintRouteForDigits(const String& digits, MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    String clean_digits = digits;
    clean_digits.trim();
    if (clean_digits.length() != 1U || clean_digits[0] < '1' || clean_digits[0] > '3') {
        return false;
    }

    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    if (resolveHotlineSceneDirectoryRoute(scene_key, g_hotline_validation_state, clean_digits, out_route)) {
        return true;
    }

    String stems[20];
    size_t stem_count = 0U;
    const String scene_stem = hotlineSceneStemFromKey(scene_key);
    const char* state_tag = hotlineValidationStateToString(g_hotline_validation_state);
    const bool has_state_tag = (strcmp(state_tag, "none") != 0);

    if (!scene_stem.isEmpty()) {
        if (has_state_tag) {
            appendHotlineStemVariants(scene_stem, String("hint_") + clean_digits + "_" + state_tag, stems, 20U, &stem_count);
            appendHotlineStemVariants(scene_stem, String(state_tag) + "_hint_" + clean_digits, stems, 20U, &stem_count);
        }
        appendHotlineStemVariants(scene_stem, String("hint_") + clean_digits, stems, 20U, &stem_count);
        appendHotlineStemVariants(scene_stem, String("indice_") + clean_digits, stems, 20U, &stem_count);
    }

    if (has_state_tag) {
        appendHotlineStemCandidate(String("hotline_hint_") + clean_digits + "_" + state_tag, stems, 20U, &stem_count);
        appendHotlineStemCandidate(String("hotline_indice_") + clean_digits + "_" + state_tag, stems, 20U, &stem_count);
    }
    appendHotlineStemCandidate(String("hotline_hint_") + clean_digits, stems, 20U, &stem_count);
    appendHotlineStemCandidate(String("hotline_indice_") + clean_digits, stems, 20U, &stem_count);
    appendHotlineStemCandidate(String("hint_") + clean_digits, stems, 20U, &stem_count);
    appendHotlineStemCandidate(String("indice_") + clean_digits, stems, 20U, &stem_count);

    return resolveHotlineVoiceRouteFromStemCandidates(stems, stem_count, out_route);
}

bool resolveHotlineSceneRoute(const String& scene_id, MediaRouteEntry& out_route) {
    const String key = normalizeHotlineSceneKey(scene_id);
    if (resolveHotlineSceneDirectoryRoute(key, g_hotline_validation_state, "none", out_route)) {
        return true;
    }
    const String stem = hotlineSceneStemFromKey(key);
    if (stem.isEmpty()) {
        out_route = MediaRouteEntry{};
        return false;
    }

    out_route = buildHotlineSdVoiceRoute(stem, false, 0U);
    return mediaRouteHasPayload(out_route);
}

bool resolveHotlineDefaultVoiceRoute(MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    String stems[8];
    size_t stem_count = 0U;
    appendHotlineStemCandidate("fiches-hotline_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_ready_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_search_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_locked_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_broken_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_camera_scan_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_media_archive_2", stems, 8U, &stem_count);
    appendHotlineStemCandidate("scene_win_2", stems, 8U, &stem_count);
    return resolveHotlineVoiceRouteFromStemCandidates(stems, stem_count, out_route);
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

    // Refresh scene/state context on each hotline dial so route resolution
    // stays aligned with the Freenove state machine.
    requestSceneSyncFromFreenove("dial", true);

    MediaRouteEntry route;
    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    String lookup_key = buildHotlineLookupKey(scene_key, g_hotline_validation_state, digits);
    String matched_suffix;
    String resolution_method;
    bool routed_from_scene_hint =
        resolveHotlineExplicitRoute(scene_key, g_hotline_validation_state, digits, route, &lookup_key, &matched_suffix);
    if (routed_from_scene_hint) {
        resolution_method = String("explicit_table:") + matched_suffix;
    } else if (resolveHotlineHintRouteForDigits(digits, route)) {
        routed_from_scene_hint = true;
        resolution_method = "heuristic_stems";
    } else if (resolveHotlineSceneRoute(g_active_scene_id, route)) {
        routed_from_scene_hint = true;
        resolution_method = "scene_route_fallback";
    } else if (resolveHotlineDefaultVoiceRoute(route)) {
        routed_from_scene_hint = true;
        resolution_method = "default_voice_fallback";
    } else if (findDialMediaRoute(digits, route)) {
        resolution_method = "dial_map";
    } else {
        g_hotline.last_route_lookup_key = lookup_key;
        g_hotline.last_route_resolution = "missing_route";
        g_hotline.last_route_target = "";
        appendHotlineLogLine("DIAL_ROUTE_MISS",
                             String("digits=") + digits + " scene=" + scene_key + " state=" +
                                 hotlineValidationStateToString(g_hotline_validation_state));
        if (out_state != nullptr) {
            *out_state = "missing_route";
        }
        return false;
    }
    if (route.kind == MediaRouteKind::FILE && !routed_from_scene_hint) {
        // Keep legacy dial-map routes cyclic, but preserve one-shot behavior
        // for scene-linked hotline prompts (needed for busy tone + 440 ACK flow).
        route.playback.loop = true;
        route.playback.pause_ms = static_cast<uint16_t>(kHotlineDefaultLoopPauseMs);
    }

    String source = dialSourceText(from_pulse);
    if (routed_from_scene_hint) {
        source += "_SCENE_HINT";
    }
    noteHotlineRouteResolution(lookup_key, resolution_method, route);
    appendHotlineLogLine("DIAL_ROUTE",
                         String("digits=") + digits + " scene=" + scene_key + " state=" +
                             hotlineValidationStateToString(g_hotline_validation_state) + " method=" + resolution_method +
                             " target=" + describeMediaRouteTarget(route));
    // HOTLINE_TRIGGER must override stale WAITING_VALIDATION pending answer media.
    clearPendingEspNowCallRoute("dial_trigger");
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
        DynamicJsonDocument doc(1024);
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
    bool started = false;
    bool started_media_file = false;
    String started_path = route.path;
    if (g_audio.playFileFromSource(route.path.c_str(), route.source)) {
        started = true;
        started_media_file = isPlayableMediaPath(route.path);
    } else {
        const String fallback_wav = buildMp3FallbackWavPath(route.path);
        if (!fallback_wav.isEmpty() && fallback_wav != route.path) {
            Serial.printf("[RTC_BL_PHONE] media fallback %s -> %s\n", route.path.c_str(), fallback_wav.c_str());
            if (g_audio.playFileFromSource(fallback_wav.c_str(), route.source)) {
                started = true;
                started_media_file = isPlayableMediaPath(fallback_wav);
                started_path = fallback_wav;
            }
        }
    }

    if (!started) {
        g_busy_tone_after_media_pending = false;
        return false;
    }

    // For one-shot scene media routes, play a busy tone when playback completes.
    g_busy_tone_after_media_pending = (!route.playback.loop) && started_media_file;
    const String active_scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    g_win_etape_validation_after_media_pending =
        (active_scene_key == "WIN_ETAPE") &&
        (route.kind == MediaRouteKind::FILE) &&
        (!route.playback.loop) &&
        started_media_file;
    if (g_win_etape_validation_after_media_pending) {
        appendHotlineLogLine("WIN_ETAPE_440_ARMED", String("route=") + started_path);
        Serial.printf("[RTC_BL_PHONE] WIN_ETAPE media armed for auto 440 validation (%s)\n",
                      started_path.c_str());
    }
    g_prev_audio_playing = g_audio.isPlaying();
    return true;
}

String dialSourceText(bool from_pulse) {
    return from_pulse ? "PULSE" : "DTMF";
}

String compactHotlineNotifyPath(const String& raw_path) {
    String path = sanitizeMediaPath(raw_path);
    if (path.isEmpty()) {
        return "";
    }

    const int slash = path.lastIndexOf('/');
    if (slash >= 0 && static_cast<size_t>(slash + 1) < path.length()) {
        path = path.substring(static_cast<unsigned int>(slash + 1));
    }

    constexpr size_t kNotifyPathMaxLen = 64U;
    if (path.length() > kNotifyPathMaxLen) {
        path = path.substring(path.length() - kNotifyPathMaxLen);
    }
    return path;
}

bool sendHotlineNotify(const char* state, const String& digit_key, const String& digits, const String& source, const MediaRouteEntry& route) {
    constexpr size_t kHotlineNotifyPayloadBudget = 220U;
    constexpr size_t kEspNowPayloadHardLimit = 240U;

    DynamicJsonDocument doc(1024);
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
        out_route["path"] = compactHotlineNotifyPath(route.path);
        out_route["source"] = mediaSourceToString(route.source);
        if (route.playback.loop || route.playback.pause_ms > 0U) {
            JsonObject playback = out_route["playback"].to<JsonObject>();
            if (route.playback.loop) {
                playback["loop"] = true;
            }
            if (route.playback.pause_ms > 0U) {
                playback["pause_ms"] = route.playback.pause_ms;
            }
        }
    }

    String wire;
    serializeJson(doc, wire);

    if (wire.length() > kHotlineNotifyPayloadBudget) {
        DynamicJsonDocument compact_doc(1024);
        compact_doc["proto"] = "rtcbl/1";
        compact_doc["event"] = "hotline_script";
        JsonObject compact_payload = compact_doc["payload"].to<JsonObject>();
        compact_payload["s"] = state == nullptr ? "" : state;
        compact_payload["k"] = digit_key;
        compact_payload["d"] = digits;
        compact_payload["src"] = source;

        if (route.kind == MediaRouteKind::TONE) {
            compact_payload["rk"] = "tone";
            compact_payload["tp"] = toneProfileToString(route.tone.profile);
            compact_payload["te"] = toneEventToString(route.tone.event);
        } else {
            compact_payload["rk"] = "file";
            compact_payload["rp"] = compactHotlineNotifyPath(route.path);
            compact_payload["rs"] = mediaSourceToString(route.source);
            if (route.playback.loop) {
                compact_payload["l"] = true;
            }
            if (route.playback.pause_ms > 0U) {
                compact_payload["p"] = route.playback.pause_ms;
            }
        }

        wire = "";
        serializeJson(compact_doc, wire);
        if (wire.length() > kEspNowPayloadHardLimit) {
            DynamicJsonDocument minimal_doc(1024);
            minimal_doc["e"] = "hotline_script";
            minimal_doc["s"] = state == nullptr ? "" : state;
            minimal_doc["k"] = digit_key;
            minimal_doc["d"] = digits;
            wire = "";
            serializeJson(minimal_doc, wire);
        }
    }

    const bool ok = g_espnow.sendJson("broadcast", wire);
    g_hotline.last_notify_event = state == nullptr ? "" : state;
    g_hotline.last_notify_ok = ok;
    if (!ok) {
        Serial.printf("[Hotline] notify failed state=%s\n", state == nullptr ? "" : state);
    }
    return ok;
}

bool warningSirenAudioBusy() {
    return g_hotline.active || g_hotline.queued || g_hotline.pending_restart || g_hotline.ringback_active ||
           g_pending_espnow_call || g_slic.isHookOff() ||
           g_telephony.state() == TelephonyState::OFF_HOOK ||
           g_telephony.state() == TelephonyState::PLAYING_MESSAGE;
}

uint32_t warningSirenTogglePeriodMs(uint8_t strength) {
    uint32_t period_ms = 920U;
    if (strength > 0U) {
        period_ms = 920U - static_cast<uint32_t>(strength) * 2U;
    }
    if (period_ms < 260U) {
        period_ms = 260U;
    } else if (period_ms > 980U) {
        period_ms = 980U;
    }
    return period_ms;
}

bool startWarningSirenTone(uint32_t now_ms, bool retune) {
    if (warningSirenAudioBusy()) {
        g_warning_siren.tone_owned = false;
        g_warning_siren.last_error = "busy";
        return false;
    }

    if (!retune && g_warning_siren.tone_owned && g_audio.isToneRenderingActive()) {
        return true;
    }

    const uint8_t phase = g_warning_siren.phase;
    ToneEvent event = ToneEvent::RINGBACK;
    if ((phase & 1U) == 0U) {
        event = (g_warning_siren.strength >= 220U) ? ToneEvent::CONGESTION : ToneEvent::BUSY;
    }
    const bool ok = g_audio.playTone(g_warning_siren.profile, event);
    if (!ok) {
        g_warning_siren.last_error = "tone_start_failed";
        g_warning_siren.tone_owned = false;
        return false;
    }

    g_warning_siren.event = event;
    g_warning_siren.toggle_period_ms = warningSirenTogglePeriodMs(g_warning_siren.strength);
    g_warning_siren.next_toggle_ms = now_ms + g_warning_siren.toggle_period_ms;
    g_warning_siren.tone_owned = true;
    g_warning_siren.last_error = "";
    return true;
}

void clearHotlineRuntimeState() {
    const String last_event = g_hotline.last_notify_event;
    const bool last_ok = g_hotline.last_notify_ok;
    const String last_lookup_key = g_hotline.last_route_lookup_key;
    const String last_resolution = g_hotline.last_route_resolution;
    const String last_target = g_hotline.last_route_target;
    g_hotline = HotlineRuntimeState{};
    g_hotline.last_notify_event = last_event;
    g_hotline.last_notify_ok = last_ok;
    g_hotline.last_route_lookup_key = last_lookup_key;
    g_hotline.last_route_resolution = last_resolution;
    g_hotline.last_route_target = last_target;
    g_win_etape_validation_after_media_pending = false;
}

void clearPendingEspNowCallRoute(const char* reason) {
    if (g_pending_espnow_call || mediaRouteHasPayload(g_pending_espnow_call_media)) {
        Serial.printf("[Hotline] pending espnow route cleared reason=%s target=%s\n",
                      reason == nullptr ? "" : reason,
                      describeMediaRouteTarget(g_pending_espnow_call_media).c_str());
    }
    g_pending_espnow_call_media = MediaRouteEntry{};
    g_pending_espnow_call = false;
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
    if (!mediaRouteHasPayload(route)) {
        return false;
    }

    const ToneProfile ringback_profile = pickRandomToneProfile();
    MediaRouteEntry ringback_route;
    ringback_route.kind = MediaRouteKind::TONE;
    ringback_route.tone.profile = ringback_profile;
    ringback_route.tone.event = ToneEvent::RINGBACK;
    if (!playMediaRoute(ringback_route)) {
        appendHotlineLogLine("RINGBACK_FAIL", String("digits=") + digits);
        return false;
    }

    const uint32_t ringback_duration_ms = pickRandomRingbackDurationMs();
    g_hotline.active = true;
    g_hotline.current_key = digit_key;
    g_hotline.current_digits = digits;
    g_hotline.current_source = source;
    g_hotline.current_route = ringback_route;
    g_hotline.pending_restart = false;
    g_hotline.next_restart_ms = 0U;
    g_hotline.ringback_active = true;
    g_hotline.ringback_until_ms = millis() + ringback_duration_ms;
    g_hotline.ringback_profile = ringback_profile;
    g_hotline.post_ringback_route = route;
    g_hotline.post_ringback_valid = true;
    sendHotlineNotify("ringback", digit_key, digits, source, ringback_route);
    Serial.printf("[Hotline] ringback profile=%s duration_ms=%lu before route=%s\n",
                  toneProfileToString(ringback_profile),
                  static_cast<unsigned long>(ringback_duration_ms),
                  describeMediaRouteTarget(route).c_str());
    appendHotlineLogLine("RINGBACK",
                         String("digits=") + digits + " profile=" + toneProfileToString(ringback_profile) +
                             " duration_ms=" + String(ringback_duration_ms) +
                             " target=" + describeMediaRouteTarget(route));
    return true;
}

void stopHotlineForHangup() {
    if (!g_hotline.active && !g_hotline.queued && !g_hotline.pending_restart) {
        return;
    }
    g_audio.stopPlayback();
    g_audio.stopTone();
    
    // CRITICAL FIX: Verify audio actually stopped (prevent race condition)
    uint32_t audio_stop_timeout = millis() + 100U;
    while ((g_audio.isPlaying() || g_audio.isToneRenderingActive()) && 
           static_cast<int32_t>(millis() - audio_stop_timeout) < 0) {
        delayMicroseconds(1000);  // Spin briefly for audio engine to catch up
    }
    if (g_audio.isPlaying() || g_audio.isToneRenderingActive()) {
        Serial.println("[RTC_BL_PHONE] WARNING: audio still active after hangup, forcing stop");
        // Force immediate stop if audio engine didn't respond
        g_audio.stopPlayback();
        g_audio.stopTone();
    }
    
    sendHotlineNotify(
        "stopped_hangup",
        g_hotline.current_key,
        g_hotline.current_digits,
        g_hotline.current_source,
        g_hotline.current_route);
    appendHotlineLogLine("STOP_HANGUP", String("digits=") + g_hotline.current_digits);
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

    if (g_hotline.ringback_active) {
        const uint32_t now = millis();
        if (static_cast<int32_t>(now - g_hotline.ringback_until_ms) < 0) {
            if (!g_audio.isToneRenderingActive()) {
                const ToneProfile profile = (g_hotline.ringback_profile == ToneProfile::NONE)
                                                ? ToneProfile::FR_FR
                                                : g_hotline.ringback_profile;
                g_audio.playTone(profile, ToneEvent::RINGBACK);
            }
            return;
        }

        g_audio.stopTone();
        g_hotline.ringback_active = false;
        g_hotline.ringback_until_ms = 0U;

        if (!g_hotline.post_ringback_valid || !mediaRouteHasPayload(g_hotline.post_ringback_route)) {
            appendHotlineLogLine("POST_RINGBACK_MISS", String("digits=") + g_hotline.current_digits);
            clearHotlineRuntimeState();
            return;
        }

        g_hotline.current_route = g_hotline.post_ringback_route;
        g_hotline.post_ringback_route = MediaRouteEntry{};
        g_hotline.post_ringback_valid = false;
        if (!playMediaRoute(g_hotline.current_route)) {
            Serial.printf("[Hotline] post-ringback start failed key=%s digits=%s\n",
                          g_hotline.current_key.c_str(),
                          g_hotline.current_digits.c_str());
            appendHotlineLogLine("POST_RINGBACK_FAIL", String("digits=") + g_hotline.current_digits);
            clearHotlineRuntimeState();
            return;
        }
        appendHotlineLogLine("POST_RINGBACK_PLAY",
                             String("digits=") + g_hotline.current_digits + " target=" +
                                 describeMediaRouteTarget(g_hotline.current_route));
        sendHotlineNotify("triggered",
                          g_hotline.current_key,
                          g_hotline.current_digits,
                          g_hotline.current_source,
                          g_hotline.current_route);
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

void scheduleNextHotlineInterlude(uint32_t now_ms) {
    g_hotline_interlude.next_due_ms = now_ms + pickRandomInterludeDelayMs();
}

bool pickRandomInterludeRoute(MediaRouteEntry& out_route, String* out_path, String* out_error) {
    out_route = MediaRouteEntry{};
    if (out_path != nullptr) {
        *out_path = "";
    }
    if (out_error != nullptr) {
        *out_error = "";
    }

    fs::FS* sd_fs = nullptr;
    if (!ensureHotlineSdMounted(sd_fs) || sd_fs == nullptr) {
        if (out_error != nullptr) {
            *out_error = "sd_unavailable";
        }
        return false;
    }

    File dir = sd_fs->open(kInterludeTtsAssetsRoot);
    if (!dir || !dir.isDirectory()) {
        if (out_error != nullptr) {
            *out_error = "interlude_dir_missing";
        }
        if (dir) {
            dir.close();
        }
        return false;
    }

    AudioPlaybackProbeResult probe;
    size_t candidate_count = 0U;
    String selected_path;
    MediaRouteEntry selected_route;

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        const String path = entry.path();
        entry.close();
        if (path.isEmpty()) {
            continue;
        }
        String lower = path;
        lower.toLowerCase();
        if (!lower.endsWith(".mp3") && !lower.endsWith(".wav")) {
            continue;
        }

        const MediaRouteEntry route = buildHotlineSdFileRoute(path, false, 0U);
        if (!mediaRouteHasPayload(route)) {
            continue;
        }
        MediaRouteEntry resolved_route = route;
        String resolved_path = route.path;

        bool playable = false;
        if (mediaPathExistsForProbe(route.path, route.source)) {
            playable = g_audio.probePlaybackFileFromSource(route.path.c_str(), route.source, probe);
        }
        if (!playable) {
            const String fallback_wav = buildMp3FallbackWavPath(route.path);
            if (!fallback_wav.isEmpty() &&
                mediaPathExistsForProbe(fallback_wav, route.source) &&
                g_audio.probePlaybackFileFromSource(fallback_wav.c_str(), route.source, probe)) {
                resolved_route = buildHotlineSdFileRoute(fallback_wav, false, 0U);
                resolved_path = fallback_wav;
                playable = true;
            }
        }
        if (!playable) {
            continue;
        }

        ++candidate_count;
        if ((nextHotlineRandom32() % static_cast<uint32_t>(candidate_count)) == 0U) {
            selected_route = resolved_route;
            selected_path = resolved_path;
        }
    }
    dir.close();

    if (candidate_count == 0U || !mediaRouteHasPayload(selected_route)) {
        if (out_error != nullptr) {
            *out_error = "interlude_no_playable_file";
        }
        return false;
    }

    out_route = selected_route;
    if (out_path != nullptr) {
        *out_path = selected_path;
    }
    return true;
}

void clearOffHookAutoRandomPlayback() {
    g_offhook_autoplay.armed = false;
    g_offhook_autoplay.play_after_ms = 0U;
    g_offhook_autoplay.route = MediaRouteEntry{};
    g_offhook_autoplay.selected_path = "";
}

void armOffHookAutoRandomPlayback(uint32_t now_ms) {
    clearOffHookAutoRandomPlayback();
    g_offhook_autoplay.last_error = "";

    if (g_pending_espnow_call || g_hotline.active || g_hotline.queued || g_hotline.pending_restart ||
        g_hotline.ringback_active) {
        g_offhook_autoplay.last_error = "busy";
        return;
    }
    if (g_audio.isPlaying()) {
        g_offhook_autoplay.last_error = "audio_playing";
        return;
    }
    if (g_audio.isToneRenderingActive() && !g_audio.isDialToneActive()) {
        g_offhook_autoplay.last_error = "tone_busy";
        return;
    }
    if (g_telephony.dialingStarted() || !g_telephony.dialBuffer().isEmpty()) {
        g_offhook_autoplay.last_error = "dialing";
        return;
    }

    MediaRouteEntry route;
    String selected_path;
    String resolve_error;
    if (!pickRandomInterludeRoute(route, &selected_path, &resolve_error) || !mediaRouteHasPayload(route)) {
        g_offhook_autoplay.last_error = resolve_error.isEmpty() ? "no_random_file" : resolve_error;
        Serial.printf("[RTC_BL_PHONE] off_hook auto random skipped reason=%s\n",
                      g_offhook_autoplay.last_error.c_str());
        return;
    }

    g_offhook_autoplay.armed = true;
    g_offhook_autoplay.play_after_ms = now_ms + kOffHookAutoRandomDelayMs;
    g_offhook_autoplay.route = route;
    g_offhook_autoplay.selected_path = selected_path;
    Serial.printf("[RTC_BL_PHONE] off_hook auto random armed delay_ms=%lu file=%s\n",
                  static_cast<unsigned long>(kOffHookAutoRandomDelayMs),
                  selected_path.c_str());
}

void tickOffHookAutoRandomPlayback(uint32_t now_ms) {
    if (!g_offhook_autoplay.armed) {
        return;
    }

    if (g_telephony.state() != TelephonyState::OFF_HOOK || !g_slic.isHookOff()) {
        clearOffHookAutoRandomPlayback();
        return;
    }

    if (g_telephony.dialingStarted() || !g_telephony.dialBuffer().isEmpty()) {
        g_offhook_autoplay.last_error = "dialing";
        clearOffHookAutoRandomPlayback();
        return;
    }

    if (static_cast<int32_t>(now_ms - g_offhook_autoplay.play_after_ms) < 0) {
        return;
    }

    if (g_audio.isDialToneActive()) {
        g_audio.stopDialTone();
    }
    if (g_audio.isToneRenderingActive()) {
        g_audio.stopTone();
    }

    const MediaRouteEntry route = g_offhook_autoplay.route;
    const String selected_path = g_offhook_autoplay.selected_path;
    clearOffHookAutoRandomPlayback();

    const bool ok = playMediaRoute(route);
    if (ok) {
        // This auto-play should not arm hotline busy/validation chains.
        g_busy_tone_after_media_pending = false;
        g_win_etape_validation_after_media_pending = false;
    }
    Serial.printf("[RTC_BL_PHONE] off_hook auto random play file=%s ok=%u\n",
                  selected_path.c_str(),
                  ok ? 1U : 0U);
}

bool triggerHotlineInterludeNow(const char* reason) {
    const uint32_t now_ms = millis();
    if (!g_hotline_interlude.enabled) {
        return false;
    }

    if (g_hotline.active || g_hotline.queued || g_hotline.pending_restart || g_pending_espnow_call ||
        g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE ||
        g_slic.isHookOff()) {
        g_hotline_interlude.last_error = "busy";
        appendHotlineLogLine("INTERLUDE_SKIP_BUSY", String("reason=") + (reason == nullptr ? "" : reason));
        g_hotline_interlude.next_due_ms = now_ms + kInterludeRetryDelayMs;
        return false;
    }

    MediaRouteEntry route;
    String selected_path;
    String resolve_error;
    if (!pickRandomInterludeRoute(route, &selected_path, &resolve_error)) {
        g_hotline_interlude.last_error = resolve_error;
        appendHotlineLogLine("INTERLUDE_RESOLVE_FAIL",
                             String("reason=") + (reason == nullptr ? "" : reason) + " err=" + resolve_error);
        g_hotline_interlude.next_due_ms = now_ms + kInterludeRetryDelayMs;
        return false;
    }

    g_pending_espnow_call_media = route;
    g_pending_espnow_call = true;
    g_telephony.triggerIncomingRing();

    g_hotline_interlude.last_file = selected_path;
    g_hotline_interlude.last_trigger_ms = now_ms;
    g_hotline_interlude.last_error = "";
    appendHotlineLogLine("INTERLUDE_RING",
                         String("reason=") + (reason == nullptr ? "" : reason) + " file=" + selected_path);
    scheduleNextHotlineInterlude(now_ms);
    Serial.printf("[RTC_BL_PHONE] interlude ring reason=%s file=%s next_due_ms=%lu\n",
                  reason == nullptr ? "" : reason,
                  selected_path.c_str(),
                  static_cast<unsigned long>(g_hotline_interlude.next_due_ms));
    return true;
}

void tickHotlineInterludeRuntime() {
    if (!g_hotline_interlude.enabled) {
        return;
    }
    const uint32_t now_ms = millis();
    if (g_hotline_interlude.next_due_ms == 0U) {
        scheduleNextHotlineInterlude(now_ms);
        return;
    }
    if (static_cast<int32_t>(now_ms - g_hotline_interlude.next_due_ms) < 0) {
        return;
    }
    triggerHotlineInterludeNow("timer");
}

void tickWarningSirenRuntime() {
    if (!g_warning_siren.enabled) {
        return;
    }

    const uint32_t now_ms = millis();
    if (static_cast<int32_t>(now_ms - g_warning_siren.last_control_ms) > static_cast<int32_t>(kWarningSirenBeatTimeoutMs)) {
        g_warning_siren.enabled = false;
        g_warning_siren.tone_owned = false;
        g_warning_siren.last_error = "control_timeout";
        Serial.println("[RTC_BL_PHONE] warning siren auto-stop timeout");
        return;
    }

    if (warningSirenAudioBusy()) {
        if (g_warning_siren.tone_owned &&
            !g_hotline.active &&
            !g_hotline.ringback_active &&
            !g_pending_espnow_call) {
            g_audio.stopTone();
        }
        g_warning_siren.tone_owned = false;
        return;
    }

    if (!g_warning_siren.tone_owned) {
        startWarningSirenTone(now_ms, true);
        return;
    }
    if (static_cast<int32_t>(now_ms - g_warning_siren.next_toggle_ms) < 0) {
        return;
    }
    ++g_warning_siren.phase;
    startWarningSirenTone(now_ms, true);
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
    DynamicJsonDocument payload(1024);
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

bool sendHotlineValidationAckEvent(const char* ack_event_name, bool ack_requested, const char* source_tag) {
    if (ack_event_name == nullptr || ack_event_name[0] == '\0') {
        return false;
    }
    if (!g_espnow.isReady()) {
        Serial.printf("[RTC_BL_PHONE] validation ack skipped (espnow not ready) event=%s\n", ack_event_name);
        return false;
    }

    DynamicJsonDocument frame(1024);
    frame["msg_id"] = String("hv-") + String(millis());
    frame["seq"] = static_cast<uint32_t>(millis());
    frame["type"] = "command";
    frame["ack"] = ack_requested;
    JsonObject payload = frame["payload"].to<JsonObject>();
    payload["cmd"] = "SC_EVENT";
    JsonObject event_args = payload["args"].to<JsonObject>();
    event_args["event_type"] = "espnow";
    event_args["event_name"] = ack_event_name;
    if (source_tag != nullptr && source_tag[0] != '\0') {
        event_args["source"] = source_tag;
    }

    String wire;
    serializeJson(frame, wire);
    const bool sent = g_espnow.sendJson("broadcast", wire);
    g_hotline.last_notify_event = String("validate_") + ack_event_name;
    g_hotline.last_notify_ok = sent;
    if (!sent) {
        Serial.printf("[RTC_BL_PHONE] validation ack send_failed event=%s\n", ack_event_name);
    }
    return sent;
}

HotlineValidationState hotlineValidationStateFromAckEvent(const char* ack_event_name) {
    if (ack_event_name != nullptr && strcmp(ack_event_name, "ACK_WARNING") == 0) {
        return HotlineValidationState::kRefused;
    }
    if (ack_event_name != nullptr &&
        (strcmp(ack_event_name, "ACK_WIN1") == 0 || strcmp(ack_event_name, "ACK_WIN2") == 0)) {
        return HotlineValidationState::kGranted;
    }
    return HotlineValidationState::kNone;
}

bool resolveHotlineValidationCueRoute(const char* ack_event_name, MediaRouteEntry& out_route) {
    out_route = MediaRouteEntry{};
    if (ack_event_name == nullptr || ack_event_name[0] == '\0') {
        return false;
    }

    const HotlineValidationState state = hotlineValidationStateFromAckEvent(ack_event_name);
    if (state == HotlineValidationState::kNone) {
        return false;
    }

    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    if (resolveHotlineSceneDirectoryRoute(scene_key, state, "none", out_route)) {
        return true;
    }

    String stems[20];
    size_t stem_count = 0U;
    const String scene_stem = hotlineSceneStemFromKey(scene_key);
    if (!scene_stem.isEmpty()) {
        if (state == HotlineValidationState::kRefused) {
            appendHotlineStemVariants(scene_stem, "validation_refused", stems, 20U, &stem_count);
            appendHotlineStemVariants(scene_stem, "validation_warning", stems, 20U, &stem_count);
            appendHotlineStemVariants(scene_stem, "warning", stems, 20U, &stem_count);
        } else {
            appendHotlineStemVariants(scene_stem, "validation_granted", stems, 20U, &stem_count);
            appendHotlineStemVariants(scene_stem, "validation_ok", stems, 20U, &stem_count);
            appendHotlineStemVariants(scene_stem, "win", stems, 20U, &stem_count);
        }
    }

    if (state == HotlineValidationState::kRefused) {
        appendHotlineStemCandidate("validation_refused_2", stems, 20U, &stem_count);
        appendHotlineStemCandidate("validation_warning_2", stems, 20U, &stem_count);
        appendHotlineStemCandidate("scene_broken_2", stems, 20U, &stem_count);
    } else {
        appendHotlineStemCandidate("validation_granted_2", stems, 20U, &stem_count);
        appendHotlineStemCandidate("validation_ok_2", stems, 20U, &stem_count);
        appendHotlineStemCandidate("scene_win_2", stems, 20U, &stem_count);
    }

    return resolveHotlineVoiceRouteFromStemCandidates(stems, stem_count, out_route);
}

void playHotlineValidationCue(const char* ack_event_name) {
    const HotlineValidationState state = hotlineValidationStateFromAckEvent(ack_event_name);
    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    String lookup_key = buildHotlineLookupKey(scene_key, state, "none");
    String matched_suffix;
    MediaRouteEntry route;
    bool from_explicit = resolveHotlineExplicitRoute(scene_key, state, "none", route, &lookup_key, &matched_suffix);
    if (!from_explicit && !resolveHotlineValidationCueRoute(ack_event_name, route)) {
        g_hotline.last_route_lookup_key = lookup_key;
        g_hotline.last_route_resolution = "validation_cue_missing";
        g_hotline.last_route_target = "";
        return;
    }
    noteHotlineRouteResolution(lookup_key,
                               from_explicit ? String("explicit_table:") + matched_suffix : String("validation_cue_heuristic"),
                               route);

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

bool parseSceneIdFromArgs(const String& args,
                          String& scene_id,
                          String* out_step_id,
                          HotlineValidationState* out_validation_state,
                          bool* out_has_validation_state);

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
    const HotlineValidationState validation_state = hotlineValidationStateFromAckEvent(ack_event_name);

    if (!g_espnow.isReady()) {
        return makeResponse(false, "HOTLINE_VALIDATE espnow_not_ready");
    }

    const bool sent = sendHotlineValidationAckEvent(ack_event_name, ack_requested, "manual");
    if (!sent) {
        return makeResponse(false, "HOTLINE_VALIDATE send_failed");
    }
    g_hotline_validation_state = validation_state;
    playHotlineValidationCue(ack_event_name);
    return makeResponse(true, String("HOTLINE_VALIDATE ") + ack_event_name);
}

DispatchResponse dispatchWaitingValidationCommand(const String& args) {
    String scene_id;
    String step_id;
    HotlineValidationState parsed_validation_state = HotlineValidationState::kNone;
    bool has_validation_state = false;
    if (!args.isEmpty() &&
        parseSceneIdFromArgs(args, scene_id, &step_id, &parsed_validation_state, &has_validation_state)) {
        g_active_scene_id = scene_id;
        g_active_step_id = step_id;
    }

    if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
        return makeResponse(false, "WAITING_VALIDATION busy");
    }
    if (has_validation_state) {
        g_hotline_validation_state = parsed_validation_state;
    } else {
        g_hotline_validation_state = HotlineValidationState::kWaiting;
    }
    const String scene_key = normalizeHotlineSceneKey(g_active_scene_id);
    String lookup_key = buildHotlineLookupKey(scene_key, HotlineValidationState::kWaiting, "none");
    String matched_suffix;
    bool from_explicit = resolveHotlineExplicitRoute(scene_key,
                                                     HotlineValidationState::kWaiting,
                                                     "none",
                                                     g_pending_espnow_call_media,
                                                     &lookup_key,
                                                     &matched_suffix);
    if (!from_explicit && !resolveHotlineWaitingPromptRoute(g_pending_espnow_call_media)) {
        g_pending_espnow_call_media = buildHotlineSdVoiceRoute(kHotlineWaitingPromptStem, false, 0U);
        noteHotlineRouteResolution(lookup_key, "fallback_waiting_prompt", g_pending_espnow_call_media);
    } else {
        noteHotlineRouteResolution(lookup_key,
                                   from_explicit ? String("explicit_table:") + matched_suffix : String("waiting_prompt_heuristic"),
                                   g_pending_espnow_call_media);
    }
    g_pending_espnow_call = mediaRouteHasPayload(g_pending_espnow_call_media);
    g_telephony.triggerIncomingRing();
    g_hotline_validation_state = HotlineValidationState::kWaiting;
    g_hotline.last_notify_event = "waiting_validation";
    g_hotline.last_notify_ok = true;
    return makeResponse(true, "WAITING_VALIDATION");
}

DispatchResponse dispatchWarningSirenCommand(const String& args) {
    String action;
    String trailing;
    if (!args.isEmpty()) {
        if (!splitFirstToken(args, action, trailing)) {
            action = args;
            trailing = "";
        }
    } else {
        action = "STATUS";
    }
    action.trim();
    action.toUpperCase();

    if (action == "STATUS") {
        DynamicJsonDocument doc(1024);
        JsonObject root = doc.to<JsonObject>();
        root["enabled"] = g_warning_siren.enabled;
        root["tone_owned"] = g_warning_siren.tone_owned;
        root["profile"] = toneProfileToString(g_warning_siren.profile);
        root["event"] = toneEventToString(g_warning_siren.event);
        root["strength"] = g_warning_siren.strength;
        root["phase"] = g_warning_siren.phase;
        root["started_ms"] = g_warning_siren.started_ms;
        root["last_control_ms"] = g_warning_siren.last_control_ms;
        root["next_toggle_ms"] = g_warning_siren.next_toggle_ms;
        root["toggle_period_ms"] = g_warning_siren.toggle_period_ms;
        root["last_error"] = g_warning_siren.last_error;
        return jsonResponse(doc);
    }

    uint8_t strength = g_warning_siren.strength;
    if (!trailing.isEmpty()) {
        String strength_token;
        String leftover;
        if (!splitFirstToken(trailing, strength_token, leftover) || !leftover.isEmpty()) {
            return makeResponse(false, "WARNING_SIREN invalid_args");
        }
        const int parsed = strength_token.toInt();
        if (parsed < 0 || parsed > 255) {
            return makeResponse(false, "WARNING_SIREN invalid_strength");
        }
        strength = static_cast<uint8_t>(parsed);
    }

    const uint32_t now_ms = millis();
    if (action == "START" || action == "BEAT") {
        if (action == "START") {
            g_warning_siren.profile = pickRandomToneProfile();
            g_warning_siren.phase = 0U;
            g_warning_siren.started_ms = now_ms;
        } else {
            ++g_warning_siren.phase;
        }
        g_warning_siren.enabled = true;
        g_warning_siren.strength = strength;
        g_warning_siren.last_control_ms = now_ms;
        g_warning_siren.toggle_period_ms = warningSirenTogglePeriodMs(strength);
        g_warning_siren.next_toggle_ms = now_ms;
        g_warning_siren.last_error = "";
        startWarningSirenTone(now_ms, true);
        if (action == "START") {
            appendHotlineLogLine("WARN_SIREN_START",
                                 String("profile=") + toneProfileToString(g_warning_siren.profile) +
                                     " strength=" + String(strength));
        }
        return makeResponse(true, String("WARNING_SIREN ") + action);
    }

    if (action == "STOP") {
        const bool busy = warningSirenAudioBusy();
        if (g_warning_siren.tone_owned && !busy) {
            g_audio.stopTone();
        }
        g_warning_siren.enabled = false;
        g_warning_siren.tone_owned = false;
        g_warning_siren.last_control_ms = now_ms;
        g_warning_siren.next_toggle_ms = 0U;
        g_warning_siren.last_error = "";
        appendHotlineLogLine("WARN_SIREN_STOP", "");
        return makeResponse(true, "WARNING_SIREN STOP");
    }

    return makeResponse(false, "WARNING_SIREN invalid_action");
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
        out = dispatchWaitingValidationCommand(args);
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

bool parseSceneIdFromArgs(const String& args,
                          String& scene_id,
                          String* out_step_id = nullptr,
                          HotlineValidationState* out_validation_state = nullptr,
                          bool* out_has_validation_state = nullptr) {
    scene_id = "";
    if (out_step_id != nullptr) {
        *out_step_id = "";
    }
    if (out_has_validation_state != nullptr) {
        *out_has_validation_state = false;
    }
    if (out_validation_state != nullptr) {
        *out_validation_state = HotlineValidationState::kNone;
    }

    String normalized = args;
    normalized.trim();
    if (normalized.isEmpty()) {
        return false;
    }

    if (normalized[0] == '{') {
        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, normalized) == DeserializationError::Ok && doc.is<JsonObject>()) {
            scene_id = doc["id"] | doc["scene_id"] | doc["scene"] | "";
            scene_id.trim();
            if (out_step_id != nullptr) {
                *out_step_id = doc["step"] | doc["step_id"] | "";
                out_step_id->trim();
            }
            if (out_has_validation_state != nullptr && out_validation_state != nullptr) {
                const String validation_token = doc["validation_state"] | doc["validation"] | "";
                HotlineValidationState parsed_validation = HotlineValidationState::kNone;
                if (parseHotlineValidationStateToken(validation_token, &parsed_validation)) {
                    *out_validation_state = parsed_validation;
                    *out_has_validation_state = true;
                }
            }
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
    cfg.hybrid_telco_clock_policy = isHybridTelcoClockPolicy(audio_cfg.clock_policy);
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
    root["active_step"] = g_active_step_id;
    root["hotline_validation_state"] = hotlineValidationStateToString(g_hotline_validation_state);

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
    telephony["hotline_ringback_active"] = g_hotline.ringback_active;
    telephony["hotline_ringback_until_ms"] = g_hotline.ringback_until_ms;
    telephony["hotline_ringback_profile"] = toneProfileToString(g_hotline.ringback_profile);
    telephony["hotline_validation_state"] = hotlineValidationStateToString(g_hotline_validation_state);
    telephony["interlude_enabled"] = g_hotline_interlude.enabled;
    telephony["interlude_next_due_ms"] = g_hotline_interlude.next_due_ms;
    telephony["interlude_last_file"] = g_hotline_interlude.last_file;
    telephony["interlude_last_trigger_ms"] = g_hotline_interlude.last_trigger_ms;
    telephony["warning_siren_enabled"] = g_warning_siren.enabled;
    telephony["warning_siren_profile"] = toneProfileToString(g_warning_siren.profile);
    telephony["warning_siren_event"] = toneEventToString(g_warning_siren.event);
    telephony["warning_siren_strength"] = g_warning_siren.strength;
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
    espnow["scene_sync_enabled"] = g_espnow_scene_sync.enabled;
    espnow["scene_sync_interval_ms"] = g_espnow_scene_sync.interval_ms;
    espnow["scene_sync_pending"] = g_espnow_scene_sync.request_pending;
    espnow["scene_sync_msg_id"] = g_espnow_scene_sync.request_msg_id;
    espnow["scene_sync_seq"] = g_espnow_scene_sync.request_seq;
    espnow["scene_sync_requests_sent"] = g_espnow_scene_sync.requests_sent;
    espnow["scene_sync_request_send_fail"] = g_espnow_scene_sync.request_send_fail;
    espnow["scene_sync_ack_ok"] = g_espnow_scene_sync.request_ack_ok;
    espnow["scene_sync_ack_fail"] = g_espnow_scene_sync.request_ack_fail;
    espnow["scene_sync_last_error"] = g_espnow_scene_sync.last_error;
    espnow["scene_sync_last_source"] = g_espnow_scene_sync.last_source;
    espnow["scene_sync_last_update_ms"] = g_espnow_scene_sync.last_update_ms;

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
        next.volume = kA252CodecMaxVolumePercent;  // 60% to prevent saturation
        next.adc_dsp_enabled = false;
        next.adc_fft_enabled = false;
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

    DynamicJsonDocument doc(1024);
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

    DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        g_active_step_id = "";
        g_hotline_validation_state = HotlineValidationState::kNone;
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
        String step_id;
        HotlineValidationState parsed_validation_state = HotlineValidationState::kNone;
        bool has_validation_state = false;
        if (!parseSceneIdFromArgs(args, scene_id, &step_id, &parsed_validation_state, &has_validation_state)) {
            return makeResponse(false, "missing_scene_id");
        }
        g_active_scene_id = scene_id;
        g_active_step_id = step_id;
        if (has_validation_state) {
            g_hotline_validation_state = parsed_validation_state;
        } else {
            const HotlineValidationState step_state = inferHotlineValidationStateFromStepId(step_id);
            if (step_state != HotlineValidationState::kNone) {
                g_hotline_validation_state = step_state;
            } else {
                g_hotline_validation_state = inferHotlineValidationStateFromSceneKey(normalizeHotlineSceneKey(scene_id));
            }
        }

        MediaRouteEntry scene_route;
        const bool scene_audio_mapped = resolveHotlineSceneRoute(scene_id, scene_route);
        bool scene_audio_started = false;
        String scene_audio_state = "none";
        if (scene_audio_mapped) {
            noteHotlineRouteResolution(buildHotlineLookupKey(normalizeHotlineSceneKey(scene_id), g_hotline_validation_state, "none"),
                                       "scene_route",
                                       scene_route);
            if (g_telephony.state() == TelephonyState::OFF_HOOK || g_telephony.state() == TelephonyState::PLAYING_MESSAGE) {
                scene_audio_state = "telephony_busy";
            } else {
                scene_audio_started = playMediaRoute(scene_route);
                scene_audio_state = scene_audio_started ? "started" : "play_failed";
            }
        }

        DynamicJsonDocument out(1024);
        JsonObject root = out.to<JsonObject>();
        root["ok"] = true;
        root["code"] = "SCENE";
        root["scene"] = scene_id;
        root["step"] = g_active_step_id;
        root["validation_state"] = hotlineValidationStateToString(g_hotline_validation_state);
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
        DynamicJsonDocument out(1024);
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
            if (value != static_cast<long>(kA252CodecMaxVolumePercent)) {
                Serial.printf("[RTC_BL_PHONE] forcing ES8388 volume to 100 (requested=%ld)\n",
                              value);
            }
            applied_value = static_cast<long>(kA252CodecMaxVolumePercent);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
        JsonObject root = doc.to<JsonObject>();
        root["device_name"] = g_espnow.deviceName();
        JsonArray peers = root["peers"].to<JsonArray>();
        g_peer_store.peers = g_espnow.peers();
        g_peer_store.device_name = g_espnow.deviceName();
        A252ConfigStore::peersToJson(g_peer_store, peers);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_STATUS", [](const String&) {
        DynamicJsonDocument doc(1024);
        g_espnow.statusToJson(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_DEVICE_NAME_GET", [](const String&) {
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
        JsonObject root = doc.to<JsonObject>();
        root["active"] = g_hotline.active;
        root["scene"] = g_active_scene_id;
        root["step"] = g_active_step_id;
        root["validation_state"] = hotlineValidationStateToString(g_hotline_validation_state);
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
        root["ringback_active"] = g_hotline.ringback_active;
        root["ringback_until_ms"] = g_hotline.ringback_until_ms;
        root["ringback_profile"] = toneProfileToString(g_hotline.ringback_profile);
        root["post_ringback_target"] = g_hotline.post_ringback_valid
                                           ? describeMediaRouteTarget(g_hotline.post_ringback_route)
                                           : String("");
        root["last_notify_event"] = g_hotline.last_notify_event;
        root["last_notify_ok"] = g_hotline.last_notify_ok;
        root["route_lookup_key"] = g_hotline.last_route_lookup_key;
        root["route_resolution"] = g_hotline.last_route_resolution;
        root["route_target"] = g_hotline.last_route_target;
        root["scene_sync_enabled"] = g_espnow_scene_sync.enabled;
        root["scene_sync_interval_ms"] = g_espnow_scene_sync.interval_ms;
        root["scene_sync_pending"] = g_espnow_scene_sync.request_pending;
        root["scene_sync_last_error"] = g_espnow_scene_sync.last_error;
        root["scene_sync_last_source"] = g_espnow_scene_sync.last_source;
        root["scene_sync_last_update_ms"] = g_espnow_scene_sync.last_update_ms;
        root["interlude_enabled"] = g_hotline_interlude.enabled;
        root["interlude_next_due_ms"] = g_hotline_interlude.next_due_ms;
        root["interlude_last_file"] = g_hotline_interlude.last_file;
        root["interlude_last_trigger_ms"] = g_hotline_interlude.last_trigger_ms;
        root["interlude_last_error"] = g_hotline_interlude.last_error;
        root["warning_siren_enabled"] = g_warning_siren.enabled;
        root["warning_siren_tone_owned"] = g_warning_siren.tone_owned;
        root["warning_siren_profile"] = toneProfileToString(g_warning_siren.profile);
        root["warning_siren_event"] = toneEventToString(g_warning_siren.event);
        root["warning_siren_strength"] = g_warning_siren.strength;
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

    g_dispatcher.registerCommand("HOTLINE_INTERLUDE_FORCE", [](const String&) {
        const bool triggered = triggerHotlineInterludeNow("force");
        if (!triggered) {
            return makeResponse(false,
                                String("HOTLINE_INTERLUDE_FORCE ") +
                                    (g_hotline_interlude.last_error.isEmpty() ? "failed" : g_hotline_interlude.last_error));
        }
        return makeResponse(true, "HOTLINE_INTERLUDE_FORCE");
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

    g_dispatcher.registerCommand("WARNING_SIREN", [](const String& args) {
        return dispatchWarningSirenCommand(args);
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
        noteHotlineRouteResolution(buildHotlineLookupKey(normalizeHotlineSceneKey(scene_id), g_hotline_validation_state, "none"),
                                   "scene_route",
                                   route);
        if (!playMediaRoute(route)) {
            return makeResponse(false, "HOTLINE_SCENE_PLAY play_failed");
        }

        DynamicJsonDocument doc(1024);
        JsonObject root = doc.to<JsonObject>();
        root["ok"] = true;
        root["code"] = "HOTLINE_SCENE_PLAY";
        root["scene"] = scene_id;
        root["path"] = route.path;
        root["source"] = mediaSourceToString(route.source);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WAITING_VALIDATION", [](const String& args) {
        return dispatchWaitingValidationCommand(args);
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_GET", [](const String&) {
        DynamicJsonDocument doc(1024);
        A252ConfigStore::pinsToJson(g_pins_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "SLIC_CONFIG_SET invalid_json");
        }

        DynamicJsonDocument doc(1024);
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

        DynamicJsonDocument out(1024);
        A252ConfigStore::pinsToJson(g_pins_cfg, out.to<JsonObject>());
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_GET", [](const String&) {
        DynamicJsonDocument doc(1024);
        A252ConfigStore::audioToJson(g_audio_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "AUDIO_CONFIG_SET invalid_json");
        }

        DynamicJsonDocument doc(1024);
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
        DynamicJsonDocument doc(1024);
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

        DynamicJsonDocument doc(1024);
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

        DynamicJsonDocument doc(1024);
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
    maybeTrackEspNowSceneSyncAck(source, payload);

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

    auto send_bounded_bridge_response = [&](JsonDocument& response, bool envelope_mode) {
        constexpr size_t kEspNowResponseBudget = 220U;
        String response_payload;
        serializeJson(response, response_payload);

        if (response_payload.length() > kEspNowResponseBudget) {
            if (envelope_mode) {
                JsonObject payload_obj = response["payload"].as<JsonObject>();
                payload_obj.remove("data");
                payload_obj.remove("data_raw");
                payload_obj["truncated"] = true;
            } else {
                response.remove("data");
                response.remove("data_raw");
                response["truncated"] = true;
            }
            response_payload = "";
            serializeJson(response, response_payload);
        }

        if (response_payload.length() > kEspNowResponseBudget) {
            DynamicJsonDocument minimal(1024);
            if (envelope_mode) {
                minimal["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
                minimal["seq"] = request_seq;
                minimal["type"] = "ack";
                minimal["ack"] = true;
                JsonObject payload_obj = minimal["payload"].to<JsonObject>();
                payload_obj["ok"] = response["payload"]["ok"] | false;
                payload_obj["code"] = response["payload"]["code"] | "";
                payload_obj["error"] = "response_truncated";
            } else {
                minimal["proto"] = "rtcbl/1";
                minimal["id"] = request_id;
                minimal["ok"] = response["ok"] | false;
                minimal["code"] = response["code"] | "";
                minimal["error"] = "response_truncated";
            }
            response_payload = "";
            serializeJson(minimal, response_payload);
        }

        g_espnow.sendJson(source, response_payload);
    };

    DispatchResponse result;
    if (handleIncomingEspNowCallCommand(cmd, result)) {
        if (is_envelope_v2 && request_ack && isMacAddressString(source)) {
            DynamicJsonDocument response(1024);
            response["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
            response["seq"] = request_seq;
            response["type"] = "ack";
            response["ack"] = true;
            JsonObject ack_payload = response["payload"].to<JsonObject>();
            ack_payload["ok"] = result.ok;
            ack_payload["code"] = result.code;
            ack_payload["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

            if (!result.json.isEmpty()) {
                DynamicJsonDocument parsed(1024);
                if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                    ack_payload["data"].set(parsed.as<JsonVariantConst>());
                } else {
                    ack_payload["data_raw"] = result.json;
                }
            } else if (!result.raw.isEmpty()) {
                ack_payload["data_raw"] = result.raw;
            }

            send_bounded_bridge_response(response, true);
            return;
        }

        if (!is_rtcbl_v1 || !isMacAddressString(source)) {
            return;
        }

        DynamicJsonDocument response(1024);
        response["proto"] = "rtcbl/1";
        response["id"] = request_id;
        response["ok"] = result.ok;
        response["code"] = result.code;
        response["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

        if (!result.json.isEmpty()) {
            DynamicJsonDocument parsed(1024);
            if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                JsonVariant data = response["data"];
                data.set(parsed.as<JsonVariantConst>());
            } else {
                response["data_raw"] = result.json;
            }
        } else if (!result.raw.isEmpty()) {
            response["data_raw"] = result.raw;
        }

        send_bounded_bridge_response(response, false);
        return;
    }

    result = executeCommandLine(cmd);

    if (is_envelope_v2 && request_ack && isMacAddressString(source)) {
        DynamicJsonDocument response(1024);
        response["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
        response["seq"] = request_seq;
        response["type"] = "ack";
        response["ack"] = true;

        JsonObject ack_payload = response["payload"].to<JsonObject>();
        ack_payload["ok"] = result.ok;
        ack_payload["code"] = result.code;
        ack_payload["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

        if (!result.json.isEmpty()) {
            DynamicJsonDocument parsed(1024);
            if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                ack_payload["data"].set(parsed.as<JsonVariantConst>());
            } else {
                ack_payload["data_raw"] = result.json;
            }
        } else if (!result.raw.isEmpty()) {
            ack_payload["data_raw"] = result.raw;
        }

        send_bounded_bridge_response(response, true);
        return;
    }

    if (!is_rtcbl_v1 || !isMacAddressString(source)) {
        return;
    }

    DynamicJsonDocument response(1024);
    response["proto"] = "rtcbl/1";
    response["id"] = request_id;
    response["ok"] = result.ok;
    response["code"] = result.code;
    response["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

    if (!result.json.isEmpty()) {
        DynamicJsonDocument parsed(1024);
        if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
            JsonVariant data = response["data"];
            data.set(parsed.as<JsonVariantConst>());
        } else {
            response["data_raw"] = result.json;
        }
    } else if (!result.raw.isEmpty()) {
        response["data_raw"] = result.raw;
    }

    send_bounded_bridge_response(response, false);
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

void enforceOnHookSilence() {
    if (g_slic.isHookOff()) {
        return;
    }
    const bool allow_warning_siren_tone = g_warning_siren.enabled && g_warning_siren.tone_owned;
    if (g_audio.isPlaying()) {
        g_audio.stopPlayback();
    }
    if (g_audio.isToneRenderingActive() && !allow_warning_siren_tone) {
        g_audio.stopTone();
    }
}

void tickPlaybackCompletionBusyTone() {
    const bool is_playing = g_audio.isPlaying();
    if (g_prev_audio_playing && !is_playing) {
        if (g_busy_tone_after_media_pending && g_slic.isHookOff()) {
            g_audio.stopDialTone();
            const bool busy_ok = g_audio.playTone(ToneProfile::FR_FR, ToneEvent::BUSY);
            Serial.printf("[RTC_BL_PHONE] media playback completed -> busy tone ok=%s\n", busy_ok ? "true" : "false");
        }
        if (g_win_etape_validation_after_media_pending && g_slic.isHookOff()) {
            const bool ack_ok = sendHotlineValidationAckEvent("ACK_WIN1", true, "auto_440");
            if (ack_ok) {
                g_hotline_validation_state = HotlineValidationState::kGranted;
                g_hotline.current_digits = "440";
            }
            appendHotlineLogLine("WIN_ETAPE_440", String("ack=") + (ack_ok ? "1" : "0"));
            Serial.printf("[RTC_BL_PHONE] WIN_ETAPE auto-compose 440 -> ACK_WIN1 sent=%s\n", ack_ok ? "true" : "false");
        }
        g_win_etape_validation_after_media_pending = false;
        g_busy_tone_after_media_pending = false;
    }
    g_prev_audio_playing = is_playing;
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
    } else {
        refreshHotlineVoiceSuffixCatalog();
        if (!g_slic.isHookOff()) {
            g_audio.stopPlayback();
            g_audio.stopTone();
            Serial.println("[RTC_BL_PHONE] boot hook=ON_HOOK -> audio autoplay blocked");
        }
    }
    registerCommands();

    g_espnow.begin(g_peer_store);
    g_peer_store.device_name = g_espnow.deviceName();
    g_peer_store.peers = g_espnow.peers();
    initEspNowPeerDiscoveryRuntime();
    initEspNowSceneSyncRuntime();
    g_hotline_interlude = HotlineInterludeRuntimeState{};
    g_hotline_interlude.enabled = true;
    scheduleNextHotlineInterlude(millis());
    Serial.printf("[RTC_BL_PHONE] hotline interlude scheduler enabled next_due_ms=%lu\n",
                  static_cast<unsigned long>(g_hotline_interlude.next_due_ms));
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
    const TelephonyState prev_telephony_state = g_telephony.state();
    g_telephony.tick();
    const uint32_t now_ms = millis();
    const TelephonyState current_telephony_state = g_telephony.state();
    if (current_telephony_state == TelephonyState::OFF_HOOK && prev_telephony_state != TelephonyState::OFF_HOOK) {
        requestSceneSyncFromFreenove("off_hook", true);
        armOffHookAutoRandomPlayback(now_ms);
    } else if (prev_telephony_state == TelephonyState::OFF_HOOK &&
               current_telephony_state != TelephonyState::OFF_HOOK) {
        clearOffHookAutoRandomPlayback();
    }
    tickOffHookAutoRandomPlayback(now_ms);
    tickHotlineRuntime();
    tickHotlineInterludeRuntime();
    tickWarningSirenRuntime();
    enforceOnHookSilence();
    tickPlaybackCompletionBusyTone();
    g_scope_display.tick();
    g_web_server.handle();
    g_espnow.tick();
    tickEspNowPeerDiscoveryRuntime();
    tickEspNowSceneSyncRuntime();
    pollSerial();
    delay(1);
}
#endif  // UNIT_TEST
