#pragma once

#include <Arduino.h>

constexpr uint16_t SSD1306_BLACK = 0U;
constexpr uint16_t SSD1306_WHITE = 1U;

class DisplayBackend : public Print {
 public:
  virtual ~DisplayBackend() = default;
  virtual bool begin(uint8_t i2cAddress) = 0;
  virtual void clearDisplay() = 0;
  virtual void display() = 0;
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;
  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) = 0;
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) = 0;
  virtual void setCursor(int16_t x, int16_t y) = 0;
  virtual void setTextSize(uint8_t size) = 0;
  virtual void setTextColor(uint16_t fg, uint16_t bg = SSD1306_BLACK) = 0;
};
