#include "ui/fx/v9/boing/boing_lvgl_layer.h"
#include "ui/fx/v9/boing/boing_shadow_darken.h"
#include <stdint.h>

#if LV_COLOR_DEPTH != 16
#error "Boing layer requires LV_COLOR_DEPTH=16 (RGB565)"
#endif
_Static_assert(sizeof(lv_color_t) == 2, "Expected 16-bit lv_color_t");

typedef struct {
    lv_obj_t *obj;
    lv_timer_t *tmr;
    boing_tables_t *t;
    boing_anim_t   *a;
    lv_area_t dirty_cur;
    lv_area_t dirty_prev;
    bool dirty_prev_valid;
    int screen_w, screen_h;
} boing_lvgl_ctx_t;

static uint16_t isqrt_u32(uint32_t x)
{
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1uL << 30;
    while (one > op) one >>= 2;
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (uint16_t)res;
}

static inline void area_clip(lv_area_t *a, int x1, int y1, int x2, int y2)
{
    if(a->x1 < x1) a->x1 = x1;
    if(a->y1 < y1) a->y1 = y1;
    if(a->x2 > x2) a->x2 = x2;
    if(a->y2 > y2) a->y2 = y2;
}

static inline void area_union(lv_area_t *out, const lv_area_t *a, const lv_area_t *b)
{
    out->x1 = (a->x1 < b->x1) ? a->x1 : b->x1;
    out->y1 = (a->y1 < b->y1) ? a->y1 : b->y1;
    out->x2 = (a->x2 > b->x2) ? a->x2 : b->x2;
    out->y2 = (a->y2 > b->y2) ? a->y2 : b->y2;
}

static bool area_intersect(lv_area_t *out, const lv_area_t *a, const lv_area_t *b)
{
    out->x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
    out->y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
    out->x2 = (a->x2 < b->x2) ? a->x2 : b->x2;
    out->y2 = (a->y2 < b->y2) ? a->y2 : b->y2;
    return (out->x1 <= out->x2) && (out->y1 <= out->y2);
}

static void compute_shadow_bounds(const boing_anim_t *a, int *rx, int *ry, int *shadow_y)
{
    int h = a->ground_y - (a->cy + BOING_R);
    if(h < 0) h = 0;
    if(h > a->jump_h) h = a->jump_h;

    int _rx = BOING_R - (h * BOING_R) / (a->jump_h * 3 + 1);
    int ry0 = BOING_R / 4;
    int _ry = ry0 - (h * ry0) / (a->jump_h * 2 + 1);
    if(_rx < 8) _rx = 8;
    if(_ry < 3) _ry = 3;

    *rx = _rx;
    *ry = _ry;
    *shadow_y = a->ground_y + 6;
}

static void compute_dirty(boing_lvgl_ctx_t *ctx)
{
    lv_area_t ball;
    ball.x1 = ctx->a->cx - BOING_R;
    ball.y1 = ctx->a->cy - BOING_R;
    ball.x2 = ball.x1 + BOING_N - 1;
    ball.y2 = ball.y1 + BOING_N - 1;

    int rx, ry, sy;
    compute_shadow_bounds(ctx->a, &rx, &ry, &sy);

    lv_area_t sh;
    sh.x1 = ctx->a->cx - rx;
    sh.x2 = ctx->a->cx + rx;
    sh.y1 = sy - ry;
    sh.y2 = sy + ry;

    area_union(&ctx->dirty_cur, &ball, &sh);

    // safety margin
    ctx->dirty_cur.x1 -= 1; ctx->dirty_cur.y1 -= 1;
    ctx->dirty_cur.x2 += 1; ctx->dirty_cur.y2 += 1;

    area_clip(&ctx->dirty_cur, 0, 0, ctx->screen_w - 1, ctx->screen_h - 1);
}

static void shadow_line_span(uint16_t *line,
                             int tile_x1_abs, int tile_w,
                             int y_abs,
                             const boing_anim_t *a,
                             int clip_x1_abs, int clip_x2_abs)
{
    int rx, ry, sy;
    compute_shadow_bounds(a, &rx, &ry, &sy);

    int dy = y_abs - sy;
    if(dy < -ry || dy > ry) return;

    uint32_t ry2 = (uint32_t)ry * (uint32_t)ry;
    uint32_t dy2 = (uint32_t)(dy * dy);
    if(dy2 > ry2) return;

    uint16_t s = isqrt_u32(ry2 - dy2);
    int dx = (int)((int64_t)rx * (int)s / ry);

    int x0_abs = a->cx - dx;
    int x1_abs = a->cx + dx + 1;

    // global clip
    if(x0_abs < clip_x1_abs) x0_abs = clip_x1_abs;
    if(x1_abs > clip_x2_abs + 1) x1_abs = clip_x2_abs + 1;
    if(x1_abs <= x0_abs) return;

    // tile clip
    int tile_x0_abs = tile_x1_abs;
    int tile_x1_excl = tile_x1_abs + tile_w;

    if(x0_abs < tile_x0_abs) x0_abs = tile_x0_abs;
    if(x1_abs > tile_x1_excl) x1_abs = tile_x1_excl;
    if(x1_abs <= x0_abs) return;

    int x0 = x0_abs - tile_x1_abs;
    int x1 = x1_abs - tile_x1_abs;

    boing_shadow_darken_span_half_rgb565(line, x0, x1);
}

static void boing_draw_event(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    boing_lvgl_ctx_t *ctx = (boing_lvgl_ctx_t *)lv_event_get_user_data(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    const lv_area_t *buf_area  = draw_ctx->buf_area;
    const lv_area_t *clip_area = draw_ctx->clip_area;

    lv_area_t tile;
    if(!area_intersect(&tile, buf_area, clip_area)) return;

    lv_area_t draw;
    if(!area_intersect(&draw, &tile, &ctx->dirty_cur)) return;

    const int stride = lv_area_get_width(buf_area);
    uint16_t *buf16 = (uint16_t *)draw_ctx->buf;

    int clip_x1_abs = draw.x1;
    int clip_x2_abs = draw.x2;

    for(int y = draw.y1; y <= draw.y2; y++) {
        int row = y - buf_area->y1;
        uint16_t *line = buf16 + row * stride;

        shadow_line_span(line, buf_area->x1, stride, y, ctx->a, clip_x1_abs, clip_x2_abs);
        boing_ball_line_render(line, buf_area->x1, clip_x1_abs, clip_x2_abs, y, ctx->t, ctx->a);
    }
}

static void boing_timer_cb(lv_timer_t *tmr)
{
    boing_lvgl_ctx_t *ctx = (boing_lvgl_ctx_t *)tmr->user_data;

    boing_anim_step(ctx->a, ctx->screen_w, ctx->screen_h);
    compute_dirty(ctx);

    lv_area_t inv = ctx->dirty_cur;
    if(ctx->dirty_prev_valid) {
        area_union(&inv, &ctx->dirty_prev, &ctx->dirty_cur);
    }

    lv_obj_invalidate_area(ctx->obj, &inv);

    ctx->dirty_prev = ctx->dirty_cur;
    ctx->dirty_prev_valid = true;
}

static void boing_delete_event(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_DELETE) return;

    boing_lvgl_ctx_t *ctx = (boing_lvgl_ctx_t *)lv_event_get_user_data(e);
    if(ctx->tmr) lv_timer_del(ctx->tmr);
    lv_mem_free(ctx);
}

lv_obj_t *boing_lvgl_create(lv_obj_t *parent, boing_tables_t *t, boing_anim_t *a,
                            int screen_w, int screen_h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, screen_w, screen_h);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);

    boing_lvgl_ctx_t *ctx = (boing_lvgl_ctx_t *)lv_mem_alloc(sizeof(*ctx));
    lv_memset_00(ctx, sizeof(*ctx));
    ctx->obj = obj;
    ctx->t = t;
    ctx->a = a;
    ctx->screen_w = screen_w;
    ctx->screen_h = screen_h;

    compute_dirty(ctx);
    ctx->dirty_prev = ctx->dirty_cur;
    ctx->dirty_prev_valid = true;

    lv_obj_add_event_cb(obj, boing_draw_event, LV_EVENT_DRAW_MAIN, ctx);
    lv_obj_add_event_cb(obj, boing_delete_event, LV_EVENT_DELETE, ctx);

    ctx->tmr = lv_timer_create(boing_timer_cb, 16, ctx);

    return obj;
}
