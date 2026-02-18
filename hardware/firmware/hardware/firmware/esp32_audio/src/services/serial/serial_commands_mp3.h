#pragma once

#include <Arduino.h>

#include "../../audio/effects/audio_effect_id.h"
#include "../../audio/mp3_player.h"
#include "ui/player_ui_model.h"
#include "serial_router.h"

struct Mp3SerialRuntimeContext {
  Mp3Player* player = nullptr;
  PlayerUiModel* ui = nullptr;
  bool (*allowPlaybackNow)() = nullptr;
  bool (*setUiPage)(PlayerUiPage page) = nullptr;
  bool (*parsePlayerUiPageToken)(const char* token, PlayerUiPage* outPage) = nullptr;
  bool (*parseBackendModeToken)(const char* token, PlayerBackendMode* outMode) = nullptr;
  bool (*parseMp3FxEffectToken)(const char* token, Mp3FxEffect* outEffect) = nullptr;
  bool (*triggerMp3Fx)(Mp3FxEffect effect, uint32_t durationMs, const char* source) = nullptr;
  void (*stopOverlayFx)(const char* reason) = nullptr;
  void (*forceUsonFunctional)(const char* source) = nullptr;
  const char* (*currentBrowsePath)() = nullptr;
  void (*setBrowsePath)(const char* path) = nullptr;
  void (*printHelp)() = nullptr;
  void (*printUiStatus)(const char* source) = nullptr;
  void (*printStatus)(const char* source) = nullptr;
  void (*printScanStatus)(const char* source) = nullptr;
  void (*printScanProgress)(const char* source) = nullptr;
  void (*printBackendStatus)(const char* source) = nullptr;
  void (*printBrowseList)(const char* source, const char* path, uint16_t offset, uint16_t limit) = nullptr;
  void (*printQueuePreview)(uint8_t count, const char* source) = nullptr;
  void (*printCaps)(const char* source) = nullptr;
  bool (*startFormatTest)(uint32_t nowMs, uint32_t dwellMs) = nullptr;
  void (*stopFormatTest)(const char* reason) = nullptr;
};

bool serialIsMp3Command(const char* token);
bool serialProcessMp3Command(const SerialCommand& cmd,
                             uint32_t nowMs,
                             const Mp3SerialRuntimeContext& ctx,
                             Print& out);
