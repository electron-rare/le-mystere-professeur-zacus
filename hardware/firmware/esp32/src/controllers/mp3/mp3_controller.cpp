#include "mp3_controller.h"

#include <cstring>

namespace {

void copyText(char* out, size_t outLen, const char* in) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (in == nullptr || in[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", in);
}

void splitTitle(const char* in, char* line1, size_t len1, char* line2, size_t len2) {
  if (line1 != nullptr && len1 > 0U) {
    line1[0] = '\0';
  }
  if (line2 != nullptr && len2 > 0U) {
    line2[0] = '\0';
  }
  if (in == nullptr || in[0] == '\0') {
    return;
  }

  const size_t maxL1 = (len1 > 0U) ? (len1 - 1U) : 0U;
  const size_t maxL2 = (len2 > 0U) ? (len2 - 1U) : 0U;
  if (strlen(in) <= maxL1) {
    copyText(line1, len1, in);
    return;
  }

  size_t split = maxL1;
  while (split > 4U && in[split] != '\0' && in[split] != ' ') {
    --split;
  }
  if (split <= 4U || in[split] == '\0') {
    split = maxL1;
  }

  if (line1 != nullptr && len1 > 0U) {
    snprintf(line1, len1, "%.*s", static_cast<int>(split), in);
  }
  const char* tail = in + split;
  while (*tail == ' ') {
    ++tail;
  }
  if (line2 != nullptr && len2 > 0U && *tail != '\0') {
    snprintf(line2, len2, "%.*s", static_cast<int>(maxL2), tail);
  }
}

UiNavAction resolveNavAction(const UiAction& action) {
  if (action.nav != UiNavAction::kNone) {
    return action.nav;
  }
  if (action.source == UiActionSource::kKeyLong && action.key == 6U) {
    return UiNavAction::kBack;
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
      return UiNavAction::kModeToggle;
    default:
      return UiNavAction::kNone;
  }
}

const char* safeTrackTitle(const TrackEntry* entry) {
  if (entry == nullptr) {
    return "-";
  }
  if (entry->title[0] != '\0') {
    return entry->title;
  }
  return entry->path;
}

}  // namespace

Mp3Controller::Mp3Controller(Mp3Player& player, PlayerUiModel& ui, RadioService* radio)
    : player_(player), ui_(ui), radio_(radio) {}

void Mp3Controller::update(uint32_t nowMs, bool allowPlayback) {
  player_.update(nowMs, allowPlayback);
  if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
    ui_.setListBounds(radio_->stationCount());
  } else {
    ui_.setListBounds(player_.trackCount());
  }
}

void Mp3Controller::refreshStorage() {
  player_.requestStorageRefresh(true);
}

bool Mp3Controller::uiNavigate(UiNavAction action, uint32_t nowMs) {
  UiAction uiAction;
  uiAction.source = UiActionSource::kSerial;
  uiAction.nav = action;
  applyUiAction(uiAction);
  if (action == UiNavAction::kOk) {
    applyOkOnCurrentPage(nowMs);
  } else if (action == UiNavAction::kLeft) {
    applyLeftRightOnCurrentPage(-1, nowMs);
  } else if (action == UiNavAction::kRight) {
    applyLeftRightOnCurrentPage(1, nowMs);
  }
  return true;
}

void Mp3Controller::applyUiAction(const UiAction& action) {
  const PlayerUiPage beforePage = ui_.page();
  ui_.applyAction(action);
  const UiNavAction nav = resolveNavAction(action);
  if (nav == UiNavAction::kBack && beforePage == PlayerUiPage::kLecture) {
    ui_.toggleSource();
  }
  const uint32_t nowMs = millis();
  if (nav == UiNavAction::kOk) {
    applyOkOnCurrentPage(nowMs);
  } else if (nav == UiNavAction::kLeft) {
    applyLeftRightOnCurrentPage(-1, nowMs);
  } else if (nav == UiNavAction::kRight) {
    applyLeftRightOnCurrentPage(1, nowMs);
  }
}

const char* Mp3Controller::browsePath() const {
  return browsePath_.isEmpty() ? "/" : browsePath_.c_str();
}

void Mp3Controller::setBrowsePath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    browsePath_ = "/";
    return;
  }
  browsePath_ = path;
}

void Mp3Controller::printUiStatus(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
  const PlayerUiSnapshot snap = ui_.snapshot();
  out.printf(
      "[MP3_UI] %s page=%s source=%s page_v2=%s cursor=%u offset=%u count=%u setting=%s tracks=%u\n",
      safeSource,
      playerUiPageLabel(snap.page),
      playerUiSourceLabel(snap.source),
      playerUiPageLabel(snap.page),
      static_cast<unsigned int>(snap.cursor),
      static_cast<unsigned int>(snap.offset),
      static_cast<unsigned int>(snap.listCount),
      uiSettingLabel(snap.settingsKey),
      static_cast<unsigned int>(player_.trackCount()));
}

void Mp3Controller::printScanStatus(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
  const CatalogStats stats = player_.catalogStats();
  const Mp3ScanProgress progress = player_.scanProgress();
  out.printf("[MP3_SCAN] %s state=%s busy=%u tracks=%u folders=%u scan_ms=%lu indexed=%u metadata_best=%u\n",
             safeSource,
             player_.scanStateLabel(),
             player_.isScanBusy() ? 1U : 0U,
             static_cast<unsigned int>(stats.tracks),
             static_cast<unsigned int>(stats.folders),
             static_cast<unsigned long>(stats.scanMs),
             stats.indexed ? 1U : 0U,
             stats.metadataBestEffort ? 1U : 0U);
  out.printf("[MP3_SCAN] %s pending=%u force=%u reason=%s ticks=%lu elapsed=%lu budget_ms=%u entry_budget=%u\n",
             safeSource,
             progress.pendingRequest ? 1U : 0U,
             progress.forceRebuild ? 1U : 0U,
             progress.reason,
             static_cast<unsigned long>(progress.ticks),
             static_cast<unsigned long>(progress.elapsedMs),
             static_cast<unsigned int>(progress.tickBudgetMs),
             static_cast<unsigned int>(progress.tickEntryBudget));
}

void Mp3Controller::printScanProgress(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
  const Mp3ScanProgress progress = player_.scanProgress();
  const CatalogStats stats = player_.catalogStats();
  out.printf(
      "[MP3_SCAN_PROGRESS] %s state=%s active=%u pending=%u force=%u reason=%s depth=%u stack=%u folders=%u files=%u tracks=%u limit=%u tick_entries=%u tick_hits=%u ticks=%lu elapsed=%lu scan_ms=%lu\n",
      safeSource,
      player_.scanStateLabel(),
      progress.active ? 1U : 0U,
      progress.pendingRequest ? 1U : 0U,
      progress.forceRebuild ? 1U : 0U,
      progress.reason,
      static_cast<unsigned int>(progress.depth),
      static_cast<unsigned int>(progress.stackSize),
      static_cast<unsigned int>(progress.foldersScanned),
      static_cast<unsigned int>(progress.filesScanned),
      static_cast<unsigned int>(progress.tracksAccepted),
      progress.limitReached ? 1U : 0U,
      static_cast<unsigned int>(progress.entriesThisTick),
      static_cast<unsigned int>(progress.entryBudgetHits),
      static_cast<unsigned long>(progress.ticks),
      static_cast<unsigned long>(progress.elapsedMs),
      static_cast<unsigned long>(stats.scanMs));
}

void Mp3Controller::printBackendStatus(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
  const Mp3BackendRuntimeStats stats = player_.backendStats();
  out.printf(
      "[MP3_BACKEND_STATUS] %s mode=%s active=%s err=%s fallback_reason=%s attempts=%lu success=%lu fail=%lu retries=%lu fallback=%lu legacy=%lu tools=%lu tools_unsupported=%lu auto_heal=%lu\n",
      safeSource,
      player_.backendModeLabel(),
      player_.activeBackendLabel(),
      player_.lastBackendError(),
      stats.lastFallbackReason,
      static_cast<unsigned long>(stats.startAttempts),
      static_cast<unsigned long>(stats.startSuccess),
      static_cast<unsigned long>(stats.startFailures),
      static_cast<unsigned long>(stats.retriesScheduled),
      static_cast<unsigned long>(stats.fallbackCount),
      static_cast<unsigned long>(stats.legacyStarts),
      static_cast<unsigned long>(stats.audioToolsStarts),
      static_cast<unsigned long>(stats.audioToolsUnsupported),
      static_cast<unsigned long>(stats.autoHealToFallback));
}

void Mp3Controller::printBrowseList(Print& out,
                                    const char* source,
                                    const char* path,
                                    uint16_t offset,
                                    uint16_t limit) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "list";
  if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
    const uint16_t total = radio_->stationCount();
    const uint16_t end = static_cast<uint16_t>(offset + limit);
    for (uint16_t i = offset; i < total && i < end; ++i) {
      const StationRepository::Station* station = radio_->stationAt(i);
      if (station == nullptr) {
        continue;
      }
      out.printf("[%u] %s | %s | %s\n",
                 static_cast<unsigned int>(i + 1U),
                 station->name,
                 station->codec,
                 station->url);
    }
    out.printf("[MP3_BROWSE] %s path=/RADIO total=%u offset=%u limit=%u\n",
               safeSource,
               static_cast<unsigned int>(total),
               static_cast<unsigned int>(offset),
               static_cast<unsigned int>(limit));
    return;
  }

  const char* safePath = (path == nullptr || path[0] == '\0') ? "/" : path;
  if (!player_.isSdReady()) {
    out.printf("[MP3_BROWSE] %s OUT_OF_CONTEXT sd=0\n", safeSource);
    return;
  }
  const uint16_t total = player_.listTracks(safePath, offset, limit, out);
  out.printf("[MP3_BROWSE] %s path=%s total=%u offset=%u limit=%u\n",
             safeSource,
             safePath,
             static_cast<unsigned int>(total),
             static_cast<unsigned int>(offset),
             static_cast<unsigned int>(limit));
}

void Mp3Controller::printQueuePreview(Print& out, uint8_t count, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "preview";
  if (count == 0U) {
    count = 5U;
  } else if (count > 12U) {
    count = 12U;
  }

  if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
    const uint16_t total = radio_->stationCount();
    if (total == 0U) {
      out.printf("[MP3_QUEUE] %s empty stations=0\n", safeSource);
      return;
    }
    const uint16_t start = ui_.listOffset();
    for (uint8_t i = 0U; i < count && i < total; ++i) {
      const uint16_t idx = static_cast<uint16_t>((start + i) % total);
      const StationRepository::Station* station = radio_->stationAt(idx);
      if (station == nullptr) {
        continue;
      }
      out.printf("[MP3_QUEUE] %s #%u %s | %s\n",
                 safeSource,
                 static_cast<unsigned int>(idx + 1U),
                 station->name,
                 station->codec);
    }
    return;
  }

  const uint16_t total = player_.trackCount();
  const uint16_t queueOffset = ui_.listOffset();
  if (total == 0U) {
    out.printf("[MP3_QUEUE] %s empty tracks=0\n", safeSource);
    return;
  }

  uint16_t current = player_.currentTrackNumber();
  if (current == 0U || current > total) {
    current = 1U;
  }
  for (uint8_t i = 0U; i < count && i < total; ++i) {
    const uint16_t nextOneBased = static_cast<uint16_t>(((current + queueOffset + i) % total) + 1U);
    const TrackEntry* entry = player_.trackEntryByNumber(nextOneBased);
    if (entry == nullptr) {
      continue;
    }
    out.printf("[MP3_QUEUE] %s #%u %s | %s\n",
               safeSource,
               static_cast<unsigned int>(nextOneBased),
               safeTrackTitle(entry),
               entry->codec[0] != '\0' ? entry->codec : "-");
  }
}

void Mp3Controller::printCapabilities(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
  out.printf(
      "[MP3_CAPS] %s codecs=MP3,WAV,AAC,FLAC,OPUS tools=wav_only legacy=mp3,wav,aac,flac,opus mode=%s active=%s ui=LECTURE,LISTE,REGLAGES source=%s\n",
      safeSource,
      player_.backendModeLabel(),
      player_.activeBackendLabel(),
      currentSourceLabel());
}

void Mp3Controller::buildTextSlots(Mp3UiTextSlots* out, uint32_t nowMs) const {
  if (out == nullptr) {
    return;
  }
  *out = Mp3UiTextSlots();

  const PlayerUiSnapshot snap = ui_.snapshot();
  const char* sourceLabel = playerUiSourceLabel(snap.source);
  const char* pageLabel = playerUiPageLabel(snap.page);
  const uint16_t trackCount = player_.trackCount();
  const uint16_t currentTrack = player_.currentTrackNumber();

  if (snap.source == PlayerUiSource::kRadio && radio_ != nullptr) {
    const RadioService::Snapshot radioSnap = radio_->snapshot();
    splitTitle((radioSnap.title[0] != '\0') ? radioSnap.title : radioSnap.activeStationName,
               out->npTitle1,
               sizeof(out->npTitle1),
               out->npTitle2,
               sizeof(out->npTitle2));
    snprintf(out->npSub,
             sizeof(out->npSub),
             "%s %s %s %uk",
             sourceLabel,
             radioSnap.active ? "PLAY" : "STOP",
             radioSnap.codec,
             static_cast<unsigned int>(radioSnap.bitrateKbps));
  } else {
    const TrackEntry* currentEntry = player_.trackEntryByNumber(currentTrack);
    splitTitle(player_.currentTrackName().c_str(),
               out->npTitle1,
               sizeof(out->npTitle1),
               out->npTitle2,
               sizeof(out->npTitle2));
    snprintf(out->npSub,
             sizeof(out->npSub),
             "%s %s %u/%u %s",
             sourceLabel,
             player_.isPaused() ? "PAUSE" : "PLAY",
             static_cast<unsigned int>(currentTrack),
             static_cast<unsigned int>(trackCount),
             (currentEntry != nullptr && currentEntry->codec[0] != '\0') ? currentEntry->codec : "-");
  }

  if (snap.page == PlayerUiPage::kListe) {
    snprintf(out->listPath, sizeof(out->listPath), "%s %s", sourceLabel, listPathLabel());
    const uint16_t base = snap.offset;
    for (uint8_t row = 0U; row < 3U; ++row) {
      const uint16_t idx = static_cast<uint16_t>(base + row);
      char* target = (row == 0U) ? out->listRow0 : (row == 1U) ? out->listRow1 : out->listRow2;
      if (snap.source == PlayerUiSource::kRadio && radio_ != nullptr) {
        const StationRepository::Station* station = radio_->stationAt(idx);
        if (station != nullptr) {
          snprintf(target, 48U, "%s", station->name);
        }
      } else {
        const TrackEntry* entry = player_.trackEntryByNumber(static_cast<uint16_t>(idx + 1U));
        if (entry != nullptr) {
          snprintf(target, 48U, "%s", safeTrackTitle(entry));
        }
      }
    }
  } else {
    snprintf(out->listPath, sizeof(out->listPath), "%s %s", pageLabel, sourceLabel);
  }

  snprintf(out->setHint,
           sizeof(out->setHint),
           "W:%s EQ:%s L:%s SAV:%s",
           uiWifiModeLabel(snap.wifiMode),
           uiEqLabel(snap.eqPreset),
           uiBrightnessLabel(snap.brightness),
           uiOnOffLabel(snap.screensaver));

  (void)nowMs;
}

Mp3Player& Mp3Controller::player() {
  return player_;
}

const Mp3Player& Mp3Controller::player() const {
  return player_;
}

PlayerUiModel& Mp3Controller::ui() {
  return ui_;
}

const PlayerUiModel& Mp3Controller::ui() const {
  return ui_;
}

bool Mp3Controller::playSelectedListItem() {
  const uint16_t idx = ui_.cursor();
  if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
    const StationRepository::Station* station = radio_->stationAt(idx);
    if (station == nullptr) {
      return false;
    }
    return radio_->playById(station->id, "ui_list_select");
  }
  return player_.selectTrackByIndex(idx, true);
}

void Mp3Controller::applyOkOnCurrentPage(uint32_t nowMs) {
  (void)nowMs;
  switch (ui_.page()) {
    case PlayerUiPage::kListe:
      playSelectedListItem();
      break;
    case PlayerUiPage::kReglages:
      ui_.applySettingAction();
      break;
    case PlayerUiPage::kLecture:
    default:
      if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
        if (radio_->snapshot().active) {
          radio_->stop("ui_ok_toggle");
        } else {
          const StationRepository::Station* station = radio_->stationAt(ui_.cursor());
          if (station != nullptr) {
            radio_->playById(station->id, "ui_ok_toggle");
          }
        }
      } else {
        player_.togglePause();
      }
      break;
  }
}

void Mp3Controller::applyLeftRightOnCurrentPage(int8_t direction, uint32_t nowMs) {
  (void)nowMs;
  if (direction == 0) {
    return;
  }
  if (ui_.page() == PlayerUiPage::kReglages) {
    ui_.applySettingDelta(direction > 0 ? 1 : -1);
    return;
  }
  if (ui_.source() == PlayerUiSource::kRadio && radio_ != nullptr) {
    if (direction < 0) {
      radio_->prev("ui_left");
    } else {
      radio_->next("ui_right");
    }
    return;
  }
  if (direction < 0) {
    player_.previousTrack();
  } else {
    player_.nextTrack();
  }
}

const char* Mp3Controller::listPathLabel() const {
  if (ui_.source() == PlayerUiSource::kRadio) {
    return "/stations";
  }
  return browsePath();
}

const char* Mp3Controller::currentSourceLabel() const {
  return playerUiSourceLabel(ui_.source());
}
