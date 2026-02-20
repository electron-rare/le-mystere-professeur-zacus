// audio_manager.cpp - audio playback over I2S.
#include "audio_manager.h"

#include <AudioFileSource.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGenerator.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <LittleFS.h>
#include <cctype>
#include <cstring>

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
  delete output_;
  output_ = nullptr;
}

bool AudioManager::begin() {
  return ensureOutput();
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
  if (fixed_path.startsWith("/littlefs/")) {
    fixed_path = fixed_path.substring(9);
    if (!fixed_path.startsWith("/")) {
      fixed_path = "/" + fixed_path;
    }
  }

  if (!LittleFS.exists(fixed_path.c_str())) {
    Serial.printf("[AUDIO] missing file: %s\n", fixed_path.c_str());
    return false;
  }
  File metadata = LittleFS.open(fixed_path.c_str(), "r");
  if (!metadata) {
    Serial.printf("[AUDIO] failed to open file metadata: %s\n", fixed_path.c_str());
    return false;
  }
  const size_t file_size = static_cast<size_t>(metadata.size());
  metadata.close();
  if (file_size == 0U) {
    Serial.printf("[AUDIO] empty audio file: %s\n", fixed_path.c_str());
    return false;
  }

  stopDecoder();

  const bool is_wav = endsWithIgnoreCase(fixed_path.c_str(), ".wav");
  source_ = new AudioFileSourceLittleFS(fixed_path.c_str());
  decoder_ = is_wav ? static_cast<AudioGenerator*>(new AudioGeneratorWAV())
                    : static_cast<AudioGenerator*>(new AudioGeneratorMP3());
  if (source_ == nullptr || decoder_ == nullptr) {
    Serial.println("[AUDIO] allocation failure");
    stopDecoder();
    return false;
  }

  output_->SetGain(volumeToGain(volume_));
  if (!decoder_->begin(source_, output_)) {
    Serial.printf("[AUDIO] decoder begin failed: %s\n", fixed_path.c_str());
    stopDecoder();
    return false;
  }

  using_diagnostic_tone_ = false;
  current_track_ = fixed_path.c_str();
  playing_ = true;
  Serial.printf("[AUDIO] playing (%s): %s\n", is_wav ? "wav" : "mp3", fixed_path.c_str());
  return true;
}

bool AudioManager::playDiagnosticTone() {
  if (!ensureOutput()) {
    return false;
  }

  stopDecoder();
  source_ = new AudioFileSourcePROGMEM(kDiagnosticRtttl, std::strlen(kDiagnosticRtttl));
  decoder_ = new AudioGeneratorRTTTL();
  if (source_ == nullptr || decoder_ == nullptr) {
    Serial.println("[AUDIO] diagnostic tone allocation failure");
    stopDecoder();
    return false;
  }

  output_->SetGain(volumeToGain(volume_));
  if (!decoder_->begin(source_, output_)) {
    Serial.println("[AUDIO] diagnostic tone begin failed");
    stopDecoder();
    return false;
  }

  using_diagnostic_tone_ = true;
  current_track_ = "builtin:rtttl";
  playing_ = true;
  Serial.printf("[AUDIO] playing diagnostic tone (profile=%u:%s)\n",
                output_profile_,
                outputProfileLabel(output_profile_));
  return true;
}

void AudioManager::stop() {
  stopDecoder();
}

void AudioManager::update() {
  if (!playing_ || decoder_ == nullptr) {
    return;
  }
  if (!decoder_->isRunning()) {
    finishPlaybackAndNotify();
    return;
  }
  if (!decoder_->loop()) {
    finishPlaybackAndNotify();
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
  if (output_ != nullptr) {
    output_->SetGain(volumeToGain(volume_));
  }
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

  output_->SetOutputModeMono(true);
  output_->SetGain(volumeToGain(volume_));
  applyOutputProfile();
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

void AudioManager::stopDecoder() {
  if (decoder_ != nullptr) {
    decoder_->stop();
    delete decoder_;
    decoder_ = nullptr;
  }
  if (source_ != nullptr) {
    source_->close();
    delete source_;
    source_ = nullptr;
  }
  using_diagnostic_tone_ = false;
  playing_ = false;
}

void AudioManager::finishPlaybackAndNotify() {
  const String finished_track = current_track_;
  stopDecoder();
  if (done_cb_ != nullptr) {
    done_cb_(finished_track.c_str(), done_ctx_);
  }
}
