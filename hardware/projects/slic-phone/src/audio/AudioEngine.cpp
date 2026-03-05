#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include "audio/AudioEngine.h"
#include <FFat.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <esp_dsp.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "config/a1s_board_pins.h"

namespace {
constexpr float kTwoPi = 6.28318530718f;
constexpr int16_t kToneAmplitude = 15000;
constexpr float kToneLinearGain = 0.58f;
// 10 ms at 8 kHz (A252 default sample rate): 80 frames.
// Smaller chunks reduce visible gaps when scheduling or I2S writes are delayed.
constexpr size_t kDialToneChunkFrames = 80;
constexpr uint8_t kToneCatchupChunksPerTick = 5U;
constexpr size_t kMaxChannels = 2;
constexpr size_t kAdcDspFirTaps = 5U;
constexpr float kDspDcBlockR = 0.995f;
constexpr float kDspHighPassHz = 250.0f;
constexpr float kDspLowPassHz = 3400.0f;
constexpr float kDspAdcScale = 1.0f / 2048.0f;
constexpr float kDspPostGain = 1.0f;
constexpr uint8_t kAdcDspMinFftDownsample = 1U;
constexpr uint8_t kAdcDspMaxFftDownsample = 64U;
constexpr float kDialToneAttackMs = 25.0f;
constexpr float kDialToneReleaseMs = 40.0f;
constexpr TickType_t kI2sWriteTimeoutMs = 30;
constexpr uint8_t kToneWriteRetryCount = 10U;
constexpr TickType_t kI2sReadTimeoutMs = 2;
constexpr size_t kPlaybackCopyBytes = 256U;
constexpr uint8_t kPlaybackCopyRetryCount = 24U;
constexpr uint8_t kPlaybackCopyRetryDelayMs = 1U;
// Keep retries bounded to avoid long loop stalls if the sink is wedged.
constexpr uint16_t kBlockingOutputMaxRetries = 120U;
constexpr uint8_t kBlockingOutputRetryDelayMs = 1U;
// Keep playback gain neutral; loudness is driven by ES8388 hardware volume.
constexpr float kPlaybackBoostLinear = 1.0f;
// Keep software gain neutral to avoid cumulative clipping with volume boosts.
constexpr float kPlaybackSoftwareGain = 1.0f;
// Hotline runtime lock: keep loudness auto processing disabled to preserve deterministic playback.
constexpr bool kHardDisableAutoLoudnessProcessing = true;
constexpr int16_t kAdcRawMax = 4095;
constexpr int16_t kAdcMidScale = kAdcRawMax / 2;
constexpr size_t kWavHeaderProbeMaxBytes = 262144U;
constexpr size_t kMp3HeaderProbeMaxBytes = 8192U;
constexpr uint32_t kStorageMountRetryIntervalMs = 3000U;
// Keep 24 kHz in the stable set: most hotline TTS MP3 prompts are encoded at 24 kHz.
// Avoiding needless 24k -> 22.05k resampling reduces write pressure and artifacts.
constexpr uint32_t kStableRatesHz[] = {8000U, 16000U, 22050U, 24000U, 32000U, 44100U, 48000U};
constexpr float kDbToLinearRef = 20.0f;
constexpr float kMinRmsLinear = 1.0e-5f;

class AudioToolsMp3OutputBridge final : public ::AudioOutput {
public:
    AudioToolsMp3OutputBridge() {
        channels = 2U;
        gainF2P6 = static_cast<uint8_t>(1U << 6U);
    }

    void setSink(Print* sink) {
        sink_ = sink;
    }

    bool begin() override {
        return sink_ != nullptr;
    }

    bool stop() override {
        return true;
    }

    bool SetRate(int hz) override {
        return ::AudioOutput::SetRate(hz);
    }

    bool SetChannels(int chan) override {
        if (!::AudioOutput::SetChannels(chan)) {
            return false;
        }
        if (channels < 1U) {
            channels = 1U;
        } else if (channels > 2U) {
            channels = 2U;
        }
        return true;
    }

    bool ConsumeSample(int16_t sample[2]) override {
        if (sink_ == nullptr || sample == nullptr) {
            return false;
        }

        if (channels <= 1U) {
            const int16_t mono = sample[::AudioOutput::LEFTCHANNEL];
            const size_t written = sink_->write(reinterpret_cast<const uint8_t*>(&mono), sizeof(mono));
            return written == sizeof(mono);
        }

        const int16_t stereo[2] = {sample[::AudioOutput::LEFTCHANNEL], sample[::AudioOutput::RIGHTCHANNEL]};
        const size_t written = sink_->write(reinterpret_cast<const uint8_t*>(stereo), sizeof(stereo));
        return written == sizeof(stereo);
    }

private:
    Print* sink_ = nullptr;
};

int16_t clampInt16(float value) {
    if (value > static_cast<float>(std::numeric_limits<int16_t>::max())) {
        return std::numeric_limits<int16_t>::max();
    }
    if (value < static_cast<float>(std::numeric_limits<int16_t>::min())) {
        return std::numeric_limits<int16_t>::min();
    }
    return static_cast<int16_t>(value);
}

void biquadLowPassCoeff(float sample_rate_hz, float frequency_hz, float q, float& b0, float& b1, float& b2, float& a1, float& a2) {
    if (frequency_hz <= 0.0f || sample_rate_hz <= 0.0f || q <= 0.0f) {
        b0 = 1.0f;
        b1 = 0.0f;
        b2 = 0.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        return;
    }
    const float omega = kTwoPi * frequency_hz / sample_rate_hz;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * q);
    const float b0o = (1.0f - cs) / 2.0f;
    const float b1o = 1.0f - cs;
    const float b2o = (1.0f - cs) / 2.0f;
    const float a0 = 1.0f + alpha;
    const float a1o = -2.0f * cs;
    const float a2o = 1.0f - alpha;

    b0 = b0o / a0;
    b1 = b1o / a0;
    b2 = b2o / a0;
    a1 = a1o / a0;
    a2 = a2o / a0;
}

void biquadHighPassCoeff(float sample_rate_hz, float frequency_hz, float q, float& b0, float& b1, float& b2, float& a1, float& a2) {
    if (frequency_hz <= 0.0f || sample_rate_hz <= 0.0f || q <= 0.0f) {
        b0 = 1.0f;
        b1 = 0.0f;
        b2 = 0.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        return;
    }
    const float omega = kTwoPi * frequency_hz / sample_rate_hz;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * q);
    const float b0o = (1.0f + cs) / 2.0f;
    const float b1o = -(1.0f + cs);
    const float b2o = (1.0f + cs) / 2.0f;
    const float a0 = 1.0f + alpha;
    const float a1o = -2.0f * cs;
    const float a2o = 1.0f - alpha;

    b0 = b0o / a0;
    b1 = b1o / a0;
    b2 = b2o / a0;
    a1 = a1o / a0;
    a2 = a2o / a0;
}

float processBiquad(float input, float& b0, float& b1, float& b2, float& a1, float& a2, float& z1, float& z2) {
    const float y = b0 * input + z1;
    z1 = b1 * input - a1 * y + z2;
    z2 = b2 * input - a2 * y;
    return y;
}

float clampFloat(float value, float lo, float hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

uint32_t saturatingAddU32(uint32_t base, size_t delta) {
    if (delta == 0U) {
        return base;
    }
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    if (delta >= static_cast<size_t>(kMax) || base >= (kMax - static_cast<uint32_t>(delta))) {
        return kMax;
    }
    return base + static_cast<uint32_t>(delta);
}

uint16_t readLeUint16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t readLeUint32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

int bitsPerSampleToInt(i2s_bits_per_sample_t bits) {
    switch (bits) {
        case I2S_BITS_PER_SAMPLE_24BIT:
            return 24;
        case I2S_BITS_PER_SAMPLE_32BIT:
            return 32;
        case I2S_BITS_PER_SAMPLE_16BIT:
        default:
            return 16;
    }
}

float linearToDb(float value) {
    const float safe = std::max(value, 1.0e-7f);
    return 20.0f * std::log10(safe);
}

float dbToLinear(float db) {
    return std::pow(10.0f, db / kDbToLinearRef);
}

bool wavAutoLoudnessEnabled(const AudioConfig& cfg) {
    return !kHardDisableAutoLoudnessProcessing && cfg.wav_auto_normalize_limiter;
}
}  // namespace

void AudioEngine::BlockingOutput::setOutput(Print* out) {
    out_ = out;
}

size_t AudioEngine::BlockingOutput::write(uint8_t b) {
    return write(&b, 1U);
}

size_t AudioEngine::BlockingOutput::write(const uint8_t* data, size_t len) {
    if (out_ == nullptr || data == nullptr || len == 0U) {
        return 0U;
    }

    size_t total_written = 0U;
    uint16_t retry = 0U;
    while (total_written < len) {
        const size_t written = out_->write(data + total_written, len - total_written);
        if (written > 0U) {
            total_written += written;
            retry = 0U;
            continue;
        }
        if (retry >= kBlockingOutputMaxRetries) {
            break;
        }
        ++retry;
        delay(kBlockingOutputRetryDelayMs);
        taskYIELD();
    }
    return total_written;
}

int AudioEngine::BlockingOutput::availableForWrite() {
    // Advertise writable capacity and let write() block/retry on the real sink.
    // This avoids StreamCopy aborting early on transient I2S backpressure spikes.
    return (out_ == nullptr) ? 0 : 4096;
}

AudioConfig defaultAudioConfigForProfile(BoardProfile profile) {
    AudioConfig cfg;
    if (profile == BoardProfile::ESP32_S3) {
        cfg.sample_rate = 8000;
        cfg.bck_pin = 40;
        cfg.ws_pin = 41;
        cfg.data_out_pin = 42;
        cfg.data_in_pin = 39;
        cfg.enable_capture = true;
    } else {
        // AI Thinker A252 defaults (ESP32-A1S + ES8388).
        cfg.sample_rate = 8000;
        cfg.bck_pin = A1S_I2S_BCLK;
        cfg.ws_pin = A1S_I2S_LRCK;
        cfg.data_out_pin = A1S_I2S_DOUT;
        cfg.data_in_pin = A1S_I2S_DIN;
        cfg.enable_capture = false;
    }
    return cfg;
}

AudioEngine::AudioEngine()
    : driver_installed_(false),
      capture_active_(false),
      capture_clients_mask_(0),
      playing_(false),
      features_(getFeatureMatrix(detectBoardProfile())) {
    playback_blocking_output_.setOutput(&i2s_stream_);
    playback_gain_stream_.setOutput(playback_blocking_output_);
    playback_volume_stream_.setOutput(playback_gain_stream_);
    playback_channel_converter_stream_.setOutput(playback_volume_stream_);
    playback_resample_stream_.setOutput(playback_channel_converter_stream_);
    wav_stream_.setOutput(playback_volume_stream_);
    wav_stream_.setDecoder(&wav_decoder_);
    wav_copy_.setCheckAvailable(false);
    wav_copy_.setCheckAvailableForWrite(false);
    wav_copy_.setMinCopySize(sizeof(int16_t));
    wav_copy_.setRetry(kPlaybackCopyRetryCount);
    wav_copy_.setRetryDelay(kPlaybackCopyRetryDelayMs);
    mp3_output_ = new AudioToolsMp3OutputBridge();
}

AudioEngine::~AudioEngine() {
    end();
}

size_t AudioEngine::activeChannelCount(i2s_channel_fmt_t channel_format) {
    switch (channel_format) {
        case I2S_CHANNEL_FMT_ONLY_LEFT:
        case I2S_CHANNEL_FMT_ONLY_RIGHT:
            return 1;
        default:
            return 2;
    }
}

bool AudioEngine::lockI2s() const {
    if (i2s_io_mutex_ == nullptr) {
        return true;
    }
    return xSemaphoreTake(i2s_io_mutex_, pdMS_TO_TICKS(1)) == pdTRUE;
}

void AudioEngine::unlockI2s() const {
    if (i2s_io_mutex_ != nullptr) {
        xSemaphoreGive(i2s_io_mutex_);
    }
}

bool AudioEngine::lockPlaybackState(TickType_t timeout_ticks) const {
    if (playback_state_mutex_ == nullptr) {
        return true;
    }
    return xSemaphoreTake(playback_state_mutex_, timeout_ticks) == pdTRUE;
}

void AudioEngine::unlockPlaybackState() const {
    if (playback_state_mutex_ != nullptr) {
        xSemaphoreGive(playback_state_mutex_);
    }
}

bool AudioEngine::ensureSdMounted() {
    static uint32_t last_sd_attempt_ms = 0U;
    if (sd_ready_ && sd_fs_ != nullptr) {
        return true;
    }

    const uint32_t now = millis();
    if (sd_mount_attempted_ && static_cast<uint32_t>(now - last_sd_attempt_ms) < kStorageMountRetryIntervalMs) {
        return false;
    }

    sd_mount_attempted_ = true;
    last_sd_attempt_ms = now;
    if (SD_MMC.begin()) {
        sd_ready_ = true;
        sd_fs_ = &SD_MMC;
        return true;
    }

    Serial.println("[AudioEngine] SD_MMC begin failed, trying SD SPI fallback");
    static bool sd_spi_bus_started = false;
    if (!sd_spi_bus_started) {
        SPI.begin(A1S_SD_SCK, A1S_SD_MISO, A1S_SD_MOSI, A1S_SD_CS);
        sd_spi_bus_started = true;
    }

    if (SD.begin(A1S_SD_CS, SPI, 10000000U)) {
        sd_ready_ = true;
        sd_fs_ = &SD;
        Serial.println("[AudioEngine] SD mounted via SPI fallback");
        return true;
    }

    sd_ready_ = false;
    sd_fs_ = nullptr;
    Serial.println("[AudioEngine] SD SPI fallback init failed");
    return false;
}

bool AudioEngine::ensureLittleFsMounted() {
    static uint32_t last_littlefs_attempt_ms = 0U;
    if (littlefs_ready_) {
        return true;
    }

    const uint32_t now = millis();
    if (littlefs_mount_attempted_ && static_cast<uint32_t>(now - last_littlefs_attempt_ms) < kStorageMountRetryIntervalMs) {
        return false;
    }

    littlefs_mount_attempted_ = true;
    last_littlefs_attempt_ms = now;
#ifdef USB_MSC_BOOT_ENABLE
    littlefs_ready_ = FFat.begin(true, "/usbmsc", 10, "usbmsc");
#else
    littlefs_ready_ = FFat.begin(true);
#endif
    if (!littlefs_ready_) {
        Serial.println("[AudioEngine] FFat begin failed");
    }
    return littlefs_ready_;
}

bool AudioEngine::ensureStorageForSource(MediaSource source) {
    switch (source) {
        case MediaSource::SD:
            return ensureSdMounted();
        case MediaSource::LITTLEFS:
            return ensureLittleFsMounted();
        case MediaSource::AUTO:
        default:
            return ensureSdMounted() || ensureLittleFsMounted();
    }
}

bool AudioEngine::openPlaybackFileForSource(const char* path, MediaSource source, fs::FS*& out_fs, MediaSource& out_source) {
    out_fs = nullptr;
    out_source = MediaSource::AUTO;

    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    auto try_open = [&](MediaSource candidate, fs::FS& fsref) -> bool {
        File file = fsref.open(path, FILE_READ);
        if (!file) {
            return false;
        }
        playback_file_ = file;
        out_fs = &fsref;
        out_source = candidate;
        return true;
    };

    if (source == MediaSource::SD) {
        if (!ensureSdMounted()) {
            return false;
        }
        if (sd_fs_ == nullptr) {
            return false;
        }
        return try_open(MediaSource::SD, *sd_fs_);
    }

    if (source == MediaSource::LITTLEFS) {
        if (!ensureLittleFsMounted()) {
            return false;
        }
        return try_open(MediaSource::LITTLEFS, FFat);
    }

    if (ensureSdMounted() && sd_fs_ != nullptr && try_open(MediaSource::SD, *sd_fs_)) {
        return true;
    }
    if (ensureLittleFsMounted() && try_open(MediaSource::LITTLEFS, FFat)) {
        return true;
    }
    return false;
}

void AudioEngine::stopPlaybackFileUnlocked() {
    wav_copy_.end();
    wav_stream_.end();
    if (mp3_decoder_ != nullptr) {
        mp3_decoder_->stop();
        delete mp3_decoder_;
        mp3_decoder_ = nullptr;
    }
    if (mp3_source_ != nullptr) {
        mp3_source_->close();
        delete mp3_source_;
        mp3_source_ = nullptr;
    }
    if (mp3_output_ != nullptr) {
        static_cast<AudioToolsMp3OutputBridge*>(mp3_output_)->setSink(nullptr);
    }
    mp3_pcm_sink_ = nullptr;
    mp3_source_last_pos_ = 0U;
    playback_resample_stream_.end();
    playback_channel_converter_stream_.end();
    playback_channel_converter_stream_.setOutput(playback_volume_stream_);
    playback_resample_stream_.setOutput(playback_channel_converter_stream_);
    wav_stream_.setOutput(playback_volume_stream_);
    playback_loudness_gain_db_ = 0.0f;
    playback_volume_stream_.setVolume(kPlaybackBoostLinear);
    restorePlaybackAudioInfo();
    if (playback_file_) {
        playback_file_.close();
    }
    playback_path_ = "";
    playback_data_remaining_ = 0;
    playback_data_offset_ = 0;
    playback_wav_direct_mode_ = false;
    playback_mp3_bitrate_bps_ = 0U;
    playback_input_channels_ = 0;
    playback_input_audio_info_.clear();
    playback_resampler_active_ = false;
    playback_channel_upmix_active_ = false;
    playback_loudness_auto_ = false;
    playback_limiter_active_ = false;
    playback_rate_fallback_ = 0;
    playback_next_chunk_ms_ = 0U;
    playback_codec_ = PlaybackCodec::NONE;
    playing_ = false;
}

void AudioEngine::stopPlaybackFile() {
    if (!lockPlaybackState(pdMS_TO_TICKS(50))) {
        return;
    }
    stopPlaybackFileUnlocked();
    unlockPlaybackState();
}

bool AudioEngine::prepareWavPlayback(File& file, const char* path) {
    if (!file) {
        return false;
    }

    playback_last_error_ = "";

    audio_tools::AudioInfo wav_info{};
    uint32_t data_offset = 0U;
    uint32_t data_size = 0U;
    const char* path_text = (path == nullptr) ? "(null)" : path;
    if (!readWavHeaderInfo(file, wav_info, &data_offset, &data_size)) {
        Serial.printf("[AudioEngine] wav header not parsed, using runtime format for %s\n", path_text);
        playback_input_audio_info_ = default_playback_audio_info_;
        playback_resampler_active_ = false;
        playback_channel_upmix_active_ = false;
        playback_rate_fallback_ = 0U;
        playback_loudness_auto_ = false;
        playback_limiter_active_ = false;
        playback_data_offset_ = 0U;
        playback_loudness_gain_db_ = 0.0f;
        playback_volume_stream_.setVolume(kPlaybackBoostLinear);
        restorePlaybackAudioInfo();
        return true;
    }

    if (!isPlaybackAudioInfoSupported(wav_info)) {
        Serial.printf("[AudioEngine] wav format unsupported by playback path: sr=%u ch=%u bits=%u for %s\n",
                      static_cast<unsigned>(wav_info.sample_rate),
                      static_cast<unsigned>(wav_info.channels),
                      static_cast<unsigned>(wav_info.bits_per_sample),
                      path_text);
        playback_input_audio_info_ = default_playback_audio_info_;
        playback_resampler_active_ = false;
        playback_channel_upmix_active_ = false;
        playback_rate_fallback_ = 0U;
        playback_loudness_auto_ = false;
        playback_limiter_active_ = false;
        playback_data_offset_ = 0U;
        playback_loudness_gain_db_ = 0.0f;
        playback_volume_stream_.setVolume(kPlaybackBoostLinear);
        restorePlaybackAudioInfo();
        return true;
    }

    playback_input_audio_info_ = wav_info;
    playback_data_offset_ = data_offset;
    playback_data_remaining_ = data_size;

    const audio_tools::AudioInfo resolved_output = resolvePlaybackFormat(wav_info);
    playback_resampler_active_ = (resolved_output.sample_rate != wav_info.sample_rate);
    playback_channel_upmix_active_ = (wav_info.channels == 1U && resolved_output.channels == 2U);
    applyPlaybackAudioInfo(resolved_output);
    if (!configureWavPlaybackPipeline(wav_info, resolved_output)) {
        playback_last_error_ = "wav_pipeline_config_failed";
        playback_resampler_active_ = false;
        playback_channel_upmix_active_ = false;
        restorePlaybackAudioInfo();
        return false;
    }

    playback_loudness_auto_ = wavAutoLoudnessEnabled(_config);
    playback_limiter_active_ = false;
    playback_loudness_gain_db_ = 0.0f;
    if (playback_loudness_auto_) {
        bool limiter_active = false;
        playback_loudness_gain_db_ = analyzeWavLoudnessGainDb(file, wav_info, data_offset, data_size, limiter_active);
        playback_limiter_active_ = limiter_active;
    }
    playback_volume_stream_.setVolume(kPlaybackBoostLinear);

    Serial.printf("[AudioEngine] wav playback header parsed sr=%u ch=%u bits=%u path=%s\n",
                  static_cast<unsigned>(wav_info.sample_rate),
                  static_cast<unsigned>(wav_info.channels),
                  static_cast<unsigned>(wav_info.bits_per_sample),
                  path_text);
    Serial.printf(
        "[AudioEngine] playback resolved in(sr=%u,ch=%u,bits=%u) -> out(sr=%u,ch=%u,bits=%u) resampler=%s upmix=%s fallback=%u gain_db=%.2f limiter=%s\n",
        static_cast<unsigned>(wav_info.sample_rate),
        static_cast<unsigned>(wav_info.channels),
        static_cast<unsigned>(wav_info.bits_per_sample),
        static_cast<unsigned>(resolved_output.sample_rate),
        static_cast<unsigned>(resolved_output.channels),
        static_cast<unsigned>(resolved_output.bits_per_sample),
        playback_resampler_active_ ? "true" : "false",
        playback_channel_upmix_active_ ? "true" : "false",
        static_cast<unsigned>(playback_rate_fallback_),
        static_cast<double>(playback_loudness_gain_db_),
        playback_limiter_active_ ? "true" : "false");
    return true;
}

bool AudioEngine::isMp3Path(const char* path) const {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    const size_t len = std::strlen(path);
    if (len < 4U) {
        return false;
    }
    const char* ext = path + (len - 4U);
    return (std::tolower(static_cast<unsigned char>(ext[0])) == '.') &&
           (std::tolower(static_cast<unsigned char>(ext[1])) == 'm') &&
           (std::tolower(static_cast<unsigned char>(ext[2])) == 'p') &&
           (std::tolower(static_cast<unsigned char>(ext[3])) == '3');
}

bool AudioEngine::readMp3HeaderInfo(File& file, audio_tools::AudioInfo& info, uint32_t* out_bitrate) const {
    info.clear();
    if (out_bitrate != nullptr) {
        *out_bitrate = 0U;
    }
    if (!file) {
        return false;
    }

    const size_t original_pos = file.position();
    if (!file.seek(0U)) {
        return false;
    }

    size_t scan_start = 0U;
    uint8_t id3_header[10] = {0};
    if (file.read(id3_header, sizeof(id3_header)) == sizeof(id3_header)) {
        if (std::memcmp(id3_header, "ID3", 3U) == 0) {
            const uint32_t id3_size = ((static_cast<uint32_t>(id3_header[6] & 0x7FU) << 21U) |
                                       (static_cast<uint32_t>(id3_header[7] & 0x7FU) << 14U) |
                                       (static_cast<uint32_t>(id3_header[8] & 0x7FU) << 7U) |
                                       static_cast<uint32_t>(id3_header[9] & 0x7FU));
            scan_start = static_cast<size_t>(10U + id3_size);
        }
    }

    if (!file.seek(scan_start)) {
        file.seek(original_pos);
        return false;
    }

    const size_t file_size = static_cast<size_t>(file.size());
    const size_t scan_end = std::min<size_t>(file_size, scan_start + kMp3HeaderProbeMaxBytes);
    const size_t chunk_size = 512U;
    std::vector<uint8_t> buffer(chunk_size + 3U, 0U);
    size_t scanned = scan_start;
    size_t prefix_len = 0U;

    while (scanned < scan_end) {
        const size_t want = std::min<size_t>(chunk_size, scan_end - scanned);
        const size_t got = file.read(buffer.data() + prefix_len, want);
        if (got == 0U) {
            break;
        }

        const size_t available = prefix_len + got;
        for (size_t i = 0U; i + 3U < available; ++i) {
            const uint8_t b0 = buffer[i + 0U];
            const uint8_t b1 = buffer[i + 1U];
            const uint8_t b2 = buffer[i + 2U];
            const uint8_t b3 = buffer[i + 3U];
            if (b0 != 0xFFU || (b1 & 0xE0U) != 0xE0U) {
                continue;
            }

            const uint8_t version_bits = static_cast<uint8_t>((b1 >> 3U) & 0x03U);
            const uint8_t layer_bits = static_cast<uint8_t>((b1 >> 1U) & 0x03U);
            const uint8_t bitrate_index = static_cast<uint8_t>((b2 >> 4U) & 0x0FU);
            const uint8_t sample_rate_index = static_cast<uint8_t>((b2 >> 2U) & 0x03U);
            if (version_bits == 0x01U || layer_bits != 0x01U || sample_rate_index == 0x03U ||
                bitrate_index == 0x00U || bitrate_index == 0x0FU) {
                continue;
            }

            static constexpr uint16_t kSampleRateTable[4][3] = {
                {11025U, 12000U, 8000U},   // MPEG 2.5
                {0U, 0U, 0U},              // reserved
                {22050U, 24000U, 16000U},  // MPEG 2
                {44100U, 48000U, 32000U},  // MPEG 1
            };
            static constexpr uint16_t kBitrateMpeg1L3[16] = {
                0U, 32U, 40U, 48U, 56U, 64U, 80U, 96U, 112U, 128U, 160U, 192U, 224U, 256U, 320U, 0U,
            };
            static constexpr uint16_t kBitrateMpeg2L3[16] = {
                0U, 8U, 16U, 24U, 32U, 40U, 48U, 56U, 64U, 80U, 96U, 112U, 128U, 144U, 160U, 0U,
            };

            const uint16_t sample_rate = kSampleRateTable[version_bits][sample_rate_index];
            if (sample_rate == 0U) {
                continue;
            }
            const uint16_t bitrate_kbps =
                (version_bits == 0x03U) ? kBitrateMpeg1L3[bitrate_index] : kBitrateMpeg2L3[bitrate_index];
            if (bitrate_kbps == 0U) {
                continue;
            }

            const uint8_t channel_mode = static_cast<uint8_t>((b3 >> 6U) & 0x03U);
            const uint8_t channels = (channel_mode == 0x03U) ? 1U : 2U;
            info.sample_rate = sample_rate;
            info.channels = channels;
            info.bits_per_sample = 16U;
            if (out_bitrate != nullptr) {
                *out_bitrate = static_cast<uint32_t>(bitrate_kbps) * 1000U;
            }
            file.seek(original_pos);
            return true;
        }

        prefix_len = std::min<size_t>(3U, available);
        if (prefix_len > 0U) {
            std::memmove(buffer.data(), buffer.data() + (available - prefix_len), prefix_len);
        }
        scanned += got;
    }

    file.seek(original_pos);
    return false;
}

bool AudioEngine::prepareMp3Playback(File& file, const char* path) {
    if (!file) {
        return false;
    }

    playback_last_error_ = "";
    playback_data_offset_ = 0U;
    playback_data_remaining_ = static_cast<uint32_t>(file.size());
    playback_mp3_bitrate_bps_ = 0U;
    playback_loudness_auto_ = false;
    playback_limiter_active_ = false;
    playback_loudness_gain_db_ = 0.0f;
    playback_rate_fallback_ = 0U;
    playback_resampler_active_ = false;
    playback_channel_upmix_active_ = false;
    playback_volume_stream_.setVolume(kPlaybackBoostLinear);

    audio_tools::AudioInfo mp3_info{};
    uint32_t mp3_bitrate_bps = 0U;
    if (!readMp3HeaderInfo(file, mp3_info, &mp3_bitrate_bps) || !isPlaybackAudioInfoSupported(mp3_info)) {
        playback_input_audio_info_ = default_playback_audio_info_;
        playback_resampler_active_ = false;
        playback_channel_upmix_active_ = false;
        playback_rate_fallback_ = 0U;
        restorePlaybackAudioInfo();
        if (!file.seek(0U)) {
            return false;
        }
        Serial.printf("[AudioEngine] mp3 header not parsed, using runtime format for %s\n",
                      path == nullptr ? "(null)" : path);
        return true;
    }

    playback_mp3_bitrate_bps_ = mp3_bitrate_bps;
    playback_input_audio_info_ = mp3_info;
    const audio_tools::AudioInfo resolved_output = resolvePlaybackFormat(mp3_info);
    playback_resampler_active_ = (resolved_output.sample_rate != mp3_info.sample_rate);
    playback_channel_upmix_active_ = (mp3_info.channels == 1U && resolved_output.channels == 2U);
    applyPlaybackAudioInfo(resolved_output);
    if (!configureMp3PlaybackPipeline(mp3_info, resolved_output)) {
        playback_last_error_ = "mp3_pipeline_config_failed";
        playback_resampler_active_ = false;
        playback_channel_upmix_active_ = false;
        restorePlaybackAudioInfo();
        return false;
    }
    if (!file.seek(0U)) {
        return false;
    }
    Serial.printf("[AudioEngine] mp3 header parsed sr=%u ch=%u bits=%u bitrate=%u path=%s out_sr=%u out_ch=%u resampler=%s upmix=%s\n",
                  static_cast<unsigned>(mp3_info.sample_rate),
                  static_cast<unsigned>(mp3_info.channels),
                  static_cast<unsigned>(mp3_info.bits_per_sample),
                  static_cast<unsigned>(playback_mp3_bitrate_bps_),
                  path == nullptr ? "(null)" : path,
                  static_cast<unsigned>(resolved_output.sample_rate),
                  static_cast<unsigned>(resolved_output.channels),
                  playback_resampler_active_ ? "true" : "false",
                  playback_channel_upmix_active_ ? "true" : "false");
    return true;
}

bool AudioEngine::readWavHeaderInfo(
    File& file,
    audio_tools::AudioInfo& info,
    uint32_t* out_data_offset,
    uint32_t* out_data_size) const {
    info.clear();
    if (out_data_offset != nullptr) {
        *out_data_offset = 0U;
    }
    if (out_data_size != nullptr) {
        *out_data_size = 0U;
    }
    const size_t original_pos = file.position();
    if (!file.seek(0)) {
        return false;
    }

    constexpr uint16_t kRiffAudioFormatPcm = 1U;
    constexpr size_t kChunkHeaderLen = 8U;

    uint8_t riff_header[12];
    const size_t riff_read = file.read(riff_header, sizeof(riff_header));
    if (riff_read != sizeof(riff_header)) {
        file.seek(original_pos);
        return false;
    }
    if (std::memcmp(riff_header, "RIFF", 4U) != 0 || std::memcmp(riff_header + 8U, "WAVE", 4U) != 0) {
        file.seek(original_pos);
        return false;
    }

    bool fmt_found = false;
    bool data_found = false;
    uint16_t audio_format = 0U;
    uint16_t channels = 0U;
    uint32_t sample_rate = 0U;
    uint16_t bits_per_sample = 0U;
    uint32_t data_offset = 0U;
    uint32_t data_size = 0U;
    size_t scanned_bytes = sizeof(riff_header);

    while (scanned_bytes + kChunkHeaderLen <= kWavHeaderProbeMaxBytes) {
        uint8_t chunk_header[kChunkHeaderLen];
        const size_t header_read = file.read(chunk_header, sizeof(chunk_header));
        if (header_read != sizeof(chunk_header)) {
            break;
        }
        scanned_bytes += kChunkHeaderLen;

        const uint32_t chunk_len = readLeUint32(chunk_header + 4U);
        const size_t chunk_data_pos = file.position();

        if (std::memcmp(chunk_header, "fmt ", 4U) == 0) {
            if (chunk_len < 16U) {
                file.seek(original_pos);
                return false;
            }
            uint8_t fmt_header[16];
            const size_t fmt_read = file.read(fmt_header, sizeof(fmt_header));
            if (fmt_read != sizeof(fmt_header)) {
                file.seek(original_pos);
                return false;
            }
            audio_format = readLeUint16(fmt_header + 0U);
            channels = readLeUint16(fmt_header + 2U);
            sample_rate = readLeUint32(fmt_header + 4U);
            bits_per_sample = readLeUint16(fmt_header + 14U);
            fmt_found = true;
        } else if (std::memcmp(chunk_header, "data", 4U) == 0) {
            data_offset = static_cast<uint32_t>(chunk_data_pos);
            data_size = chunk_len;
            data_found = true;
            break;
        }

        size_t next_pos = chunk_data_pos + static_cast<size_t>(chunk_len);
        if ((chunk_len & 1U) != 0U) {
            ++next_pos;
        }
        scanned_bytes += static_cast<size_t>(chunk_len) + static_cast<size_t>(chunk_len & 1U);
        if (scanned_bytes > kWavHeaderProbeMaxBytes) {
            break;
        }
        if (!file.seek(next_pos)) {
            break;
        }
    }

    file.seek(original_pos);

    if (!fmt_found) {
        return false;
    }
    if (audio_format != kRiffAudioFormatPcm) {
        return false;
    }
    if (channels == 0U || sample_rate == 0U || bits_per_sample == 0U) {
        return false;
    }

    info.sample_rate = sample_rate;
    info.channels = channels;
    info.bits_per_sample = bits_per_sample;
    if (data_found && out_data_offset != nullptr) {
        *out_data_offset = data_offset;
    }
    if (data_found && out_data_size != nullptr) {
        *out_data_size = data_size;
    }
    return true;
}

bool AudioEngine::isPlaybackAudioInfoSupported(const audio_tools::AudioInfo& info) const {
    const uint32_t sample_rate = info.sample_rate;
    if (sample_rate < 8000U || sample_rate > 48000U || info.channels == 0U || info.channels > 2U ||
        info.bits_per_sample == 0U) {
        return false;
    }
    if (info.bits_per_sample != 8U && info.bits_per_sample != 16U && info.bits_per_sample != 24U &&
        info.bits_per_sample != 32U) {
        return false;
    }
    return true;
}

uint32_t AudioEngine::resolveStableSampleRate(uint32_t requested_rate_hz, uint32_t& fallback_rate_hz) const {
    fallback_rate_hz = 0U;
    for (uint32_t rate_hz : kStableRatesHz) {
        if (rate_hz == requested_rate_hz) {
            return requested_rate_hz;
        }
    }

    uint32_t best = kStableRatesHz[0];
    uint32_t best_delta = static_cast<uint32_t>(std::abs(static_cast<int32_t>(requested_rate_hz) -
                                                         static_cast<int32_t>(best)));
    for (uint32_t rate_hz : kStableRatesHz) {
        const uint32_t delta = static_cast<uint32_t>(std::abs(static_cast<int32_t>(requested_rate_hz) -
                                                              static_cast<int32_t>(rate_hz)));
        if (delta < best_delta) {
            best = rate_hz;
            best_delta = delta;
        }
    }
    fallback_rate_hz = best;
    return best;
}

audio_tools::AudioInfo AudioEngine::resolvePlaybackFormat(const audio_tools::AudioInfo& input) {
    audio_tools::AudioInfo output = input;
    uint32_t fallback_rate_hz = 0U;
    if (_config.hybrid_telco_clock_policy) {
        // Hybrid policy for media fidelity: follow WAV input rate when stable,
        // fallback to nearest stable rate only when needed.
        uint32_t stable_fallback_rate_hz = 0U;
        output.sample_rate = resolveStableSampleRate(input.sample_rate, stable_fallback_rate_hz);
        if (output.sample_rate != input.sample_rate) {
            fallback_rate_hz = output.sample_rate;
        }
    } else {
        output.sample_rate = _config.sample_rate;
        if (output.sample_rate != input.sample_rate) {
            fallback_rate_hz = output.sample_rate;
        }
    }
    playback_rate_fallback_ = fallback_rate_hz;
    output.bits_per_sample = 16U;
    // Keep source channel count when possible: mono WAV prompts are native on A252.
    output.channels = (input.channels == 0U) ? 1U : std::min<uint16_t>(input.channels, 2U);
    return output;
}

bool AudioEngine::configureWavPlaybackPipeline(const audio_tools::AudioInfo& input, const audio_tools::AudioInfo& output) {
    playback_resample_stream_.end();
    playback_channel_converter_stream_.end();
    playback_channel_converter_stream_.setOutput(playback_volume_stream_);
    playback_resample_stream_.setOutput(playback_channel_converter_stream_);
    wav_stream_.setOutput(playback_volume_stream_);

    const bool channel_convert_active = (output.channels != input.channels);

    if (playback_resampler_active_) {
        if (channel_convert_active) {
            playback_resample_stream_.setOutput(playback_channel_converter_stream_);
        } else {
            playback_resample_stream_.setOutput(playback_volume_stream_);
        }
        wav_stream_.setOutput(playback_resample_stream_);
        if (!playback_resample_stream_.begin(input, static_cast<int>(output.sample_rate))) {
            Serial.printf("[AudioEngine] wav resampler begin failed in_sr=%u out_sr=%u\n",
                          static_cast<unsigned>(input.sample_rate),
                          static_cast<unsigned>(output.sample_rate));
            return false;
        }
    } else {
        if (channel_convert_active) {
            wav_stream_.setOutput(playback_channel_converter_stream_);
        } else {
            wav_stream_.setOutput(playback_volume_stream_);
        }
    }

    if (channel_convert_active) {
        audio_tools::AudioInfo converter_input = input;
        if (playback_resampler_active_) {
            converter_input.sample_rate = output.sample_rate;
        }
        if (!playback_channel_converter_stream_.begin(converter_input, static_cast<int>(output.channels))) {
            Serial.printf("[AudioEngine] wav channel converter begin failed in_ch=%u out_ch=%u\n",
                          static_cast<unsigned>(converter_input.channels),
                          static_cast<unsigned>(output.channels));
            return false;
        }
    }

    return true;
}

bool AudioEngine::configureMp3PlaybackPipeline(const audio_tools::AudioInfo& input, const audio_tools::AudioInfo& output) {
    playback_resample_stream_.end();
    playback_channel_converter_stream_.end();
    playback_channel_converter_stream_.setOutput(playback_volume_stream_);
    playback_resample_stream_.setOutput(playback_channel_converter_stream_);
    mp3_pcm_sink_ = &playback_volume_stream_;

    const bool channel_convert_active = (output.channels != input.channels);

    if (playback_resampler_active_) {
        if (channel_convert_active) {
            playback_resample_stream_.setOutput(playback_channel_converter_stream_);
        } else {
            playback_resample_stream_.setOutput(playback_volume_stream_);
        }
        mp3_pcm_sink_ = &playback_resample_stream_;
        if (!playback_resample_stream_.begin(input, static_cast<int>(output.sample_rate))) {
            Serial.printf("[AudioEngine] mp3 resampler begin failed in_sr=%u out_sr=%u\n",
                          static_cast<unsigned>(input.sample_rate),
                          static_cast<unsigned>(output.sample_rate));
            return false;
        }
    } else {
        if (channel_convert_active) {
            mp3_pcm_sink_ = &playback_channel_converter_stream_;
        } else {
            mp3_pcm_sink_ = &playback_volume_stream_;
        }
    }

    if (channel_convert_active) {
        audio_tools::AudioInfo converter_input = input;
        if (playback_resampler_active_) {
            converter_input.sample_rate = output.sample_rate;
        }
        if (!playback_channel_converter_stream_.begin(converter_input, static_cast<int>(output.channels))) {
            Serial.printf("[AudioEngine] mp3 channel converter begin failed in_ch=%u out_ch=%u\n",
                          static_cast<unsigned>(converter_input.channels),
                          static_cast<unsigned>(output.channels));
            return false;
        }
    }

    if (mp3_output_ != nullptr) {
        static_cast<AudioToolsMp3OutputBridge*>(mp3_output_)->setSink(mp3_pcm_sink_);
    }

    return mp3_pcm_sink_ != nullptr;
}

void AudioEngine::applyPlaybackAudioInfo(const audio_tools::AudioInfo& info) {
    if (!driver_installed_) {
        return;
    }

    audio_tools::AudioInfo normalized = info;
    if (normalized.channels < 1U) {
        normalized.channels = active_playback_audio_info_.channels > 0U ? active_playback_audio_info_.channels : 1U;
    } else if (normalized.channels > 2U) {
        normalized.channels = 2U;
    }
    if (normalized.sample_rate < 8000U || normalized.sample_rate > 48000U) {
        return;
    }
    if (normalized.bits_per_sample != 16U) {
        return;
    }

    if (normalized == active_playback_audio_info_) {
        return;
    }

    Serial.printf("[AudioEngine] playback format override sr=%u ch=%u bits=%u\n",
                  static_cast<unsigned>(normalized.sample_rate),
                  static_cast<unsigned>(normalized.channels),
                  static_cast<unsigned>(normalized.bits_per_sample));

    playback_volume_stream_.setAudioInfo(normalized);
    i2s_stream_.setAudioInfo(normalized);
    active_playback_audio_info_ = normalized;
    playback_audio_info_overridden_ = (normalized != default_playback_audio_info_);
}

bool AudioEngine::decodePcmSample(const uint8_t* bytes, uint8_t bits_per_sample, int32_t& out) const {
    if (bytes == nullptr) {
        return false;
    }
    switch (bits_per_sample) {
        case 8: {
            const int32_t u = static_cast<int32_t>(bytes[0]);
            out = (u - 128) << 8;
            return true;
        }
        case 16: {
            out = static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) |
                                       (static_cast<uint16_t>(bytes[1]) << 8));
            return true;
        }
        case 24: {
            int32_t v = static_cast<int32_t>(bytes[0]) |
                        (static_cast<int32_t>(bytes[1]) << 8) |
                        (static_cast<int32_t>(bytes[2]) << 16);
            if ((v & 0x00800000) != 0) {
                v |= ~0x00FFFFFF;
            }
            out = v;
            return true;
        }
        case 32: {
            out = static_cast<int32_t>(bytes[0]) |
                  (static_cast<int32_t>(bytes[1]) << 8) |
                  (static_cast<int32_t>(bytes[2]) << 16) |
                  (static_cast<int32_t>(bytes[3]) << 24);
            return true;
        }
        default:
            break;
    }
    return false;
}

float AudioEngine::analyzeWavLoudnessGainDb(
    File& file,
    const audio_tools::AudioInfo& input,
    uint32_t data_offset,
    uint32_t data_size,
    bool& out_limiter_active) const {
    out_limiter_active = false;
    if (!wavAutoLoudnessEnabled(_config) || input.channels == 0U || data_offset == 0U || data_size == 0U) {
        return 0.0f;
    }

    const uint8_t bits = static_cast<uint8_t>(input.bits_per_sample);
    const uint8_t bytes_per_sample = static_cast<uint8_t>(bits / 8U);
    if (!(bytes_per_sample == 1U || bytes_per_sample == 2U || bytes_per_sample == 3U || bytes_per_sample == 4U)) {
        return 0.0f;
    }

    const size_t bytes_per_frame = static_cast<size_t>(bytes_per_sample) * static_cast<size_t>(input.channels);
    if (bytes_per_frame == 0U) {
        return 0.0f;
    }

    const size_t max_scan_bytes = 32768U;
    const size_t scan_bytes = std::min<size_t>(max_scan_bytes, static_cast<size_t>(data_size));
    if (scan_bytes < bytes_per_frame) {
        return 0.0f;
    }

    const size_t original_pos = file.position();
    if (!file.seek(data_offset)) {
        return 0.0f;
    }

    uint8_t buffer[1024];
    size_t total_read = 0U;
    double sum_sq = 0.0;
    float peak = 0.0f;
    uint32_t sample_count = 0U;
    const float full_scale = (bits == 8U) ? 32768.0f
                           : (bits == 16U) ? 32768.0f
                           : (bits == 24U) ? 8388608.0f
                                           : 2147483648.0f;

    while (total_read < scan_bytes) {
        const size_t remaining = scan_bytes - total_read;
        const size_t chunk = std::min<size_t>(remaining, sizeof(buffer));
        const size_t read_len = file.read(buffer, chunk);
        if (read_len == 0U) {
            break;
        }
        total_read += read_len;

        const size_t frames = read_len / bytes_per_frame;
        for (size_t frame = 0U; frame < frames; ++frame) {
            for (uint8_t ch = 0U; ch < input.channels; ++ch) {
                const uint8_t* sample_ptr =
                    &buffer[frame * bytes_per_frame + static_cast<size_t>(ch) * bytes_per_sample];
                int32_t sample = 0;
                if (!decodePcmSample(sample_ptr, bits, sample)) {
                    continue;
                }
                const float normalized = std::abs(static_cast<float>(sample) / full_scale);
                peak = std::max(peak, normalized);
                sum_sq += static_cast<double>(normalized) * static_cast<double>(normalized);
                ++sample_count;
            }
        }
    }

    file.seek(original_pos);

    if (sample_count == 0U) {
        return 0.0f;
    }

    const float rms = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(sample_count)));
    const float current_rms = std::max(rms, kMinRmsLinear);
    const float current_peak = std::max(peak, kMinRmsLinear);
    const float target_rms = dbToLinear(static_cast<float>(_config.wav_target_rms_dbfs));
    const float ceiling = dbToLinear(static_cast<float>(_config.wav_limiter_ceiling_dbfs));

    float desired_gain = target_rms / current_rms;
    const float peak_limited_gain = ceiling / current_peak;
    if (desired_gain > peak_limited_gain) {
        desired_gain = peak_limited_gain;
        out_limiter_active = true;
    }

    desired_gain = clampFloat(desired_gain, 0.125f, 4.0f);
    return linearToDb(desired_gain);
}

void AudioEngine::restorePlaybackAudioInfo() {
    if (!driver_installed_) {
        return;
    }
    if (!playback_audio_info_overridden_) {
        active_playback_audio_info_ = default_playback_audio_info_;
        return;
    }
    playback_volume_stream_.setAudioInfo(default_playback_audio_info_);
    i2s_stream_.setAudioInfo(default_playback_audio_info_);
    active_playback_audio_info_ = default_playback_audio_info_;
    playback_audio_info_overridden_ = false;
    Serial.printf("[AudioEngine] playback format restored sr=%u ch=%u bits=%u\n",
                  static_cast<unsigned>(default_playback_audio_info_.sample_rate),
                  static_cast<unsigned>(default_playback_audio_info_.channels),
                  static_cast<unsigned>(default_playback_audio_info_.bits_per_sample));
}

void AudioEngine::initAdcDspChain(uint32_t sample_rate_hz) {
    const float sr = static_cast<float>(sample_rate_hz == 0U ? kAdcDspDefaultSampleRateHz : sample_rate_hz);

    const float high_cut = std::min(sr * 0.45f - 20.0f, kDspLowPassHz);
    const float low_cut = std::min(std::max(kDspHighPassHz, 10.0f), sr * 0.45f - 100.0f);

    biquadHighPassCoeff(sr, low_cut, 0.707f, adc_dsp_biquad_hp_b0_, adc_dsp_biquad_hp_b1_, adc_dsp_biquad_hp_b2_,
                       adc_dsp_biquad_hp_a1_, adc_dsp_biquad_hp_a2_);
    biquadLowPassCoeff(sr, high_cut > 0.0f ? high_cut : 1.0f, 0.707f, adc_dsp_biquad_lp_b0_, adc_dsp_biquad_lp_b1_,
                      adc_dsp_biquad_lp_b2_, adc_dsp_biquad_lp_a1_, adc_dsp_biquad_lp_a2_);
    resetAdcDspState();
    initAdcFftDspBackend();
    adc_dsp_chain_enabled_ = true;
    Serial.printf("[AudioEngine] ADC DSP chain enabled (sr=%u, hp=%.1fHz, lp=%.1fHz)\n",
                  static_cast<unsigned>(sample_rate_hz),
                  low_cut,
                  high_cut > 0.0f ? high_cut : 1.0f);
}

void AudioEngine::initAdcFftDspBackend() {
    adc_dsp_fft_probe_backend_ready_ = false;
    if (!adc_dsp_fft_probe_enabled_ || !adc_fft_enabled_ || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    const esp_err_t ret = dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("[AudioEngine] FFT backend init failed: %d\n", ret);
        return;
    }
    adc_dsp_fft_probe_backend_ready_ = true;
}

void AudioEngine::deinitAdcFftDspBackend() {
    if (!adc_dsp_fft_probe_backend_ready_) {
        return;
    }

    dsps_fft2r_deinit_fc32();
    adc_dsp_fft_probe_backend_ready_ = false;
}

void AudioEngine::updateAdcDspConfig(const AudioConfig& cfg) {
    adc_dsp_chain_enabled_ = (use_adc_capture_ && cfg.adc_dsp_enabled);
    adc_fft_enabled_ = (adc_dsp_chain_enabled_ && cfg.adc_fft_enabled);
    adc_dsp_fft_probe_enabled_ = adc_fft_enabled_;
    adc_dsp_fft_downsample_ = cfg.adc_dsp_fft_downsample;
    if (adc_dsp_fft_downsample_ < kAdcDspMinFftDownsample) {
        adc_dsp_fft_downsample_ = kAdcDspMinFftDownsample;
    } else if (adc_dsp_fft_downsample_ > kAdcDspMaxFftDownsample) {
        adc_dsp_fft_downsample_ = kAdcDspMaxFftDownsample;
    }

    const uint16_t max_ignore_bin = static_cast<uint16_t>((kAdcDspFftWindowSamples / 2U > 0U) ? (kAdcDspFftWindowSamples / 2U - 1U) : 0U);
    adc_fft_ignore_low_bin_ = std::min<uint16_t>(cfg.adc_fft_ignore_low_bin, max_ignore_bin);
    adc_fft_ignore_high_bin_ = std::min<uint16_t>(cfg.adc_fft_ignore_high_bin, max_ignore_bin);

    if (!adc_dsp_chain_enabled_) {
        deinitAdcFftDspBackend();
        return;
    }

    initAdcDspChain(cfg.sample_rate);
}

void AudioEngine::resetAdcDspState() {
    std::memset(adc_dsp_fir_state_, 0, sizeof(adc_dsp_fir_state_));
    adc_dsp_fir_pos_ = 0U;
    adc_dsp_prev_input_ = 0.0f;
    adc_dsp_prev_output_ = 0.0f;
    adc_dsp_biquad_hp_z1_ = 0.0f;
    adc_dsp_biquad_hp_z2_ = 0.0f;
    adc_dsp_biquad_lp_z1_ = 0.0f;
    adc_dsp_biquad_lp_z2_ = 0.0f;
    std::memset(adc_dsp_fft_buffer_, 0, sizeof(adc_dsp_fft_buffer_));
    adc_dsp_fft_head_ = 0U;
    adc_dsp_fft_fill_ = 0U;
    adc_dsp_fft_decimator_ = 0U;
    metrics_.adc_fft_peak_bin = 0U;
    const uint32_t effective_downsample = std::max<uint32_t>(1U, static_cast<uint32_t>(adc_dsp_fft_downsample_));
    metrics_.adc_fft_probe_rate_hz =
        static_cast<uint16_t>(std::max(1U, _config.sample_rate / std::max(1U, effective_downsample)));
    metrics_.adc_fft_peak_freq_hz = 0.0f;
    metrics_.adc_fft_peak_magnitude = 0.0f;
}

float AudioEngine::applyDcBlocker(float sample) {
    const float filtered = sample - adc_dsp_prev_input_ + (kDspDcBlockR * adc_dsp_prev_output_);
    adc_dsp_prev_input_ = sample;
    adc_dsp_prev_output_ = filtered;
    return filtered;
}

float AudioEngine::applyFirNoiseReduction(float sample) {
    adc_dsp_fir_state_[adc_dsp_fir_pos_] = sample;

    // FIR taps: 1/16 * [1, 4, 6, 4, 1] (soft anti-alias + anti-click).
    constexpr float kFirCoeff[kAdcDspFirTaps] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};

    float result = 0.0f;
    uint8_t idx = adc_dsp_fir_pos_;
    for (size_t tap = 0U; tap < kAdcDspFirTaps; ++tap) {
        const size_t fir_idx = (idx + kAdcDspFirTaps - tap) % kAdcDspFirTaps;
        result += kFirCoeff[tap] * adc_dsp_fir_state_[fir_idx];
    }

    adc_dsp_fir_pos_ = static_cast<uint8_t>((adc_dsp_fir_pos_ + 1U) % kAdcDspFirTaps);
    return result;
}

int16_t AudioEngine::applyBiquadChain(float sample) {
    const float hp = processBiquad(sample, adc_dsp_biquad_hp_b0_, adc_dsp_biquad_hp_b1_, adc_dsp_biquad_hp_b2_,
                                  adc_dsp_biquad_hp_a1_, adc_dsp_biquad_hp_a2_, adc_dsp_biquad_hp_z1_,
                                  adc_dsp_biquad_hp_z2_);
    const float lp = processBiquad(hp, adc_dsp_biquad_lp_b0_, adc_dsp_biquad_lp_b1_, adc_dsp_biquad_lp_b2_,
                                 adc_dsp_biquad_lp_a1_, adc_dsp_biquad_lp_a2_, adc_dsp_biquad_lp_z1_,
                                 adc_dsp_biquad_lp_z2_);
    return clampInt16(clampFloat(lp * kDspPostGain * 32768.0f, -32768.0f, 32767.0f));
}

void AudioEngine::appendAdcFftSample(float sample) {
    if (!adc_dsp_fft_probe_enabled_ || adc_dsp_fft_downsample_ == 0U || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    if (++adc_dsp_fft_decimator_ < adc_dsp_fft_downsample_) {
        return;
    }
    adc_dsp_fft_decimator_ = 0U;

    adc_dsp_fft_buffer_[adc_dsp_fft_head_] = sample;
    adc_dsp_fft_head_ = static_cast<uint8_t>((adc_dsp_fft_head_ + 1U) % kAdcDspFftWindowSamples);
    if (adc_dsp_fft_fill_ < kAdcDspFftWindowSamples) {
        ++adc_dsp_fft_fill_;
        return;
    }

    runAdcFftProbe();
}

void AudioEngine::runAdcFftProbe() {
    if (!adc_dsp_fft_probe_enabled_ || adc_dsp_fft_fill_ < kAdcDspFftWindowSamples || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    const size_t kHalfBins = kAdcDspFftWindowSamples / 2U;
    if (kHalfBins < 2U) {
        return;
    }
    float best_power = 0.0f;
    uint16_t best_bin = 0U;
    const uint32_t effective_downsample = std::max<uint32_t>(1U, static_cast<uint32_t>(adc_dsp_fft_downsample_));
    const float probe_sr = static_cast<float>(std::max(1U, _config.sample_rate)) / static_cast<float>(effective_downsample);
    const uint16_t ignore_low = std::min<uint16_t>(adc_fft_ignore_low_bin_, static_cast<uint16_t>(kHalfBins - 1U));
    const uint16_t ignore_high = std::min<uint16_t>(adc_fft_ignore_high_bin_, static_cast<uint16_t>(kHalfBins));
    const uint16_t upper_limit = (ignore_high >= kHalfBins) ? 1U : static_cast<uint16_t>(kHalfBins - ignore_high);

    for (size_t i = 0U; i < kAdcDspFftWindowSamples; ++i) {
        const size_t src_idx = (adc_dsp_fft_head_ + i) % kAdcDspFftWindowSamples;
        const float phase = static_cast<float>(i) / static_cast<float>(kAdcDspFftWindowSamples - 1U);
        const float sample = adc_dsp_fft_buffer_[src_idx] * (0.5f - 0.5f * std::cos(kTwoPi * phase));
        adc_dsp_fft_complex_buffer_[i * 2U] = sample;
        adc_dsp_fft_complex_buffer_[i * 2U + 1U] = 0.0f;
    }

    if (adc_dsp_fft_probe_backend_ready_) {
        esp_err_t ret = dsps_fft2r_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_fft2r_fc32 failed: %d\n", ret);
            return;
        }
        ret = dsps_bit_rev_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_bit_rev_fc32 failed: %d\n", ret);
            return;
        }
        ret = dsps_cplx2reC_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_cplx2reC_fc32 failed: %d\n", ret);
            return;
        }
    } else {
        return;
    }

    for (size_t bin = 1U; bin < kHalfBins; ++bin) {
        if (bin <= ignore_low || bin >= upper_limit) {
            continue;
        }
        const float re = adc_dsp_fft_complex_buffer_[bin * 2U];
        const float im = adc_dsp_fft_complex_buffer_[bin * 2U + 1U];
        const float power = (re * re) + (im * im);
        if (power > best_power) {
            best_power = power;
            best_bin = static_cast<uint16_t>(bin);
        }
    }

    metrics_.adc_fft_peak_bin = best_bin;
    metrics_.adc_fft_peak_magnitude = std::sqrt(best_power);
    metrics_.adc_fft_peak_freq_hz = best_bin == 0U ? 0.0f
                                                  : (static_cast<float>(best_bin) *
                                                     (probe_sr / static_cast<float>(kAdcDspFftWindowSamples)));
}

int16_t AudioEngine::processAdcSample(int16_t raw_sample) {
    float sample = static_cast<float>(raw_sample) * kDspAdcScale;
    if (!adc_dsp_chain_enabled_) {
        return clampInt16(sample * kDspPostGain * 32768.0f);
    }

    sample = applyDcBlocker(sample);
    sample = applyFirNoiseReduction(sample);
    appendAdcFftSample(sample);
    return applyBiquadChain(sample);
}

bool AudioEngine::begin(const AudioConfig& config) {
    end();
    _config = config;
    if (kHardDisableAutoLoudnessProcessing && _config.wav_auto_normalize_limiter) {
        Serial.println("[AudioEngine] wav auto loudness requested but hard-disabled by firmware policy");
    }
    _config.wav_auto_normalize_limiter = wavAutoLoudnessEnabled(_config);
    adc_capture_pin_ = config.capture_adc_pin;
    use_adc_capture_ = (adc_capture_pin_ >= 0);
    const int max_gpio = (detectBoardProfile() == BoardProfile::ESP32_S3) ? 48 : 39;

    if (use_adc_capture_) {
        if (adc_capture_pin_ < 0 || adc_capture_pin_ > max_gpio) {
            Serial.printf("[AudioEngine] invalid ADC pin for capture: %d\n", adc_capture_pin_);
            return false;
        }

        pinMode(adc_capture_pin_, INPUT);
        analogReadResolution(12);
        analogSetPinAttenuation(adc_capture_pin_, ADC_11db);
        adc_capture_sample_interval_us_ = std::max(1U, 1000000U / _config.sample_rate);
    } else {
        adc_capture_sample_interval_us_ = 0U;
    }
    updateAdcDspConfig(config);
    next_adc_capture_us_ = 0U;

    const bool full_duplex = (_config.enable_capture && features_.has_full_duplex_i2s);
    const audio_tools::RxTxMode mode = full_duplex ? audio_tools::RXTX_MODE : audio_tools::TX_MODE;
    auto i2s_cfg = i2s_stream_.defaultConfig(mode);
    i2s_cfg.port_no = static_cast<int>(_config.port);
    i2s_cfg.sample_rate = _config.sample_rate;
    i2s_cfg.bits_per_sample = 16;
    i2s_cfg.channels = static_cast<int>(activeChannelCount(_config.channel_format));
    i2s_cfg.channel_format = _config.channel_format;
    i2s_cfg.pin_bck = _config.bck_pin;
    i2s_cfg.pin_ws = _config.ws_pin;
    i2s_cfg.pin_data = _config.data_out_pin;
    i2s_cfg.pin_data_rx = _config.data_in_pin;
    // A252/ES8388 requires MCLK on GPIO0 for reliable analog output.
    // Keep this bound to the known A252 pin mapping to avoid impacting other boards.
    if (_config.bck_pin == A1S_I2S_BCLK && _config.ws_pin == A1S_I2S_LRCK && _config.data_out_pin == A1S_I2S_DOUT) {
        i2s_cfg.pin_mck = A1S_I2S_MCLK;
        i2s_cfg.use_apll = true;
#if defined(USE_LEGACY_I2S) && USE_LEGACY_I2S
        i2s_cfg.fixed_mclk = static_cast<uint32_t>(_config.sample_rate) * 256U;
#endif
    }
    i2s_cfg.buffer_count = _config.dma_buf_count;
    i2s_cfg.buffer_size = _config.dma_buf_len;
    i2s_cfg.auto_clear = true;

    if (!i2s_stream_.begin(i2s_cfg)) {
        Serial.printf("[AudioEngine] i2s begin failed: port=%d mode=%d sr=%u bits=%d ch=%d bck=%d ws=%d dout=%d din=%d mck=%d dma_cnt=%u dma_len=%u\n",
                      static_cast<int>(i2s_cfg.port_no),
                      static_cast<int>(mode),
                      static_cast<unsigned>(i2s_cfg.sample_rate),
                      static_cast<int>(i2s_cfg.bits_per_sample),
                      static_cast<int>(i2s_cfg.channels),
                      static_cast<int>(i2s_cfg.pin_bck),
                      static_cast<int>(i2s_cfg.pin_ws),
                      static_cast<int>(i2s_cfg.pin_data),
                      static_cast<int>(i2s_cfg.pin_data_rx),
                      static_cast<int>(i2s_cfg.pin_mck),
                      static_cast<unsigned>(i2s_cfg.buffer_count),
                      static_cast<unsigned>(i2s_cfg.buffer_size));
        driver_installed_ = false;
        return false;
    }

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(kI2sReadTimeoutMs);
        i2s_stream_.driver()->setWaitTimeWriteMs(kI2sWriteTimeoutMs);
    }

    const int playback_channels = static_cast<int>(activeChannelCount(_config.channel_format));
    playback_gain_scaler_.reset(new audio_tools::ConverterScaler<int16_t>(
        kPlaybackSoftwareGain,
        0,
        std::numeric_limits<int16_t>::max(),
        playback_channels));
    playback_gain_stream_.setConverter(*playback_gain_scaler_);

    default_playback_audio_info_ = audio_tools::AudioInfo(
        _config.sample_rate,
        static_cast<uint16_t>(activeChannelCount(_config.channel_format)),
        16U);
    active_playback_audio_info_ = default_playback_audio_info_;
    playback_input_audio_info_ = default_playback_audio_info_;
    playback_audio_info_overridden_ = false;
    playback_resampler_active_ = false;
    playback_channel_upmix_active_ = false;
    playback_loudness_auto_ = false;
    playback_loudness_gain_db_ = 0.0f;
    playback_limiter_active_ = false;
    playback_rate_fallback_ = 0U;
    playback_mp3_bitrate_bps_ = 0U;
    playback_copy_source_bytes_ = 0U;
    playback_copy_accepted_bytes_ = 0U;
    playback_copy_loss_bytes_ = 0U;
    playback_copy_loss_events_ = 0U;
    playback_last_error_ = "";
    playback_data_offset_ = 0U;
    playback_data_remaining_ = 0U;
    playback_next_chunk_ms_ = 0U;

    audio_tools::VolumeStreamConfig volume_cfg = playback_volume_stream_.defaultConfig();
    volume_cfg.bits_per_sample = 16;
    volume_cfg.channels = static_cast<int>(activeChannelCount(_config.channel_format));
    volume_cfg.allow_boost = true;
    volume_cfg.volume = kPlaybackBoostLinear;
    playback_volume_stream_.begin(volume_cfg);
    Serial.printf("[AudioEngine] playback boost set to %.2fx + software %.2fx\n",
                  static_cast<double>(kPlaybackBoostLinear),
                  static_cast<double>(kPlaybackSoftwareGain));

    if (!tone_lut_ready_) {
        for (size_t i = 0; i < kToneLutSize; ++i) {
            const float phase = (kTwoPi * static_cast<float>(i)) / static_cast<float>(kToneLutSize);
            tone_lut_[i] = static_cast<int16_t>(std::sin(phase) * 32767.0f);
        }
        tone_lut_ready_ = true;
    }

    if (i2s_io_mutex_ == nullptr) {
        i2s_io_mutex_ = xSemaphoreCreateMutex();
        if (i2s_io_mutex_ == nullptr) {
            Serial.println("[AudioEngine] i2s mutex unavailable");
        }
    }
    if (playback_state_mutex_ == nullptr) {
        playback_state_mutex_ = xSemaphoreCreateMutex();
        if (playback_state_mutex_ == nullptr) {
            Serial.println("[AudioEngine] playback state mutex unavailable");
        }
    }

    driver_installed_ = true;
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = 0U;
    capture_active_ = false;
    portEXIT_CRITICAL(&capture_lock_);
    capture_active_ = false;
    playing_ = false;
    tone_route_active_ = false;
    tone_active_ = false;
    tone_profile_ = ToneProfile::NONE;
    tone_event_ = ToneEvent::NONE;
    tone_pattern_ = TonePattern{};
    tone_step_ = ToneStep{};
    tone_step_index_ = 0U;
    tone_step_remaining_frames_ = 0U;
    dial_tone_gain_ = 0.0f;
    tone_phase_a_ = 0.0f;
    tone_phase_b_ = 0.0f;
    next_dial_tone_push_ms_ = 0;
    metrics_.tone_jitter_us_max = 0U;
    metrics_.tone_write_miss_count = 0U;
    stopPlaybackFile();
    startTask();
    Serial.printf("[AudioEngine] ready (full_duplex=%s)\n",
                  supportsFullDuplex() ? "true" : "false");
    return true;
}

void AudioEngine::end() {
    if (!driver_installed_) {
        return;
    }
    deinitAdcFftDspBackend();
    stopTask();
    stopTone();
    stopPlaybackFile();
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = 0U;
    capture_active_ = false;
    portEXIT_CRITICAL(&capture_lock_);
    if (i2s_io_mutex_ != nullptr) {
        vSemaphoreDelete(i2s_io_mutex_);
        i2s_io_mutex_ = nullptr;
    }
    if (playback_state_mutex_ != nullptr) {
        vSemaphoreDelete(playback_state_mutex_);
        playback_state_mutex_ = nullptr;
    }
    i2s_stream_.end();
    playback_volume_stream_.end();
    if (mp3_output_ != nullptr) {
        delete static_cast<AudioToolsMp3OutputBridge*>(mp3_output_);
        mp3_output_ = nullptr;
    }
    driver_installed_ = false;
}

void AudioEngine::audioTaskFn(void* arg) {
    auto* self = static_cast<AudioEngine*>(arg);
    while (self != nullptr && self->running_task_) {
        self->tick();
        const bool audio_busy = self->capture_active_ || self->isToneRenderingActive() || self->playing_;
        vTaskDelay(pdMS_TO_TICKS(audio_busy ? 1U : 6U));
    }
    if (self != nullptr) {
        self->task_handle_ = nullptr;
    }
    vTaskDelete(nullptr);
}

void AudioEngine::startTask() {
    if (!driver_installed_ || task_handle_ != nullptr) {
        return;
    }
    running_task_ = true;
    const BaseType_t rc = xTaskCreatePinnedToCore(
        audioTaskFn,
        "audio_engine",
        kAudioTaskStackWords,
        this,
        kAudioTaskPriority,
        &task_handle_,
        1);
    if (rc != pdPASS) {
        running_task_ = false;
        task_handle_ = nullptr;
        Serial.println("[AudioEngine] failed to start audio task");
    }
}

void AudioEngine::stopTask() {
    if (task_handle_ == nullptr) {
        return;
    }
    running_task_ = false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

bool AudioEngine::playFile(const char* path) {
    return playFileWithPolicy(path);
}

bool AudioEngine::playFileWithPolicy(const char* path) {
    return playFileFromSource(path, MediaSource::AUTO);
}

void AudioEngine::stopPlayback() {
    stopPlaybackFile();
}

bool AudioEngine::playFileFromSource(const char* path, MediaSource source) {
    if (!driver_installed_ || path == nullptr || path[0] == '\0') {
        playback_last_error_ = "invalid_play_request";
        return false;
    }
    if (!ensureStorageForSource(source)) {
        playback_last_error_ = "storage_unavailable";
        return false;
    }

    stopTone();

    if (!lockPlaybackState(pdMS_TO_TICKS(200))) {
        playback_last_error_ = "playback_lock_timeout";
        return false;
    }
    stopPlaybackFileUnlocked();
    playback_last_error_ = "";

    fs::FS* mounted_fs = nullptr;
    MediaSource selected_source = MediaSource::AUTO;
    if (!openPlaybackFileForSource(path, source, mounted_fs, selected_source) || mounted_fs == nullptr || !playback_file_) {
        Serial.printf("[AudioEngine] playback file not found source=%s path=%s\n", mediaSourceToString(source), path);
        playback_last_error_ = "file_not_found";
        unlockPlaybackState();
        return false;
    }

    const bool use_mp3_decoder = isMp3Path(path);
    if (use_mp3_decoder) {
        if (!prepareMp3Playback(playback_file_, path)) {
            playback_last_error_ = "mp3_prepare_failed";
            stopPlaybackFileUnlocked();
            unlockPlaybackState();
            return false;
        }

        if (playback_file_) {
            playback_file_.close();
        }
        if (mp3_output_ == nullptr) {
            mp3_output_ = new AudioToolsMp3OutputBridge();
        }
        if (mp3_output_ == nullptr) {
            playback_last_error_ = "decoder_output_alloc_failed";
            stopPlaybackFileUnlocked();
            unlockPlaybackState();
            return false;
        }

        static_cast<AudioToolsMp3OutputBridge*>(mp3_output_)->setSink(mp3_pcm_sink_);
        mp3_source_ = new AudioFileSourceFS(*mounted_fs, path);
        if (mp3_source_ == nullptr || !mp3_source_->isOpen()) {
            playback_last_error_ = "source_open_failed";
            stopPlaybackFileUnlocked();
            Serial.printf("[AudioEngine] mp3 source open failed: %s\n", path);
            unlockPlaybackState();
            return false;
        }

        mp3_decoder_ = new AudioGeneratorMP3();
        auto* output_bridge = static_cast<AudioToolsMp3OutputBridge*>(mp3_output_);
        if (mp3_decoder_ == nullptr || output_bridge == nullptr || !mp3_decoder_->begin(mp3_source_, output_bridge)) {
            playback_last_error_ = "decoder_begin_failed";
            stopPlaybackFileUnlocked();
            Serial.printf("[AudioEngine] mp3 decoder begin failed: %s\n", path);
            unlockPlaybackState();
            return false;
        }

        playback_volume_stream_.setVolume(kPlaybackBoostLinear);
        mp3_source_last_pos_ = mp3_source_ != nullptr ? mp3_source_->getPos() : 0U;
        playback_codec_ = PlaybackCodec::MP3;
    } else {
        if (!prepareWavPlayback(playback_file_, path)) {
            playback_last_error_ = "wav_prepare_failed";
            stopPlaybackFileUnlocked();
            unlockPlaybackState();
            return false;
        }

        const float gain_linear = dbToLinear(playback_loudness_gain_db_);
        const float requested_volume = clampFloat(kPlaybackBoostLinear * gain_linear, 0.05f, 4.0f);
        playback_volume_stream_.setVolume(requested_volume);
        playback_wav_direct_mode_ =
            (playback_input_audio_info_.bits_per_sample == 16U) &&
            !playback_resampler_active_ &&
            !playback_channel_upmix_active_ &&
            (playback_data_offset_ > 0U);
        if (playback_wav_direct_mode_) {
            if (!playback_file_.seek(playback_data_offset_)) {
                playback_last_error_ = "wav_seek_data_failed";
                stopPlaybackFileUnlocked();
                unlockPlaybackState();
                return false;
            }
        } else {
            if (!wav_stream_.begin()) {
                playback_last_error_ = "decoder_begin_failed";
                stopPlaybackFileUnlocked();
                Serial.printf("[AudioEngine] wav decoder begin failed: %s\n", path);
                unlockPlaybackState();
                return false;
            }
            wav_copy_.begin(wav_stream_, playback_file_);
        }
        playback_codec_ = PlaybackCodec::WAV;
    }

    // Re-assert runtime volume after decoder init because stream reconfiguration
    // may reset volume state on some runs.
    playback_path_ = path;
    last_storage_path_ = path;
    last_storage_source_ = selected_source;
    playing_ = true;
    Serial.printf("[AudioEngine] play %s from %s: %s\n",
                  use_mp3_decoder ? "mp3" : "wav",
                  mediaSourceToString(selected_source),
                  path);
    unlockPlaybackState();
    return true;
}

bool AudioEngine::requestCapture(CaptureClient client) {
    if (!driver_installed_ || !_config.enable_capture) {
        return false;
    }
    const uint8_t bit = static_cast<uint8_t>(client);
    if (bit == 0U) {
        return false;
    }
    if (!supportsFullDuplex() && playing_) {
        return false;
    }

    portENTER_CRITICAL(&capture_lock_);
    const bool was_active = capture_active_;
    capture_clients_mask_ = static_cast<uint8_t>(capture_clients_mask_ | bit);
    capture_active_ = (capture_clients_mask_ != 0U);
    if (capture_active_ && !was_active && use_adc_capture_ && adc_dsp_chain_enabled_) {
        resetAdcDspState();
        next_adc_capture_us_ = 0U;
    }
    portEXIT_CRITICAL(&capture_lock_);
    return true;
}

void AudioEngine::releaseCapture(CaptureClient client) {
    const uint8_t bit = static_cast<uint8_t>(client);
    if (bit == 0U) {
        return;
    }
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = static_cast<uint8_t>(capture_clients_mask_ & static_cast<uint8_t>(~bit));
    capture_active_ = (capture_clients_mask_ != 0U);
    portEXIT_CRITICAL(&capture_lock_);
}

bool AudioEngine::startCapture() {
    return requestCapture(CAPTURE_CLIENT_GENERIC);
}

size_t AudioEngine::readCaptureFrame(int16_t* dst, size_t samples) {
    if (!capture_active_ || !driver_installed_ || !_config.enable_capture || dst == nullptr || samples == 0) {
        return 0;
    }
    if (use_adc_capture_) {
        return captureFromAdc(dst, samples, true);
    }
    if (!lockI2s()) {
        return 0;
    }

    metrics_.frames_requested += static_cast<uint32_t>(samples);
    const uint32_t start_ms = millis();
    const size_t byte_count = samples * sizeof(int16_t);
    size_t bytes_read = i2s_stream_.readBytes(reinterpret_cast<uint8_t*>(dst), byte_count);
    if (bytes_read == 0) {
        std::memset(dst, 0, byte_count);
        metrics_.underrun_count++;
        metrics_.drop_frames += static_cast<uint32_t>(samples);
        metrics_.last_latency_ms = millis() - start_ms;
        metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
        unlockI2s();
        return 0;
    }
    const size_t read_samples = bytes_read / sizeof(int16_t);
    metrics_.frames_read += static_cast<uint32_t>(read_samples);
    if (read_samples < samples) {
        metrics_.drop_frames += static_cast<uint32_t>(samples - read_samples);
    }
    metrics_.last_latency_ms = millis() - start_ms;
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
    unlockI2s();
    return read_samples;
}

size_t AudioEngine::readCaptureFrameNonBlocking(int16_t* dst, size_t samples) {
    if (!capture_active_ || !driver_installed_ || !_config.enable_capture || dst == nullptr || samples == 0) {
        return 0;
    }
    if (use_adc_capture_) {
        return captureFromAdc(dst, samples, false);
    }
    if (!lockI2s()) {
        return 0;
    }

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(0);
    }

    metrics_.frames_requested += static_cast<uint32_t>(samples);
    const size_t byte_count = samples * sizeof(int16_t);
    const size_t bytes_read = i2s_stream_.readBytes(reinterpret_cast<uint8_t*>(dst), byte_count);

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(kI2sReadTimeoutMs);
    }

    if (bytes_read == 0) {
        unlockI2s();
        return 0;
    }

    const size_t read_samples = bytes_read / sizeof(int16_t);
    metrics_.frames_read += static_cast<uint32_t>(read_samples);
    if (read_samples < samples) {
        metrics_.drop_frames += static_cast<uint32_t>(samples - read_samples);
    }
    unlockI2s();
    return read_samples;
}

size_t AudioEngine::writePlaybackFrame(const int16_t* src, size_t samples) {
    if (!driver_installed_ || src == nullptr || samples == 0) {
        return 0;
    }
    if (!lockI2s()) {
        return 0;
    }

    const size_t byte_count = samples * sizeof(int16_t);
    size_t bytes_written_total = 0U;
    const uint8_t* cursor = reinterpret_cast<const uint8_t*>(src);
    for (uint8_t retry = 0U; retry < kToneWriteRetryCount; ++retry) {
        if (bytes_written_total >= byte_count) {
            break;
        }

        const size_t bytes_remaining = byte_count - bytes_written_total;
        const size_t bytes_written = playback_volume_stream_.write(cursor + bytes_written_total, bytes_remaining);
        bytes_written_total += bytes_written;

        if (bytes_written_total >= byte_count) {
            break;
        }

        if (bytes_written == 0U) {
            taskYIELD();
        }
    }

    unlockI2s();
    if (bytes_written_total < sizeof(int16_t)) {
        return 0;
    }
    return std::min(samples, bytes_written_total / sizeof(int16_t));
}

void AudioEngine::stopCapture() {
    releaseCapture(CAPTURE_CLIENT_GENERIC);
}

size_t AudioEngine::captureFromAdc(int16_t* dst, size_t samples, bool blocking) {
    if (dst == nullptr || samples == 0) {
        return 0;
    }

    const uint32_t start_ms = millis();
    metrics_.frames_requested += static_cast<uint32_t>(samples);
    size_t captured = 0;

    if (next_adc_capture_us_ == 0U) {
        next_adc_capture_us_ = static_cast<uint64_t>(micros());
    }

    if (adc_capture_sample_interval_us_ == 0U) {
        adc_capture_sample_interval_us_ = 1000000U / std::max(1U, _config.sample_rate);
    }

    while (captured < samples) {
        const uint64_t target_us = next_adc_capture_us_;
        const uint64_t now_us = micros();
        if (!blocking && now_us < target_us) {
            break;
        }
        if (blocking && now_us < target_us) {
            delayMicroseconds(static_cast<unsigned long>(target_us - now_us));
        }

        const int raw = analogRead(adc_capture_pin_);
        const int16_t centered = static_cast<int16_t>(raw - kAdcMidScale);
        dst[captured] = processAdcSample(centered);
        ++captured;
        next_adc_capture_us_ = target_us + adc_capture_sample_interval_us_;
    }

    metrics_.frames_read += static_cast<uint32_t>(captured);
    if (captured < samples) {
        metrics_.underrun_count++;
        metrics_.drop_frames += static_cast<uint32_t>(samples - captured);
    }

    metrics_.last_latency_ms = millis() - start_ms;
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
    return captured;
}

bool AudioEngine::loadTonePattern(ToneProfile profile, ToneEvent event) {
    TonePattern resolved;
    if (!ToneCatalog::resolve(profile, event, resolved) || resolved.steps == nullptr || resolved.step_count == 0U) {
        return false;
    }

    tone_pattern_ = resolved;
    tone_step_index_ = 0U;
    tone_step_remaining_frames_ = 0U;
    tone_step_ = ToneStep{};
    tone_phase_a_ = 0.0f;
    tone_phase_b_ = 0.0f;
    return true;
}

bool AudioEngine::advanceToneStep() {
    if (!tone_route_active_ || tone_pattern_.steps == nullptr || tone_pattern_.step_count == 0U) {
        tone_step_ = ToneStep{};
        tone_step_remaining_frames_ = 0U;
        return false;
    }

    if (tone_step_index_ >= tone_pattern_.step_count) {
        if (!tone_pattern_.loop) {
            tone_step_remaining_frames_ = 0U;
            return false;
        }
        tone_step_index_ = (tone_pattern_.loop_start_index < tone_pattern_.step_count) ? tone_pattern_.loop_start_index : 0U;
    }

    tone_step_ = tone_pattern_.steps[tone_step_index_];
    const uint32_t frames =
        static_cast<uint32_t>(tone_step_.duration_ms) * static_cast<uint32_t>(std::max(1U, _config.sample_rate)) / 1000U;
    tone_step_remaining_frames_ = std::max<uint32_t>(1U, frames);
    ++tone_step_index_;
    return true;
}

int16_t AudioEngine::sampleToneWave(float& phase, uint16_t freq_hz) const {
    if (freq_hz == 0U || _config.sample_rate == 0U) {
        return 0;
    }
    if (!tone_lut_ready_) {
        return 0;
    }
    const float phase_step =
        (static_cast<float>(freq_hz) * static_cast<float>(kToneLutSize)) / static_cast<float>(std::max(1U, _config.sample_rate));
    phase += phase_step;
    while (phase >= static_cast<float>(kToneLutSize)) {
        phase -= static_cast<float>(kToneLutSize);
    }
    while (phase < 0.0f) {
        phase += static_cast<float>(kToneLutSize);
    }
    const float phase_floor = std::floor(phase);
    const int idx0 = static_cast<int>(phase_floor) & static_cast<int>(kToneLutSize - 1U);
    const int idx1 = (idx0 + 1) & static_cast<int>(kToneLutSize - 1U);
    const float frac = phase - phase_floor;
    const float s0 = static_cast<float>(tone_lut_[idx0]);
    const float s1 = static_cast<float>(tone_lut_[idx1]);
    const float interpolated = s0 + (s1 - s0) * frac;
    return clampInt16(interpolated);
}

bool AudioEngine::playTone(ToneProfile profile, ToneEvent event) {
    if (!driver_installed_) {
        return false;
    }
    if (profile == ToneProfile::NONE) {
        profile = ToneProfile::FR_FR;
    }
    if (event == ToneEvent::NONE) {
        return false;
    }

    stopPlaybackFile();
    stopTone();
    if (!loadTonePattern(profile, event)) {
        Serial.printf("[AudioEngine] unsupported tone profile=%s event=%s\n", toneProfileToString(profile), toneEventToString(event));
        return false;
    }
    tone_route_active_ = true;
    tone_active_ = true;
    tone_profile_ = profile;
    tone_event_ = event;
    if (dial_tone_gain_ <= 0.0001f) {
        tone_phase_a_ = 0.0f;
        tone_phase_b_ = 0.0f;
    }
    next_dial_tone_push_ms_ = 0U;
    ++tone_state_seq_;
    return true;
}

void AudioEngine::stopTone() {
    tone_route_active_ = false;
    tone_active_ = false;
    tone_step_remaining_frames_ = 0U;
    tone_step_ = ToneStep{};
    tone_pattern_ = TonePattern{};
    tone_step_index_ = 0U;
    tone_active_ = false;
    ++tone_state_seq_;
    // Keep the release tail active so stop stays continuous and avoids clicks.
    next_dial_tone_push_ms_ = 0U;
}

bool AudioEngine::isToneActive() const {
    return isToneRenderingActive();
}

bool AudioEngine::isToneRouteActive() const {
    return tone_route_active_;
}

bool AudioEngine::isToneRenderingActive() const {
    return tone_route_active_ || dial_tone_gain_ > 0.001f;
}

ToneProfile AudioEngine::activeToneProfile() const {
    if (!isToneRenderingActive()) {
        return ToneProfile::NONE;
    }
    return tone_profile_;
}

ToneEvent AudioEngine::activeToneEvent() const {
    if (!isToneRenderingActive()) {
        return ToneEvent::NONE;
    }
    return tone_event_;
}

bool AudioEngine::startDialTone() {
    return playTone(ToneProfile::FR_FR, ToneEvent::DIAL);
}

void AudioEngine::stopDialTone() {
    stopTone();
}

uint16_t AudioEngine::playbackInputSampleRate() const {
    return playback_input_audio_info_.sample_rate;
}

uint8_t AudioEngine::playbackInputBitsPerSample() const {
    return playback_input_audio_info_.bits_per_sample;
}

uint8_t AudioEngine::playbackInputChannels() const {
    return playback_input_audio_info_.channels;
}

uint16_t AudioEngine::playbackOutputSampleRate() const {
    return active_playback_audio_info_.sample_rate;
}

uint8_t AudioEngine::playbackOutputBitsPerSample() const {
    return active_playback_audio_info_.bits_per_sample;
}

uint8_t AudioEngine::playbackOutputChannels() const {
    return active_playback_audio_info_.channels;
}

bool AudioEngine::playbackResamplerActive() const {
    return playback_resampler_active_;
}

bool AudioEngine::playbackChannelUpmixActive() const {
    return playback_channel_upmix_active_;
}

bool AudioEngine::playbackLoudnessAuto() const {
    return playback_loudness_auto_;
}

float AudioEngine::playbackLoudnessGainDb() const {
    return playback_loudness_gain_db_;
}

bool AudioEngine::playbackLimiterActive() const {
    return playback_limiter_active_;
}

uint32_t AudioEngine::playbackRateFallback() const {
    return playback_rate_fallback_;
}

uint32_t AudioEngine::playbackCopySourceBytes() const {
    return playback_copy_source_bytes_;
}

uint32_t AudioEngine::playbackCopyAcceptedBytes() const {
    return playback_copy_accepted_bytes_;
}

uint32_t AudioEngine::playbackCopyLossBytes() const {
    return playback_copy_loss_bytes_;
}

uint32_t AudioEngine::playbackCopyLossEvents() const {
    return playback_copy_loss_events_;
}

String AudioEngine::playbackLastError() const {
    return playback_last_error_;
}

bool AudioEngine::isDialToneActive() const {
    return isToneActive() && tone_event_ == ToneEvent::DIAL;
}

uint16_t AudioEngine::playbackSampleRate() const {
    return playbackOutputSampleRate();
}

uint8_t AudioEngine::playbackBitsPerSample() const {
    return playbackOutputBitsPerSample();
}

uint8_t AudioEngine::playbackChannels() const {
    return playbackOutputChannels();
}

bool AudioEngine::playbackFormatOverridden() const {
    return playback_audio_info_overridden_;
}

uint32_t AudioEngine::toneJitterUsMax() const {
    return metrics_.tone_jitter_us_max;
}

uint32_t AudioEngine::toneWriteMissCount() const {
    return metrics_.tone_write_miss_count;
}

bool AudioEngine::supportsFullDuplex() const {
    return features_.has_full_duplex_i2s && _config.enable_capture;
}

void AudioEngine::clearToneStateIfIdle() {
    if (tone_route_active_ || dial_tone_gain_ > 0.001f) {
        return;
    }
    tone_profile_ = ToneProfile::NONE;
    tone_event_ = ToneEvent::NONE;
    tone_active_ = false;
    tone_step_ = ToneStep{};
    tone_pattern_ = TonePattern{};
    tone_step_index_ = 0U;
    tone_step_remaining_frames_ = 0U;
    tone_phase_a_ = 0.0f;
    tone_phase_b_ = 0.0f;
}

bool AudioEngine::isPlaying() const {
    return playing_;
}

bool AudioEngine::isSdReady() const {
    return sd_ready_;
}

bool AudioEngine::isLittleFsReady() const {
    return littlefs_ready_;
}

bool AudioEngine::isReady() const {
    return driver_installed_;
}

MediaSource AudioEngine::lastStorageSource() const {
    return last_storage_source_;
}

String AudioEngine::lastStoragePath() const {
    return last_storage_path_;
}

AudioRuntimeMetrics AudioEngine::metrics() const {
    return metrics_;
}

void AudioEngine::resetMetrics() {
    metrics_ = AudioRuntimeMetrics{};
    playback_copy_source_bytes_ = 0U;
    playback_copy_accepted_bytes_ = 0U;
    playback_copy_loss_bytes_ = 0U;
    playback_copy_loss_events_ = 0U;
    playback_last_error_ = "";
    playback_next_chunk_ms_ = 0U;
}

bool AudioEngine::probePlaybackFileFromSource(const char* path, MediaSource source, AudioPlaybackProbeResult& out) {
    out = AudioPlaybackProbeResult{};
    out.path = (path == nullptr) ? "" : String(path);
    out.source = source;

    if (playing_) {
        out.error = "playback_busy";
        return false;
    }
    if (path == nullptr || path[0] == '\0') {
        out.error = "invalid_path";
        return false;
    }
    if (!ensureStorageForSource(source)) {
        out.error = "storage_unavailable";
        return false;
    }

    fs::FS* mounted_fs = nullptr;
    MediaSource selected_source = MediaSource::AUTO;
    if (!openPlaybackFileForSource(path, source, mounted_fs, selected_source) || mounted_fs == nullptr || !playback_file_) {
        out.error = "file_not_found";
        return false;
    }

    if (isMp3Path(path)) {
        audio_tools::AudioInfo mp3_info{};
        uint32_t bitrate = 0U;
        const uint32_t file_size_bytes = static_cast<uint32_t>(playback_file_.size());
        if (!readMp3HeaderInfo(playback_file_, mp3_info, &bitrate) || !isPlaybackAudioInfoSupported(mp3_info)) {
            mp3_info = default_playback_audio_info_;
            bitrate = 0U;
        }
        playback_file_.close();
        const uint32_t runtime_rate_fallback = playback_rate_fallback_;
        const audio_tools::AudioInfo output_info = resolvePlaybackFormat(mp3_info);
        const uint32_t fallback_rate_hz = playback_rate_fallback_;
        playback_rate_fallback_ = runtime_rate_fallback;

        out.ok = true;
        out.source = selected_source;
        out.input_sample_rate = mp3_info.sample_rate;
        out.input_bits_per_sample = static_cast<uint8_t>(mp3_info.bits_per_sample);
        out.input_channels = static_cast<uint8_t>(mp3_info.channels);
        out.output_sample_rate = output_info.sample_rate;
        out.output_bits_per_sample = static_cast<uint8_t>(output_info.bits_per_sample);
        out.output_channels = static_cast<uint8_t>(output_info.channels);
        out.resampler_active = (output_info.sample_rate != mp3_info.sample_rate);
        out.channel_upmix_active = (mp3_info.channels == 1U && output_info.channels == 2U);
        out.loudness_auto = false;
        out.loudness_gain_db = 0.0f;
        out.limiter_active = false;
        out.rate_fallback = fallback_rate_hz;
        out.data_size_bytes = file_size_bytes;
        out.duration_ms = 0U;
        if (bitrate > 0U && out.data_size_bytes > 0U) {
            const uint64_t duration_ms =
                (static_cast<uint64_t>(out.data_size_bytes) * 8ULL * 1000ULL) / static_cast<uint64_t>(bitrate);
            out.duration_ms = duration_ms > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
                                  ? std::numeric_limits<uint32_t>::max()
                                  : static_cast<uint32_t>(duration_ms);
        }
        return true;
    }

    audio_tools::AudioInfo wav_info{};
    uint32_t data_offset = 0U;
    uint32_t data_size = 0U;
    const bool parsed = readWavHeaderInfo(playback_file_, wav_info, &data_offset, &data_size);
    if (!parsed) {
        playback_file_.close();
        out.error = "wav_header_parse_failed";
        return false;
    }
    if (!isPlaybackAudioInfoSupported(wav_info)) {
        playback_file_.close();
        out.error = "unsupported_wav_format";
        return false;
    }

    const uint32_t runtime_rate_fallback = playback_rate_fallback_;
    const audio_tools::AudioInfo output_info = resolvePlaybackFormat(wav_info);
    const uint32_t fallback_rate_hz = playback_rate_fallback_;
    playback_rate_fallback_ = runtime_rate_fallback;

    bool limiter_active = false;
    float gain_db = 0.0f;
    const bool loudness_auto = wavAutoLoudnessEnabled(_config);
    if (loudness_auto) {
        gain_db = analyzeWavLoudnessGainDb(playback_file_, wav_info, data_offset, data_size, limiter_active);
    }
    playback_file_.close();

    out.ok = true;
    out.source = selected_source;
    out.input_sample_rate = wav_info.sample_rate;
    out.input_bits_per_sample = static_cast<uint8_t>(wav_info.bits_per_sample);
    out.input_channels = static_cast<uint8_t>(wav_info.channels);
    out.output_sample_rate = output_info.sample_rate;
    out.output_bits_per_sample = static_cast<uint8_t>(output_info.bits_per_sample);
    out.output_channels = static_cast<uint8_t>(output_info.channels);
    out.resampler_active = (output_info.sample_rate != wav_info.sample_rate);
    out.channel_upmix_active = (wav_info.channels == 1U && output_info.channels == 2U);
    out.loudness_auto = loudness_auto;
    out.loudness_gain_db = gain_db;
    out.limiter_active = limiter_active;
    out.rate_fallback = fallback_rate_hz;
    out.data_size_bytes = data_size;
    out.duration_ms = 0U;
    const uint32_t bytes_per_sample = static_cast<uint32_t>(wav_info.bits_per_sample / 8U);
    const uint32_t bytes_per_frame = bytes_per_sample * static_cast<uint32_t>(wav_info.channels);
    if (bytes_per_frame > 0U && wav_info.sample_rate > 0U && data_size > 0U) {
        const uint64_t frames = static_cast<uint64_t>(data_size / bytes_per_frame);
        const uint64_t duration_ms = (frames * 1000ULL) / static_cast<uint64_t>(wav_info.sample_rate);
        out.duration_ms = duration_ms > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
                              ? std::numeric_limits<uint32_t>::max()
                              : static_cast<uint32_t>(duration_ms);
    }
    return true;
}

void AudioEngine::updateToneJitter(uint32_t now_ms) {
    if (next_dial_tone_push_ms_ == 0U || now_ms <= next_dial_tone_push_ms_) {
        return;
    }
    const uint32_t late_ms = now_ms - next_dial_tone_push_ms_;
    const uint32_t late_us = late_ms * 1000U;
    metrics_.tone_jitter_us_max = std::max(metrics_.tone_jitter_us_max, late_us);
}

bool AudioEngine::streamPlaybackChunk() {
    if (!lockPlaybackState(0)) {
        return true;
    }

    if (playback_codec_ == PlaybackCodec::MP3) {
        if (mp3_decoder_ == nullptr || mp3_source_ == nullptr) {
            stopPlaybackFileUnlocked();
            unlockPlaybackState();
            return false;
        }
    } else if (!playback_file_) {
        stopPlaybackFileUnlocked();
        unlockPlaybackState();
        return false;
    }

    const uint32_t now_ms = millis();
    if (playback_next_chunk_ms_ != 0U &&
        static_cast<int32_t>(now_ms - playback_next_chunk_ms_) < 0) {
        unlockPlaybackState();
        return true;
    }

    size_t total_source_advanced = 0U;
    size_t total_copied = 0U;
    if (playback_codec_ == PlaybackCodec::MP3) {
        const uint32_t pos_before = mp3_source_->getPos();
        const bool decoder_running = mp3_decoder_->loop();
        const uint32_t pos_after = mp3_source_->getPos();
        total_source_advanced = (pos_after >= pos_before) ? (pos_after - pos_before) : 0U;
        total_copied = total_source_advanced;
        mp3_source_last_pos_ = pos_after;
        if (!decoder_running || !mp3_decoder_->isRunning()) {
            stopPlaybackFileUnlocked();
            unlockPlaybackState();
            return false;
        }
    } else {
        if (playback_wav_direct_mode_) {
            uint8_t pcm_buf[kPlaybackCopyBytes];
            size_t wanted = kPlaybackCopyBytes;
            if (playback_data_remaining_ > 0U) {
                wanted = std::min<size_t>(wanted, playback_data_remaining_);
            }
            const size_t align = sizeof(int16_t);
            wanted = (wanted / align) * align;
            if (wanted == 0U) {
                stopPlaybackFileUnlocked();
                unlockPlaybackState();
                return false;
            }

            const size_t pos_before = playback_file_.position();
            const size_t bytes_read = playback_file_.read(pcm_buf, wanted);
            if (bytes_read == 0U) {
                stopPlaybackFileUnlocked();
                unlockPlaybackState();
                return false;
            }

            const size_t aligned_read = (bytes_read / align) * align;
            const size_t sample_count = aligned_read / sizeof(int16_t);
            const size_t samples_written =
                writePlaybackFrame(reinterpret_cast<const int16_t*>(pcm_buf), sample_count);
            total_copied = samples_written * sizeof(int16_t);
            total_source_advanced = total_copied;

            if (total_copied < aligned_read) {
                const size_t rewind = aligned_read - total_copied;
                if (rewind > 0U) {
                    const size_t safe_pos = playback_file_.position();
                    if (safe_pos >= rewind) {
                        playback_file_.seek(safe_pos - rewind);
                    }
                }
            }

            if (playback_data_remaining_ > 0U) {
                const uint32_t consumed =
                    static_cast<uint32_t>(std::min<size_t>(total_source_advanced, playback_data_remaining_));
                playback_data_remaining_ -= consumed;
                if (playback_data_remaining_ == 0U) {
                    stopPlaybackFileUnlocked();
                    unlockPlaybackState();
                    return false;
                }
            } else {
                const size_t pos_after = playback_file_.position();
                if (pos_after <= pos_before || !playback_file_.available()) {
                    stopPlaybackFileUnlocked();
                    unlockPlaybackState();
                    return false;
                }
            }
        } else {
            const size_t copy_bytes = kPlaybackCopyBytes;
            const size_t pos_before = playback_file_.position();
            total_copied = wav_copy_.copyBytes(copy_bytes);
            const size_t pos_after = playback_file_.position();
            total_source_advanced = (pos_after >= pos_before) ? (pos_after - pos_before) : 0U;
        }
    }

    const size_t progress_bytes = (total_source_advanced > 0U) ? total_source_advanced : total_copied;
    if (progress_bytes > 0U) {
        playback_copy_source_bytes_ = saturatingAddU32(playback_copy_source_bytes_, progress_bytes);
        playback_copy_accepted_bytes_ = saturatingAddU32(playback_copy_accepted_bytes_, progress_bytes);
    }

    if (progress_bytes > 0U) {
        uint32_t next_delay_ms = 1U;
        if (playback_codec_ == PlaybackCodec::MP3 && playback_mp3_bitrate_bps_ > 0U) {
            const uint64_t bits = static_cast<uint64_t>(progress_bytes) * 8ULL;
            const uint64_t chunk_ms_u64 =
                std::max<uint64_t>(1ULL, (bits * 1000ULL) / static_cast<uint64_t>(playback_mp3_bitrate_bps_));
            const uint32_t chunk_ms =
                (chunk_ms_u64 > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
                    ? std::numeric_limits<uint32_t>::max()
                    : static_cast<uint32_t>(chunk_ms_u64);
            next_delay_ms = std::max<uint32_t>(1U, chunk_ms / 2U);
        } else {
            const uint32_t bytes_per_sample = std::max<uint32_t>(1U, playback_input_audio_info_.bits_per_sample / 8U);
            const uint32_t channels = std::max<uint32_t>(1U, playback_input_audio_info_.channels);
            const uint32_t bytes_per_frame = bytes_per_sample * channels;
            const uint32_t input_rate = std::max<uint32_t>(1U, playback_input_audio_info_.sample_rate);
            if (bytes_per_frame > 0U) {
                const uint32_t frames = static_cast<uint32_t>(progress_bytes / bytes_per_frame);
                if (frames > 0U) {
                    const uint32_t chunk_ms = std::max<uint32_t>(1U, (frames * 1000U) / input_rate);
                    // WAV flow is already light; pacing at real-time avoids saturating I2S buffers.
                    next_delay_ms = chunk_ms;
                }
            }
        }
        playback_next_chunk_ms_ = now_ms + next_delay_ms;
        unlockPlaybackState();
        return true;
    }

    if (playback_codec_ == PlaybackCodec::MP3) {
        playback_next_chunk_ms_ = now_ms + 1U;
        unlockPlaybackState();
        return true;
    }

    if (!playback_file_.available()) {
        stopPlaybackFileUnlocked();
        unlockPlaybackState();
        return false;
    }

    playback_next_chunk_ms_ = now_ms + 1U;
    unlockPlaybackState();
    return true;
}

void AudioEngine::tick() {
    if (!driver_installed_) {
        return;
    }

    if (playing_) {
        if (streamPlaybackChunk()) {
            return;
        }
    }

    const bool tone_tail_active = dial_tone_gain_ > 0.0005f;
    if (!tone_route_active_ && !tone_tail_active) {
        clearToneStateIfIdle();
        return;
    }

    const uint32_t now = millis();
    const uint32_t tick_state_seq = tone_state_seq_;
    const uint8_t next_step_index_snapshot = tone_step_index_;
    const uint32_t next_step_frames_snapshot = tone_step_remaining_frames_;
    const ToneStep next_step_snapshot = tone_step_;
    const bool next_route_snapshot = tone_route_active_;
    const float next_gain_snapshot = dial_tone_gain_;
    const float next_phase_a_snapshot = tone_phase_a_;
    const float next_phase_b_snapshot = tone_phase_b_;
    const TonePattern next_pattern_snapshot = tone_pattern_;
    const ToneProfile next_profile_snapshot = tone_profile_;
    const ToneEvent next_event_snapshot = tone_event_;

    if (next_dial_tone_push_ms_ != 0 && now < next_dial_tone_push_ms_) {
        return;
    }
    updateToneJitter(now);

    bool local_route_active = tone_route_active_;
    uint8_t local_step_index = tone_step_index_;
    uint32_t local_step_remaining_frames = tone_step_remaining_frames_;
    ToneStep local_step = tone_step_;
    float local_gain = dial_tone_gain_;
    float local_phase_a = tone_phase_a_;
    float local_phase_b = tone_phase_b_;
    TonePattern local_pattern = tone_pattern_;
    ToneProfile local_profile = tone_profile_;
    ToneEvent local_event = tone_event_;

    const auto advanceStepLocal = [&]() -> bool {
        if (!local_route_active || local_pattern.steps == nullptr || local_pattern.step_count == 0U) {
            local_step = ToneStep{};
            local_step_remaining_frames = 0U;
            return false;
        }

        if (local_step_index >= local_pattern.step_count) {
            if (!local_pattern.loop) {
                local_step_remaining_frames = 0U;
                return false;
            }
            local_step_index = (local_pattern.loop_start_index < local_pattern.step_count) ? local_pattern.loop_start_index : 0U;
        }

        local_step = local_pattern.steps[local_step_index];
        const uint32_t frames =
            static_cast<uint32_t>(local_step.duration_ms) * static_cast<uint32_t>(std::max(1U, _config.sample_rate)) / 1000U;
        local_step_remaining_frames = std::max<uint32_t>(1U, frames);
        ++local_step_index;
        return true;
    };

    const size_t channels = activeChannelCount(_config.channel_format);
    if (channels == 0U || channels > kMaxChannels) {
        return;
    }

    const size_t requested_frames = kDialToneChunkFrames;
    const uint32_t chunk_ms = std::max(1U, static_cast<uint32_t>((1000U * requested_frames) / std::max(1U, _config.sample_rate)));
    const size_t requested_samples = requested_frames * channels;
    int16_t frame[kDialToneChunkFrames * kMaxChannels] = {0};

    const float attack_step =
        1.0f / std::max(1.0f, (static_cast<float>(_config.sample_rate) * (kDialToneAttackMs / 1000.0f)));
    const float release_step =
        1.0f / std::max(1.0f, (static_cast<float>(_config.sample_rate) * (kDialToneReleaseMs / 1000.0f)));

    const uint32_t push_origin_ms = (next_dial_tone_push_ms_ == 0U) ? now : next_dial_tone_push_ms_;
    uint8_t chunks_to_render = 1U;
    if (next_dial_tone_push_ms_ != 0U && now >= next_dial_tone_push_ms_ && chunk_ms > 0U) {
        const uint32_t late_ms = now - next_dial_tone_push_ms_;
        const uint32_t required_chunks = (late_ms / chunk_ms) + 1U;
        if (required_chunks > static_cast<uint32_t>(kToneCatchupChunksPerTick)) {
            chunks_to_render = kToneCatchupChunksPerTick;
        } else if (required_chunks > 0U) {
            chunks_to_render = static_cast<uint8_t>(required_chunks);
        }
    }

    bool wrote_any_chunk = false;
    for (uint8_t chunk_index = 0U; chunk_index < chunks_to_render; ++chunk_index) {
        for (size_t i = 0; i < kDialToneChunkFrames; ++i) {
            if (local_route_active) {
                local_gain = std::min(1.0f, local_gain + attack_step);
            } else {
                local_gain = std::max(0.0f, local_gain - release_step);
            }

            if (local_route_active && local_step_remaining_frames == 0U && !advanceStepLocal()) {
                local_route_active = false;
            }

            int16_t sample = 0;
            const bool tone_rendering = local_route_active || local_gain > 0.0005f;
            if (tone_rendering && !local_step.silence) {
                const int16_t sample_a = sampleToneWave(local_phase_a, local_step.freq_a_hz);
                const int16_t sample_b = sampleToneWave(local_phase_b, local_step.freq_b_hz);
                int32_t mix = static_cast<int32_t>(sample_a);
                if (local_step.freq_b_hz > 0U) {
                    mix += static_cast<int32_t>(sample_b);
                    mix /= 2;
                }
                sample = clampInt16(static_cast<float>(mix) * (static_cast<float>(kToneAmplitude) / 32767.0f));
            }

            if (local_route_active && local_step_remaining_frames > 0U) {
                --local_step_remaining_frames;
            }

            const int16_t out = clampInt16(static_cast<float>(sample) * local_gain * kToneLinearGain);
            for (size_t ch = 0; ch < channels; ++ch) {
                frame[i * channels + ch] = out;
            }
        }

        const size_t written_samples = writePlaybackFrame(frame, requested_samples);
        if (written_samples == 0U) {
            metrics_.tone_write_miss_count++;
            if (tick_state_seq == tone_state_seq_ && !wrote_any_chunk) {
                tone_step_index_ = next_step_index_snapshot;
                tone_step_remaining_frames_ = next_step_frames_snapshot;
                tone_step_ = next_step_snapshot;
                dial_tone_gain_ = next_gain_snapshot;
                tone_phase_a_ = next_phase_a_snapshot;
                tone_phase_b_ = next_phase_b_snapshot;
                tone_pattern_ = next_pattern_snapshot;
                tone_profile_ = next_profile_snapshot;
                tone_event_ = next_event_snapshot;
                tone_route_active_ = next_route_snapshot;
                tone_active_ = next_route_snapshot || (next_gain_snapshot > 0.0001f);
            }
            next_dial_tone_push_ms_ = now + 1U;
            return;
        }
        if (written_samples < requested_samples) {
            metrics_.tone_write_miss_count++;
        }

        if (tick_state_seq != tone_state_seq_) {
            return;
        }

        tone_route_active_ = local_route_active;
        tone_step_index_ = local_step_index;
        tone_step_remaining_frames_ = local_step_remaining_frames;
        tone_step_ = local_step;
        dial_tone_gain_ = local_gain;
        tone_phase_a_ = local_phase_a;
        tone_phase_b_ = local_phase_b;
        tone_pattern_ = local_pattern;
        tone_profile_ = local_profile;
        tone_event_ = local_event;
        tone_active_ = local_route_active || local_gain > 0.0001f;
        wrote_any_chunk = true;
    }

    if (tick_state_seq != tone_state_seq_ || !wrote_any_chunk) {
        return;
    }

    next_dial_tone_push_ms_ = push_origin_ms + (chunk_ms * static_cast<uint32_t>(chunks_to_render));
}

const AudioConfig& AudioEngine::config() const {
    return _config;
}
