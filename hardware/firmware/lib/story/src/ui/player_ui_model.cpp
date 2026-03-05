#include "player_ui_model.h"

namespace {

constexpr uint16_t kBrowserPageSize = 5;

}  // namespace

const char* playerUiPageLabel(PlayerUiPage page) {
  switch (page) {
    case PlayerUiPage::kBrowser:
      return "BROWSE";
    case PlayerUiPage::kQueue:
      return "QUEUE";
    case PlayerUiPage::kSettings:
      return "SET";
    case PlayerUiPage::kNowPlaying:
    default:
      return "NOW";
  }
}

void PlayerUiModel::reset() {
  page_ = PlayerUiPage::kNowPlaying;
  browserCount_ = 0;
  browserCursor_ = 0;
  browserOffset_ = 0;
  queueOffset_ = 0;
  settingsIndex_ = 0;
  dirty_ = true;
}

void PlayerUiModel::setPage(PlayerUiPage page) {
  if (page_ == page) {
    return;
  }
  page_ = page;
  clampBrowser();
  dirty_ = true;
}

void PlayerUiModel::setBrowserBounds(uint16_t count) {
  browserCount_ = count;
  clampBrowser();
}

void PlayerUiModel::applyAction(const UiAction& action) {
  if (action.hasTargetPage) {
    setPage(action.targetPage);
    return;
  }

  if (action.source == UiActionSource::kSerial) {
    return;
  }

  const bool isLong = (action.source == UiActionSource::kKeyLong);
  switch (action.key) {
    case 2:
      if (isLong) {
        if (page_ == PlayerUiPage::kBrowser) {
          moveBrowserCursor(-1);
        } else if (page_ == PlayerUiPage::kQueue) {
          moveQueueOffset(-1);
        } else if (page_ == PlayerUiPage::kSettings) {
          moveSettings(-1);
        }
      } else if (page_ == PlayerUiPage::kBrowser) {
        moveBrowserCursor(-1);
      } else if (page_ == PlayerUiPage::kQueue) {
        moveQueueOffset(-1);
      } else if (page_ == PlayerUiPage::kSettings) {
        moveSettings(-1);
      }
      break;
    case 3:
      if (isLong) {
        if (page_ == PlayerUiPage::kBrowser) {
          moveBrowserCursor(1);
        } else if (page_ == PlayerUiPage::kQueue) {
          moveQueueOffset(1);
        } else if (page_ == PlayerUiPage::kSettings) {
          moveSettings(1);
        }
      } else if (page_ == PlayerUiPage::kBrowser) {
        moveBrowserCursor(1);
      } else if (page_ == PlayerUiPage::kQueue) {
        moveQueueOffset(1);
      } else if (page_ == PlayerUiPage::kSettings) {
        moveSettings(1);
      }
      break;
    case 6:
      if (isLong) {
        nextPage();
      } else {
        prevPage();
      }
      break;
    default:
      break;
  }
}

PlayerUiSnapshot PlayerUiModel::snapshot() const {
  PlayerUiSnapshot out;
  out.page = page_;
  out.cursor = cursor();
  out.offset = offset();
  out.browseCount = browserCount_;
  out.queueOffset = queueOffset_;
  out.settingsIndex = settingsIndex_;
  out.dirty = dirty_;
  return out;
}

PlayerUiPage PlayerUiModel::page() const {
  return page_;
}

uint16_t PlayerUiModel::cursor() const {
  switch (page_) {
    case PlayerUiPage::kBrowser:
      return browserCursor_;
    case PlayerUiPage::kQueue:
      return queueOffset_;
    case PlayerUiPage::kSettings:
      return settingsIndex_;
    case PlayerUiPage::kNowPlaying:
    default:
      return 0U;
  }
}

uint16_t PlayerUiModel::offset() const {
  switch (page_) {
    case PlayerUiPage::kBrowser:
      return browserOffset_;
    case PlayerUiPage::kQueue:
      return queueOffset_;
    case PlayerUiPage::kSettings:
      return 0U;
    case PlayerUiPage::kNowPlaying:
    default:
      return 0U;
  }
}

uint16_t PlayerUiModel::browseCount() const {
  return browserCount_;
}

uint16_t PlayerUiModel::queueOffset() const {
  return queueOffset_;
}

uint8_t PlayerUiModel::settingsIndex() const {
  return settingsIndex_;
}

bool PlayerUiModel::consumeDirty() {
  const bool current = dirty_;
  dirty_ = false;
  return current;
}

void PlayerUiModel::clampBrowser() {
  if (browserCount_ == 0U) {
    browserCursor_ = 0;
    browserOffset_ = 0;
    return;
  }
  if (browserCursor_ >= browserCount_) {
    browserCursor_ = static_cast<uint16_t>(browserCount_ - 1U);
    dirty_ = true;
  }
  if (browserCursor_ < browserOffset_) {
    browserOffset_ = browserCursor_;
    dirty_ = true;
  } else if (browserCursor_ >= static_cast<uint16_t>(browserOffset_ + kBrowserPageSize)) {
    browserOffset_ = static_cast<uint16_t>(browserCursor_ - (kBrowserPageSize - 1U));
    dirty_ = true;
  }
}

void PlayerUiModel::moveBrowserCursor(int16_t delta) {
  if (browserCount_ == 0U) {
    return;
  }
  int32_t next = static_cast<int32_t>(browserCursor_) + static_cast<int32_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next >= static_cast<int32_t>(browserCount_)) {
    next = static_cast<int32_t>(browserCount_ - 1U);
  }
  if (static_cast<uint16_t>(next) != browserCursor_) {
    browserCursor_ = static_cast<uint16_t>(next);
    dirty_ = true;
  }
  clampBrowser();
}

void PlayerUiModel::moveQueueOffset(int16_t delta) {
  const uint16_t maxOffset = (browserCount_ > 0U) ? static_cast<uint16_t>(browserCount_ - 1U) : 0U;
  int32_t next = static_cast<int32_t>(queueOffset_) + static_cast<int32_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next > static_cast<int32_t>(maxOffset)) {
    next = static_cast<int32_t>(maxOffset);
  }
  if (static_cast<uint16_t>(next) != queueOffset_) {
    queueOffset_ = static_cast<uint16_t>(next);
    dirty_ = true;
  }
}

void PlayerUiModel::moveSettings(int8_t delta) {
  int16_t next = static_cast<int16_t>(settingsIndex_) + static_cast<int16_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next > 2) {
    next = 2;
  }
  if (static_cast<uint8_t>(next) != settingsIndex_) {
    settingsIndex_ = static_cast<uint8_t>(next);
    dirty_ = true;
  }
}

void PlayerUiModel::nextPage() {
  const PlayerUiPage previous = page_;
  switch (page_) {
    case PlayerUiPage::kNowPlaying:
      page_ = PlayerUiPage::kBrowser;
      break;
    case PlayerUiPage::kBrowser:
      page_ = PlayerUiPage::kQueue;
      break;
    case PlayerUiPage::kQueue:
      page_ = PlayerUiPage::kSettings;
      break;
    case PlayerUiPage::kSettings:
    default:
      page_ = PlayerUiPage::kNowPlaying;
      break;
  }
  if (page_ != previous) {
    dirty_ = true;
    clampBrowser();
  }
}

void PlayerUiModel::prevPage() {
  const PlayerUiPage previous = page_;
  switch (page_) {
    case PlayerUiPage::kNowPlaying:
      page_ = PlayerUiPage::kSettings;
      break;
    case PlayerUiPage::kBrowser:
      page_ = PlayerUiPage::kNowPlaying;
      break;
    case PlayerUiPage::kQueue:
      page_ = PlayerUiPage::kBrowser;
      break;
    case PlayerUiPage::kSettings:
    default:
      page_ = PlayerUiPage::kQueue;
      break;
  }
  if (page_ != previous) {
    dirty_ = true;
    clampBrowser();
  }
}
