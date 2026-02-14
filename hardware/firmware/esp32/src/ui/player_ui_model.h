#pragma once

#include <Arduino.h>

enum class PlayerUiPage : uint8_t {
  kLecture = 0,
  kListe = 1,
  kReglages = 2,
};

enum class UiActionSource : uint8_t {
  kKeyShort = 0,
  kKeyLong = 1,
  kSerial = 2,
};

enum class PlayerUiSource : uint8_t {
  kSd = 0,
  kRadio = 1,
};

enum class UiNavAction : uint8_t {
  kNone = 0,
  kUp,
  kDown,
  kLeft,
  kRight,
  kOk,
  kBack,
  kModeToggle,
};

enum class UiSettingKey : uint8_t {
  kWifi = 0,
  kEq = 1,
  kLuminosite = 2,
  kScreensaver = 3,
};

struct UiAction {
  UiActionSource source = UiActionSource::kKeyShort;
  uint8_t key = 0;
  UiNavAction nav = UiNavAction::kNone;
  PlayerUiPage targetPage = PlayerUiPage::kLecture;
  bool hasTargetPage = false;
};

struct PlayerUiSnapshot {
  PlayerUiPage page = PlayerUiPage::kLecture;
  PlayerUiSource source = PlayerUiSource::kSd;
  uint16_t cursor = 0;
  uint16_t offset = 0;
  uint16_t listCount = 0;
  uint16_t listOffset = 0;
  uint8_t settingsIndex = 0;
  UiSettingKey settingsKey = UiSettingKey::kWifi;
  uint8_t wifiMode = 0;
  uint8_t eqPreset = 0;
  uint8_t brightness = 2;
  bool screensaver = false;
  bool dirty = false;
};

const char* playerUiPageLabel(PlayerUiPage page);
const char* playerUiSourceLabel(PlayerUiSource source);
const char* uiSettingLabel(UiSettingKey key);
UiSettingKey uiSettingFromIndex(uint8_t idx);
uint8_t uiSettingIndex(UiSettingKey key);
const char* uiWifiModeLabel(uint8_t mode);
const char* uiEqLabel(uint8_t preset);
const char* uiBrightnessLabel(uint8_t level);
const char* uiOnOffLabel(bool enabled);

class PlayerUiModel {
 public:
  void reset();
  void setPage(PlayerUiPage page);
  void setSource(PlayerUiSource source);
  void toggleSource();
  void setListBounds(uint16_t count);
  void applyAction(const UiAction& action);
  bool applySettingAction();
  bool applySettingDelta(int8_t delta);

  PlayerUiSnapshot snapshot() const;
  PlayerUiPage page() const;
  PlayerUiSource source() const;
  uint16_t cursor() const;
  uint16_t offset() const;
  uint16_t listCount() const;
  uint16_t listOffset() const;
  uint8_t settingsIndex() const;
  UiSettingKey settingsKey() const;
  uint8_t wifiMode() const;
  uint8_t eqPreset() const;
  uint8_t brightness() const;
  bool screensaver() const;
  bool consumeDirty();

 private:
  UiNavAction resolveAction(const UiAction& action) const;
  void clampList();
  void moveListCursor(int16_t delta);
  void moveSettings(int8_t delta);
  void nextPage();
  void prevPage();
  void markDirty();

  PlayerUiPage page_ = PlayerUiPage::kLecture;
  PlayerUiSource source_ = PlayerUiSource::kSd;
  uint16_t listCount_ = 0;
  uint16_t listCursor_ = 0;
  uint16_t listOffset_ = 0;
  uint8_t settingsIndex_ = 0;
  uint8_t wifiMode_ = 0;
  uint8_t eqPreset_ = 0;
  uint8_t brightness_ = 2;
  bool screensaver_ = false;
  bool dirty_ = true;
};
