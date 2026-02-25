#include "ui/fx/v9/boing/boing_shadow_darken.h"
#include <stdint.h>

static inline uint16_t rgb565_half(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }

#ifndef UI_BOING_SHADOW_ASM
#define UI_BOING_SHADOW_ASM 1
#endif

static void darken_span_half_rgb565_32bit(uint16_t *line, int x0, int x1)
{
    if (x1 <= x0) return;

    int i = x0;

    if ((((uintptr_t)&line[i]) & 3u) && i < x1) {
        line[i] = rgb565_half(line[i]);
        i++;
    }

    uint32_t *p32 = (uint32_t *)&line[i];
    int n2 = (x1 - i) / 2;

    for (int k = 0; k < n2; k++) {
        uint32_t v = p32[k];
        v = (v >> 1) & 0x7BEF7BEFul;
        p32[k] = v;
    }

    i += n2 * 2;
    if (i < x1) line[i] = rgb565_half(line[i]);
}

#if UI_BOING_SHADOW_ASM && (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3))
extern void boing_shadow_darken_half_s3(uint16_t *p, int n_px);
#define BOING_HAS_S3_ASM 1
#else
#define BOING_HAS_S3_ASM 0
#endif

static inline int is_aligned_16(const void* p) { return ((((uintptr_t)p) & 15u) == 0u); }

void boing_shadow_darken_span_half_rgb565(uint16_t *line, int x0, int x1)
{
  if (!line || x1 <= x0) return;

#if BOING_HAS_S3_ASM
    int i = x0;

    while (i < x1 && !is_aligned_16(&line[i])) {
        line[i] = rgb565_half(line[i]);
        i++;
    }

    int n = x1 - i;
    int n8 = n & ~7;

    if (n8 >= 8 && is_aligned_16(&line[i])) {
        boing_shadow_darken_half_s3(&line[i], n8);
        i += n8;
    }

    if (i < x1) darken_span_half_rgb565_32bit(line, i, x1);
#else
    darken_span_half_rgb565_32bit(line, x0, x1);
#endif
}

int boing_shadow_asm_enabled(void)
{
#if BOING_HAS_S3_ASM
    return 1;
#else
    return 0;
#endif
}
