#pragma once

#include <U8g2lib.h>

#include "display_backend.h"

class U8g2DisplayBackend final : public DisplayBackend {
 public:
  U8g2DisplayBackend();

  bool begin(uint8_t i2cAddress) override;
  void clearDisplay() override;
  void display() override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void setCursor(int16_t x, int16_t y) override;
  void setTextSize(uint8_t size) override;
  void setTextColor(uint16_t fg, uint16_t bg = SSD1306_BLACK) override;

  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;

 private:
  void applyColor(uint16_t color);
  void applyFont();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_;
  uint8_t textSize_ = 1U;
  uint16_t textFg_ = SSD1306_WHITE;
  uint16_t textBg_ = SSD1306_BLACK;
};
