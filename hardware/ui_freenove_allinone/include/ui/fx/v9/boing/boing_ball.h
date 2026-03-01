#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOING_N
#define BOING_N 96
#endif

#define BOING_R (BOING_N/2)

#ifndef BOING_CHECK_SHIFT
#define BOING_CHECK_SHIFT 5
#endif

typedef struct {
    uint8_t  x0;
    uint8_t  len;
    uint16_t offset;
} boing_row_t;

typedef struct {
    boing_row_t row[BOING_N];
    uint16_t tex_count;
    uint32_t *tex; // u8 | v8<<8 | (diff4|spec4<<4)<<16
    uint16_t lut[2][16][16];
} boing_tables_t;

typedef struct {
    uint8_t angle;
    uint8_t phase_u;
    uint8_t speed;
    uint8_t rot_speed;
    int cx, cy;
    int ground_y;
    int jump_h;
} boing_anim_t;

bool boing_init_tables(boing_tables_t *t);
void boing_free_tables(boing_tables_t *t);
void boing_build_lut(boing_tables_t *t, uint16_t red565, uint16_t white565, float spec_weight);

void boing_anim_init(boing_anim_t *a, int ground_y, int jump_h);
void boing_anim_step(boing_anim_t *a, int screen_w, int screen_h);

void boing_ball_line_render(uint16_t *line, int tile_x1_abs, int clip_x1_abs, int clip_x2_abs,
                            int y_abs, const boing_tables_t *t, const boing_anim_t *a);

#ifdef __cplusplus
}
#endif
