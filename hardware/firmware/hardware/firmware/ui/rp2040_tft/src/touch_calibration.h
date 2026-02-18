#pragma once

#include <Arduino.h>

class TFT_eSPI;
class XPT2046_Touchscreen;

class TouchCalibration {
 public:
  struct Data {
    bool valid = false;
    bool swapXY = false;
    bool invertX = false;
    bool invertY = false;
    int32_t xMin = 200;
    int32_t xMax = 3900;
    int32_t yMin = 200;
    int32_t yMax = 3900;
  };

  bool begin();
  bool load();
  bool save() const;
  bool runWizard(TFT_eSPI& tft, XPT2046_Touchscreen& touch, uint16_t screenW, uint16_t screenH);
  bool mapRaw(int32_t rawX, int32_t rawY, uint16_t screenW, uint16_t screenH, uint16_t* outX, uint16_t* outY) const;

  const Data& data() const;
  Data* mutableData();

 private:
  bool captureRawPoint(TFT_eSPI& tft,
                       XPT2046_Touchscreen& touch,
                       int16_t targetX,
                       int16_t targetY,
                       int32_t* outRawX,
                       int32_t* outRawY);
  void drawTarget(TFT_eSPI& tft, int16_t x, int16_t y, const char* label);

  Data data_;
};
