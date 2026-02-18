#include "async_audio_service.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../../config.h"

#include <AudioFileSourceFS.h>
#include <AudioGenerator.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

#if USON_ENABLE_CODEC_AAC
#include <AudioGeneratorAAC.h>
#endif

#if USON_ENABLE_CODEC_FLAC
#include <AudioGeneratorFLAC.h>
#endif

#if USON_ENABLE_CODEC_OPUS
#include <AudioGeneratorOpus.h>
#endif

AsyncAudioService::AsyncAudioService(uint8_t i2sBclk,
                                     uint8_t i2sLrc,
                                     uint8_t i2sDout,
                                     uint8_t i2sPort,
                                     uint16_t fxChunkMs)
    : i2sBclk_(i2sBclk),
      i2sLrc_(i2sLrc),
      i2sDout_(i2sDout),
      i2sPort_(i2sPort),
      fxChunkMs_(fxChunkMs) {}

AsyncAudioService::~AsyncAudioService() {
  cancel("destructor");
}

void AsyncAudioService::copyTag(char* out, size_t outLen, const char* tag) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (tag == nullptr || tag[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", tag);
}

AsyncAudioService::FsCodec AsyncAudioService::codecFromPath(const char* path) {
  if (path == nullptr) {
    return FsCodec::kUnknown;
  }

  const char* dot = strrchr(path, '.');
  if (dot == nullptr) {
    return FsCodec::kUnknown;
  }

  char ext[8] = {};
  size_t i = 0;
  ++dot;
  while (dot[i] != '\0' && i < (sizeof(ext) - 1U)) {
    ext[i] = static_cast<char>(tolower(static_cast<unsigned char>(dot[i])));
    ++i;
  }
  ext[i] = '\0';

  if (strcmp(ext, "mp3") == 0) {
    return FsCodec::kMp3;
  }
  if (strcmp(ext, "wav") == 0) {
    return FsCodec::kWav;
  }
#if USON_ENABLE_CODEC_AAC
  if (strcmp(ext, "aac") == 0) {
    return FsCodec::kAac;
  }
#endif
#if USON_ENABLE_CODEC_FLAC
  if (strcmp(ext, "flac") == 0) {
    return FsCodec::kFlac;
  }
#endif
#if USON_ENABLE_CODEC_OPUS
  if (strcmp(ext, "opus") == 0 || strcmp(ext, "ogg") == 0) {
    return FsCodec::kOpus;
  }
#endif
  return FsCodec::kUnknown;
}

AudioGenerator* AsyncAudioService::createDecoder(FsCodec codec) {
  switch (codec) {
    case FsCodec::kMp3:
      return new AudioGeneratorMP3();
    case FsCodec::kWav:
      return new AudioGeneratorWAV();
#if USON_ENABLE_CODEC_AAC
    case FsCodec::kAac:
      return new AudioGeneratorAAC();
#endif
#if USON_ENABLE_CODEC_FLAC
    case FsCodec::kFlac:
      return new AudioGeneratorFLAC();
#endif
#if USON_ENABLE_CODEC_OPUS
    case FsCodec::kOpus:
      return new AudioGeneratorOpus();
#endif
    default:
      return nullptr;
  }
}

void AsyncAudioService::cleanupFs() {
  if (fsDecoder_ != nullptr) {
    fsDecoder_->stop();
    delete fsDecoder_;
    fsDecoder_ = nullptr;
  }
  if (fsOutput_ != nullptr) {
    fsOutput_->stop();
    delete fsOutput_;
    fsOutput_ = nullptr;
  }
  if (fsFile_ != nullptr) {
    delete fsFile_;
    fsFile_ = nullptr;
  }
  fsStorage_ = nullptr;
}

void AsyncAudioService::complete(Kind kind, Result result, const char* tag) {
  activeKind_ = Kind::kNone;
  startMs_ = 0U;
  deadlineMs_ = 0U;
  activeTag_[0] = '\0';
  fx_ = nullptr;
  cleanupFs();

  event_.kind = kind;
  event_.result = result;
  copyTag(event_.tag, sizeof(event_.tag), tag);
  hasEvent_ = true;
}

bool AsyncAudioService::startFs(fs::FS& storage,
                                const char* path,
                                float gain,
                                uint32_t maxDurationMs,
                                const char* tag) {
  cancel("replace");

  if (path == nullptr || path[0] == '\0' || !storage.exists(path)) {
    return false;
  }

  const FsCodec codec = codecFromPath(path);
  if (codec == FsCodec::kUnknown) {
    return false;
  }

  fsFile_ = new AudioFileSourceFS(storage, path);
  fsOutput_ = new AudioOutputI2S(static_cast<int>(i2sPort_), AudioOutputI2S::EXTERNAL_I2S);
  fsOutput_->SetPinout(static_cast<int>(i2sBclk_),
                       static_cast<int>(i2sLrc_),
                       static_cast<int>(i2sDout_));
  fsOutput_->SetGain(gain);

  fsDecoder_ = createDecoder(codec);
  if (fsDecoder_ == nullptr) {
    cleanupFs();
    return false;
  }

  if (!fsDecoder_->begin(fsFile_, fsOutput_)) {
    cleanupFs();
    return false;
  }

  fsStorage_ = &storage;
  startMs_ = millis();
  deadlineMs_ = (maxDurationMs > 0U) ? (startMs_ + maxDurationMs) : 0U;
  activeKind_ = Kind::kFs;
  copyTag(activeTag_, sizeof(activeTag_), tag);
  return true;
}

bool AsyncAudioService::startFx(FmRadioScanFx& fx,
                                FmRadioScanFx::Effect effect,
                                uint32_t durationMs,
                                float gain,
                                const char* tag) {
  cancel("replace");

  if (durationMs == 0U) {
    return false;
  }

  fx.setGain(gain);
  if (!fx.start(effect)) {
    return false;
  }

  startMs_ = millis();
  deadlineMs_ = startMs_ + durationMs;
  activeKind_ = Kind::kFx;
  fx_ = &fx;
  copyTag(activeTag_, sizeof(activeTag_), tag);
  return true;
}

void AsyncAudioService::update(uint32_t nowMs) {
  if (activeKind_ == Kind::kNone) {
    return;
  }

  if (activeKind_ == Kind::kFs) {
    if (deadlineMs_ != 0U && static_cast<int32_t>(nowMs - deadlineMs_) >= 0) {
      complete(Kind::kFs, Result::kTimeout, activeTag_);
      return;
    }

    if (fsDecoder_ == nullptr || !fsDecoder_->isRunning()) {
      complete(Kind::kFs, Result::kDone, activeTag_);
      return;
    }

    if (fsDecoder_->loop()) {
      return;
    }
    complete(Kind::kFs, Result::kDone, activeTag_);
    return;
  }

  if (activeKind_ == Kind::kFx) {
    if (fx_ == nullptr) {
      complete(Kind::kFx, Result::kFailed, activeTag_);
      return;
    }

    if (deadlineMs_ != 0U && static_cast<int32_t>(nowMs - deadlineMs_) >= 0) {
      fx_->stop();
      complete(Kind::kFx, Result::kDone, activeTag_);
      return;
    }

    if (!fx_->isActive()) {
      complete(Kind::kFx, Result::kDone, activeTag_);
      return;
    }

    fx_->update(nowMs, fxChunkMs_);
  }
}

void AsyncAudioService::cancel(const char* tag) {
  if (activeKind_ == Kind::kNone) {
    return;
  }

  if (activeKind_ == Kind::kFx && fx_ != nullptr) {
    fx_->stop();
  }

  complete(activeKind_, Result::kCanceled, (tag != nullptr) ? tag : activeTag_);
}

bool AsyncAudioService::isBusy() const {
  return activeKind_ != Kind::kNone;
}

AsyncAudioService::Kind AsyncAudioService::activeKind() const {
  return activeKind_;
}

const char* AsyncAudioService::activeTag() const {
  return activeTag_;
}

bool AsyncAudioService::hasEvent() const {
  return hasEvent_;
}

AsyncAudioService::Event AsyncAudioService::popEvent() {
  Event out = event_;
  hasEvent_ = false;
  event_ = Event{};
  return out;
}
