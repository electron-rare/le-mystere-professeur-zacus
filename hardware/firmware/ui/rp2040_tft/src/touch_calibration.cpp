#include "touch_calibration.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "../include/ui_config.h"

namespace {

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int32_t abs32(int32_t v) {
  return (v < 0) ? -v : v;
}

}  // namespace

bool TouchCalibration::begin() {
  return LittleFS.begin();
}

bool TouchCalibration::load() {
  File f = LittleFS.open(ui_config::kCalibrationPath, "r");
  if (!f) {
    return false;
  }
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }

  Data tmp;
  tmp.valid = doc["valid"] | false;
  tmp.swapXY = doc["swapXY"] | false;
  tmp.invertX = doc["invertX"] | false;
  tmp.invertY = doc["invertY"] | false;
  tmp.xMin = doc["xMin"] | 200;
  tmp.xMax = doc["xMax"] | 3900;
  tmp.yMin = doc["yMin"] | 200;
  tmp.yMax = doc["yMax"] | 3900;
  if (tmp.xMax <= tmp.xMin || tmp.yMax <= tmp.yMin) {
    return false;
  }
  data_ = tmp;
  return data_.valid;
}

bool TouchCalibration::save() const {
  File f = LittleFS.open(ui_config::kCalibrationPath, "w");
  if (!f) {
    return false;
  }
  StaticJsonDocument<256> doc;
  doc["valid"] = data_.valid;
  doc["swapXY"] = data_.swapXY;
  doc["invertX"] = data_.invertX;
  doc["invertY"] = data_.invertY;
  doc["xMin"] = data_.xMin;
  doc["xMax"] = data_.xMax;
  doc["yMin"] = data_.yMin;
  doc["yMax"] = data_.yMax;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

const TouchCalibration::Data& TouchCalibration::data() const {
  return data_;
}

TouchCalibration::Data* TouchCalibration::mutableData() {
  return &data_;
}

void TouchCalibration::drawTarget(TFT_eSPI& tft, int16_t x, int16_t y, const char* label) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("Calibration");
  tft.setTextSize(1);
  tft.setCursor(12, 38);
  tft.print(label);

  const int16_t size = 14;
  tft.drawCircle(x, y, size, TFT_YELLOW);
  tft.drawLine(x - size, y, x + size, y, TFT_YELLOW);
  tft.drawLine(x, y - size, x, y + size, TFT_YELLOW);
}

bool TouchCalibration::captureRawPoint(TFT_eSPI& tft,
                                       XPT2046_Touchscreen& touch,
                                       int16_t targetX,
                                       int16_t targetY,
                                       int32_t* outRawX,
                                       int32_t* outRawY) {
  drawTarget(tft, targetX, targetY, "Touchez la cible");
  delay(100);
  uint32_t deadline = millis() + 15000U;

  while (static_cast<int32_t>(millis() - deadline) < 0) {
    if (!touch.touched()) {
      delay(10);
      continue;
    }
    int64_t sumX = 0;
    int64_t sumY = 0;
    uint8_t samples = 0;
    while (touch.touched() && samples < 18U) {
      TS_Point p = touch.getPoint();
      if (p.z > 80) {
        sumX += p.x;
        sumY += p.y;
        ++samples;
      }
      delay(12);
    }
    if (samples < 4U) {
      continue;
    }
    *outRawX = static_cast<int32_t>(sumX / samples);
    *outRawY = static_cast<int32_t>(sumY / samples);
    return true;
  }
  return false;
}

bool TouchCalibration::runWizard(TFT_eSPI& tft,
                                 XPT2046_Touchscreen& touch,
                                 uint16_t screenW,
                                 uint16_t screenH) {
  int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
  const int16_t margin = 28;
  if (!captureRawPoint(tft, touch, margin, margin, &x1, &y1)) {
    return false;
  }
  if (!captureRawPoint(tft, touch, static_cast<int16_t>(screenW - margin), margin, &x2, &y2)) {
    return false;
  }
  if (!captureRawPoint(tft, touch, margin, static_cast<int16_t>(screenH - margin), &x3, &y3)) {
    return false;
  }

  Data out;
  const int32_t dxTop = abs32(x2 - x1);
  const int32_t dyTop = abs32(y2 - y1);
  out.swapXY = (dyTop > dxTop);

  int32_t rawXLeft = 0;
  int32_t rawXRight = 0;
  int32_t rawYTop = 0;
  int32_t rawYBottom = 0;

  if (!out.swapXY) {
    rawXLeft = (x1 + x3) / 2;
    rawXRight = x2;
    rawYTop = (y1 + y2) / 2;
    rawYBottom = y3;
  } else {
    rawXLeft = (y1 + y3) / 2;
    rawXRight = y2;
    rawYTop = (x1 + x2) / 2;
    rawYBottom = x3;
  }

  out.invertX = rawXRight < rawXLeft;
  out.invertY = rawYBottom < rawYTop;
  out.xMin = (rawXLeft < rawXRight) ? rawXLeft : rawXRight;
  out.xMax = (rawXLeft > rawXRight) ? rawXLeft : rawXRight;
  out.yMin = (rawYTop < rawYBottom) ? rawYTop : rawYBottom;
  out.yMax = (rawYTop > rawYBottom) ? rawYTop : rawYBottom;
  out.valid = (out.xMax - out.xMin) > 200 && (out.yMax - out.yMin) > 200;
  if (!out.valid) {
    return false;
  }
  data_ = out;
  save();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(14, 24);
  tft.print("Calibration OK");
  tft.setTextSize(1);
  tft.setCursor(14, 58);
  tft.printf("swap=%u invX=%u invY=%u", out.swapXY ? 1U : 0U, out.invertX ? 1U : 0U, out.invertY ? 1U : 0U);
  delay(700);
  return true;
}

bool TouchCalibration::mapRaw(int32_t rawX,
                              int32_t rawY,
                              uint16_t screenW,
                              uint16_t screenH,
                              uint16_t* outX,
                              uint16_t* outY) const {
  if (!data_.valid || outX == nullptr || outY == nullptr) {
    return false;
  }

  int32_t ax = rawX;
  int32_t ay = rawY;
  if (data_.swapXY) {
    ax = rawY;
    ay = rawX;
  }

  if (data_.invertX) {
    ax = data_.xMax - (ax - data_.xMin);
  }
  if (data_.invertY) {
    ay = data_.yMax - (ay - data_.yMin);
  }

  ax = clampValue(ax, data_.xMin, data_.xMax);
  ay = clampValue(ay, data_.yMin, data_.yMax);
  const int32_t dx = data_.xMax - data_.xMin;
  const int32_t dy = data_.yMax - data_.yMin;
  if (dx <= 0 || dy <= 0) {
    return false;
  }

  const int32_t mappedX = ((ax - data_.xMin) * static_cast<int32_t>(screenW - 1U)) / dx;
  const int32_t mappedY = ((ay - data_.yMin) * static_cast<int32_t>(screenH - 1U)) / dy;
  *outX = static_cast<uint16_t>(clampValue<int32_t>(mappedX, 0, static_cast<int32_t>(screenW - 1U)));
  *outY = static_cast<uint16_t>(clampValue<int32_t>(mappedY, 0, static_cast<int32_t>(screenH - 1U)));
  return true;
}
