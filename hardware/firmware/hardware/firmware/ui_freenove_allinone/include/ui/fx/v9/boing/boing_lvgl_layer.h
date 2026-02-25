#pragma once
#include "lvgl.h"
#include "ui/fx/v9/boing/boing_ball.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *boing_lvgl_create(lv_obj_t *parent, boing_tables_t *t, boing_anim_t *a,
                            int screen_w, int screen_h);

#ifdef __cplusplus
}
#endif
