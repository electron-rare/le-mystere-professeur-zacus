#include "basic_widgets.h"

#include <cstring>

#include "../layout_metrics.h"

namespace screen_gfx {

namespace {

void drawText(DisplayBackend& display, int16_t x, int16_t y, uint8_t size, const char* text) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print((text != nullptr) ? text : "");
}

}  // namespace

void drawHeader(DisplayBackend& display, const char* title, const char* rightTag) {
  display.fillRect(0, 0, kScreenWidth, kHeaderHeight, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  drawText(display, 2, 9, 1, title);
  if (rightTag != nullptr && rightTag[0] != '\0') {
    const int16_t rightX = static_cast<int16_t>(kScreenWidth - (strlen(rightTag) * 6) - 3);
    drawText(display, rightX, 9, 1, rightTag);
  }
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void drawProgressBar(DisplayBackend& display, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) {
  if (pct > 100U) {
    pct = 100U;
  }
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  const int16_t fill = ((w - 2) * static_cast<int16_t>(pct)) / 100;
  display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void drawVuMini(DisplayBackend& display, int16_t x, int16_t y, uint8_t pct, uint32_t nowMs) {
  constexpr uint8_t kBars = 5U;
  constexpr int16_t kBarW = 2;
  constexpr int16_t kBarGap = 1;
  constexpr int16_t kMaxH = 9;
  for (uint8_t i = 0; i < kBars; ++i) {
    const uint8_t phase = static_cast<uint8_t>((nowMs / 80U) + i * 11U);
    const uint8_t dyn = static_cast<uint8_t>((phase % 16U) * 6U);
    const uint8_t mixed = static_cast<uint8_t>((pct + dyn) / 2U);
    const int16_t h = 1 + (static_cast<int16_t>(mixed) * kMaxH) / 100;
    const int16_t bx = x + i * (kBarW + kBarGap);
    display.fillRect(bx, y + (kMaxH - h), kBarW, h, SSD1306_WHITE);
  }
}

void drawListRow(DisplayBackend& display,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 const char* text,
                 bool selected) {
  if (selected) {
    display.fillRect(x, y - 8, w, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  } else {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  }
  display.setTextSize(1);
  display.setCursor(x + 1, y);
  display.print((text != nullptr && text[0] != '\0') ? text : "-");
  if (selected) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  }
}

}  // namespace screen_gfx
