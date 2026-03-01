#include "ui/fx/v9/engine/mods.h"
#include <cmath>

namespace fx {

static inline uint32_t xorshift32(uint32_t& s)
{
  uint32_t x = s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s = x;
  return x;
}

float easeInOut(float x)
{
  // smoothstep
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;
  return x * x * (3.0f - 2.0f * x);
}

float applyMod(const Mod& m, float clipT, float /*dt*/, uint32_t beat, uint32_t bar, float beatPhase,
               bool beatHit, bool barHit)
{
  switch (m.type) {
    case ModType::SINE: {
      float v = m.base + m.amp * sinf(2.0f * 3.14159265f * m.freqHz * clipT + m.phase);
      return v;
    }
    case ModType::RAMP: {
      if (clipT <= m.t0) return m.v0;
      if (clipT >= m.t1) return m.v1;
      float x = (clipT - m.t0) / (m.t1 - m.t0);
      return m.v0 + (m.v1 - m.v0) * x;
    }
    case ModType::EASE: {
      if (clipT <= m.t0) return m.v0;
      if (clipT >= m.t1) return m.v1;
      float x = (clipT - m.t0) / (m.t1 - m.t0);
      x = easeInOut(x);
      return m.v0 + (m.v1 - m.v0) * x;
    }
    case ModType::BEAT_PULSE: {
      // amount on beat, exponential decay within beat; user should add this to a base value.
      float pulse = 0.0f;
      if (beatHit) pulse = m.amount;
      // Within beat, let it decay: pulse * decay^(phase*beats)
      float d = powf(std::max(0.001f, m.decay), beatPhase * 1.0f);
      return pulse * d;
    }
    case ModType::RANDOM_HOLD: {
      // Deterministic: change every holdBeats on beat boundary
      (void)bar; (void)barHit;
      // Not fully stateful here because applyMod is const; state is handled in applyMods.
      return m.base;
    }
    case ModType::TOGGLE_ON_BAR: {
      (void)beat; (void)beatHit;
      // State is handled in applyMods.
      return m.base;
    }
  }
  return 0.0f;
}

void applyMods(std::vector<Mod>& mods, ParamTable& params, float clipT, float dt,
               uint32_t beat, uint32_t bar, float beatPhase, bool beatHit, bool barHit)
{
  for (Mod& m : mods) {
    float v = 0.0f;

    if (m.type == ModType::RANDOM_HOLD) {
      if (beatHit) {
        if (m.st.lastBeat < 0) m.st.lastBeat = (int)beat;
        int beatsSince = (int)beat - m.st.lastBeat;
        if (beatsSince >= m.holdBeats) {
          m.st.lastBeat = (int)beat;
          uint32_t r = xorshift32(m.st.rng);
          float u = (float)(r & 0x00FFFFFFu) / (float)0x01000000u;
          m.st.held = m.minV + (m.maxV - m.minV) * u;
        }
      }
      v = m.st.held;
    }
    else if (m.type == ModType::TOGGLE_ON_BAR) {
      if (barHit) {
        if (m.st.lastBar < 0) m.st.lastBar = (int)bar;
        if ((int)bar != m.st.lastBar) {
          m.st.lastBar = (int)bar;
          m.st.toggle = !m.st.toggle;
        }
      }
      v = m.st.toggle ? m.b : m.a;
    }
    else if (m.type == ModType::BEAT_PULSE) {
      // add to existing param
      v = applyMod(m, clipT, dt, beat, bar, beatPhase, beatHit, barHit);
      params.f[m.param] += v;
      continue;
    }
    else {
      v = applyMod(m, clipT, dt, beat, bar, beatPhase, beatHit, barHit);
    }

    params.f[m.param] = v;
  }
}

} // namespace fx
