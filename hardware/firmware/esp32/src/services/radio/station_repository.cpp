#include "station_repository.h"

namespace {

void copyText(char* out, size_t outLen, const String& in) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (in.length() == 0U) {
    return;
  }
  snprintf(out, outLen, "%s", in.c_str());
}

int findKeyValueStart(const String& obj, const char* key) {
  if (key == nullptr || key[0] == '\0') {
    return -1;
  }
  String marker = String("\"") + key + "\"";
  int pos = obj.indexOf(marker);
  if (pos < 0) {
    return -1;
  }
  pos = obj.indexOf(':', pos + static_cast<int>(marker.length()));
  if (pos < 0) {
    return -1;
  }
  ++pos;
  while (pos < static_cast<int>(obj.length()) && isspace(static_cast<unsigned char>(obj[pos])) != 0) {
    ++pos;
  }
  return pos;
}

}  // namespace

bool StationRepository::loadFromFs(fs::FS& fs, const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  fs::File f = fs.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  String json;
  json.reserve(static_cast<size_t>(f.size()) + 8U);
  while (f.available()) {
    json += static_cast<char>(f.read());
  }
  f.close();
  return parseJson(json);
}

void StationRepository::loadDefaults() {
  count_ = 0U;

  Station s1 = {};
  s1.id = 1U;
  snprintf(s1.name, sizeof(s1.name), "NOVA Radio");
  snprintf(s1.url, sizeof(s1.url), "http://novazz.ice.infomaniak.ch/novazz-128.mp3");
  snprintf(s1.codec, sizeof(s1.codec), "MP3");
  s1.favorite = true;
  stations_[count_++] = s1;

  Station s2 = {};
  s2.id = 2U;
  snprintf(s2.name, sizeof(s2.name), "FG Chic");
  snprintf(s2.url, sizeof(s2.url), "http://radiofg.impek.com/fg");
  snprintf(s2.codec, sizeof(s2.codec), "MP3");
  stations_[count_++] = s2;

  Station s3 = {};
  s3.id = 3U;
  snprintf(s3.name, sizeof(s3.name), "SomaFM Groove");
  snprintf(s3.url, sizeof(s3.url), "http://ice1.somafm.com/groovesalad-128-mp3");
  snprintf(s3.codec, sizeof(s3.codec), "MP3");
  stations_[count_++] = s3;
}

uint16_t StationRepository::count() const {
  return count_;
}

const StationRepository::Station* StationRepository::at(uint16_t index) const {
  if (index >= count_) {
    return nullptr;
  }
  return &stations_[index];
}

int16_t StationRepository::indexById(uint16_t id) const {
  for (uint16_t i = 0U; i < count_; ++i) {
    if (stations_[i].id == id) {
      return static_cast<int16_t>(i);
    }
  }
  return -1;
}

bool StationRepository::parseJson(const String& json) {
  count_ = 0U;
  if (json.length() == 0U) {
    return false;
  }

  int pos = 0;
  while (pos >= 0 && count_ < kMaxStations) {
    const int begin = json.indexOf('{', pos);
    if (begin < 0) {
      break;
    }
    const int end = json.indexOf('}', begin + 1);
    if (end < 0) {
      break;
    }
    String obj = json.substring(begin, end + 1);
    Station station = {};
    if (parseObject(obj, &station)) {
      stations_[count_++] = station;
    }
    pos = end + 1;
  }
  return count_ > 0U;
}

bool StationRepository::parseObject(const String& obj, Station* out) const {
  if (out == nullptr) {
    return false;
  }

  uint32_t id = 0U;
  String name;
  String url;
  String codec;
  bool enabled = true;
  bool favorite = false;

  if (!extractUInt(obj, "id", &id)) {
    return false;
  }
  if (!extractQuoted(obj, "name", &name)) {
    return false;
  }
  if (!extractQuoted(obj, "url", &url)) {
    return false;
  }
  (void)extractQuoted(obj, "codec", &codec);
  (void)extractBool(obj, "enabled", &enabled);
  (void)extractBool(obj, "favorite", &favorite);

  out->id = static_cast<uint16_t>(id);
  copyText(out->name, sizeof(out->name), name);
  copyText(out->url, sizeof(out->url), url);
  if (codec.length() == 0) {
    codec = "AUTO";
  }
  copyText(out->codec, sizeof(out->codec), codec);
  out->enabled = enabled;
  out->favorite = favorite;
  return out->name[0] != '\0' && out->url[0] != '\0';
}

bool StationRepository::extractQuoted(const String& obj, const char* key, String* out) {
  if (out == nullptr) {
    return false;
  }
  const int pos = findKeyValueStart(obj, key);
  if (pos < 0 || pos >= static_cast<int>(obj.length()) || obj[pos] != '"') {
    return false;
  }
  const int end = obj.indexOf('"', pos + 1);
  if (end < 0) {
    return false;
  }
  *out = obj.substring(pos + 1, end);
  return true;
}

bool StationRepository::extractUInt(const String& obj, const char* key, uint32_t* out) {
  if (out == nullptr) {
    return false;
  }
  const int pos = findKeyValueStart(obj, key);
  if (pos < 0) {
    return false;
  }
  char* endPtr = nullptr;
  const unsigned long value = strtoul(obj.c_str() + pos, &endPtr, 10);
  if (endPtr == obj.c_str() + pos) {
    return false;
  }
  *out = static_cast<uint32_t>(value);
  return true;
}

bool StationRepository::extractBool(const String& obj, const char* key, bool* out) {
  if (out == nullptr) {
    return false;
  }
  const int pos = findKeyValueStart(obj, key);
  if (pos < 0) {
    return false;
  }
  if (strncmp(obj.c_str() + pos, "true", 4) == 0) {
    *out = true;
    return true;
  }
  if (strncmp(obj.c_str() + pos, "false", 5) == 0) {
    *out = false;
    return true;
  }
  return false;
}
