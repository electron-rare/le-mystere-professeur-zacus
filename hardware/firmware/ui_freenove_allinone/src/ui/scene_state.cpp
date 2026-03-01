// scene_state.cpp - precomputed scene state for LA overlay rendering.
#include "ui/scene_state.h"

SceneState SceneState::fromLaSample(bool in_locked,
                                    uint16_t in_freq_hz,
                                    int16_t in_cents,
                                    uint8_t in_confidence,
                                    uint8_t in_level_pct,
                                    uint8_t in_stability_pct) {
  SceneState state;
  state.locked = in_locked;
  state.freq_hz = in_freq_hz;
  state.cents = in_cents;
  state.confidence = (in_confidence > 100U) ? 100U : in_confidence;
  state.level_pct = (in_level_pct > 100U) ? 100U : in_level_pct;
  state.stability_pct = (in_stability_pct > 100U) ? 100U : in_stability_pct;
  state.abs_cents = (in_cents < 0) ? static_cast<int16_t>(-in_cents) : in_cents;

  if (state.locked) {
    state.status_text = "SIGNAL VERROUILLE";
    state.status_rgb = 0x9DFF63UL;
  } else if (state.freq_hz == 0U || state.confidence < 20U) {
    state.status_text = "AUCUN SIGNAL";
    state.status_rgb = 0x66B7FFUL;
  } else if (state.abs_cents <= 8) {
    state.status_text = "SIGNAL STABLE";
    state.status_rgb = 0xC9FF6EUL;
  } else if (state.cents < 0) {
    state.status_text = "MONTE EN FREQUENCE";
    state.status_rgb = 0xFFD772UL;
  } else {
    state.status_text = "DESCENDS EN FREQUENCE";
    state.status_rgb = 0xFFAA66UL;
  }

  return state;
}
