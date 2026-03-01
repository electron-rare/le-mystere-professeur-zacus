#pragma once
// font_select.h — 4 fonts selectable at runtime.
#include <stdint.h>

#include "font_6x8_basic.h"
#include "font_6x8_bold.h"
#include "font_6x8_outline.h"
#include "font_6x8_italic.h"

typedef enum {
  FX_FONT_BASIC = 0,
  FX_FONT_BOLD = 1,
  FX_FONT_OUTLINE = 2,
  FX_FONT_ITALIC = 3,
} fx_font_t;

static inline const uint8_t* fx_font_get_rows(fx_font_t f, char c) {
  switch (f) {
    default:
    case FX_FONT_BASIC:   return font6x8_basic_get_rows(c);
    case FX_FONT_BOLD:    return font6x8_bold_get_rows(c);
    case FX_FONT_OUTLINE: return font6x8_outline_get_rows(c);
    case FX_FONT_ITALIC:  return font6x8_italic_get_rows(c);
  }
}
