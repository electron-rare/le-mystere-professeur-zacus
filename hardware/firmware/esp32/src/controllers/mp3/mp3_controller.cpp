#include "mp3_controller.h"

#include <cstring>

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
  out.printf(
      "[MP3_CAPS] %s codecs=MP3,WAV,AAC,FLAC,OPUS tools=WAV legacy=MP3,WAV,AAC,FLAC,OPUS mode=%s active=%s\n",
      safeSource,
      player_.backendModeLabel(),
      player_.activeBackendLabel());
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
