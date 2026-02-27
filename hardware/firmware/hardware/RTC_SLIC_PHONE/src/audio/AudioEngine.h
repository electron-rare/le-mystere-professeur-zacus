#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <FS.h>
#include <driver/i2s.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "core/PlatformProfile.h"
#include "audio/ToneCatalog.h"
#include "media/MediaRouting.h"

struct AudioConfig {
    i2s_port_t port = I2S_NUM_0;
    uint32_t sample_rate = 16000;
    i2s_bits_per_sample_t bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_channel_fmt_t channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    int bck_pin = 27;
    int ws_pin = 25;
    int data_out_pin = 26;
    int data_in_pin = 35;
    int capture_adc_pin = -1;
    bool enable_capture = true;
    bool adc_dsp_enabled = true;
    bool adc_fft_enabled = true;
    uint8_t adc_dsp_fft_downsample = 2U;
    uint16_t adc_fft_ignore_low_bin = 1U;
    uint16_t adc_fft_ignore_high_bin = 1U;
    uint8_t dma_buf_count = 8;
    uint16_t dma_buf_len = 256;
    bool hybrid_telco_clock_policy = true;
    bool wav_auto_normalize_limiter = true;
    int16_t wav_target_rms_dbfs = -18;
    int16_t wav_limiter_ceiling_dbfs = -2;
    uint16_t wav_limiter_attack_ms = 8;
    uint16_t wav_limiter_release_ms = 120;
};

struct AudioRuntimeMetrics {
    uint32_t frames_requested = 0;
    uint32_t frames_read = 0;
    uint32_t drop_frames = 0;
    uint32_t underrun_count = 0;
    uint32_t last_latency_ms = 0;
    uint32_t max_latency_ms = 0;
    uint16_t adc_fft_peak_bin = 0;
    uint16_t adc_fft_probe_rate_hz = 0;
    float adc_fft_peak_freq_hz = 0.0f;
    float adc_fft_peak_magnitude = 0.0f;
    uint32_t tone_jitter_us_max = 0;
    uint32_t tone_write_miss_count = 0;
};

struct AudioPlaybackProbeResult {
    bool ok = false;
    String error;
    String path;
    MediaSource source = MediaSource::AUTO;
    uint32_t input_sample_rate = 0;
    uint8_t input_bits_per_sample = 0;
    uint8_t input_channels = 0;
    uint32_t output_sample_rate = 0;
    uint8_t output_bits_per_sample = 0;
    uint8_t output_channels = 0;
    bool resampler_active = false;
    bool channel_upmix_active = false;
    bool loudness_auto = false;
    float loudness_gain_db = 0.0f;
    bool limiter_active = false;
    uint32_t rate_fallback = 0;
    uint32_t data_size_bytes = 0;
    uint32_t duration_ms = 0;
};

AudioConfig defaultAudioConfigForProfile(BoardProfile profile);

class AudioEngine {
public:
    enum CaptureClient : uint8_t {
        CAPTURE_CLIENT_GENERIC = 0x01,
        CAPTURE_CLIENT_TELEPHONY = 0x02,
        CAPTURE_CLIENT_BLUETOOTH = 0x04,
    };

    virtual ~AudioEngine();
    AudioEngine();
    virtual bool begin(const AudioConfig& config);
    virtual void end();
    virtual bool playFile(const char* path);
    virtual bool playFileFromSource(const char* path, MediaSource source);
    virtual bool playFileWithPolicy(const char* path);
    virtual void stopPlayback();
    virtual bool requestCapture(CaptureClient client);
    virtual void releaseCapture(CaptureClient client);
    virtual bool startCapture();
    virtual size_t readCaptureFrame(int16_t* dst, size_t samples);
    virtual size_t readCaptureFrameNonBlocking(int16_t* dst, size_t samples);
    virtual size_t writePlaybackFrame(const int16_t* src, size_t samples);
    virtual void stopCapture();
    virtual bool playTone(ToneProfile profile, ToneEvent event);
    virtual void stopTone();
    virtual bool isToneActive() const;
    virtual bool isToneRouteActive() const;
    virtual bool isToneRenderingActive() const;
    virtual ToneProfile activeToneProfile() const;
    virtual ToneEvent activeToneEvent() const;
    virtual bool startDialTone();
    virtual void stopDialTone();
    virtual uint16_t playbackInputSampleRate() const;
    virtual uint8_t playbackInputBitsPerSample() const;
    virtual uint8_t playbackInputChannels() const;
    virtual uint16_t playbackOutputSampleRate() const;
    virtual uint8_t playbackOutputBitsPerSample() const;
    virtual uint8_t playbackOutputChannels() const;
    virtual bool playbackResamplerActive() const;
    virtual bool playbackChannelUpmixActive() const;
    virtual bool playbackLoudnessAuto() const;
    virtual float playbackLoudnessGainDb() const;
    virtual bool playbackLimiterActive() const;
    virtual uint32_t playbackRateFallback() const;
    virtual uint32_t playbackCopySourceBytes() const;
    virtual uint32_t playbackCopyAcceptedBytes() const;
    virtual uint32_t playbackCopyLossBytes() const;
    virtual uint32_t playbackCopyLossEvents() const;
    virtual String playbackLastError() const;
    virtual uint16_t playbackSampleRate() const;
    virtual uint8_t playbackBitsPerSample() const;
    virtual uint8_t playbackChannels() const;
    virtual bool playbackFormatOverridden() const;
    virtual uint32_t toneJitterUsMax() const;
    virtual uint32_t toneWriteMissCount() const;
    virtual bool isDialToneActive() const;
    virtual bool supportsFullDuplex() const;
    virtual bool isPlaying() const;
    virtual bool isSdReady() const;
    virtual bool isLittleFsReady() const;
    virtual bool isReady() const;
    virtual MediaSource lastStorageSource() const;
    virtual String lastStoragePath() const;
    virtual AudioRuntimeMetrics metrics() const;
    virtual void resetMetrics();
    virtual bool probePlaybackFileFromSource(const char* path, MediaSource source, AudioPlaybackProbeResult& out);
    virtual void tick();
    const AudioConfig& config() const;

private:
    class BlockingOutput : public Print {
    public:
        void setOutput(Print* out);
        size_t write(uint8_t b) override;
        size_t write(const uint8_t* data, size_t len) override;
        int availableForWrite() override;

    private:
        Print* out_ = nullptr;
    };

    static size_t activeChannelCount(i2s_channel_fmt_t channel_format);
    static void audioTaskFn(void* arg);
    size_t captureFromAdc(int16_t* dst, size_t samples, bool blocking);
    void initAdcDspChain(uint32_t sample_rate_hz);
    int16_t processAdcSample(int16_t raw_sample);
    void resetAdcDspState();
    float applyDcBlocker(float sample);
    float applyFirNoiseReduction(float sample);
    int16_t applyBiquadChain(float sample);
    void appendAdcFftSample(float sample);
    void runAdcFftProbe();
    void initAdcFftDspBackend();
    void deinitAdcFftDspBackend();
    void startTask();
    void stopTask();
    bool lockI2s() const;
    void unlockI2s() const;
    bool ensureSdMounted();
    bool ensureLittleFsMounted();
    bool ensureStorageForSource(MediaSource source);
    bool openPlaybackFileForSource(const char* path, MediaSource source, fs::FS*& out_fs, MediaSource& out_source);
    void stopPlaybackFile();
    bool prepareWavPlayback(File& file, const char* path);
    bool prepareMp3Playback(File& file, const char* path);
    bool isMp3Path(const char* path) const;
    bool readMp3HeaderInfo(File& file, audio_tools::AudioInfo& info, uint32_t* out_bitrate = nullptr) const;
    bool readWavHeaderInfo(
        File& file,
        audio_tools::AudioInfo& info,
        uint32_t* out_data_offset = nullptr,
        uint32_t* out_data_size = nullptr) const;
    bool isPlaybackAudioInfoSupported(const audio_tools::AudioInfo& info) const;
    audio_tools::AudioInfo resolvePlaybackFormat(const audio_tools::AudioInfo& input);
    uint32_t resolveStableSampleRate(uint32_t requested_rate_hz, uint32_t& fallback_rate_hz) const;
    void applyPlaybackAudioInfo(const audio_tools::AudioInfo& info);
    float analyzeWavLoudnessGainDb(
        File& file,
        const audio_tools::AudioInfo& input,
        uint32_t data_offset,
        uint32_t data_size,
        bool& out_limiter_active) const;
    bool decodePcmSample(const uint8_t* bytes, uint8_t bits_per_sample, int32_t& out) const;
    void updateToneJitter(uint32_t now_ms);
    void restorePlaybackAudioInfo();
    bool streamPlaybackChunk();
    bool advanceToneStep();
    bool configureWavPlaybackPipeline(const audio_tools::AudioInfo& input, const audio_tools::AudioInfo& output);
    bool configureMp3PlaybackPipeline(const audio_tools::AudioInfo& input, const audio_tools::AudioInfo& output);
    bool loadTonePattern(ToneProfile profile, ToneEvent event);
    int16_t sampleToneWave(float& phase, uint16_t freq_hz) const;
    void updateAdcDspConfig(const AudioConfig& cfg);
    void clearToneStateIfIdle();

    bool driver_installed_ = false;
    bool capture_active_ = false;
    uint8_t capture_clients_mask_ = 0;
    bool playing_ = false;
    bool tone_active_ = false;
    bool tone_route_active_ = false;
    uint32_t tone_state_seq_ = 0U;
    ToneProfile tone_profile_ = ToneProfile::NONE;
    ToneEvent tone_event_ = ToneEvent::NONE;
    TonePattern tone_pattern_;
    ToneStep tone_step_;
    uint8_t tone_step_index_ = 0U;
    uint32_t tone_step_remaining_frames_ = 0U;
    float tone_phase_a_ = 0.0f;
    float tone_phase_b_ = 0.0f;
    volatile bool running_task_ = false;
    float dial_tone_gain_ = 0.0f;
    uint32_t next_dial_tone_push_ms_ = 0;
    static constexpr size_t kToneLutSize = 1024U;
    bool tone_lut_ready_ = false;
    int16_t tone_lut_[kToneLutSize] = {0};
    bool sd_mount_attempted_ = false;
    bool sd_ready_ = false;
    bool littlefs_mount_attempted_ = false;
    bool littlefs_ready_ = false;
    MediaSource last_storage_source_ = MediaSource::AUTO;
    enum class PlaybackCodec : uint8_t {
        NONE = 0,
        WAV,
        MP3,
    };
    PlaybackCodec playback_codec_ = PlaybackCodec::NONE;
    String last_storage_path_;
    File playback_file_;
    String playback_path_;
    uint32_t playback_data_remaining_ = 0;
    uint16_t playback_input_channels_ = 0;
    bool playback_audio_info_overridden_ = false;
    uint32_t playback_data_offset_ = 0;
    audio_tools::AudioInfo playback_input_audio_info_;
    audio_tools::AudioInfo default_playback_audio_info_;
    audio_tools::AudioInfo active_playback_audio_info_;
    bool playback_resampler_active_ = false;
    bool playback_channel_upmix_active_ = false;
    bool playback_loudness_auto_ = false;
    float playback_loudness_gain_db_ = 0.0f;
    bool playback_limiter_active_ = false;
    uint32_t playback_rate_fallback_ = 0;
    uint32_t playback_copy_source_bytes_ = 0U;
    uint32_t playback_copy_accepted_bytes_ = 0U;
    uint32_t playback_copy_loss_bytes_ = 0U;
    uint32_t playback_copy_loss_events_ = 0U;
    String playback_last_error_;
    uint32_t playback_next_chunk_ms_ = 0U;
    AudioConfig _config;
    FeatureMatrix features_;
    AudioRuntimeMetrics metrics_;
    int adc_capture_pin_ = -1;
    uint32_t adc_capture_sample_interval_us_ = 0;
    uint64_t next_adc_capture_us_ = 0;
    bool use_adc_capture_ = false;
    bool adc_dsp_chain_enabled_ = false;
    bool adc_fft_enabled_ = false;
    uint8_t adc_dsp_fft_downsample_ = kAdcDspDefaultFftDownsample;
    uint16_t adc_fft_ignore_low_bin_ = 1U;
    uint16_t adc_fft_ignore_high_bin_ = 1U;
    static constexpr uint32_t kAdcDspDefaultSampleRateHz = 16000U;
    static constexpr uint8_t kAdcDspDefaultFftDownsample = 2U;
    float adc_dsp_prev_input_ = 0.0f;
    float adc_dsp_prev_output_ = 0.0f;
    float adc_dsp_fir_state_[5U] = {0.0f};
    uint8_t adc_dsp_fir_pos_ = 0U;
    float adc_dsp_biquad_hp_b0_ = 1.0f;
    float adc_dsp_biquad_hp_b1_ = 0.0f;
    float adc_dsp_biquad_hp_b2_ = 0.0f;
    float adc_dsp_biquad_hp_a1_ = 0.0f;
    float adc_dsp_biquad_hp_a2_ = 0.0f;
    float adc_dsp_biquad_hp_z1_ = 0.0f;
    float adc_dsp_biquad_hp_z2_ = 0.0f;
    float adc_dsp_biquad_lp_b0_ = 1.0f;
    float adc_dsp_biquad_lp_b1_ = 0.0f;
    float adc_dsp_biquad_lp_b2_ = 0.0f;
    float adc_dsp_biquad_lp_a1_ = 0.0f;
    float adc_dsp_biquad_lp_a2_ = 0.0f;
    float adc_dsp_biquad_lp_z1_ = 0.0f;
    float adc_dsp_biquad_lp_z2_ = 0.0f;
    static constexpr size_t kAdcDspFftWindowSamples = 64U;
    float adc_dsp_fft_buffer_[kAdcDspFftWindowSamples] = {0.0f};
    uint8_t adc_dsp_fft_head_ = 0U;
    uint8_t adc_dsp_fft_fill_ = 0U;
    uint8_t adc_dsp_fft_decimator_ = 0U;
    float adc_dsp_fft_complex_buffer_[kAdcDspFftWindowSamples * 2U] = {0.0f};
    bool adc_dsp_fft_probe_enabled_ = false;
    bool adc_dsp_fft_probe_backend_ready_ = false;
    audio_tools::I2SStream i2s_stream_;
    BlockingOutput playback_blocking_output_;
    audio_tools::VolumeStream playback_volume_stream_;
    std::unique_ptr<audio_tools::ConverterScaler<int16_t>> playback_gain_scaler_;
    audio_tools::ConverterStream<int16_t> playback_gain_stream_;
    audio_tools::ResampleStream playback_resample_stream_;
    audio_tools::ChannelFormatConverterStream playback_channel_converter_stream_;
    audio_tools::WAVDecoder wav_decoder_;
    audio_tools::EncodedAudioStream wav_stream_;
    audio_tools::StreamCopy wav_copy_;
    audio_tools::MP3DecoderHelix mp3_decoder_;
    audio_tools::EncodedAudioStream mp3_stream_;
    audio_tools::StreamCopy mp3_copy_;
    mutable SemaphoreHandle_t i2s_io_mutex_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    static constexpr uint16_t kAudioTaskStackWords = 4096;
    static constexpr uint8_t kAudioTaskPriority = 8;
    portMUX_TYPE capture_lock_ = portMUX_INITIALIZER_UNLOCKED;
};

#endif  // AUDIO_ENGINE_H
