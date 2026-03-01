#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void fx_luts_init(void);
int8_t fx_sin8(uint8_t phase);
int8_t fx_cos8(uint8_t phase);
uint8_t fx_fade_curve(uint8_t t);
uint16_t fx_palette_plasma565(uint8_t idx6);
uint16_t fx_palette_copper565(uint8_t idx);
#ifdef __cplusplus
}
#endif
