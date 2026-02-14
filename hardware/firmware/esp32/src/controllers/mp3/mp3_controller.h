#pragma once

#include <Arduino.h>

#include "../../audio/mp3_player.h"
#include "../../services/radio/radio_service.h"
#include "../../ui/player_ui_model.h"

struct Mp3UiTextSlots {
  char npTitle1[40] = {};
  char npTitle2[40] = {};
  char npSub[48] = {};
  char listPath[48] = {};
  char listRow0[48] = {};
  char listRow1[48] = {};
  char listRow2[48] = {};
  char setHint[48] = {};
};

class Mp3Controller {
 public:
  Mp3Controller(Mp3Player& player, PlayerUiModel& ui, RadioService* radio = nullptr);

  void update(uint32_t nowMs, bool allowPlayback);
  void refreshStorage();
  void applyUiAction(const UiAction& action);
  bool uiNavigate(UiNavAction action, uint32_t nowMs);
  void buildTextSlots(Mp3UiTextSlots* out, uint32_t nowMs) const;
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
  bool playSelectedListItem();
  void applyOkOnCurrentPage(uint32_t nowMs);
  void applyLeftRightOnCurrentPage(int8_t direction, uint32_t nowMs);
  const char* listPathLabel() const;
  const char* currentSourceLabel() const;

  Mp3Player& player_;
  PlayerUiModel& ui_;
  RadioService* radio_ = nullptr;
  String browsePath_ = "/";
};
