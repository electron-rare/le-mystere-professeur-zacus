#ifndef CONFIG_A252_CONFIG_STORE_H
#define CONFIG_A252_CONFIG_STORE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "config/a1s_board_pins.h"
#include "media/MediaRouting.h"

struct A252PinsConfig {
    int i2s_bck = A1S_I2S_BCLK;
    int i2s_ws = A1S_I2S_LRCK;
    int i2s_dout = A1S_I2S_DOUT;
    int i2s_din = A1S_I2S_DIN;

    int es8388_sda = A1S_I2C_SDA;
    int es8388_scl = A1S_I2C_SCL;

    // A252 bench defaults.
    int slic_rm = A1S_SLIC_RM;
    int slic_fr = A1S_SLIC_FR;
    int slic_shk = A1S_SLIC_SHK;
    int slic_line = -1;
    int slic_pd = A1S_SLIC_PD;
    int slic_adc_in = -1;
    bool hook_active_high = true;

    int pcm_flt = -1;
    int pcm_demp = -1;
    int pcm_xsmt = -1;
    int pcm_fmt = -1;
};

// Intentional aliases for board-centric naming in S3-focused firmware branches.
using S3PinsConfig = A252PinsConfig;

struct A252AudioConfig {
    uint32_t sample_rate = 8000;
    uint8_t bits_per_sample = 16;
    bool enable_capture = true;
    bool adc_dsp_enabled = true;
    bool adc_fft_enabled = true;
    uint8_t adc_dsp_fft_downsample = 2U;
    uint16_t adc_fft_ignore_low_bin = 1U;
    uint16_t adc_fft_ignore_high_bin = 1U;
    uint8_t volume = 100;
    bool mute = false;
    String route = "rtc";
    String clock_policy = "HYBRID_TELCO";
    String wav_loudness_policy = "FIXED_GAIN_ONLY";
    int16_t wav_target_rms_dbfs = -18;
    int16_t wav_limiter_ceiling_dbfs = -2;
    uint16_t wav_limiter_attack_ms = 8;
    uint16_t wav_limiter_release_ms = 120;
};

// Intentional aliases for board-centric naming in S3-focused firmware branches.
using S3AudioConfig = A252AudioConfig;

struct EspNowCallMapEntry {
    String keyword;
    MediaRouteEntry route;
};

using EspNowCallMap = std::vector<EspNowCallMapEntry>;

struct DialMediaMapEntry {
    String number;
    MediaRouteEntry route;
};

using DialMediaMap = std::vector<DialMediaMapEntry>;

struct EspNowPeerStore {
    std::vector<String> peers;
    String device_name = "HOTLINE_PHONE";
};

class A252ConfigStore {
public:
    // Legacy board-agnostic interface.
    static A252PinsConfig defaultPins();
    static A252AudioConfig defaultAudio();

    // S3/board-clarity façade.
    static S3PinsConfig defaultS3Pins();
    static S3AudioConfig defaultS3Audio();

    static bool loadPins(A252PinsConfig& out);
    static bool savePins(const A252PinsConfig& cfg, String* error = nullptr);

    static bool loadS3Pins(S3PinsConfig& out);
    static bool saveS3Pins(const S3PinsConfig& cfg, String* error = nullptr);

    static bool loadAudio(A252AudioConfig& out);
    static bool saveAudio(const A252AudioConfig& cfg, String* error = nullptr);

    static bool loadS3Audio(S3AudioConfig& out);
    static bool saveS3Audio(const S3AudioConfig& cfg, String* error = nullptr);

    static bool loadEspNowPeers(EspNowPeerStore& out);
    static bool saveEspNowPeers(const EspNowPeerStore& store, String* error = nullptr);
    static bool loadEspNowCallMap(EspNowCallMap& out);
    static bool saveEspNowCallMap(const EspNowCallMap& map, String* error = nullptr);
    static bool loadDialMediaMap(DialMediaMap& out);
    static bool saveDialMediaMap(const DialMediaMap& map, String* error = nullptr);

    static bool validatePins(const A252PinsConfig& cfg, String& error);
    static bool validateAudio(const A252AudioConfig& cfg, String& error);

    static void pinsToJson(const A252PinsConfig& cfg, JsonObject obj);
    static void audioToJson(const A252AudioConfig& cfg, JsonObject obj);
    static void peersToJson(const EspNowPeerStore& store, JsonArray arr);
    static void espNowCallMapToJson(const EspNowCallMap& map, JsonObject obj);
    static void dialMediaMapToJson(const DialMediaMap& map, JsonObject obj);

    static String normalizeMac(const String& value);
    static String normalizeDeviceName(const String& value);
    static bool parseMac(const String& value, uint8_t out[6]);
};

#endif  // CONFIG_A252_CONFIG_STORE_H
