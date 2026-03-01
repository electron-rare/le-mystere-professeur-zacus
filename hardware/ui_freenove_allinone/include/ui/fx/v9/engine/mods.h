#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fx {

// Automation target: we keep params as float values per clip instance.
// Each Fx can expose a ParamTable for modulations to write into.
struct ParamTable {
  std::unordered_map<std::string, float> f; // param -> value
};

struct ModState {
  // Used by random_hold / toggle mods to keep state deterministic.
  uint32_t rng = 0x12345678u;
  float held = 0.0f;
  int lastBeat = -1;
  int lastBar = -1;
  bool toggle = false;
};

enum class ModType : uint8_t {
  SINE,
  RAMP,
  EASE,
  BEAT_PULSE,
  RANDOM_HOLD,
  TOGGLE_ON_BAR
};

struct Mod {
  std::string clipId;
  std::string param;
  ModType type = ModType::SINE;

  // Generic args
  float base = 0.0f;
  float amp = 0.0f;
  float freqHz = 0.0f;
  float phase = 0.0f;

  float t0 = 0.0f;
  float t1 = 1.0f;
  float v0 = 0.0f;
  float v1 = 1.0f;

  float amount = 0.0f; // beat_pulse
  float decay = 0.75f;

  int holdBeats = 4;   // random_hold
  float minV = 0.0f, maxV = 1.0f;

  float a = 0.0f; // toggle value A
  float b = 1.0f; // toggle value B

  ModState st;
};

float easeInOut(float x);
float applyMod(const Mod& m, float clipT, float dt, uint32_t beat, uint32_t bar, float beatPhase,
               bool beatHit, bool barHit);

// Apply list of mods to a per-clip ParamTable.
void applyMods(std::vector<Mod>& mods, ParamTable& params, float clipT, float dt,
               uint32_t beat, uint32_t bar, float beatPhase, bool beatHit, bool barHit);

} // namespace fx
