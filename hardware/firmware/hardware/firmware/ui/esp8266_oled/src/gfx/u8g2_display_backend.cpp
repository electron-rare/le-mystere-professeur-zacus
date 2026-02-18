#include "u8g2_display_backend.h"

namespace {

const uint8_t* fontForSize(uint8_t size) {
  switch (size) {
    case 2:
      return u8g2_font_10x20_tf;
    case 1:
    default:
      return u8g2_font_6x10_tf;
  }
}

}  // namespace

U8g2DisplayBackend::U8g2DisplayBackend()
    : u8g2_(U8G2_R0, /* reset=*/U8X8_PIN_NONE) {}

bool U8g2DisplayBackend::begin(uint8_t i2cAddress) {
  u8g2_.setI2CAddress(static_cast<uint8_t>(i2cAddress << 1U));
  u8g2_.begin();
  u8g2_.setFontMode(1);
  u8g2_.setBitmapMode(1);
  applyFont();
  setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  clearDisplay();
  display();
  return true;
}

void U8g2DisplayBackend::clearDisplay() {
  u8g2_.clearBuffer();
}

void U8g2DisplayBackend::display() {
  u8g2_.sendBuffer();
}

void U8g2DisplayBackend::drawPixel(int16_t x, int16_t y, uint16_t color) {
  applyColor(color);
  u8g2_.drawPixel(x, y);
}

void U8g2DisplayBackend::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  applyColor(color);
  u8g2_.drawLine(x0, y0, x1, y1);
}

void U8g2DisplayBackend::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  applyColor(color);
  u8g2_.drawFrame(x, y, w, h);
}

void U8g2DisplayBackend::drawRoundRect(int16_t x,
                                       int16_t y,
                                       int16_t w,
                                       int16_t h,
                                       int16_t r,
                                       uint16_t color) {
  applyColor(color);
  u8g2_.drawRFrame(x, y, w, h, r);
}

void U8g2DisplayBackend::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  applyColor(color);
  u8g2_.drawBox(x, y, w, h);
}

void U8g2DisplayBackend::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  applyColor(color);
  u8g2_.drawCircle(x0, y0, r);
}

void U8g2DisplayBackend::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  applyColor(color);
  u8g2_.drawHLine(x, y, w);
}

void U8g2DisplayBackend::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  applyColor(color);
  u8g2_.drawVLine(x, y, h);
}

void U8g2DisplayBackend::setCursor(int16_t x, int16_t y) {
  u8g2_.setCursor(x, y);
}

void U8g2DisplayBackend::setTextSize(uint8_t size) {
  if (size == 0U) {
    size = 1U;
  }
  textSize_ = size;
  applyFont();
}

void U8g2DisplayBackend::setTextColor(uint16_t fg, uint16_t bg) {
  textFg_ = fg;
  textBg_ = bg;
  (void)textBg_;
  applyColor(textFg_);
}

size_t U8g2DisplayBackend::write(uint8_t c) {
  return u8g2_.write(c);
}

size_t U8g2DisplayBackend::write(const uint8_t* buffer, size_t size) {
  return u8g2_.write(buffer, size);
}

void U8g2DisplayBackend::applyColor(uint16_t color) {
  u8g2_.setDrawColor((color == SSD1306_BLACK) ? 0U : 1U);
}

void U8g2DisplayBackend::applyFont() {
  u8g2_.setFont(fontForSize(textSize_));
}
