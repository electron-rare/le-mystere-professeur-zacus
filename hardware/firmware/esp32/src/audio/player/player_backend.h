#pragma once

#include <Arduino.h>

enum class AudioCodec : uint8_t {
  kUnknown = 0,
  kMp3,
  kWav,
  kAac,
  kFlac,
  kOpus,
};

enum class PlayerBackendMode : uint8_t {
  kAutoFallback = 0,
  kAudioToolsOnly = 1,
  kLegacyOnly = 2,
};

enum class PlayerBackendId : uint8_t {
  kNone = 0,
  kAudioTools = 1,
  kLegacy = 2,
};

enum class PlayerBackendError : uint8_t {
  kOk = 0,
  kBadPath,
  kUnsupportedCodec,
  kOpenFail,
  kDecoderAllocFail,
  kDecoderInitFail,
  kI2sFail,
  kRuntimeError,
  kOutOfMemory,
  kUnknown,
};

struct PlayerBackendCapabilities {
  bool mp3 = false;
  bool wav = false;
  bool aac = false;
  bool flac = false;
  bool opus = false;
  bool supportsOverlayFx = true;
};

struct PlayerBackendStatus {
  PlayerBackendMode mode = PlayerBackendMode::kAutoFallback;
  PlayerBackendId active = PlayerBackendId::kNone;
  bool fallbackUsed = false;
  bool supportsOverlayFx = true;
  PlayerBackendCapabilities capabilities;
  PlayerBackendError lastErrorCode = PlayerBackendError::kOk;
  char lastError[24] = {};
};

inline const char* audioCodecLabel(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kMp3:
      return "MP3";
    case AudioCodec::kWav:
      return "WAV";
    case AudioCodec::kAac:
      return "AAC";
    case AudioCodec::kFlac:
      return "FLAC";
    case AudioCodec::kOpus:
      return "OPUS";
    case AudioCodec::kUnknown:
    default:
      return "UNKNOWN";
  }
}

inline const char* playerBackendModeLabel(PlayerBackendMode mode) {
  switch (mode) {
    case PlayerBackendMode::kAudioToolsOnly:
      return "AUDIO_TOOLS_ONLY";
    case PlayerBackendMode::kLegacyOnly:
      return "LEGACY_ONLY";
    case PlayerBackendMode::kAutoFallback:
    default:
      return "AUTO_FALLBACK";
  }
}

inline const char* playerBackendIdLabel(PlayerBackendId id) {
  switch (id) {
    case PlayerBackendId::kAudioTools:
      return "AUDIO_TOOLS";
    case PlayerBackendId::kLegacy:
      return "LEGACY";
    case PlayerBackendId::kNone:
    default:
      return "NONE";
  }
}

inline const char* playerBackendErrorLabel(PlayerBackendError error) {
  switch (error) {
    case PlayerBackendError::kOk:
      return "OK";
    case PlayerBackendError::kBadPath:
      return "BAD_PATH";
    case PlayerBackendError::kUnsupportedCodec:
      return "UNSUPPORTED_CODEC";
    case PlayerBackendError::kOpenFail:
      return "OPEN_FAIL";
    case PlayerBackendError::kDecoderAllocFail:
      return "DECODER_ALLOC_FAIL";
    case PlayerBackendError::kDecoderInitFail:
      return "DECODER_INIT_FAIL";
    case PlayerBackendError::kI2sFail:
      return "I2S_FAIL";
    case PlayerBackendError::kRuntimeError:
      return "RUNTIME_ERROR";
    case PlayerBackendError::kOutOfMemory:
      return "OOM";
    case PlayerBackendError::kUnknown:
    default:
      return "UNKNOWN";
  }
}

inline bool playerBackendSupportsCodec(const PlayerBackendCapabilities& caps, AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kMp3:
      return caps.mp3;
    case AudioCodec::kWav:
      return caps.wav;
    case AudioCodec::kAac:
      return caps.aac;
    case AudioCodec::kFlac:
      return caps.flac;
    case AudioCodec::kOpus:
      return caps.opus;
    case AudioCodec::kUnknown:
    default:
      return false;
  }
}
