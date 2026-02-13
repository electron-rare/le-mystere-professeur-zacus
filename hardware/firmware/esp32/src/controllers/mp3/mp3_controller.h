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

  Mp3Player& player();
  const Mp3Player& player() const;

 private:
  Mp3Player& player_;
  PlayerUiModel& ui_;
};
