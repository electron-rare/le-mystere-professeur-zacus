#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "ui/fx/v9/engine/types.h"

namespace fx {

struct InternalFormat {
  int w = 160;
  int h = 120;
  PixelFormat fmt = PixelFormat::I8;
};

struct TimelineMeta {
  std::string title;
  int fps = 50;
  float bpm = 125.0f;
  uint32_t seed = 1337;
  InternalFormat internal;
};

struct Clip {
  std::string id;
  float t0 = 0.0f;
  float t1 = 0.0f;
  std::string track;   // BG/MID/UI
  std::string fx;      // effect name
  std::unordered_map<std::string, std::string> params; // stringly typed for portability
  uint32_t seed = 0;   // optional
};

struct Modulation {
  std::string clip;
  std::string param;
  std::string type; // sine/ramp/ease/beat_pulse/random_hold/toggle_on_bar
  std::unordered_map<std::string, std::string> args;
};

struct Event {
  // one of: t (seconds), beat, bar
  float t = -1.0f;
  int beat = -1;
  int bar = -1;
  std::string type;
  std::unordered_map<std::string, std::string> args;
};

struct Timeline {
  TimelineMeta meta;
  std::vector<Clip> clips;
  std::vector<Modulation> mods;
  std::vector<Event> events;
};

} // namespace fx
