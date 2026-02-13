#pragma once

#include <Arduino.h>

enum class PlayerUiPage : uint8_t {
  kNowPlaying = 0,
  kBrowser = 1,
  kQueue = 2,
  kSettings = 3,
};

enum class UiActionSource : uint8_t {
  kKeyShort = 0,
  kKeyLong = 1,
  kSerial = 2,
};

struct UiAction {
  UiActionSource source = UiActionSource::kKeyShort;
  uint8_t key = 0;
  PlayerUiPage targetPage = PlayerUiPage::kNowPlaying;
  bool hasTargetPage = false;
};

struct PlayerUiSnapshot {
  PlayerUiPage page = PlayerUiPage::kNowPlaying;
  uint16_t cursor = 0;
  uint16_t offset = 0;
  bool dirty = false;
};

const char* playerUiPageLabel(PlayerUiPage page);

class PlayerUiModel {
 public:
  void reset();
  void setPage(PlayerUiPage page);
  void setBrowserBounds(uint16_t count);
  void applyAction(const UiAction& action);

  PlayerUiSnapshot snapshot() const;
  PlayerUiPage page() const;
  uint16_t cursor() const;
  uint16_t offset() const;
  bool consumeDirty();

 private:
  void clampBrowser();
  void moveCursor(int16_t delta);
  void nextPage();
  void prevPage();

  PlayerUiPage page_ = PlayerUiPage::kNowPlaying;
  uint16_t browserCount_ = 0;
  uint16_t cursor_ = 0;
  uint16_t offset_ = 0;
  bool dirty_ = true;
};
