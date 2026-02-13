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
  cursor_ = 0;
  offset_ = 0;
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
        moveCursor(-1);
      } else if (page_ != PlayerUiPage::kNowPlaying) {
        moveCursor(-1);
      }
      break;
    case 3:
      if (isLong) {
        moveCursor(1);
      } else if (page_ != PlayerUiPage::kNowPlaying) {
        moveCursor(1);
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
  out.cursor = cursor_;
  out.offset = offset_;
  out.dirty = dirty_;
  return out;
}

PlayerUiPage PlayerUiModel::page() const {
  return page_;
}

uint16_t PlayerUiModel::cursor() const {
  return cursor_;
}

uint16_t PlayerUiModel::offset() const {
  return offset_;
}

bool PlayerUiModel::consumeDirty() {
  const bool current = dirty_;
  dirty_ = false;
  return current;
}

void PlayerUiModel::clampBrowser() {
  if (page_ != PlayerUiPage::kBrowser) {
    cursor_ = 0;
    offset_ = 0;
    return;
  }
  if (browserCount_ == 0U) {
    cursor_ = 0;
    offset_ = 0;
    return;
  }
  if (cursor_ >= browserCount_) {
    cursor_ = static_cast<uint16_t>(browserCount_ - 1U);
    dirty_ = true;
  }
  if (cursor_ < offset_) {
    offset_ = cursor_;
    dirty_ = true;
  } else if (cursor_ >= static_cast<uint16_t>(offset_ + kBrowserPageSize)) {
    offset_ = static_cast<uint16_t>(cursor_ - (kBrowserPageSize - 1U));
    dirty_ = true;
  }
}

void PlayerUiModel::moveCursor(int16_t delta) {
  if (page_ != PlayerUiPage::kBrowser || browserCount_ == 0U) {
    return;
  }
  int32_t next = static_cast<int32_t>(cursor_) + static_cast<int32_t>(delta);
  if (next < 0) {
    next = 0;
  } else if (next >= static_cast<int32_t>(browserCount_)) {
    next = static_cast<int32_t>(browserCount_ - 1U);
  }
  if (static_cast<uint16_t>(next) != cursor_) {
    cursor_ = static_cast<uint16_t>(next);
    dirty_ = true;
  }
  clampBrowser();
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
