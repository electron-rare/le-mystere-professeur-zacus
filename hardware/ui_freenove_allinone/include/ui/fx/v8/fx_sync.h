#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  uint16_t bpm;
  uint32_t t_ms;
  uint32_t beat_index;
  uint8_t beat_in_bar;
  uint16_t bar_index;
  bool on_beat;
  bool on_bar;
  bool on_phrase;
} fx_sync_t;
void fx_sync_init(fx_sync_t* s, uint16_t bpm);
void fx_sync_step(fx_sync_t* s, uint32_t dt_ms, uint8_t phrase_bars);
#ifdef __cplusplus
}
#endif
