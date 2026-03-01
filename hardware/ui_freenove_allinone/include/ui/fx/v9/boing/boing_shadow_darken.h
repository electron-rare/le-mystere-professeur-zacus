#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void boing_shadow_darken_span_half_rgb565(uint16_t *line, int x0, int x1);
int boing_shadow_asm_enabled(void);

#ifdef __cplusplus
}
#endif
