#include "mp3_controller.h"

#include <cstring>
#include <cstdio>

namespace {

const char* boolToFlag(bool value) {
  return value ? "1" : "0";
}

void formatCaps(const PlayerBackendCapabilities& caps, char* out, size_t outLen) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  snprintf(out,
           outLen,
           "MP3:%s,WAV:%s,AAC:%s,FLAC:%s,OPUS:%s",
           boolToFlag(caps.mp3),
           boolToFlag(caps.wav),
           boolToFlag(caps.aac),
           boolToFlag(caps.flac),
           boolToFlag(caps.opus));
}

}  // namespace

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
  out.printf("[MP3_UI] %s page=%s cursor=%u offset=%u browse=%u queue_off=%u set_idx=%u tracks=%u\n",
             safeSource,
             playerUiPageLabel(ui_.page()),
             static_cast<unsigned int>(ui_.cursor()),
             static_cast<unsigned int>(ui_.offset()),
             static_cast<unsigned int>(ui_.browseCount()),
             static_cast<unsigned int>(ui_.queueOffset()),
             static_cast<unsigned int>(ui_.settingsIndex()),
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
<<<<<<< HEAD
      "[MP3_BACKEND_STATUS] %s mode=%s active=%s err=%s last_fallback_reason=%s attempts=%lu success=%lu fail=%lu retries=%lu fallback=%lu legacy=%lu tools=%lu tools_attempt=%lu tools_ok=%lu tools_fail=%lu tools_retry=%lu legacy_attempt=%lu legacy_ok=%lu legacy_fail=%lu legacy_retry=%lu\n",
=======
      "[MP3_BACKEND_STATUS] %s mode=%s active=%s err=%s attempts=%lu success=%lu fail=%lu retries=%lu fallback=%lu legacy=%lu tools=%lu last_fail=%s last_fallback=%s\n",
>>>>>>> feature/MPRC-RC1-mp3-audio
      safeSource,
      player_.backendModeLabel(),
      player_.activeBackendLabel(),
      player_.lastBackendError(),
      player_.lastFallbackReason(),
      static_cast<unsigned long>(stats.startAttempts),
      static_cast<unsigned long>(stats.startSuccess),
      static_cast<unsigned long>(stats.startFailures),
      static_cast<unsigned long>(stats.retriesScheduled),
      static_cast<unsigned long>(stats.fallbackCount),
      static_cast<unsigned long>(stats.legacyStarts),
      static_cast<unsigned long>(stats.audioToolsStarts),
<<<<<<< HEAD
      static_cast<unsigned long>(stats.audioToolsAttempts),
      static_cast<unsigned long>(stats.audioToolsSuccess),
      static_cast<unsigned long>(stats.audioToolsFailures),
      static_cast<unsigned long>(stats.audioToolsRetries),
      static_cast<unsigned long>(stats.legacyAttempts),
      static_cast<unsigned long>(stats.legacySuccess),
      static_cast<unsigned long>(stats.legacyFailures),
      static_cast<unsigned long>(stats.legacyRetries));
=======
      stats.lastFailureReason,
      stats.lastFallbackPath);
>>>>>>> feature/MPRC-RC1-mp3-audio
}

void Mp3Controller::printBrowseList(Print& out,
                                    const char* source,
                                    const char* path,
                                    uint16_t offset,
                                    uint16_t limit) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "list";
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
  const uint16_t total = player_.trackCount();
  const uint16_t queueOffset = ui_.queueOffset();
  if (count == 0U) {
    count = 5U;
  } else if (count > 12U) {
    count = 12U;
  }

  if (total == 0U) {
    out.printf("[MP3_QUEUE] %s empty tracks=0\n", safeSource);
    return;
  }

  uint16_t current = player_.currentTrackNumber();
  if (current == 0U || current > total) {
    current = 1U;
  }
  uint8_t emitted = 0U;
  for (uint8_t i = 0U; i < count && i < total; ++i) {
    const uint16_t nextOneBased =
        static_cast<uint16_t>(((current + queueOffset + i) % total) + 1U);
    const TrackEntry* entry = player_.trackEntryByNumber(nextOneBased);
    if (entry == nullptr) {
      continue;
    }
    const char* title = (entry->title[0] != '\0') ? entry->title : entry->path;
    out.printf("[MP3_QUEUE] %s #%u %s | %s\n",
               safeSource,
               static_cast<unsigned int>(nextOneBased),
               title,
               entry->codec[0] != '\0' ? entry->codec : "-");
    ++emitted;
  }

  if (emitted == 0U) {
    out.printf("[MP3_QUEUE] %s empty tracks=%u\n", safeSource, static_cast<unsigned int>(total));
  }
}

void Mp3Controller::printCapabilities(Print& out, const char* source) const {
  const char* safeSource = (source != nullptr && source[0] != '\0') ? source : "status";
<<<<<<< HEAD
  char toolsCaps[72] = {};
  char legacyCaps[72] = {};
  formatCaps(player_.audioToolsCapabilities(), toolsCaps, sizeof(toolsCaps));
  formatCaps(player_.legacyCapabilities(), legacyCaps, sizeof(legacyCaps));
  out.printf(
      "[MP3_CAPS] %s codecs=MP3,WAV,AAC,FLAC,OPUS tools=%s legacy=%s mode=%s active=%s\n",
=======
  const Mp3BackendRuntimeStats stats = player_.backendStats();
  out.printf(
      "[MP3_CAPS] %s codecs=MP3,WAV,AAC,FLAC,OPUS tools=WAV legacy=MP3,WAV,AAC,FLAC,OPUS mode=%s active=%s fallback=%lu last_fail=%s\n",
>>>>>>> feature/MPRC-RC1-mp3-audio
      safeSource,
      toolsCaps,
      legacyCaps,
      player_.backendModeLabel(),
      player_.activeBackendLabel(),
      static_cast<unsigned long>(stats.fallbackCount),
      stats.lastFailureReason);
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
