// scene_state.h - precomputed scene state for LA overlay rendering.
#pragma once

#include <Arduino.h>

struct SceneState {
  bool locked = false;
  uint16_t freq_hz = 0U;
  int16_t cents = 0;
  uint8_t confidence = 0U;
  uint8_t level_pct = 0U;
  uint8_t stability_pct = 0U;
  int16_t abs_cents = 0;
  uint32_t status_rgb = 0x86CCFFUL;
  const char* status_text = "ECOUTE...";

  static SceneState fromLaSample(bool locked,
                                 uint16_t freq_hz,
                                 int16_t cents,
                                 uint8_t confidence,
                                 uint8_t level_pct,
                                 uint8_t stability_pct);
};
