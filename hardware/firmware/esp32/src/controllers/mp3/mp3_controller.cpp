#include "mp3_controller.h"

Mp3Controller::Mp3Controller(Mp3Player& player, PlayerUiModel& ui) : player_(player), ui_(ui) {}

void Mp3Controller::update(uint32_t nowMs, bool allowPlayback) {
  player_.update(nowMs, allowPlayback);
  ui_.setBrowserBounds(player_.trackCount());
}

void Mp3Controller::refreshStorage() {
  player_.requestStorageRefresh(true);
}

void Mp3Controller::applyUiAction(const UiAction& action) {
  ui_.applyAction(action);
}

Mp3Player& Mp3Controller::player() {
  return player_;
}

const Mp3Player& Mp3Controller::player() const {
  return player_;
}
