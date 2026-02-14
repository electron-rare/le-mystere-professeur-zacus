#include "player_ui_model.h"

namespace {

constexpr uint16_t kListPageSize = 3U;
constexpr uint8_t kWifiModeMax = 2U;
constexpr uint8_t kEqPresetMax = 3U;
constexpr uint8_t kBrightnessMax = 4U;

}  // namespace

const char* playerUiPageLabel(PlayerUiPage page) {
  switch (page) {
    case PlayerUiPage::kListe:
      return "LISTE";
    case PlayerUiPage::kReglages:
      return "REGLAGES";
    case PlayerUiPage::kLecture:
    default:
      return "LECTURE";
  }
}

const char* playerUiSourceLabel(PlayerUiSource source) {
  return (source == PlayerUiSource::kRadio) ? "RADIO" : "SD";
}

const char* uiSettingLabel(UiSettingKey key) {
  switch (key) {
    case UiSettingKey::kEq:
      return "EQ";
    case UiSettingKey::kLuminosite:
      return "LUM";
    case UiSettingKey::kScreensaver:
      return "SAVE";
    case UiSettingKey::kWifi:
    default:
      return "WIFI";
  }
}

UiSettingKey uiSettingFromIndex(uint8_t idx) {
  switch (idx) {
    case 1:
      return UiSettingKey::kEq;
    case 2:
      return UiSettingKey::kLuminosite;
    case 3:
      return UiSettingKey::kScreensaver;
    case 0:
    default:
      return UiSettingKey::kWifi;
  }
}

uint8_t uiSettingIndex(UiSettingKey key) {
  switch (key) {
    case UiSettingKey::kEq:
      return 1U;
    case UiSettingKey::kLuminosite:
      return 2U;
    case UiSettingKey::kScreensaver:
      return 3U;
    case UiSettingKey::kWifi:
    default:
      return 0U;
  }
}

const char* uiWifiModeLabel(uint8_t mode) {
  switch (mode) {
    case 1:
      return "AP";
    case 2:
      return "OFF";
    case 0:
    default:
      return "AUTO";
  }
}

const char* uiEqLabel(uint8_t preset) {
  switch (preset) {
    case 1:
      return "WARM";
    case 2:
      return "VOICE";
    case 3:
      return "BASS";
    case 0:
    default:
      return "FLAT";
  }
}

const char* uiBrightnessLabel(uint8_t level) {
  switch (level) {
    case 0:
      return "LOW";
    case 1:
      return "MED";
    case 2:
      return "HIGH";
    case 3:
      return "MAX";
    case 4:
    default:
      return "AUTO";
  }
}

const char* uiOnOffLabel(bool enabled) {
  return enabled ? "ON" : "OFF";
}

void PlayerUiModel::reset() {
  page_ = PlayerUiPage::kLecture;
  source_ = PlayerUiSource::kSd;
  listCount_ = 0U;
  listCursor_ = 0U;
  listOffset_ = 0U;
  settingsIndex_ = 0U;
  wifiMode_ = 0U;
  eqPreset_ = 0U;
  brightness_ = 2U;
  screensaver_ = false;
  dirty_ = true;
}

void PlayerUiModel::setPage(PlayerUiPage page) {
  if (page_ == page) {
    return;
  }
  page_ = page;
  clampList();
  markDirty();
}

void PlayerUiModel::setSource(PlayerUiSource source) {
  if (source_ == source) {
    return;
  }
  source_ = source;
  listCursor_ = 0U;
  listOffset_ = 0U;
  markDirty();
}

void PlayerUiModel::toggleSource() {
  setSource(source_ == PlayerUiSource::kSd ? PlayerUiSource::kRadio : PlayerUiSource::kSd);
}

void PlayerUiModel::setListBounds(uint16_t count) {
  listCount_ = count;
  clampList();
}

void PlayerUiModel::applyAction(const UiAction& action) {
  if (action.hasTargetPage) {
    setPage(action.targetPage);
    return;
  }

  const UiNavAction nav = resolveAction(action);
  switch (nav) {
    case UiNavAction::kUp:
      if (page_ == PlayerUiPage::kListe) {
        moveListCursor(-1);
      } else if (page_ == PlayerUiPage::kReglages) {
        moveSettings(-1);
      }
      break;
    case UiNavAction::kDown:
      if (page_ == PlayerUiPage::kListe) {
        moveListCursor(1);
      } else if (page_ == PlayerUiPage::kReglages) {
        moveSettings(1);
      }
      break;
    case UiNavAction::kBack:
      nextPage();
      break;
    case UiNavAction::kModeToggle:
      toggleSource();
      setPage(PlayerUiPage::kLecture);
      break;
    case UiNavAction::kLeft:
    case UiNavAction::kRight:
    case UiNavAction::kOk:
    case UiNavAction::kNone:
    default:
      break;
  }
}

bool PlayerUiModel::applySettingAction() {
  switch (settingsKey()) {
    case UiSettingKey::kWifi:
      wifiMode_ = static_cast<uint8_t>((wifiMode_ + 1U) % (kWifiModeMax + 1U));
      markDirty();
      return true;
    case UiSettingKey::kEq:
      eqPreset_ = static_cast<uint8_t>((eqPreset_ + 1U) % (kEqPresetMax + 1U));
      markDirty();
      return true;
    case UiSettingKey::kLuminosite:
      brightness_ = static_cast<uint8_t>((brightness_ + 1U) % (kBrightnessMax + 1U));
      markDirty();
      return true;
    case UiSettingKey::kScreensaver:
      screensaver_ = !screensaver_;
      markDirty();
      return true;
    default:
      return false;
  }
}

bool PlayerUiModel::applySettingDelta(int8_t delta) {
  if (delta == 0) {
    return false;
  }
  switch (settingsKey()) {
    case UiSettingKey::kWifi: {
      int16_t next = static_cast<int16_t>(wifiMode_) + static_cast<int16_t>(delta);
      if (next < 0) {
        next = static_cast<int16_t>(kWifiModeMax);
      } else if (next > static_cast<int16_t>(kWifiModeMax)) {
        next = 0;
      }
      wifiMode_ = static_cast<uint8_t>(next);
      markDirty();
      return true;
    }
    case UiSettingKey::kEq: {
      int16_t next = static_cast<int16_t>(eqPreset_) + static_cast<int16_t>(delta);
      if (next < 0) {
        next = static_cast<int16_t>(kEqPresetMax);
      } else if (next > static_cast<int16_t>(kEqPresetMax)) {
        next = 0;
      }
      eqPreset_ = static_cast<uint8_t>(next);
      markDirty();
      return true;
    }
    case UiSettingKey::kLuminosite: {
      int16_t next = static_cast<int16_t>(brightness_) + static_cast<int16_t>(delta);
      if (next < 0) {
        next = static_cast<int16_t>(kBrightnessMax);
      } else if (next > static_cast<int16_t>(kBrightnessMax)) {
        next = 0;
      }
      brightness_ = static_cast<uint8_t>(next);
      markDirty();
      return true;
    }
    case UiSettingKey::kScreensaver:
      screensaver_ = !screensaver_;
      markDirty();
      return true;
    default:
      return false;
  }
}

PlayerUiSnapshot PlayerUiModel::snapshot() const {
  PlayerUiSnapshot out;
  out.page = page_;
  out.source = source_;
  out.cursor = cursor();
  out.offset = offset();
  out.listCount = listCount_;
  out.listOffset = listOffset_;
  out.settingsIndex = settingsIndex_;
  out.settingsKey = settingsKey();
  out.wifiMode = wifiMode_;
  out.eqPreset = eqPreset_;
  out.brightness = brightness_;
  out.screensaver = screensaver_;
  out.dirty = dirty_;
  return out;
}

PlayerUiPage PlayerUiModel::page() const {
  return page_;
}

PlayerUiSource PlayerUiModel::source() const {
  return source_;
}

uint16_t PlayerUiModel::cursor() const {
  switch (page_) {
    case PlayerUiPage::kListe:
      return listCursor_;
    case PlayerUiPage::kReglages:
      return settingsIndex_;
    case PlayerUiPage::kLecture:
    default:
      return 0U;
  }
}

uint16_t PlayerUiModel::offset() const {
  return (page_ == PlayerUiPage::kListe) ? listOffset_ : 0U;
}

uint16_t PlayerUiModel::listCount() const {
  return listCount_;
}

uint16_t PlayerUiModel::listOffset() const {
  return listOffset_;
}

uint8_t PlayerUiModel::settingsIndex() const {
  return settingsIndex_;
}

UiSettingKey PlayerUiModel::settingsKey() const {
  return uiSettingFromIndex(settingsIndex_);
}

uint8_t PlayerUiModel::wifiMode() const {
  return wifiMode_;
}

uint8_t PlayerUiModel::eqPreset() const {
  return eqPreset_;
}

uint8_t PlayerUiModel::brightness() const {
  return brightness_;
}

bool PlayerUiModel::screensaver() const {
  return screensaver_;
}

bool PlayerUiModel::consumeDirty() {
  const bool current = dirty_;
  dirty_ = false;
  return current;
}

UiNavAction PlayerUiModel::resolveAction(const UiAction& action) const {
  if (action.nav != UiNavAction::kNone) {
    return action.nav;
  }
  if (action.source == UiActionSource::kSerial) {
    return UiNavAction::kNone;
  }
  if (action.source == UiActionSource::kKeyLong && action.key == 6U) {
    return UiNavAction::kModeToggle;
  }
  switch (action.key) {
    case 1:
      return UiNavAction::kOk;
    case 2:
      return UiNavAction::kUp;
    case 3:
      return UiNavAction::kDown;
    case 4:
      return UiNavAction::kLeft;
    case 5:
      return UiNavAction::kRight;
    case 6:
      return UiNavAction::kBack;
    default:
      return UiNavAction::kNone;
  }
}

void PlayerUiModel::clampList() {
  if (listCount_ == 0U) {
    listCursor_ = 0U;
    listOffset_ = 0U;
    return;
  }
  if (listCursor_ >= listCount_) {
    listCursor_ = static_cast<uint16_t>(listCount_ - 1U);
    markDirty();
  }
  if (listCursor_ < listOffset_) {
    listOffset_ = listCursor_;
    markDirty();
  } else if (listCursor_ >= static_cast<uint16_t>(listOffset_ + kListPageSize)) {
    listOffset_ = static_cast<uint16_t>(listCursor_ - (kListPageSize - 1U));
    markDirty();
  }
}

void PlayerUiModel::moveListCursor(int16_t delta) {
  if (listCount_ == 0U) {
    return;
  }
  int32_t next = static_cast<int32_t>(listCursor_) + static_cast<int32_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next >= static_cast<int32_t>(listCount_)) {
    next = static_cast<int32_t>(listCount_ - 1U);
  }
  if (static_cast<uint16_t>(next) != listCursor_) {
    listCursor_ = static_cast<uint16_t>(next);
    markDirty();
  }
  clampList();
}

void PlayerUiModel::moveSettings(int8_t delta) {
  int16_t next = static_cast<int16_t>(settingsIndex_) + static_cast<int16_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next > static_cast<int16_t>(uiSettingIndex(UiSettingKey::kScreensaver))) {
    next = static_cast<int16_t>(uiSettingIndex(UiSettingKey::kScreensaver));
  }
  if (static_cast<uint8_t>(next) != settingsIndex_) {
    settingsIndex_ = static_cast<uint8_t>(next);
    markDirty();
  }
}

void PlayerUiModel::nextPage() {
  const PlayerUiPage previous = page_;
  switch (page_) {
    case PlayerUiPage::kLecture:
      page_ = PlayerUiPage::kListe;
      break;
    case PlayerUiPage::kListe:
      page_ = PlayerUiPage::kReglages;
      break;
    case PlayerUiPage::kReglages:
    default:
      page_ = PlayerUiPage::kLecture;
      break;
  }
  if (page_ != previous) {
    markDirty();
  }
}

void PlayerUiModel::prevPage() {
  const PlayerUiPage previous = page_;
  switch (page_) {
    case PlayerUiPage::kLecture:
      page_ = PlayerUiPage::kReglages;
      break;
    case PlayerUiPage::kListe:
      page_ = PlayerUiPage::kLecture;
      break;
    case PlayerUiPage::kReglages:
    default:
      page_ = PlayerUiPage::kListe;
      break;
  }
  if (page_ != previous) {
    markDirty();
  }
}

void PlayerUiModel::markDirty() {
  dirty_ = true;
}
