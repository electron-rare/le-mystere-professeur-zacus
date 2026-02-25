#include "ui/fx/v9/engine/timeline_load.h"

#include <ArduinoJson.h>

#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace fx {

namespace {

static PixelFormat parseFmt(const std::string& s) {
  if (s == "RGB565" || s == "rgb565") {
    return PixelFormat::RGB565;
  }
  return PixelFormat::I8;
}

static std::string toStringValue(JsonVariantConst value) {
  if (value.is<const char*>()) {
    const char* str = value.as<const char*>();
    return (str != nullptr) ? std::string(str) : std::string();
  }
  if (value.is<bool>()) {
    return value.as<bool>() ? "true" : "false";
  }
  if (value.is<long long>()) {
    return std::to_string(value.as<long long>());
  }
  if (value.is<unsigned long long>()) {
    return std::to_string(value.as<unsigned long long>());
  }
  if (value.is<double>()) {
    std::ostringstream stream;
    stream << std::setprecision(7) << value.as<double>();
    return stream.str();
  }
  if (value.isNull()) {
    return std::string();
  }
  return std::string();
}

static void fillStringMap(std::unordered_map<std::string, std::string>& dst, JsonObjectConst object) {
  for (JsonPairConst entry : object) {
    const char* key = entry.key().c_str();
    if (key == nullptr || key[0] == '\0') {
      continue;
    }
    dst[key] = toStringValue(entry.value());
  }
}

static bool isReservedModKey(const char* key) {
  if (key == nullptr) {
    return true;
  }
  return std::strcmp(key, "clip") == 0 || std::strcmp(key, "param") == 0 ||
         std::strcmp(key, "type") == 0 || std::strcmp(key, "args") == 0;
}

static bool isReservedEventKey(const char* key) {
  if (key == nullptr) {
    return true;
  }
  return std::strcmp(key, "t") == 0 || std::strcmp(key, "beat") == 0 ||
         std::strcmp(key, "bar") == 0 || std::strcmp(key, "type") == 0 ||
         std::strcmp(key, "args") == 0;
}

template <typename Predicate>
static void fillDirectArgs(std::unordered_map<std::string, std::string>& dst,
                           JsonObjectConst object,
                           Predicate is_reserved_key) {
  for (JsonPairConst entry : object) {
    const char* key = entry.key().c_str();
    if (is_reserved_key(key)) {
      continue;
    }
    if (key == nullptr || key[0] == '\0') {
      continue;
    }
    dst[key] = toStringValue(entry.value());
  }
}

}  // namespace

bool loadTimelineFromJson(Timeline& out, IJsonParser& parser, const std::string& text) {
  (void)parser;

  DynamicJsonDocument doc(16384);
  const DeserializationError error = deserializeJson(doc, text.c_str(), text.size());
  if (error) {
    return false;
  }
  if (!doc.is<JsonObjectConst>()) {
    return false;
  }
  const JsonObjectConst root = doc.as<JsonObjectConst>();

  out = Timeline{};

  const JsonObjectConst meta = root["meta"].as<JsonObjectConst>();
  if (!meta.isNull()) {
    out.meta.title = std::string(meta["title"] | "");
    out.meta.fps = meta["fps"] | 50;
    out.meta.bpm = meta["bpm"] | 125.0f;
    out.meta.seed = meta["seed"] | 1337U;

    const JsonObjectConst internal = meta["internal"].as<JsonObjectConst>();
    if (!internal.isNull()) {
      out.meta.internal.w = internal["w"] | 160;
      out.meta.internal.h = internal["h"] | 120;
      out.meta.internal.fmt = parseFmt(std::string(internal["fmt"] | "I8"));
    }
  }

  const JsonArrayConst clips = root["clips"].as<JsonArrayConst>();
  if (!clips.isNull()) {
    out.clips.reserve(clips.size());
    for (JsonVariantConst node : clips) {
      if (!node.is<JsonObjectConst>()) {
        continue;
      }
      const JsonObjectConst clip_obj = node.as<JsonObjectConst>();
      Clip clip;
      clip.id = std::string(clip_obj["id"] | "");
      clip.t0 = clip_obj["t0"] | 0.0f;
      clip.t1 = clip_obj["t1"] | 0.0f;
      clip.track = std::string(clip_obj["track"] | "BG");
      clip.fx = std::string(clip_obj["fx"] | "");
      clip.seed = clip_obj["seed"] | 0U;

      const JsonObjectConst params = clip_obj["params"].as<JsonObjectConst>();
      if (!params.isNull()) {
        fillStringMap(clip.params, params);
      }

      out.clips.push_back(clip);
    }
  }

  const JsonArrayConst mods = root["mods"].as<JsonArrayConst>();
  if (!mods.isNull()) {
    out.mods.reserve(mods.size());
    for (JsonVariantConst node : mods) {
      if (!node.is<JsonObjectConst>()) {
        continue;
      }
      const JsonObjectConst mod_obj = node.as<JsonObjectConst>();
      Modulation mod;
      mod.clip = std::string(mod_obj["clip"] | "");
      mod.param = std::string(mod_obj["param"] | "");
      mod.type = std::string(mod_obj["type"] | "");

      const JsonObjectConst args = mod_obj["args"].as<JsonObjectConst>();
      if (!args.isNull()) {
        fillStringMap(mod.args, args);
      }
      fillDirectArgs(mod.args, mod_obj, isReservedModKey);

      out.mods.push_back(mod);
    }
  }

  const JsonArrayConst events = root["events"].as<JsonArrayConst>();
  if (!events.isNull()) {
    out.events.reserve(events.size());
    for (JsonVariantConst node : events) {
      if (!node.is<JsonObjectConst>()) {
        continue;
      }
      const JsonObjectConst event_obj = node.as<JsonObjectConst>();
      Event event;
      event.t = event_obj["t"] | -1.0f;
      event.beat = event_obj["beat"] | -1;
      event.bar = event_obj["bar"] | -1;
      event.type = std::string(event_obj["type"] | "");

      const JsonObjectConst args = event_obj["args"].as<JsonObjectConst>();
      if (!args.isNull()) {
        fillStringMap(event.args, args);
      }
      fillDirectArgs(event.args, event_obj, isReservedEventKey);

      out.events.push_back(event);
    }
  }

  return true;
}

float paramFloat(const std::unordered_map<std::string, std::string>& m, const char* k, float def) {
  std::unordered_map<std::string, std::string>::const_iterator it = m.find(k ? k : "");
  if (it == m.end()) {
    return def;
  }
  return (float)std::strtod(it->second.c_str(), nullptr);
}

int paramInt(const std::unordered_map<std::string, std::string>& m, const char* k, int def) {
  std::unordered_map<std::string, std::string>::const_iterator it = m.find(k ? k : "");
  if (it == m.end()) {
    return def;
  }
  return (int)std::strtol(it->second.c_str(), nullptr, 10);
}

bool paramBool(const std::unordered_map<std::string, std::string>& m, const char* k, bool def) {
  std::unordered_map<std::string, std::string>::const_iterator it = m.find(k ? k : "");
  if (it == m.end()) {
    return def;
  }
  const std::string& s = it->second;
  if (s == "1" || s == "true" || s == "TRUE") {
    return true;
  }
  if (s == "0" || s == "false" || s == "FALSE") {
    return false;
  }
  return def;
}

const char* paramStr(const std::unordered_map<std::string, std::string>& m, const char* k, const char* def) {
  std::unordered_map<std::string, std::string>::const_iterator it = m.find(k ? k : "");
  if (it == m.end()) {
    return def;
  }
  return it->second.c_str();
}

}  // namespace fx
