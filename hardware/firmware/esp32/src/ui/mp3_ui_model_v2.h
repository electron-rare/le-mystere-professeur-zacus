#pragma once

#include "player_ui_model.h"

// V2 aliases keep legacy code compiling while exposing explicit MP3 UI names.
using Mp3UiPageV2 = PlayerUiPage;
using Mp3UiSourceV2 = PlayerUiSource;
using Mp3UiActionV2 = UiNavAction;
using Mp3UiSnapshotV2 = PlayerUiSnapshot;
using Mp3UiModelV2 = PlayerUiModel;

constexpr Mp3UiPageV2 kMp3UiPageLecture = Mp3UiPageV2::kLecture;
constexpr Mp3UiPageV2 kMp3UiPageListe = Mp3UiPageV2::kListe;
constexpr Mp3UiPageV2 kMp3UiPageReglages = Mp3UiPageV2::kReglages;
