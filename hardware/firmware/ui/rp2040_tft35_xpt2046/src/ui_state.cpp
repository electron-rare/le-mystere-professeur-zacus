#include "ui_state.h"

#include <cstring>

#include "../include/ui_config.h"

namespace {

void copyText(char* out, size_t len, const char* in) {
  if (out == nullptr || len == 0U) {
    return;
  }
  out[0] = '\0';
  if (in == nullptr) {
    return;
  }
  snprintf(out, len, "%s", in);
}

int32_t clampI32(int32_t v, int32_t minV, int32_t maxV) {
  if (v < minV) {
    return minV;
  }
  if (v > maxV) {
    return maxV;
  }
  return v;
}

}  // namespace

void UiStateModel::begin() {
  state_ = UiRemoteState();
  tick_ = UiRemoteTick();
  list_ = UiRemoteList();
  list_.source = state_.source;
  list_.total = 0U;
  list_.count = 0U;
  connected_ = false;
  lastHeartbeatMs_ = 0U;
  nextStateRequestMs_ = 0U;
  page_ = UiPage::kNowPlaying;
  listCursor_ = 0U;
  settingsIndex_ = 0U;
  markDirty();
}

void UiStateModel::applyState(const UiRemoteState& state, uint32_t nowMs) {
  state_ = state;
  tick_.posSec = state.posSec;
  tick_.bufferPercent = state.bufferPercent;
  list_.source = state.source;
  connected_ = true;
  lastHeartbeatMs_ = nowMs;
  markDirty();
}

void UiStateModel::applyTick(const UiRemoteTick& tick, uint32_t nowMs) {
  tick_ = tick;
  if (tick.posSec >= 0) {
    state_.posSec = tick.posSec;
  }
  if (tick.bufferPercent >= -1) {
    state_.bufferPercent = tick.bufferPercent;
  }
  connected_ = true;
  lastHeartbeatMs_ = nowMs;
  markDirty();
}

void UiStateModel::applyList(const UiRemoteList& list, uint32_t nowMs) {
  list_ = list;
  hasRemoteList_ = true;
  if (listCursor_ >= list_.count) {
    listCursor_ = 0U;
  }
  connected_ = true;
  lastHeartbeatMs_ = nowMs;
  markDirty();
}

void UiStateModel::onHeartbeat(uint32_t nowMs) {
  connected_ = true;
  lastHeartbeatMs_ = nowMs;
}

void UiStateModel::updateConnection(uint32_t nowMs) {
  if (lastHeartbeatMs_ == 0U) {
    return;
  }
  if (static_cast<int32_t>(nowMs - lastHeartbeatMs_) > static_cast<int32_t>(ui_config::kHbTimeoutMs)) {
    if (connected_) {
      connected_ = false;
      markDirty();
    }
  }
}

bool UiStateModel::shouldRequestState(uint32_t nowMs) {
  if (connected_) {
    return false;
  }
  if (static_cast<int32_t>(nowMs - nextStateRequestMs_) < 0) {
    return false;
  }
  nextStateRequestMs_ = nowMs + ui_config::kRequestStateRetryMs;
  return true;
}

void UiStateModel::setPage(UiPage page) {
  if (page_ == page) {
    return;
  }
  page_ = page;
  markDirty();
}

void UiStateModel::toggleSource(UiOutgoingCommand* outCmd) {
  if (outCmd == nullptr) {
    return;
  }
  state_.source = (state_.source == UiSource::kSd) ? UiSource::kRadio : UiSource::kSd;
  list_.source = state_.source;
  outCmd->cmd = UiOutCmd::kSourceSet;
  copyText(outCmd->textValue, sizeof(outCmd->textValue), uiSourceToken(state_.source));
  markDirty();
}

bool UiStateModel::buildDeltaCommand(int16_t delta, UiOutgoingCommand* outCmd) {
  if (outCmd == nullptr || delta == 0) {
    return false;
  }
  outCmd->cmd = UiOutCmd::kStationDelta;
  outCmd->value = delta;
  return true;
}

bool UiStateModel::applyListDelta(int16_t delta, UiOutgoingCommand* outCmd) {
  if (delta == 0) {
    return false;
  }
  if (list_.count > 0U) {
    int16_t next = static_cast<int16_t>(listCursor_) + delta;
    if (next < 0) {
      next = 0;
    }
    if (next >= static_cast<int16_t>(list_.count)) {
      next = static_cast<int16_t>(list_.count - 1U);
    }
    listCursor_ = static_cast<uint8_t>(next);
    markDirty();
  }
  return buildDeltaCommand(delta, outCmd);
}

bool UiStateModel::applySettingsDelta(int8_t delta, UiOutgoingCommand* outCmd) {
  if (delta == 0) {
    return false;
  }
  const int8_t next = static_cast<int8_t>(settingsIndex_) + delta;
  settingsIndex_ = static_cast<uint8_t>(clampI32(next, 0, 3));
  markDirty();
  (void)outCmd;
  return false;
}

void UiStateModel::applySettingAction(UiOutgoingCommand* outCmd) {
  switch (settingsIndex_) {
    case 0:  // Wi-Fi mode indicator only (cycles local state)
      wifiMode_ = static_cast<uint8_t>((wifiMode_ + 1U) % 3U);
      break;
    case 1:  // EQ preset local only for now
      eqPreset_ = static_cast<uint8_t>((eqPreset_ + 1U) % 4U);
      break;
    case 2:  // Brightness local
      brightness_ = static_cast<uint8_t>((brightness_ + 1U) % 4U);
      break;
    case 3:  // Screensaver local
      screensaver_ = !screensaver_;
      break;
    default:
      break;
  }
  markDirty();
  if (outCmd != nullptr && settingsIndex_ == 0U) {
    outCmd->cmd = UiOutCmd::kRequestState;
  }
}

bool UiStateModel::handleNowTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd) {
  if (y >= 250U) {
    const uint16_t col = x / (ui_config::kScreenWidth / 5U);
    switch (col) {
      case 0:
        outCmd->cmd = UiOutCmd::kPrev;
        return true;
      case 1:
        outCmd->cmd = UiOutCmd::kPlayPause;
        return true;
      case 2:
        outCmd->cmd = UiOutCmd::kNext;
        return true;
      case 3:
        outCmd->cmd = UiOutCmd::kVolDelta;
        outCmd->value = -2;
        return true;
      case 4:
      default:
        outCmd->cmd = UiOutCmd::kVolDelta;
        outCmd->value = 2;
        return true;
    }
  }

  // Progress seek zone (SD only).
  if (y >= 200U && y <= 230U && state_.source == UiSource::kSd && state_.durSec > 1) {
    const uint16_t barX = 22U;
    const uint16_t barW = 360U;
    if (x >= barX && x <= (barX + barW)) {
      const uint32_t rel = static_cast<uint32_t>(x - barX);
      const uint32_t target =
          (rel * static_cast<uint32_t>(state_.durSec)) / static_cast<uint32_t>(barW);
      outCmd->cmd = UiOutCmd::kSeek;
      outCmd->value = static_cast<int32_t>(target);
      return true;
    }
  }

  // Badge source toggle.
  if (x <= 108U && y <= 44U) {
    toggleSource(outCmd);
    return true;
  }
  return false;
}

bool UiStateModel::handleListTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd) {
  if (y >= 250U) {
    const uint16_t col = x / (ui_config::kScreenWidth / 5U);
    switch (col) {
      case 0:
        return applyListDelta(-1, outCmd);
      case 1:
        return applyListDelta(1, outCmd);
      case 2:
        if (list_.count == 0U) {
          return false;
        }
        // Select highlighted line -> jump with delta relative to remote cursor if known.
        if (list_.count > listCursor_) {
          const int16_t absolute = static_cast<int16_t>(list_.offset + listCursor_);
          const int16_t remoteCursor = static_cast<int16_t>(list_.cursor);
          int16_t delta = absolute - remoteCursor;
          if (delta == 0) {
            outCmd->cmd = UiOutCmd::kPlayPause;
            return true;
          }
          delta = static_cast<int16_t>(clampI32(delta, -12, 12));
          return buildDeltaCommand(delta, outCmd);
        }
        return false;
      case 3:
        setPage(UiPage::kNowPlaying);
        return false;
      case 4:
      default:
        toggleSource(outCmd);
        return true;
    }
  }

  // Tapping rows.
  if (y >= 56U && y <= 230U) {
    const uint8_t row = static_cast<uint8_t>((y - 56U) / 44U);
    if (row < list_.count && row < 4U) {
      listCursor_ = row;
      markDirty();
      return false;
    }
  }
  return false;
}

bool UiStateModel::handleSettingsTap(uint16_t x, uint16_t y, UiOutgoingCommand* outCmd) {
  if (y >= 58U && y <= 226U) {
    const uint8_t row = static_cast<uint8_t>((y - 58U) / 42U);
    if (row < 4U) {
      settingsIndex_ = row;
      markDirty();
    }
  }

  if (y >= 250U) {
    const uint16_t col = x / (ui_config::kScreenWidth / 5U);
    switch (col) {
      case 0:
        return applySettingsDelta(-1, outCmd);
      case 1:
        return applySettingsDelta(1, outCmd);
      case 2:
        applySettingAction(outCmd);
        return outCmd != nullptr && outCmd->cmd != UiOutCmd::kNone;
      case 3:
        setPage(UiPage::kNowPlaying);
        return false;
      case 4:
      default:
        toggleSource(outCmd);
        return true;
    }
  }
  return false;
}

bool UiStateModel::onTap(uint16_t x, uint16_t y, uint32_t nowMs, UiOutgoingCommand* outCmd) {
  (void)nowMs;
  if (outCmd == nullptr) {
    return false;
  }
  *outCmd = UiOutgoingCommand();

  // Header tabs.
  if (y <= 34U) {
    if (x < 160U) {
      setPage(UiPage::kNowPlaying);
    } else if (x < 320U) {
      setPage(UiPage::kList);
    } else {
      setPage(UiPage::kSettings);
    }
    return false;
  }

  switch (page_) {
    case UiPage::kNowPlaying:
      return handleNowTap(x, y, outCmd);
    case UiPage::kList:
      return handleListTap(x, y, outCmd);
    case UiPage::kSettings:
      return handleSettingsTap(x, y, outCmd);
    default:
      return false;
  }
}

bool UiStateModel::onSwipe(int16_t dx, int16_t dy, uint32_t nowMs, UiOutgoingCommand* outCmd) {
  (void)nowMs;
  if (outCmd == nullptr) {
    return false;
  }
  *outCmd = UiOutgoingCommand();

  const int16_t adx = (dx < 0) ? -dx : dx;
  const int16_t ady = (dy < 0) ? -dy : dy;
  if (adx >= ady) {
    outCmd->cmd = (dx > 0) ? UiOutCmd::kNext : UiOutCmd::kPrev;
    return true;
  }
  outCmd->cmd = UiOutCmd::kVolDelta;
  outCmd->value = (dy < 0) ? 2 : -2;
  return true;
}

UiPage UiStateModel::page() const { return page_; }
UiSource UiStateModel::source() const { return state_.source; }
bool UiStateModel::connected() const { return connected_; }
bool UiStateModel::playing() const { return state_.playing; }
int32_t UiStateModel::volume() const { return clampI32(state_.volume, 0, 100); }
int32_t UiStateModel::posSec() const { return state_.posSec; }
int32_t UiStateModel::durSec() const { return state_.durSec; }
int32_t UiStateModel::rssi() const { return state_.rssi; }
int32_t UiStateModel::bufferPercent() const { return state_.bufferPercent; }
float UiStateModel::vu() const { return clampI32(static_cast<int32_t>(tick_.vu * 100.0f), 0, 100) / 100.0f; }
const char* UiStateModel::title() const { return state_.title; }
const char* UiStateModel::artist() const { return state_.artist; }
const char* UiStateModel::station() const { return state_.station; }
const char* UiStateModel::error() const { return state_.error; }
uint8_t UiStateModel::settingsIndex() const { return settingsIndex_; }
uint8_t UiStateModel::wifiMode() const { return wifiMode_; }
uint8_t UiStateModel::eqPreset() const { return eqPreset_; }
uint8_t UiStateModel::brightness() const { return brightness_; }
bool UiStateModel::screensaver() const { return screensaver_; }
const UiRemoteList& UiStateModel::list() const { return list_; }
uint8_t UiStateModel::listCursor() const { return listCursor_; }

void UiStateModel::markDirty() { dirty_ = true; }

bool UiStateModel::consumeDirty() {
  const bool was = dirty_;
  dirty_ = false;
  return was;
}
