#include "config/A252ConfigStore.h"

#include <Preferences.h>

#include <algorithm>

#include "core/PlatformProfile.h"

namespace {
constexpr const char* kPinsNs = "a252-pins";
constexpr const char* kAudioNs = "a252-audio";
constexpr const char* kEspNowNs = "espnow";
constexpr const char* kEspNowCallMapNs = "espnow-call";
constexpr const char* kDialMediaMapNs = "dial-media";
constexpr uint16_t kMaxPlaybackPauseMs = 10000U;
constexpr const char* kEspNowKeyPeers = "peers";
constexpr const char* kEspNowKeyDeviceName = "dev_name";
constexpr const char* kDefaultEspNowDeviceName = "HOTLINE_PHONE";

// NVS keys are limited to 15 visible chars on ESP32 Preferences/NVS.
constexpr const char* kAudioKeySampleRate = "sr";
constexpr const char* kAudioKeyBitsPerSample = "bits";
constexpr const char* kAudioKeyEnableCapture = "capture";
constexpr const char* kAudioKeyAdcDspEnabled = "adc_dsp";
constexpr const char* kAudioKeyAdcFftEnabled = "adc_fft";
constexpr const char* kAudioKeyAdcDspFftDownsample = "adc_fft_ds";
constexpr const char* kAudioKeyAdcFftIgnoreLowBin = "adc_fft_lo";
constexpr const char* kAudioKeyAdcFftIgnoreHighBin = "adc_fft_hi";
constexpr const char* kAudioKeyVolume = "vol";
constexpr const char* kAudioKeyRoute = "route";
constexpr const char* kAudioKeyMute = "mute";
constexpr const char* kAudioKeyClockPolicy = "clock_policy";
constexpr const char* kAudioKeyWavLoudnessPolicy = "wav_loud_pol";
constexpr const char* kAudioKeyWavTargetRmsDbfs = "wav_rms_dbfs";
constexpr const char* kAudioKeyWavLimiterCeilingDbfs = "wav_ceil_db";
constexpr const char* kAudioKeyWavLimiterAttackMs = "wav_attack_ms";
constexpr const char* kAudioKeyWavLimiterReleaseMs = "wav_release_ms";
constexpr int kMaxGpioA252 = 39;
constexpr int kMaxGpioS3 = 48;

int maxAllowedPinForProfile(BoardProfile profile) {
    return profile == BoardProfile::ESP32_S3 ? kMaxGpioS3 : kMaxGpioA252;
}

bool saveString(Preferences& prefs, const char* key, const String& value) {
    return prefs.putString(key, value) > 0U;
}

bool saveUChar(Preferences& prefs, const char* key, uint8_t value) {
    return prefs.putUChar(key, value) == 1U;
}

bool saveUInt(Preferences& prefs, const char* key, uint32_t value) {
    return prefs.putUInt(key, value) == sizeof(uint32_t);
}

bool saveInt(Preferences& prefs, const char* key, int32_t value) {
    return prefs.putInt(key, value) == sizeof(int32_t);
}

bool saveBool(Preferences& prefs, const char* key, bool value) {
    return prefs.putBool(key, value) == 1U;
}

bool loadJsonArray(const String& raw, JsonDocument& doc) {
    if (raw.isEmpty()) {
        doc.to<JsonArray>();
        return true;
    }
    const auto err = deserializeJson(doc, raw);
    return err == DeserializationError::Ok && doc.is<JsonArray>();
}

bool loadJsonObject(const String& raw, JsonDocument& doc) {
    if (raw.isEmpty()) {
        doc.to<JsonObject>();
        return true;
    }
    const auto err = deserializeJson(doc, raw);
    return err == DeserializationError::Ok && doc.is<JsonObject>();
}

String normalizeEspNowCallKeyword(const String& keyword) {
    String normalized = keyword;
    normalized.trim();
    normalized.toUpperCase();
    return normalized;
}

void mergeCallMapEntry(EspNowCallMap& map, const String& keyword, const MediaRouteEntry& route) {
    const String normalized_keyword = normalizeEspNowCallKeyword(keyword);
    if (normalized_keyword.isEmpty()) {
        return;
    }
    if (!mediaRouteHasPayload(route)) {
        return;
    }

    for (EspNowCallMapEntry& entry : map) {
        if (entry.keyword == normalized_keyword) {
            entry.route = route;
            return;
        }
    }
    EspNowCallMapEntry created;
    created.keyword = normalized_keyword;
    created.route = route;
    map.push_back(created);
}

void mergeDialMediaMapEntry(DialMediaMap& map, const String& number, const MediaRouteEntry& route) {
    String normalized_number = number;
    normalized_number.trim();
    if (normalized_number.isEmpty()) {
        return;
    }
    if (!mediaRouteHasPayload(route)) {
        return;
    }

    for (DialMediaMapEntry& entry : map) {
        if (entry.number == normalized_number) {
            entry.route = route;
            return;
        }
    }
    DialMediaMapEntry created;
    created.number = normalized_number;
    created.route = route;
    map.push_back(created);
}

bool parseMediaRouteEntry(JsonVariantConst value, MediaRouteEntry& out) {
    out = MediaRouteEntry{};
    if (value.is<const char*>()) {
        out.kind = MediaRouteKind::FILE;
        out.path = sanitizeMediaPath(value.as<const char*>());
        out.source = MediaSource::AUTO;
        return !out.path.isEmpty();
    }
    if (!value.is<JsonObjectConst>()) {
        return false;
    }
    JsonObjectConst obj = value.as<JsonObjectConst>();

    MediaRouteKind kind = MediaRouteKind::FILE;
    if (obj["kind"].is<const char*>()) {
        if (!parseMediaRouteKind(obj["kind"].as<const char*>(), kind)) {
            return false;
        }
    }
    out.kind = kind;

    if (kind == MediaRouteKind::TONE) {
        if (!obj["profile"].is<const char*>() || !obj["event"].is<const char*>()) {
            return false;
        }
        if (!parseToneProfile(obj["profile"].as<const char*>(), out.tone.profile)) {
            return false;
        }
        if (!parseToneEvent(obj["event"].as<const char*>(), out.tone.event)) {
            return false;
        }
        if (out.tone.profile == ToneProfile::NONE || out.tone.event == ToneEvent::NONE) {
            return false;
        }
        out.path = "";
        out.source = MediaSource::AUTO;
        return true;
    }

    if (!obj["path"].is<const char*>()) {
        return false;
    }

    out.path = sanitizeMediaPath(obj["path"].as<const char*>());
    if (out.path.isEmpty()) {
        return false;
    }

    out.source = MediaSource::AUTO;
    if (obj["source"].is<const char*>()) {
        MediaSource parsed = MediaSource::AUTO;
        if (!parseMediaSource(obj["source"].as<const char*>(), parsed)) {
            return false;
        }
        out.source = parsed;
    }

    bool loop = false;
    if (obj["playback"]["loop"].is<bool>()) {
        loop = obj["playback"]["loop"].as<bool>();
    } else if (obj["loop"].is<bool>()) {
        loop = obj["loop"].as<bool>();
    }

    uint16_t pause_ms = 0U;
    if (obj["playback"]["pause_ms"].is<int>()) {
        const int raw = obj["playback"]["pause_ms"].as<int>();
        if (raw < 0 || raw > static_cast<int>(kMaxPlaybackPauseMs)) {
            return false;
        }
        pause_ms = static_cast<uint16_t>(raw);
    } else if (obj["pause_ms"].is<int>()) {
        const int raw = obj["pause_ms"].as<int>();
        if (raw < 0 || raw > static_cast<int>(kMaxPlaybackPauseMs)) {
            return false;
        }
        pause_ms = static_cast<uint16_t>(raw);
    }

    out.playback.loop = loop;
    out.playback.pause_ms = pause_ms;
    return true;
}

void writeMediaRouteToObject(JsonObject obj, const char* key, const MediaRouteEntry& route) {
    if (key == nullptr || key[0] == '\0') {
        return;
    }
    if (route.kind == MediaRouteKind::TONE) {
        JsonObject tone_obj = obj[key].to<JsonObject>();
        tone_obj["kind"] = "tone";
        tone_obj["profile"] = toneProfileToString(route.tone.profile);
        tone_obj["event"] = toneEventToString(route.tone.event);
        return;
    }
    const bool has_playback_policy = route.playback.loop || route.playback.pause_ms > 0U;
    if (route.source == MediaSource::AUTO && !has_playback_policy) {
        obj[key] = route.path;
        return;
    }
    JsonObject route_obj = obj[key].to<JsonObject>();
    route_obj["kind"] = "file";
    route_obj["path"] = route.path;
    if (route.source != MediaSource::AUTO) {
        route_obj["source"] = mediaSourceToString(route.source);
    }
    if (has_playback_policy) {
        JsonObject playback = route_obj["playback"].to<JsonObject>();
        playback["loop"] = route.playback.loop;
        playback["pause_ms"] = route.playback.pause_ms;
    }
}

}  // namespace

A252PinsConfig A252ConfigStore::defaultPins() {
    A252PinsConfig cfg;
    if (detectBoardProfile() == BoardProfile::ESP32_S3) {
        cfg.i2s_bck = 40;
        cfg.i2s_ws = 41;
        cfg.i2s_dout = 42;
        cfg.i2s_din = 39;
        cfg.es8388_sda = -1;
        cfg.es8388_scl = -1;
        cfg.slic_rm = 32;
        cfg.slic_fr = 5;
        cfg.slic_shk = 23;
        cfg.slic_pd = 14;
        cfg.slic_adc_in = 34;
        cfg.hook_active_high = true;
        cfg.pcm_flt = -1;
        cfg.pcm_demp = -1;
        cfg.pcm_xsmt = -1;
        cfg.pcm_fmt = -1;
    }
    return cfg;
}

S3PinsConfig A252ConfigStore::defaultS3Pins() {
    return defaultPins();
}

A252AudioConfig A252ConfigStore::defaultAudio() {
    return A252AudioConfig{};
}

S3AudioConfig A252ConfigStore::defaultS3Audio() {
    return defaultAudio();
}

bool A252ConfigStore::loadPins(A252PinsConfig& out) {
    out = defaultPins();
    Preferences prefs;
    if (!prefs.begin(kPinsNs, false)) {
        return false;
    }

    out.i2s_bck = prefs.getInt("i2s_bck", out.i2s_bck);
    out.i2s_ws = prefs.getInt("i2s_ws", out.i2s_ws);
    out.i2s_dout = prefs.getInt("i2s_dout", out.i2s_dout);
    out.i2s_din = prefs.getInt("i2s_din", out.i2s_din);

    out.es8388_sda = prefs.getInt("i2c_sda", out.es8388_sda);
    out.es8388_scl = prefs.getInt("i2c_scl", out.es8388_scl);

    out.slic_rm = prefs.getInt("slic_rm", out.slic_rm);
    out.slic_fr = prefs.getInt("slic_fr", out.slic_fr);
    out.slic_shk = prefs.getInt("slic_shk", out.slic_shk);
    out.slic_line = prefs.getInt("slic_line", out.slic_line);
    out.slic_pd = prefs.getInt("slic_pd", out.slic_pd);
    out.slic_adc_in = prefs.getInt("slic_adc_in", out.slic_adc_in);
    out.hook_active_high = prefs.getBool("hook_hi", out.hook_active_high);
    out.pcm_flt = prefs.getInt("pcm_flt", out.pcm_flt);
    out.pcm_demp = prefs.getInt("pcm_demp", out.pcm_demp);
    out.pcm_xsmt = prefs.getInt("pcm_xsmt", out.pcm_xsmt);
    out.pcm_fmt = prefs.getInt("pcm_fmt", out.pcm_fmt);
    prefs.end();

    String error;
    if (!validatePins(out, error)) {
        out = defaultPins();
        return false;
    }
    return true;
}

bool A252ConfigStore::loadS3Pins(S3PinsConfig& out) {
    return loadPins(out);
}

bool A252ConfigStore::saveS3Pins(const S3PinsConfig& cfg, String* error) {
    return savePins(cfg, error);
}

bool A252ConfigStore::savePins(const A252PinsConfig& cfg, String* error) {
    String local_error;
    if (!validatePins(cfg, local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(kPinsNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }

    prefs.putInt("i2s_bck", cfg.i2s_bck);
    prefs.putInt("i2s_ws", cfg.i2s_ws);
    prefs.putInt("i2s_dout", cfg.i2s_dout);
    prefs.putInt("i2s_din", cfg.i2s_din);

    prefs.putInt("i2c_sda", cfg.es8388_sda);
    prefs.putInt("i2c_scl", cfg.es8388_scl);

    prefs.putInt("slic_rm", cfg.slic_rm);
    prefs.putInt("slic_fr", cfg.slic_fr);
    prefs.putInt("slic_shk", cfg.slic_shk);
    prefs.putInt("slic_line", cfg.slic_line);
    prefs.putInt("slic_pd", cfg.slic_pd);
    prefs.putInt("slic_adc_in", cfg.slic_adc_in);
    prefs.putBool("hook_hi", cfg.hook_active_high);
    prefs.putInt("pcm_flt", cfg.pcm_flt);
    prefs.putInt("pcm_demp", cfg.pcm_demp);
    prefs.putInt("pcm_xsmt", cfg.pcm_xsmt);
    prefs.putInt("pcm_fmt", cfg.pcm_fmt);
    prefs.end();
    return true;
}

bool A252ConfigStore::loadAudio(A252AudioConfig& out) {
    out = defaultAudio();
    Preferences prefs;
    if (!prefs.begin(kAudioNs, false)) {
        return false;
    }

    out.sample_rate = prefs.getUInt(kAudioKeySampleRate, out.sample_rate);
    out.bits_per_sample = static_cast<uint8_t>(prefs.getUChar(kAudioKeyBitsPerSample, out.bits_per_sample));
    out.enable_capture = prefs.getBool(kAudioKeyEnableCapture, out.enable_capture);
    out.adc_dsp_enabled = prefs.getBool(kAudioKeyAdcDspEnabled, out.adc_dsp_enabled);
    out.adc_fft_enabled = prefs.getBool(kAudioKeyAdcFftEnabled, out.adc_fft_enabled);
    out.adc_dsp_fft_downsample = static_cast<uint8_t>(prefs.getUChar(kAudioKeyAdcDspFftDownsample, out.adc_dsp_fft_downsample));
    out.adc_fft_ignore_low_bin =
        static_cast<uint16_t>(prefs.getUInt(kAudioKeyAdcFftIgnoreLowBin, out.adc_fft_ignore_low_bin));
    out.adc_fft_ignore_high_bin =
        static_cast<uint16_t>(prefs.getUInt(kAudioKeyAdcFftIgnoreHighBin, out.adc_fft_ignore_high_bin));
    out.volume = static_cast<uint8_t>(prefs.getUChar(kAudioKeyVolume, out.volume));
    out.mute = prefs.getBool(kAudioKeyMute, out.mute);
    if (prefs.isKey(kAudioKeyRoute)) {
        out.route = prefs.getString(kAudioKeyRoute, out.route);
    }
    if (prefs.isKey(kAudioKeyClockPolicy)) {
        out.clock_policy = prefs.getString(kAudioKeyClockPolicy, out.clock_policy);
    }
    if (prefs.isKey(kAudioKeyWavLoudnessPolicy)) {
        out.wav_loudness_policy = prefs.getString(kAudioKeyWavLoudnessPolicy, out.wav_loudness_policy);
    }
    out.wav_target_rms_dbfs = static_cast<int16_t>(prefs.getInt(kAudioKeyWavTargetRmsDbfs, out.wav_target_rms_dbfs));
    out.wav_limiter_ceiling_dbfs =
        static_cast<int16_t>(prefs.getInt(kAudioKeyWavLimiterCeilingDbfs, out.wav_limiter_ceiling_dbfs));
    out.wav_limiter_attack_ms = static_cast<uint16_t>(prefs.getUInt(kAudioKeyWavLimiterAttackMs, out.wav_limiter_attack_ms));
    out.wav_limiter_release_ms = static_cast<uint16_t>(prefs.getUInt(kAudioKeyWavLimiterReleaseMs, out.wav_limiter_release_ms));
    prefs.end();

    String error;
    if (!validateAudio(out, error)) {
        out = defaultAudio();
        return false;
    }
    return true;
}

bool A252ConfigStore::loadS3Audio(S3AudioConfig& out) {
    return loadAudio(out);
}

bool A252ConfigStore::saveS3Audio(const S3AudioConfig& cfg, String* error) {
    return saveAudio(cfg, error);
}

bool A252ConfigStore::saveAudio(const A252AudioConfig& cfg, String* error) {
    String local_error;
    if (!validateAudio(cfg, local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(kAudioNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }

    bool ok = true;
    ok = ok && saveUInt(prefs, kAudioKeySampleRate, cfg.sample_rate);
    ok = ok && saveUChar(prefs, kAudioKeyBitsPerSample, cfg.bits_per_sample);
    ok = ok && saveBool(prefs, kAudioKeyEnableCapture, cfg.enable_capture);
    ok = ok && saveBool(prefs, kAudioKeyAdcDspEnabled, cfg.adc_dsp_enabled);
    ok = ok && saveBool(prefs, kAudioKeyAdcFftEnabled, cfg.adc_fft_enabled);
    ok = ok && saveUChar(prefs, kAudioKeyAdcDspFftDownsample, cfg.adc_dsp_fft_downsample);
    ok = ok && saveUInt(prefs, kAudioKeyAdcFftIgnoreLowBin, cfg.adc_fft_ignore_low_bin);
    ok = ok && saveUInt(prefs, kAudioKeyAdcFftIgnoreHighBin, cfg.adc_fft_ignore_high_bin);
    ok = ok && saveUChar(prefs, kAudioKeyVolume, cfg.volume);
    ok = ok && saveString(prefs, kAudioKeyRoute, cfg.route);
    ok = ok && saveBool(prefs, kAudioKeyMute, cfg.mute);
    ok = ok && saveString(prefs, kAudioKeyClockPolicy, cfg.clock_policy);
    ok = ok && saveString(prefs, kAudioKeyWavLoudnessPolicy, cfg.wav_loudness_policy);
    ok = ok && saveInt(prefs, kAudioKeyWavTargetRmsDbfs, cfg.wav_target_rms_dbfs);
    ok = ok && saveInt(prefs, kAudioKeyWavLimiterCeilingDbfs, cfg.wav_limiter_ceiling_dbfs);
    ok = ok && saveUInt(prefs, kAudioKeyWavLimiterAttackMs, cfg.wav_limiter_attack_ms);
    ok = ok && saveUInt(prefs, kAudioKeyWavLimiterReleaseMs, cfg.wav_limiter_release_ms);
    prefs.end();
    if (!ok) {
        if (error) {
            *error = "nvs_write_failed";
        }
        return false;
    }
    return true;
}

bool A252ConfigStore::loadEspNowPeers(EspNowPeerStore& out) {
    out.peers.clear();
    out.device_name = kDefaultEspNowDeviceName;

    Preferences prefs;
    if (!prefs.begin(kEspNowNs, false)) {
        return false;
    }
    const String raw = prefs.isKey(kEspNowKeyPeers) ? prefs.getString(kEspNowKeyPeers, "[]") : String("[]");
    if (prefs.isKey(kEspNowKeyDeviceName)) {
        const String normalized_name = normalizeDeviceName(prefs.getString(kEspNowKeyDeviceName, kDefaultEspNowDeviceName));
        if (!normalized_name.isEmpty()) {
            out.device_name = normalized_name;
        }
    }
    prefs.end();

    JsonDocument doc;
    if (!loadJsonArray(raw, doc)) {
        return false;
    }

    for (const JsonVariantConst item : doc.as<JsonArrayConst>()) {
        if (!item.is<const char*>()) {
            continue;
        }
        const String norm = normalizeMac(item.as<const char*>());
        if (norm.isEmpty()) {
            continue;
        }
        if (std::find(out.peers.begin(), out.peers.end(), norm) == out.peers.end()) {
            out.peers.push_back(norm);
        }
    }
    return true;
}

bool A252ConfigStore::loadEspNowCallMap(EspNowCallMap& out) {
    out.clear();
    Preferences prefs;
    if (!prefs.begin(kEspNowCallMapNs, false)) {
        return false;
    }
    const String raw = prefs.isKey("mappings") ? prefs.getString("mappings", "{}") : String("{}");
    prefs.end();

    JsonDocument doc;
    if (!loadJsonObject(raw, doc)) {
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair item : obj) {
        MediaRouteEntry route;
        if (!parseMediaRouteEntry(item.value(), route)) {
            continue;
        }
        const String key = item.key().c_str();
        mergeCallMapEntry(out, key, route);
    }
    return true;
}

bool A252ConfigStore::saveEspNowCallMap(const EspNowCallMap& map, String* error) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    for (const EspNowCallMapEntry& entry : map) {
        if (entry.keyword.isEmpty() || !mediaRouteHasPayload(entry.route)) {
            continue;
        }
        writeMediaRouteToObject(obj, entry.keyword.c_str(), entry.route);
    }

    String raw;
    serializeJson(obj, raw);

    Preferences prefs;
    if (!prefs.begin(kEspNowCallMapNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }
    const bool ok = prefs.putString("mappings", raw) >= 0;
    prefs.end();
    return ok;
}

bool A252ConfigStore::loadDialMediaMap(DialMediaMap& out) {
    out.clear();
    Preferences prefs;
    if (!prefs.begin(kDialMediaMapNs, false)) {
        return false;
    }
    const String raw = prefs.isKey("mappings") ? prefs.getString("mappings", "{}") : String("{}");
    prefs.end();

    JsonDocument doc;
    if (!loadJsonObject(raw, doc)) {
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair item : obj) {
        MediaRouteEntry route;
        if (!parseMediaRouteEntry(item.value(), route)) {
            continue;
        }
        mergeDialMediaMapEntry(out, item.key().c_str(), route);
    }
    return true;
}

bool A252ConfigStore::saveDialMediaMap(const DialMediaMap& map, String* error) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    for (const DialMediaMapEntry& entry : map) {
        if (entry.number.isEmpty() || !mediaRouteHasPayload(entry.route)) {
            continue;
        }
        writeMediaRouteToObject(obj, entry.number.c_str(), entry.route);
    }

    String raw;
    serializeJson(obj, raw);

    Preferences prefs;
    if (!prefs.begin(kDialMediaMapNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }
    const bool ok = prefs.putString("mappings", raw) >= 0;
    prefs.end();
    return ok;
}

void A252ConfigStore::espNowCallMapToJson(const EspNowCallMap& map, JsonObject obj) {
    for (const EspNowCallMapEntry& entry : map) {
        if (entry.keyword.isEmpty() || !mediaRouteHasPayload(entry.route)) {
            continue;
        }
        writeMediaRouteToObject(obj, entry.keyword.c_str(), entry.route);
    }
}

void A252ConfigStore::dialMediaMapToJson(const DialMediaMap& map, JsonObject obj) {
    for (const DialMediaMapEntry& entry : map) {
        if (entry.number.isEmpty() || !mediaRouteHasPayload(entry.route)) {
            continue;
        }
        writeMediaRouteToObject(obj, entry.number.c_str(), entry.route);
    }
}

bool A252ConfigStore::saveEspNowPeers(const EspNowPeerStore& store, String* error) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const String& peer : store.peers) {
        if (!normalizeMac(peer).isEmpty()) {
            arr.add(normalizeMac(peer));
        }
    }

    String raw;
    serializeJson(arr, raw);

    Preferences prefs;
    if (!prefs.begin(kEspNowNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }
    const String normalized_name = normalizeDeviceName(store.device_name);
    const String device_name = normalized_name.isEmpty() ? String(kDefaultEspNowDeviceName) : normalized_name;

    bool ok = true;
    ok = ok && saveString(prefs, kEspNowKeyPeers, raw);
    ok = ok && saveString(prefs, kEspNowKeyDeviceName, device_name);
    prefs.end();
    if (!ok) {
        if (error) {
            *error = "nvs_write_failed";
        }
        return false;
    }
    return true;
}

bool A252ConfigStore::validatePins(const A252PinsConfig& cfg, String& error) {
    std::vector<int> used;
    used.reserve(14);
    const int max_gpio = maxAllowedPinForProfile(detectBoardProfile());

    const int required_pins[] = {
        cfg.i2s_bck,
        cfg.i2s_ws,
        cfg.i2s_dout,
        cfg.i2s_din,
        cfg.slic_rm,
        cfg.slic_fr,
        cfg.slic_shk,
        cfg.slic_pd,
    };

    const int optional_pins[] = {
        cfg.slic_adc_in,
        cfg.pcm_flt,
        cfg.pcm_demp,
        cfg.pcm_xsmt,
        cfg.pcm_fmt,
    };

    for (int pin : required_pins) {
        if (pin < 0 || pin > max_gpio) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), pin) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(pin);
    }

    for (int pin : optional_pins) {
        if (pin == -1) {
            continue;
        }
        if (pin < 0 || pin > max_gpio) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), pin) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(pin);
    }

    if (detectBoardProfile() == BoardProfile::ESP32_A252) {
        if (cfg.es8388_sda < 0 || cfg.es8388_scl < 0) {
            error = "invalid_pin_range";
            return false;
        }
        if (cfg.es8388_sda == cfg.es8388_scl) {
            error = "pin_conflict";
            return false;
        }
        if (cfg.es8388_sda < 0 || cfg.es8388_sda > max_gpio || cfg.es8388_scl < 0 || cfg.es8388_scl > max_gpio) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), cfg.es8388_sda) != used.end() ||
            std::find(used.begin(), used.end(), cfg.es8388_scl) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(cfg.es8388_sda);
        used.push_back(cfg.es8388_scl);
    } else {
        if (cfg.es8388_sda >= 0) {
            if (cfg.es8388_sda > max_gpio || std::find(used.begin(), used.end(), cfg.es8388_sda) != used.end()) {
                error = cfg.es8388_sda > max_gpio ? "invalid_pin_range" : "pin_conflict";
                return false;
            }
            used.push_back(cfg.es8388_sda);
        }
        if (cfg.es8388_scl >= 0) {
            if (cfg.es8388_scl > max_gpio || std::find(used.begin(), used.end(), cfg.es8388_scl) != used.end()) {
                error = cfg.es8388_scl > max_gpio ? "invalid_pin_range" : "pin_conflict";
                return false;
            }
            used.push_back(cfg.es8388_scl);
        }
    }

    // Optional legacy line-enable pin, retired by default (-1).
    if (cfg.slic_line != -1) {
        if (cfg.slic_line < 0 || cfg.slic_line > max_gpio) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), cfg.slic_line) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(cfg.slic_line);
    }

    error = "";
    return true;
}

bool A252ConfigStore::validateAudio(const A252AudioConfig& cfg, String& error) {
    if (cfg.sample_rate < 8000 || cfg.sample_rate > 48000) {
        error = "invalid_sample_rate";
        return false;
    }
    if (!(cfg.bits_per_sample == 16 || cfg.bits_per_sample == 24 || cfg.bits_per_sample == 32)) {
        error = "invalid_bits_per_sample";
        return false;
    }
    if (cfg.adc_dsp_fft_downsample == 0U || cfg.adc_dsp_fft_downsample > 64U) {
        error = "invalid_adc_dsp_fft_downsample";
        return false;
    }
    if (cfg.adc_fft_ignore_low_bin > 32U) {
        error = "invalid_adc_fft_ignore_low_bin";
        return false;
    }
    if (cfg.adc_fft_ignore_high_bin > 32U) {
        error = "invalid_adc_fft_ignore_high_bin";
        return false;
    }
    if (cfg.volume > 100) {
        error = "invalid_volume";
        return false;
    }

    String route = cfg.route;
    route.trim();
    route.toLowerCase();
    if (!(route == "rtc" || route == "none")) {
        error = "invalid_route";
        return false;
    }

    String clock_policy = cfg.clock_policy;
    clock_policy.trim();
    clock_policy.toUpperCase();
    if (!(clock_policy == "HYBRID_TELCO")) {
        error = "invalid_clock_policy";
        return false;
    }

    String wav_policy = cfg.wav_loudness_policy;
    wav_policy.trim();
    wav_policy.toUpperCase();
    if (!(wav_policy == "AUTO_NORMALIZE_LIMITER" || wav_policy == "FIXED_GAIN_ONLY")) {
        error = "invalid_wav_loudness_policy";
        return false;
    }
    if (cfg.wav_target_rms_dbfs < -36 || cfg.wav_target_rms_dbfs > -6) {
        error = "invalid_wav_target_rms_dbfs";
        return false;
    }
    if (cfg.wav_limiter_ceiling_dbfs < -12 || cfg.wav_limiter_ceiling_dbfs > 0) {
        error = "invalid_wav_limiter_ceiling_dbfs";
        return false;
    }
    if (cfg.wav_limiter_attack_ms < 1 || cfg.wav_limiter_attack_ms > 1000) {
        error = "invalid_wav_limiter_attack_ms";
        return false;
    }
    if (cfg.wav_limiter_release_ms < 1 || cfg.wav_limiter_release_ms > 5000) {
        error = "invalid_wav_limiter_release_ms";
        return false;
    }

    error = "";
    return true;
}

void A252ConfigStore::pinsToJson(const A252PinsConfig& cfg, JsonObject obj) {
    JsonObject i2s = obj["i2s"].to<JsonObject>();
    i2s["bck"] = cfg.i2s_bck;
    i2s["ws"] = cfg.i2s_ws;
    i2s["dout"] = cfg.i2s_dout;
    i2s["din"] = cfg.i2s_din;

    JsonObject i2c = obj["codec_i2c"].to<JsonObject>();
    i2c["sda"] = cfg.es8388_sda;
    i2c["scl"] = cfg.es8388_scl;

    JsonObject slic = obj["slic"].to<JsonObject>();
    slic["rm"] = cfg.slic_rm;
    slic["fr"] = cfg.slic_fr;
    slic["shk"] = cfg.slic_shk;
    slic["line"] = cfg.slic_line;
    slic["pd"] = cfg.slic_pd;
    slic["adc_in"] = cfg.slic_adc_in;
    slic["hook_active_high"] = cfg.hook_active_high;

    JsonObject pcm = obj["pcm"].to<JsonObject>();
    pcm["flt"] = cfg.pcm_flt;
    pcm["demp"] = cfg.pcm_demp;
    pcm["xsmt"] = cfg.pcm_xsmt;
    pcm["fmt"] = cfg.pcm_fmt;
}

void A252ConfigStore::audioToJson(const A252AudioConfig& cfg, JsonObject obj) {
    obj["sample_rate"] = cfg.sample_rate;
    obj["bits_per_sample"] = cfg.bits_per_sample;
    obj["enable_capture"] = cfg.enable_capture;
    obj["adc_dsp_enabled"] = cfg.adc_dsp_enabled;
    obj["adc_fft_enabled"] = cfg.adc_fft_enabled;
    obj["adc_dsp_fft_downsample"] = cfg.adc_dsp_fft_downsample;
    obj["adc_fft_ignore_low_bin"] = cfg.adc_fft_ignore_low_bin;
    obj["adc_fft_ignore_high_bin"] = cfg.adc_fft_ignore_high_bin;
    obj["volume"] = cfg.volume;
    obj["mute"] = cfg.mute;
    obj["route"] = cfg.route;
    obj["clock_policy"] = cfg.clock_policy;
    obj["wav_loudness_policy"] = cfg.wav_loudness_policy;
    obj["wav_target_rms_dbfs"] = cfg.wav_target_rms_dbfs;
    obj["wav_limiter_ceiling_dbfs"] = cfg.wav_limiter_ceiling_dbfs;
    obj["wav_limiter_attack_ms"] = cfg.wav_limiter_attack_ms;
    obj["wav_limiter_release_ms"] = cfg.wav_limiter_release_ms;
}

void A252ConfigStore::peersToJson(const EspNowPeerStore& store, JsonArray arr) {
    for (const String& peer : store.peers) {
        arr.add(peer);
    }
}

String A252ConfigStore::normalizeMac(const String& value) {
    String mac = value;
    mac.trim();
    mac.toUpperCase();

    String compact;
    compact.reserve(12);
    for (size_t i = 0; i < mac.length(); ++i) {
        const char c = mac[i];
        if (c == ':' || c == '-' || c == ' ') {
            continue;
        }
        const bool is_hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        if (!is_hex) {
            return "";
        }
        compact += c;
    }

    if (compact.length() != 12) {
        return "";
    }

    String formatted;
    formatted.reserve(17);
    for (int i = 0; i < 12; i += 2) {
        if (i > 0) {
            formatted += ':';
        }
        formatted += compact.substring(i, i + 2);
    }
    return formatted;
}

String A252ConfigStore::normalizeDeviceName(const String& value) {
    String name = value;
    name.trim();
    name.toUpperCase();
    if (name.isEmpty()) {
        return "";
    }

    constexpr size_t kMaxDeviceNameLength = 24;
    String normalized;
    normalized.reserve(std::min<size_t>(name.length(), kMaxDeviceNameLength));
    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name[i];
        const bool is_alpha = (c >= 'A' && c <= 'Z');
        const bool is_digit = (c >= '0' && c <= '9');
        const bool is_allowed_symbol = (c == '_' || c == '-');
        if (!(is_alpha || is_digit || is_allowed_symbol)) {
            return "";
        }
        if (normalized.length() >= kMaxDeviceNameLength) {
            break;
        }
        normalized += c;
    }
    return normalized;
}

bool A252ConfigStore::parseMac(const String& value, uint8_t out[6]) {
    const String formatted = normalizeMac(value);
    if (formatted.isEmpty()) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        const String chunk = formatted.substring(i * 3, i * 3 + 2);
        out[i] = static_cast<uint8_t>(strtoul(chunk.c_str(), nullptr, 16));
    }
    return true;
}
