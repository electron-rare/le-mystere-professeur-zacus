#pragma once

#include <Arduino.h>

#include "../../audio/mp3_player.h"
#include "../../ui/player_ui_model.h"

class Mp3Controller {
 public:
  Mp3Controller(Mp3Player& player, PlayerUiModel& ui);

  void update(uint32_t nowMs, bool allowPlayback);
  void refreshStorage();
  void applyUiAction(const UiAction& action);
  const char* browsePath() const;
  void setBrowsePath(const char* path);
  void printUiStatus(Print& out, const char* source) const;
  void printScanStatus(Print& out, const char* source) const;
  void printScanProgress(Print& out, const char* source) const;
  void printBackendStatus(Print& out, const char* source) const;
  void printBrowseList(Print& out, const char* source, const char* path, uint16_t offset, uint16_t limit) const;
  void printQueuePreview(Print& out, uint8_t count, const char* source) const;
  void printCapabilities(Print& out, const char* source) const;

  Mp3Player& player();
  const Mp3Player& player() const;
  PlayerUiModel& ui();
  const PlayerUiModel& ui() const;

 private:
  Mp3Player& player_;
  PlayerUiModel& ui_;
  String browsePath_ = "/";
};
