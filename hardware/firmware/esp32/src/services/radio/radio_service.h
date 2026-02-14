#pragma once

#include <Arduino.h>
#include <FS.h>

#include "station_repository.h"
#include "stream_pipeline.h"
#include "../network/wifi_service.h"

class RadioService {
 public:
  struct Snapshot {
    bool enabled = true;
    bool active = false;
    uint16_t activeStationId = 0U;
    char activeStationName[40] = "";
    char streamState[16] = "IDLE";
    char title[96] = "";
    char codec[12] = "AUTO";
    uint16_t bitrateKbps = 0U;
    uint8_t bufferPercent = 0U;
    char lastError[32] = "OK";
    char lastEvent[32] = "INIT";
  };

  void begin(fs::FS* fs, const char* stationsPath, WifiService* wifiService);
  void update(uint32_t nowMs);

  bool playById(uint16_t stationId, const char* source);
  bool playByUrl(const char* url, const char* source);
  bool stop(const char* source);
  bool next(const char* source);
  bool prev(const char* source);

  Snapshot snapshot() const;
  uint16_t stationCount() const;
  const StationRepository::Station* stationAt(uint16_t index) const;
  const StationRepository::Station* currentStation() const;

 private:
  bool playStationIndex(uint16_t index, const char* source);
  void updateSnapshot(uint32_t nowMs);
  void setEvent(const char* event);
  void setError(const char* error);

  StationRepository repo_;
  StreamPipeline pipeline_;
  WifiService* wifi_ = nullptr;
  Snapshot snap_;
  bool initialized_ = false;
  uint16_t currentIndex_ = 0U;
};
