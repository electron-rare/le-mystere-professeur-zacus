#pragma once

#include <Arduino.h>

#include "../include/ui_protocol.h"

struct UiTouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
  bool pressed = false;
};

class UiStateModel {
 public:
  void begin();

  void applyState(const UiRemoteState& state, uint32_t nowMs);
  void applyTick(const UiRemoteTick& tick, uint32_t nowMs);
  void applyList(const UiRemoteList& list, uint32_t nowMs);
  void onHeartbeat(uint32_t nowMs);
  void updateConnection(uint32_t nowMs);

  bool shouldRequestState(uint32_t nowMs);

  bool onTap(uint16_t x, uint16_t y, uint32_t nowMs, UiOutgoingCommand* outCmd);
  bool onSwipe(int16_t dx, int16_t dy, uint32_t nowMs, UiOutgoingCommand* outCmd);

  UiPage page() const;
  UiSource source() const;
  bool connected() const;
  bool playing() const;
  int32_t volume() const;
  int32_t posSec() const;
  int32_t durSec() const;
  int32_t rssi() const;
  int32_t bufferPercent() const;
  float vu() const;
  const char* title() const;
  const char* artist() const;
  const char* station() const;
  const char* error() const;

  uint8_t settingsIndex() const;
  uint8_t wifiMode() const;
  uint8_t eqPreset() const;
  uint8_t brightness() const;
  bool screensaver() const;

  const UiRemoteList& list() const;
  uint8_t listCursor() const;

  bool consumeDirty();

 private:
  void markDirty();
  void setPage(UiPage page);
  void toggleSource(UiOutgoingCommand* outCmd);
  bool handleNowTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd);
  bool handleListTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd);
  bool handleSettingsTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd);
  bool applyListDelta(int16_t delta, UiOutgoingCommand* outCmd);
  bool applySettingsDelta(int8_t delta, UiOutgoingCommand* outCmd);
  void applySettingAction(UiOutgoingCommand* outCmd);
  bool buildDeltaCommand(int16_t delta, UiOutgoingCommand* outCmd);

  UiPage page_ = UiPage::kNowPlaying;
  UiRemoteState state_;
  UiRemoteTick tick_;
  UiRemoteList list_;
  bool hasRemoteList_ = false;
  bool connected_ = false;
  uint32_t lastHeartbeatMs_ = 0U;
  uint32_t nextStateRequestMs_ = 0U;
  bool dirty_ = true;
  uint8_t listCursor_ = 0U;

  uint8_t settingsIndex_ = 0U;
  uint8_t wifiMode_ = 0U;
  uint8_t eqPreset_ = 0U;
  uint8_t brightness_ = 2U;
  bool screensaver_ = false;
};
