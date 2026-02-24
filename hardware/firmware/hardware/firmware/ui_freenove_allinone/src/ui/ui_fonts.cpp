#include "ui_fonts.h"

#include <ui_freenove_config.h>

#ifndef UI_FONT_PIXEL_ENABLE
#define UI_FONT_PIXEL_ENABLE 1
#endif

#ifndef UI_FONT_EXTERNAL_SET
#define UI_FONT_EXTERNAL_SET 1
#endif

#if UI_FONT_EXTERNAL_SET
// Generated LVGL font declarations (optional external pack).
LV_FONT_DECLARE(lv_font_inter_14);
LV_FONT_DECLARE(lv_font_inter_18);
LV_FONT_DECLARE(lv_font_inter_24);
LV_FONT_DECLARE(lv_font_inter_32);
LV_FONT_DECLARE(lv_font_orbitron_28);
LV_FONT_DECLARE(lv_font_orbitron_40);
LV_FONT_DECLARE(lv_font_ibmplexmono_14);
LV_FONT_DECLARE(lv_font_ibmplexmono_18);
#if UI_FONT_PIXEL_ENABLE
LV_FONT_DECLARE(lv_font_pressstart2p_16);
LV_FONT_DECLARE(lv_font_pressstart2p_24);
#endif
#endif

namespace UiFonts {

namespace {

bool g_inited = false;
lv_style_t g_style_body;
lv_style_t g_style_title;
lv_style_t g_style_title_xl;
lv_style_t g_style_mono;
lv_style_t g_style_pixel;

}  // namespace

void init() {
  if (g_inited) {
    return;
  }

  lv_style_init(&g_style_body);
  lv_style_set_text_font(&g_style_body, fontBodyM());

  lv_style_init(&g_style_title);
  lv_style_set_text_font(&g_style_title, fontTitle());
  lv_style_set_text_letter_space(&g_style_title, 1);

  lv_style_init(&g_style_title_xl);
  lv_style_set_text_font(&g_style_title_xl, fontTitleXL());
  lv_style_set_text_letter_space(&g_style_title_xl, 1);

  lv_style_init(&g_style_mono);
  lv_style_set_text_font(&g_style_mono, fontMono());

  lv_style_init(&g_style_pixel);
  lv_style_set_text_font(&g_style_pixel, fontPixel());

  g_inited = true;
}

const lv_font_t* fontBodyS() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_inter_14;
#else
  return &lv_font_montserrat_14;
#endif
}

const lv_font_t* fontBodyM() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_inter_18;
#else
  return &lv_font_montserrat_18;
#endif
}

const lv_font_t* fontBodyL() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_inter_24;
#else
  return &lv_font_montserrat_24;
#endif
}

const lv_font_t* fontTitle() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_orbitron_28;
#else
  return &lv_font_montserrat_28;
#endif
}

const lv_font_t* fontTitleXL() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_orbitron_40;
#else
  return &lv_font_montserrat_40;
#endif
}

const lv_font_t* fontMono() {
#if UI_FONT_EXTERNAL_SET
  return &lv_font_ibmplexmono_18;
#else
  return &lv_font_montserrat_18;
#endif
}

const lv_font_t* fontPixel() {
#if UI_FONT_EXTERNAL_SET && UI_FONT_PIXEL_ENABLE
  return &lv_font_pressstart2p_16;
#elif UI_FONT_EXTERNAL_SET
  return &lv_font_inter_14;
#elif UI_FONT_PIXEL_ENABLE
  return &lv_font_montserrat_24;
#else
  return &lv_font_montserrat_14;
#endif
}

const lv_style_t* styleBody() {
  return &g_style_body;
}

const lv_style_t* styleTitle() {
  return &g_style_title;
}

const lv_style_t* styleTitleXL() {
  return &g_style_title_xl;
}

const lv_style_t* styleMono() {
  return &g_style_mono;
}

const lv_style_t* stylePixel() {
  return &g_style_pixel;
}

}  // namespace UiFonts
