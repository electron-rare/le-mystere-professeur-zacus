#include "radio_service.h"

namespace {

void copyText(char* out, size_t outLen, const char* text) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", text);
}

}  // namespace

void RadioService::begin(fs::FS* fs, const char* stationsPath, WifiService* wifiService) {
  wifi_ = wifiService;
  pipeline_.begin();
  snap_ = Snapshot();
  initialized_ = true;

  bool loaded = false;
  if (fs != nullptr && stationsPath != nullptr && stationsPath[0] != '\0') {
    loaded = repo_.loadFromFs(*fs, stationsPath);
  }
  if (!loaded) {
    repo_.loadDefaults();
    setEvent("STATIONS_DEFAULT");
  } else {
    setEvent("STATIONS_FS");
  }
}

void RadioService::update(uint32_t nowMs) {
  if (!initialized_) {
    return;
  }
  const bool networkReady = (wifi_ != nullptr) && (wifi_->isConnected() || wifi_->isApEnabled());
  pipeline_.update(nowMs, networkReady);
  updateSnapshot(nowMs);
}

bool RadioService::playById(uint16_t stationId, const char* source) {
  const int16_t idx = repo_.indexById(stationId);
  if (idx < 0) {
    setError("STATION_NOT_FOUND");
    return false;
  }
  return playStationIndex(static_cast<uint16_t>(idx), source);
}

bool RadioService::playByUrl(const char* url, const char* source) {
  if (!pipeline_.start(url, "AUTO", source)) {
    setError("PLAY_URL_FAIL");
    return false;
  }
  currentIndex_ = 0U;
  snap_.activeStationId = 0U;
  copyText(snap_.activeStationName, sizeof(snap_.activeStationName), "Direct URL");
  setEvent(source != nullptr ? source : "PLAY_URL");
  return true;
}

bool RadioService::stop(const char* source) {
  pipeline_.stop(source != nullptr ? source : "STOP");
  setEvent(source != nullptr ? source : "STOP");
  return true;
}

bool RadioService::next(const char* source) {
  const uint16_t total = repo_.count();
  if (total == 0U) {
    setError("NO_STATION");
    return false;
  }
  const uint16_t nextIndex = static_cast<uint16_t>((currentIndex_ + 1U) % total);
  return playStationIndex(nextIndex, source != nullptr ? source : "NEXT");
}

bool RadioService::prev(const char* source) {
  const uint16_t total = repo_.count();
  if (total == 0U) {
    setError("NO_STATION");
    return false;
  }
  const uint16_t prevIndex = (currentIndex_ == 0U) ? static_cast<uint16_t>(total - 1U)
                                                    : static_cast<uint16_t>(currentIndex_ - 1U);
  return playStationIndex(prevIndex, source != nullptr ? source : "PREV");
}

RadioService::Snapshot RadioService::snapshot() const {
  return snap_;
}

uint16_t RadioService::stationCount() const {
  return repo_.count();
}

const StationRepository::Station* RadioService::stationAt(uint16_t index) const {
  return repo_.at(index);
}

const StationRepository::Station* RadioService::currentStation() const {
  return repo_.at(currentIndex_);
}

bool RadioService::playStationIndex(uint16_t index, const char* source) {
  const StationRepository::Station* station = repo_.at(index);
  if (station == nullptr || !station->enabled) {
    setError("STATION_DISABLED");
    return false;
  }

  if (!pipeline_.start(station->url, station->codec, source != nullptr ? source : "PLAY_STATION")) {
    setError("STREAM_START_FAIL");
    return false;
  }

  currentIndex_ = index;
  snap_.activeStationId = station->id;
  copyText(snap_.activeStationName, sizeof(snap_.activeStationName), station->name);
  setEvent(source != nullptr ? source : "PLAY_STATION");
  return true;
}

void RadioService::updateSnapshot(uint32_t nowMs) {
  (void)nowMs;
  const StreamPipeline::Snapshot stream = pipeline_.snapshot();
  snap_.active = pipeline_.isActive();
  copyText(snap_.streamState, sizeof(snap_.streamState), StreamPipeline::stateLabel(stream.state));
  copyText(snap_.title, sizeof(snap_.title), stream.title);
  copyText(snap_.codec, sizeof(snap_.codec), stream.codec);
  snap_.bitrateKbps = stream.bitrateKbps;
  snap_.bufferPercent = stream.bufferPercent;
  if (strncmp(stream.lastError, "OK", sizeof(stream.lastError)) != 0) {
    copyText(snap_.lastError, sizeof(snap_.lastError), stream.lastError);
  }
}

void RadioService::setEvent(const char* event) {
  copyText(snap_.lastEvent, sizeof(snap_.lastEvent), event);
}

void RadioService::setError(const char* error) {
  copyText(snap_.lastError, sizeof(snap_.lastError), error);
}
