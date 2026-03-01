#pragma once

#include <Arduino.h>

#include "../display_backend.h"

namespace screen_gfx {

void drawHeader(DisplayBackend& display, const char* title, const char* rightTag = nullptr);
void drawProgressBar(DisplayBackend& display, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct);
void drawVuMini(DisplayBackend& display, int16_t x, int16_t y, uint8_t pct, uint32_t nowMs);
void drawListRow(DisplayBackend& display,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 const char* text,
                 bool selected);

}  // namespace screen_gfx
