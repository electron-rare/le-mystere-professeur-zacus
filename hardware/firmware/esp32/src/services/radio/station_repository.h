#pragma once

#include <Arduino.h>
#include <FS.h>

class StationRepository {
 public:
  struct Station {
    uint16_t id = 0U;
    char name[40] = {};
    char url[160] = {};
    char codec[12] = {};
    bool favorite = false;
    bool enabled = true;
  };

  static constexpr uint16_t kMaxStations = 40U;

  bool loadFromFs(fs::FS& fs, const char* path);
  void loadDefaults();

  uint16_t count() const;
  const Station* at(uint16_t index) const;
  const Station* findById(uint16_t id) const;
  int16_t indexById(uint16_t id) const;

 private:
  bool parseJson(const String& json);
  bool parseObject(const String& obj, Station* out) const;
  static bool extractQuoted(const String& obj, const char* key, String* out);
  static bool extractUInt(const String& obj, const char* key, uint32_t* out);
  static bool extractBool(const String& obj, const char* key, bool* out);

  Station stations_[kMaxStations] = {};
  uint16_t count_ = 0U;
};
