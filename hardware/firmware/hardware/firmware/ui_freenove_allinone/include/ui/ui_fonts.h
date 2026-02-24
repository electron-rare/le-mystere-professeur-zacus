#pragma once

#include <lvgl.h>

// Unified font/style registry for the Freenove LVGL UI stack.
namespace UiFonts {

void init();

// Compatibility aliases for callers expecting a compact API surface.
const lv_font_t* fontBody();
const lv_font_t* fontBodyBoldOrTitle();

const lv_font_t* fontBodyS();
const lv_font_t* fontBodyM();
const lv_font_t* fontBodyL();
const lv_font_t* fontTitle();
const lv_font_t* fontTitleXL();
const lv_font_t* fontMono();
const lv_font_t* fontPixel();

const lv_style_t* styleBody();
const lv_style_t* styleTitle();
const lv_style_t* styleTitleXL();
const lv_style_t* styleMono();
const lv_style_t* stylePixel();

}  // namespace UiFonts
