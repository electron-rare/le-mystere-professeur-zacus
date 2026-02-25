#include "ui/fx/v9/boing/boing_ball.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t g_sin256[256];
static bool g_sin_init = false;

static void sin256_init(void)
{
    if (g_sin_init) return;
    for (int i = 0; i < 256; i++) {
        float a = (float)i * (2.0f * (float)M_PI / 256.0f);
        g_sin256[i] = (int16_t)lrintf(sinf(a) * 32767.0f);
    }
    g_sin_init = true;
}

static inline int16_t sin256(uint8_t a) { return g_sin256[a]; }
static inline int16_t cos256(uint8_t a) { return g_sin256[(uint8_t)(a + 64)]; }

static inline uint8_t clamp_u8_int(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static inline void rgb565_to_rgb888(uint16_t c, int *r, int *g, int *b)
{
    int rr = (c >> 11) & 31;
    int gg = (c >> 5) & 63;
    int bb = c & 31;
    *r = (rr * 255 + 15) / 31;
    *g = (gg * 255 + 31) / 63;
    *b = (bb * 255 + 15) / 31;
}

static inline uint16_t rgb888_to_rgb565(int r, int g, int b)
{
    uint16_t rr = (uint16_t)((r * 31 + 127) / 255);
    uint16_t gg = (uint16_t)((g * 63 + 127) / 255);
    uint16_t bb = (uint16_t)((b * 31 + 127) / 255);
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

bool boing_init_tables(boing_tables_t *t)
{
    if (!t) return false;
    memset(t, 0, sizeof(*t));

    const float R = (float)BOING_R;
    const float invR = 1.0f / R;

    uint16_t count = 0;
    for (int y = 0; y < BOING_N; y++) {
        int x0 = BOING_N, x1 = -1;
        for (int x = 0; x < BOING_N; x++) {
            float dx = ((float)x + 0.5f - R) * invR;
            float dy = ((float)y + 0.5f - R) * invR;
            if (dx*dx + dy*dy <= 1.0f) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
            }
        }
        if (x1 >= x0) {
            uint8_t len = (uint8_t)(x1 - x0 + 1);
            t->row[y].x0 = (uint8_t)x0;
            t->row[y].len = len;
            t->row[y].offset = count;
            count = (uint16_t)(count + len);
        } else {
            t->row[y].x0 = 0;
            t->row[y].len = 0;
            t->row[y].offset = count;
        }
    }

    t->tex_count = count;
    t->tex = (uint32_t *)malloc((size_t)count * sizeof(uint32_t));
    if (!t->tex) return false;

    float Lx = -0.45f, Ly = -0.35f, Lz = 0.82f;
    float Ll = 1.0f / sqrtf(Lx*Lx + Ly*Ly + Lz*Lz);
    Lx *= Ll; Ly *= Ll; Lz *= Ll;

    float Hx = Lx, Hy = Ly, Hz = (Lz + 1.0f);
    float Hl = 1.0f / sqrtf(Hx*Hx + Hy*Hy + Hz*Hz);
    Hx *= Hl; Hy *= Hl; Hz *= Hl;

    const float ambient = 0.25f;
    const float kd = 0.85f;
    const float ks = 0.90f;
    const float shininess = 36.0f;

    uint16_t idx = 0;
    for (int y = 0; y < BOING_N; y++) {
        uint8_t x0 = t->row[y].x0;
        uint8_t len = t->row[y].len;
        if (!len) continue;

        for (int i = 0; i < len; i++) {
            int x = (int)x0 + i;

            float nx = ((float)x + 0.5f - R) * invR;
            float ny = ((float)y + 0.5f - R) * invR;
            float rr = nx*nx + ny*ny;
            float nz = sqrtf(fmaxf(0.0f, 1.0f - rr));

            float u = atan2f(nx, nz);
            float v = asinf(ny);
            float un = (u * (1.0f / (2.0f * (float)M_PI))) + 0.5f;
            float vn = (v * (1.0f / (float)M_PI)) + 0.5f;

            int u8 = (int)floorf(un * 256.0f) & 255;
            int v8 = (int)floorf(vn * 256.0f) & 255;

            float ndl = nx*Lx + ny*Ly + nz*Lz;
            if (ndl < 0.0f) ndl = 0.0f;

            float diff = ambient + kd * ndl;
            if (diff > 1.0f) diff = 1.0f;

            float ndh = nx*Hx + ny*Hy + nz*Hz;
            if (ndh < 0.0f) ndh = 0.0f;

            float spec = ks * powf(ndh, shininess);
            if (spec > 1.0f) spec = 1.0f;

            int diff4 = (int)lrintf(diff * 15.0f);
            int spec4 = (int)lrintf(spec * 15.0f);
            if (diff4 < 0) diff4 = 0; if (diff4 > 15) diff4 = 15;
            if (spec4 < 0) spec4 = 0; if (spec4 > 15) spec4 = 15;

            uint8_t ds = (uint8_t)((diff4 & 0x0F) | ((spec4 & 0x0F) << 4));
            t->tex[idx++] = (uint32_t)u8 | ((uint32_t)v8 << 8) | ((uint32_t)ds << 16);
        }
    }

    return true;
}

void boing_free_tables(boing_tables_t *t)
{
    if (!t) return;
    free(t->tex);
    t->tex = NULL;
    t->tex_count = 0;
}

void boing_build_lut(boing_tables_t *t, uint16_t red565, uint16_t white565, float spec_weight)
{
    if (!t) return;

    int br[2], bg[2], bb[2];
    rgb565_to_rgb888(red565, &br[0], &bg[0], &bb[0]);
    rgb565_to_rgb888(white565, &br[1], &bg[1], &bb[1]);

    if (spec_weight < 0.0f) spec_weight = 0.0f;
    if (spec_weight > 1.0f) spec_weight = 1.0f;

    for (int c = 0; c < 2; c++) {
        for (int diff4 = 0; diff4 < 16; diff4++) {
            float diff = (float)diff4 / 15.0f;
            for (int spec4 = 0; spec4 < 16; spec4++) {
                float spec = ((float)spec4 / 15.0f) * spec_weight;

                int r = (int)lrintf((float)br[c] * diff + 255.0f * spec);
                int g = (int)lrintf((float)bg[c] * diff + 255.0f * spec);
                int b = (int)lrintf((float)bb[c] * diff + 255.0f * spec);

                r = clamp_u8_int(r);
                g = clamp_u8_int(g);
                b = clamp_u8_int(b);

                t->lut[c][diff4][spec4] = rgb888_to_rgb565(r, g, b);
            }
        }
    }
}

void boing_anim_init(boing_anim_t *a, int ground_y, int jump_h)
{
    if (!a) return;
    sin256_init();

    a->angle = 0;
    a->phase_u = 0;
    a->speed = 2;
    a->rot_speed = 5;

    a->ground_y = ground_y;
    a->jump_h = jump_h;

    a->cx = 160;
    a->cy = ground_y - BOING_R;
}

void boing_anim_step(boing_anim_t *a, int screen_w, int screen_h)
{
    (void)screen_h;
    if (!a) return;

    a->angle = (uint8_t)(a->angle + a->speed);
    a->phase_u = (uint8_t)(a->phase_u + a->rot_speed);

    int amp_x = (screen_w - BOING_N) / 2;
    int center_x = screen_w / 2;

    int16_t s = sin256(a->angle);
    int16_t c = cos256(a->angle);

    a->cx = center_x + (int)((int32_t)s * amp_x / 32767);

    int32_t ac = (c < 0) ? -c : c;
    int32_t bounce_q15 = (int32_t)(((int64_t)ac * (int64_t)ac) >> 15);

    int y_off = (int)((int64_t)a->jump_h * bounce_q15 / 32767);
    a->cy = a->ground_y - BOING_R - y_off;
}

void boing_ball_line_render(uint16_t *line, int tile_x1_abs, int clip_x1_abs, int clip_x2_abs,
                            int y_abs, const boing_tables_t *t, const boing_anim_t *a)
{
    if (!line || !t || !t->tex || !a) return;

    int y0 = a->cy - BOING_R;
    int ly = y_abs - y0;
    if ((unsigned)ly >= BOING_N) return;

    boing_row_t row = t->row[ly];
    if (!row.len) return;

    int x_base = a->cx - BOING_R;
    int x0_abs = x_base + row.x0;
    int x1_abs = x0_abs + row.len;

    int c0 = (x0_abs < clip_x1_abs) ? clip_x1_abs : x0_abs;
    int c1 = (x1_abs > (clip_x2_abs + 1)) ? (clip_x2_abs + 1) : x1_abs;
    if (c1 <= c0) return;

    int skip = c0 - x0_abs;
    uint16_t tex_i = (uint16_t)(row.offset + skip);

    for (int x_abs = c0; x_abs < c1; x_abs++, tex_i++) {
        uint32_t p = t->tex[tex_i];
        uint8_t u  = (uint8_t)(p & 0xFF);
        uint8_t v  = (uint8_t)((p >> 8) & 0xFF);
        uint8_t ds = (uint8_t)((p >> 16) & 0xFF);

        uint8_t urot  = (uint8_t)(u + a->phase_u);
        uint8_t check = (uint8_t)(((urot >> BOING_CHECK_SHIFT) ^ (v >> BOING_CHECK_SHIFT)) & 1);
        uint8_t diff4 = ds & 0x0F;
        uint8_t spec4 = ds >> 4;

        int x = x_abs - tile_x1_abs;
        line[x] = t->lut[check][diff4][spec4];
    }
}
