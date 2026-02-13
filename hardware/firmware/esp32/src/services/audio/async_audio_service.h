#pragma once

#include <Arduino.h>
#include <FS.h>

#include "../../audio/fm_radio_scan_fx.h"

class AudioGenerator;
class AudioFileSourceFS;
class AudioOutputI2S;

class AsyncAudioService {
 public:
  enum class Kind : uint8_t {
    kNone = 0,
    kFs = 1,
    kFx = 2,
  };

  enum class Result : uint8_t {
    kNone = 0,
    kDone = 1,
    kFailed = 2,
    kTimeout = 3,
    kCanceled = 4,
  };

  struct Event {
    Kind kind = Kind::kNone;
    Result result = Result::kNone;
    char tag[24] = {};
  };

  AsyncAudioService(uint8_t i2sBclk,
                    uint8_t i2sLrc,
                    uint8_t i2sDout,
                    uint8_t i2sPort,
                    uint16_t fxChunkMs);

  ~AsyncAudioService();

  bool startFs(fs::FS& storage,
               const char* path,
               float gain,
               uint32_t maxDurationMs,
               const char* tag);

  bool startFx(FmRadioScanFx& fx,
               FmRadioScanFx::Effect effect,
               uint32_t durationMs,
               float gain,
               const char* tag);

  void update(uint32_t nowMs);
  void cancel(const char* tag = nullptr);

  bool isBusy() const;
  Kind activeKind() const;
  const char* activeTag() const;

  bool hasEvent() const;
  Event popEvent();

 private:
  enum class FsCodec : uint8_t {
    kUnknown = 0,
    kMp3 = 1,
    kWav = 2,
    kAac = 3,
    kFlac = 4,
    kOpus = 5,
  };

  static FsCodec codecFromPath(const char* path);
  static AudioGenerator* createDecoder(FsCodec codec);
  static void copyTag(char* out, size_t outLen, const char* tag);

  void cleanupFs();
  void complete(Kind kind, Result result, const char* tag);

  uint8_t i2sBclk_;
  uint8_t i2sLrc_;
  uint8_t i2sDout_;
  uint8_t i2sPort_;
  uint16_t fxChunkMs_;

  Kind activeKind_ = Kind::kNone;
  uint32_t startMs_ = 0U;
  uint32_t deadlineMs_ = 0U;
  char activeTag_[24] = {};

  fs::FS* fsStorage_ = nullptr;
  AudioFileSourceFS* fsFile_ = nullptr;
  AudioOutputI2S* fsOutput_ = nullptr;
  AudioGenerator* fsDecoder_ = nullptr;

  FmRadioScanFx* fx_ = nullptr;

  bool hasEvent_ = false;
  Event event_;
};
