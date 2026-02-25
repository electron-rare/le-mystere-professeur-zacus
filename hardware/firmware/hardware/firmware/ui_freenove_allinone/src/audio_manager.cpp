// audio_manager.cpp - audio playback over I2S.
#include "audio_manager.h"

#include <AudioFileSource.h>
#include <AudioFileSourceFS.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGenerator.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <AudioOutputMixer.h>
#include <FS.h>
#include <LittleFS.h>
#include <cctype>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<SD_MMC.h>)
#include <SD_MMC.h>
#define ZACUS_HAS_SD_AUDIO 1
#else
#define ZACUS_HAS_SD_AUDIO 0
#endif

#include "ui_freenove_config.h"

namespace {

constexpr uint8_t kVolumeMax = 21;
constexpr char kDiagnosticRtttl[] PROGMEM = "zacus:d=4,o=5,b=196:c,8e,8g,2c6";

struct AudioPinProfile {
  int bck;
  int ws;
  int dout;
  const char* label;
};

constexpr AudioPinProfile kAudioPinProfiles[] = {
    {FREENOVE_I2S_BCK, FREENOVE_I2S_WS, FREENOVE_I2S_DOUT, "sketch19"},
    {FREENOVE_I2S_WS, FREENOVE_I2S_BCK, FREENOVE_I2S_DOUT, "swap_bck_ws"},
    {FREENOVE_I2S_BCK, FREENOVE_I2S_WS, 2, "dout2_alt"},
};

constexpr uint8_t kAudioPinProfileCount = static_cast<uint8_t>(sizeof(kAudioPinProfiles) / sizeof(kAudioPinProfiles[0]));

float volumeToGain(uint8_t volume) {
  if (volume > kVolumeMax) {
    volume = kVolumeMax;
  }
  return static_cast<float>(volume) / static_cast<float>(kVolumeMax);
}

bool endsWithIgnoreCase(const char* value, const char* suffix) {
  if (value == nullptr || suffix == nullptr) {
    return false;
  }
  const size_t value_len = std::strlen(value);
  const size_t suffix_len = std::strlen(suffix);
  if (suffix_len == 0U || value_len < suffix_len) {
    return false;
  }
  const char* tail = value + (value_len - suffix_len);
  for (size_t index = 0; index < suffix_len; ++index) {
    const int lhs = std::tolower(static_cast<unsigned char>(tail[index]));
    const int rhs = std::tolower(static_cast<unsigned char>(suffix[index]));
    if (lhs != rhs) {
      return false;
    }
  }
  return true;
}

}  // namespace

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
  stop();
  delete mixer_;
  mixer_ = nullptr;
  delete output_;
  output_ = nullptr;
}

bool AudioManager::begin() {
  return ensureOutput();
}

bool AudioManager::playOnChannel(uint8_t channel_index,
                                 AudioFileSource* source,
                                 AudioGenerator* decoder,
                                 const char* track) {
  if (channel_index >= 2U || source == nullptr || decoder == nullptr || track == nullptr || track[0] == '\0') {
    delete decoder;
    delete source;
    return false;
  }
  if (!ensureOutput() || channels_[channel_index].stub == nullptr) {
    delete decoder;
    delete source;
    return false;
  }

  stopChannel(channel_index);
  channels_[channel_index].source = source;
  channels_[channel_index].decoder = decoder;
  channels_[channel_index].track = track;
  channels_[channel_index].stub->SetGain(0.0f);

  if (!channels_[channel_index].decoder->begin(channels_[channel_index].source, channels_[channel_index].stub)) {
    Serial.printf("[AUDIO] decoder begin failed: %s\n", track);
    stopChannel(channel_index);
    return false;
  }
  return true;
}

uint8_t AudioManager::selectTargetChannel() const {
  if (!anyChannelRunning()) {
    return 0U;
  }
  return (active_channel_ == 0U) ? 1U : 0U;
}

void AudioManager::startCrossfade(uint8_t from_channel, uint8_t to_channel) {
  crossfade_active_ = true;
  crossfade_from_ = from_channel;
  crossfade_to_ = to_channel;
  crossfade_started_ms_ = millis();
  active_channel_ = to_channel;
  applyChannelGains();
}

void AudioManager::stopCrossfade() {
  crossfade_active_ = false;
}

void AudioManager::applyChannelGains() {
  if (channels_[0].stub == nullptr || channels_[1].stub == nullptr) {
    return;
  }
  const float base_gain = volumeToGain(volume_);

  if (crossfade_active_) {
    const uint32_t elapsed_ms = millis() - crossfade_started_ms_;
    uint16_t progress_per_mille = 1000U;
    if (crossfade_duration_ms_ > 0U && elapsed_ms < crossfade_duration_ms_) {
      progress_per_mille = static_cast<uint16_t>((elapsed_ms * 1000U) / crossfade_duration_ms_);
    }
    if (progress_per_mille > 1000U) {
      progress_per_mille = 1000U;
    }
    const float to_gain = base_gain * (static_cast<float>(progress_per_mille) / 1000.0f);
    const float from_gain = base_gain - to_gain;
    channels_[crossfade_from_].stub->SetGain(from_gain);
    channels_[crossfade_to_].stub->SetGain(to_gain);
    if (progress_per_mille >= 1000U) {
      stopChannel(crossfade_from_);
      stopCrossfade();
    }
    return;
  }

  for (uint8_t index = 0U; index < 2U; ++index) {
    if (channels_[index].decoder == nullptr) {
      channels_[index].stub->SetGain(0.0f);
      continue;
    }
    channels_[index].stub->SetGain((index == active_channel_) ? base_gain : 0.0f);
  }
}

bool AudioManager::play(const char* filename) {
  if (filename == nullptr || filename[0] == '\0') {
    Serial.println("[AUDIO] empty filename");
    return false;
  }
  if (!ensureOutput()) {
    return false;
  }

  String fixed_path = filename;
  bool use_sd_mmc = false;
  if (fixed_path.startsWith("/littlefs/")) {
    fixed_path = fixed_path.substring(9);
    if (!fixed_path.startsWith("/")) {
      fixed_path = "/" + fixed_path;
    }
  }
  if (fixed_path.startsWith("/sd/")) {
    fixed_path = fixed_path.substring(3);
    if (!fixed_path.startsWith("/")) {
      fixed_path = "/" + fixed_path;
    }
    use_sd_mmc = true;
  }

  fs::FS* file_system = &LittleFS;
  const char* storage_label = "LittleFS";
  if (use_sd_mmc) {
#if ZACUS_HAS_SD_AUDIO
    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("[AUDIO] SD_MMC unavailable for audio path");
      return false;
    }
    file_system = &SD_MMC;
    storage_label = "SD_MMC";
#else
    Serial.println("[AUDIO] SD audio path requested but SD_MMC support is unavailable");
    return false;
#endif
  }

  if (file_system == nullptr || !file_system->exists(fixed_path.c_str())) {
    Serial.printf("[AUDIO] missing file: %s (%s)\n", fixed_path.c_str(), storage_label);
    return false;
  }
  File metadata = file_system->open(fixed_path.c_str(), "r");
  if (!metadata) {
    Serial.printf("[AUDIO] failed to open file metadata: %s (%s)\n", fixed_path.c_str(), storage_label);
    return false;
  }
  const size_t file_size = static_cast<size_t>(metadata.size());
  metadata.close();
  if (file_size == 0U) {
    Serial.printf("[AUDIO] empty audio file: %s\n", fixed_path.c_str());
    return false;
  }

  const bool had_running = anyChannelRunning();
  const uint8_t previous_active = active_channel_;
  const uint8_t target_channel = selectTargetChannel();
  const bool is_wav = endsWithIgnoreCase(fixed_path.c_str(), ".wav");
  AudioFileSource* source = new AudioFileSourceFS(*file_system, fixed_path.c_str());
  AudioGenerator* decoder = is_wav ? static_cast<AudioGenerator*>(new AudioGeneratorWAV())
                                   : static_cast<AudioGenerator*>(new AudioGeneratorMP3());
  if (!playOnChannel(target_channel, source, decoder, fixed_path.c_str())) {
    return false;
  }

  using_diagnostic_tone_ = false;
  current_track_ = fixed_path.c_str();
  playing_ = true;
  if (had_running && channels_[previous_active].decoder != nullptr && previous_active != target_channel) {
    startCrossfade(previous_active, target_channel);
  } else {
    active_channel_ = target_channel;
    stopCrossfade();
    applyChannelGains();
  }
  Serial.printf("[AUDIO] playing (%s): %s (%s)\n", is_wav ? "wav" : "mp3", fixed_path.c_str(), storage_label);
  return true;
}

bool AudioManager::playDiagnosticTone() {
  if (!ensureOutput()) {
    return false;
  }
  const bool had_running = anyChannelRunning();
  const uint8_t previous_active = active_channel_;
  const uint8_t target_channel = selectTargetChannel();

  AudioFileSource* source = new AudioFileSourcePROGMEM(kDiagnosticRtttl, std::strlen(kDiagnosticRtttl));
  AudioGenerator* decoder = new AudioGeneratorRTTTL();
  if (!playOnChannel(target_channel, source, decoder, "builtin:rtttl")) {
    Serial.println("[AUDIO] diagnostic tone begin failed");
    return false;
  }

  using_diagnostic_tone_ = true;
  current_track_ = "builtin:rtttl";
  playing_ = true;
  if (had_running && channels_[previous_active].decoder != nullptr && previous_active != target_channel) {
    startCrossfade(previous_active, target_channel);
  } else {
    active_channel_ = target_channel;
    stopCrossfade();
    applyChannelGains();
  }
  Serial.printf("[AUDIO] playing diagnostic tone (profile=%u:%s)\n",
                output_profile_,
                outputProfileLabel(output_profile_));
  return true;
}

void AudioManager::stopChannel(uint8_t channel_index) {
  if (channel_index >= 2U) {
    return;
  }
  AudioChannel& channel = channels_[channel_index];
  if (channel.decoder != nullptr) {
    channel.decoder->stop();
    delete channel.decoder;
    channel.decoder = nullptr;
  }
  if (channel.source != nullptr) {
    channel.source->close();
    delete channel.source;
    channel.source = nullptr;
  }
  channel.track.remove(0);
  if (channel.stub != nullptr) {
    channel.stub->SetGain(0.0f);
  }
}

void AudioManager::stopAllChannels() {
  stopChannel(0U);
  stopChannel(1U);
}

void AudioManager::stop() {
  stopCrossfade();
  stopAllChannels();
  playing_ = false;
  using_diagnostic_tone_ = false;
  current_track_.remove(0);
}

bool AudioManager::anyChannelRunning() const {
  return channels_[0].decoder != nullptr || channels_[1].decoder != nullptr;
}

void AudioManager::update() {
  if (!playing_) {
    return;
  }

  applyChannelGains();
  String finished_track;
  bool active_finished = false;

  for (uint8_t index = 0U; index < 2U; ++index) {
    AudioChannel& channel = channels_[index];
    if (channel.decoder == nullptr) {
      continue;
    }
    if (!channel.decoder->isRunning() || !channel.decoder->loop()) {
      const bool was_active = (index == active_channel_);
      const String ended_track = channel.track;
      stopChannel(index);
      if (was_active) {
        active_finished = true;
        finished_track = ended_track;
      }
    }
  }

  if (!crossfade_active_) {
    applyChannelGains();
  }
  playing_ = anyChannelRunning();

  if (playing_ && channels_[active_channel_].decoder != nullptr) {
    current_track_ = channels_[active_channel_].track;
  }

  if (active_finished) {
    if (!playing_) {
      current_track_.remove(0);
    }
    finishPlaybackAndNotify(finished_track.c_str());
  }
}

bool AudioManager::isPlaying() const {
  return playing_;
}

void AudioManager::setVolume(uint8_t volume) {
  if (volume > kVolumeMax) {
    volume = kVolumeMax;
  }
  volume_ = volume;
  applyChannelGains();
}

uint8_t AudioManager::volume() const {
  return volume_;
}

const char* AudioManager::currentTrack() const {
  return current_track_.c_str();
}

bool AudioManager::setOutputProfile(uint8_t profile_index) {
  if (profile_index >= kAudioPinProfileCount) {
    return false;
  }
  output_profile_ = profile_index;
  applyOutputProfile();
  return true;
}

uint8_t AudioManager::outputProfile() const {
  return output_profile_;
}

uint8_t AudioManager::outputProfileCount() const {
  return kAudioPinProfileCount;
}

const char* AudioManager::outputProfileLabel(uint8_t profile_index) const {
  if (profile_index >= kAudioPinProfileCount) {
    return "invalid";
  }
  return kAudioPinProfiles[profile_index].label;
}

void AudioManager::setAudioDoneCallback(AudioDoneCallback cb, void* ctx) {
  done_cb_ = cb;
  done_ctx_ = ctx;
}

bool AudioManager::ensureOutput() {
  if (output_ == nullptr) {
    output_ = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S);
  }
  if (output_ == nullptr) {
    Serial.println("[AUDIO] failed to allocate I2S output");
    return false;
  }

  if (mixer_ == nullptr) {
    mixer_ = new AudioOutputMixer(32, output_);
  }
  if (mixer_ == nullptr) {
    Serial.println("[AUDIO] failed to allocate mixer");
    return false;
  }
  if (channels_[0].stub == nullptr) {
    channels_[0].stub = mixer_->NewInput();
  }
  if (channels_[1].stub == nullptr) {
    channels_[1].stub = mixer_->NewInput();
  }
  if (channels_[0].stub == nullptr || channels_[1].stub == nullptr) {
    Serial.println("[AUDIO] failed to allocate mixer stubs");
    return false;
  }

  output_->SetOutputModeMono(true);
  output_->SetGain(1.0f);
  applyOutputProfile();
  applyChannelGains();
  return true;
}

void AudioManager::applyOutputProfile() {
  if (output_ == nullptr) {
    return;
  }
  if (output_profile_ >= kAudioPinProfileCount) {
    output_profile_ = 0U;
  }
  const AudioPinProfile& profile = kAudioPinProfiles[output_profile_];
  output_->SetPinout(profile.bck, profile.ws, profile.dout);
  Serial.printf("[AUDIO] ready (profile=%u:%s bck=%d ws=%d dout=%d mode=mono)\n",
                output_profile_,
                profile.label,
                profile.bck,
                profile.ws,
                profile.dout);
}

void AudioManager::finishPlaybackAndNotify(const char* track) {
  if (done_cb_ != nullptr) {
    const char* reported_track = (track != nullptr && track[0] != '\0') ? track : current_track_.c_str();
    done_cb_(reported_track, done_ctx_);
  }
}
