#include "audio_tools_backend.h"

#include <FS.h>
#include <SD_MMC.h>

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>

namespace {

using audio_tools::AudioDecoder;
using audio_tools::EncodedAudioStream;
using audio_tools::I2SConfig;
using audio_tools::I2SStream;
using audio_tools::StreamCopy;
using audio_tools::TX_MODE;
using audio_tools::WAVDecoder;

bool endsWithIgnoreCase(const char* value, const char* suffix) {
  if (value == nullptr || suffix == nullptr) {
    return false;
  }
  const size_t valueLen = strlen(value);
  const size_t suffixLen = strlen(suffix);
  if (valueLen < suffixLen) {
    return false;
  }
  const char* tail = value + (valueLen - suffixLen);
  for (size_t i = 0U; i < suffixLen; ++i) {
    const char a = static_cast<char>(tolower(static_cast<unsigned char>(tail[i])));
    const char b = static_cast<char>(tolower(static_cast<unsigned char>(suffix[i])));
    if (a != b) {
      return false;
    }
  }
  return true;
}

AudioCodec codecFromPath(const char* path) {
  if (endsWithIgnoreCase(path, ".mp3")) {
    return AudioCodec::kMp3;
  }
  if (endsWithIgnoreCase(path, ".wav")) {
    return AudioCodec::kWav;
  }
  if (endsWithIgnoreCase(path, ".aac") || endsWithIgnoreCase(path, ".m4a")) {
    return AudioCodec::kAac;
  }
  if (endsWithIgnoreCase(path, ".flac")) {
    return AudioCodec::kFlac;
  }
  if (endsWithIgnoreCase(path, ".opus") || endsWithIgnoreCase(path, ".ogg")) {
    return AudioCodec::kOpus;
  }
  return AudioCodec::kUnknown;
}

}  // namespace

AudioToolsBackend::AudioToolsBackend(uint8_t i2sBclk,
                                     uint8_t i2sLrc,
                                     uint8_t i2sDout,
                                     uint8_t i2sPort)
    : i2sBclk_(i2sBclk),
      i2sLrc_(i2sLrc),
      i2sDout_(i2sDout),
      i2sPort_(i2sPort) {}

bool AudioToolsBackend::start(const char* path, float gain) {
  stop();
  setGain(gain);

  if (path == nullptr || path[0] == '\0') {
    setLastError(PlayerBackendError::kBadPath);
    return false;
  }
  const AudioCodec codec = codecForPath(path);
  if (!supportsCodec(codec)) {
    setLastError(PlayerBackendError::kUnsupportedCodec);
    return false;
  }
  if (!setupI2s()) {
    return false;
  }

  auto* file = new fs::File();
  *file = SD_MMC.open(path, FILE_READ);
  if (!(*file) || file->isDirectory()) {
    delete file;
    setLastError(PlayerBackendError::kOpenFail);
    return false;
  }

  file_ = file;
  if (!setupDecoderForCodec(codec)) {
    stop();
    return false;
  }

  auto* encoded = static_cast<EncodedAudioStream*>(encoded_);
  auto* fileStream = static_cast<fs::File*>(file_);
  auto* copy = new StreamCopy(*encoded, *fileStream);
  copier_ = copy;

  active_ = true;
  eof_ = false;
  idleLoops_ = 0U;
  activeCodec_ = codec;
  setLastError(PlayerBackendError::kOk);
  return true;
}

void AudioToolsBackend::update() {
  if (!active_) {
    return;
  }

  auto* copy = static_cast<StreamCopy*>(copier_);
  auto* file = static_cast<fs::File*>(file_);
  if (copy == nullptr || file == nullptr || !(*file)) {
    setLastError(PlayerBackendError::kRuntimeError);
    stop();
    return;
  }

  const size_t moved = copy->copy();
  if (moved > 0U) {
    idleLoops_ = 0U;
    return;
  }

  if (file->available() > 0) {
    return;
  }

  ++idleLoops_;
  if (idleLoops_ > 2U) {
    eof_ = true;
    stop();
  }
}

void AudioToolsBackend::stop() {
  active_ = false;
  idleLoops_ = 0U;
  activeCodec_ = AudioCodec::kUnknown;

  auto* copy = static_cast<StreamCopy*>(copier_);
  if (copy != nullptr) {
    delete copy;
    copier_ = nullptr;
  }

  auto* encoded = static_cast<EncodedAudioStream*>(encoded_);
  if (encoded != nullptr) {
    encoded->end();
    delete encoded;
    encoded_ = nullptr;
  }

  auto* decoder = static_cast<AudioDecoder*>(decoder_);
  if (decoder != nullptr) {
    decoder->end();
    delete decoder;
    decoder_ = nullptr;
  }

  auto* file = static_cast<fs::File*>(file_);
  if (file != nullptr) {
    if (*file) {
      file->close();
    }
    delete file;
    file_ = nullptr;
  }

  auto* i2s = static_cast<I2SStream*>(i2s_);
  if (i2s != nullptr) {
    i2s->end();
  }
}

bool AudioToolsBackend::isActive() const {
  return active_;
}

bool AudioToolsBackend::canHandlePath(const char* path) const {
  return supportsCodec(codecForPath(path));
}

AudioCodec AudioToolsBackend::codecForPath(const char* path) const {
  return codecFromPath(path);
}

bool AudioToolsBackend::supportsCodec(AudioCodec codec) const {
  switch (codec) {
    case AudioCodec::kWav:
      return true;
    case AudioCodec::kMp3:
    case AudioCodec::kAac:
    case AudioCodec::kFlac:
    case AudioCodec::kOpus:
    case AudioCodec::kUnknown:
    default:
      return false;
  }
}

PlayerBackendCapabilities AudioToolsBackend::capabilities() const {
  PlayerBackendCapabilities caps;
  caps.mp3 = false;
  caps.wav = true;
  caps.aac = false;
  caps.flac = false;
  caps.opus = false;
  caps.supportsOverlayFx = false;
  return caps;
}

PlayerBackendError AudioToolsBackend::lastErrorCode() const {
  return lastErrorCode_;
}

const char* AudioToolsBackend::lastError() const {
  return lastError_;
}

void AudioToolsBackend::setGain(float gain) {
  if (gain < 0.0f) {
    gain = 0.0f;
  } else if (gain > 1.0f) {
    gain = 1.0f;
  }
  gain_ = gain;
}

float AudioToolsBackend::gain() const {
  return gain_;
}

bool AudioToolsBackend::setupI2s() {
  auto* i2s = static_cast<I2SStream*>(i2s_);
  if (i2s == nullptr) {
    i2s = new I2SStream();
    i2s_ = i2s;
  }

  I2SConfig cfg = i2s->defaultConfig(TX_MODE);
  cfg.pin_bck = static_cast<int>(i2sBclk_);
  cfg.pin_ws = static_cast<int>(i2sLrc_);
  cfg.pin_data = static_cast<int>(i2sDout_);
  cfg.port_no = static_cast<int>(i2sPort_);
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;

  if (!i2s->begin(cfg)) {
    setLastError(PlayerBackendError::kI2sFail);
    return false;
  }
  return true;
}

bool AudioToolsBackend::setupDecoderForCodec(AudioCodec codec) {
  AudioDecoder* decoder = nullptr;
  switch (codec) {
    case AudioCodec::kWav:
      decoder = new WAVDecoder();
      break;
    case AudioCodec::kMp3:
    case AudioCodec::kAac:
    case AudioCodec::kFlac:
    case AudioCodec::kOpus:
    case AudioCodec::kUnknown:
    default:
      setLastError(PlayerBackendError::kUnsupportedCodec);
      return false;
  }

  if (decoder == nullptr) {
    setLastError(PlayerBackendError::kDecoderAllocFail);
    return false;
  }

  auto* i2s = static_cast<I2SStream*>(i2s_);
  auto* encoded = new EncodedAudioStream(i2s, decoder);
  if (encoded == nullptr) {
    delete decoder;
    setLastError(PlayerBackendError::kOutOfMemory);
    return false;
  }
  if (!encoded->begin()) {
    delete encoded;
    delete decoder;
    setLastError(PlayerBackendError::kDecoderInitFail);
    return false;
  }

  decoder_ = decoder;
  encoded_ = encoded;
  return true;
}

void AudioToolsBackend::setLastError(PlayerBackendError code) {
  lastErrorCode_ = code;
  snprintf(lastError_, sizeof(lastError_), "%s", playerBackendErrorLabel(code));
}
