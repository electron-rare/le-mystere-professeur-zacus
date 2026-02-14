#pragma once

#include <Arduino.h>

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

struct PlayerBackendStatus {
  PlayerBackendMode mode = PlayerBackendMode::kAutoFallback;
  PlayerBackendId active = PlayerBackendId::kNone;
  bool fallbackUsed = false;
  bool supportsOverlayFx = true;
  char lastError[24] = {};
};

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
