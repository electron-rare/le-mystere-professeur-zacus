#pragma once

#include <Arduino.h>

class TFT_eSPI;
class UiStateModel;

class UiRenderer {
 public:
  explicit UiRenderer(TFT_eSPI& tft);

  void begin();
  void drawBootScreen(const char* line1, const char* line2);
  void render(UiStateModel& ui, uint32_t nowMs, bool forceFull);

 private:
  void drawFrame(UiStateModel& ui);
  void drawHeader(const UiStateModel& ui);
  void drawNowPlaying(const UiStateModel& ui, uint32_t nowMs, bool full);
  void drawList(const UiStateModel& ui, bool full);
  void drawSettings(const UiStateModel& ui, bool full);
  void drawBottomButtons(const char* labels[5], uint16_t color);
  void drawVuMeter(int16_t x, int16_t y, int16_t w, int16_t h, float vu);
  void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int32_t pos, int32_t dur, bool live);
  void drawWrappedTitle(const char* title, int16_t x, int16_t y, int16_t w, uint32_t nowMs);
  const char* wifiModeLabel(uint8_t mode) const;
  const char* eqLabel(uint8_t preset) const;
  const char* brightnessLabel(uint8_t level) const;

  TFT_eSPI& tft_;
  uint8_t lastPage_ = 255U;
  bool lastConnected_ = false;
  int32_t lastPosSec_ = -1;
  int32_t lastVol_ = -1;
  int32_t lastBuffer_ = -2;
  int32_t lastRssi_ = -255;
  float lastVu_ = -1.0f;
  char marqueeTitle_[96] = {};
  uint16_t marqueeOffset_ = 0U;
  uint32_t marqueeStartedMs_ = 0U;
  uint32_t lastMarqueeStepMs_ = 0U;
};
