#include "drivers/display/display_hal.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "drivers/display/spi_bus_manager.h"
#include "ui_freenove_config.h"

#ifndef UI_FX_BACKEND_LGFX
#define UI_FX_BACKEND_LGFX 0
#endif

namespace drivers::display {

DisplayHal* createLovyanGfxDisplayHal();

namespace {

class TftEsPiDisplayHal final : public DisplayHal {
 public:
  TftEsPiDisplayHal() : tft_(FREENOVE_LCD_WIDTH, FREENOVE_LCD_HEIGHT) {}

  bool begin(const DisplayHalConfig& config) override {
    SpiBusManager::instance().begin();
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return false;
    }
    (void)config;
    tft_.begin();
    tft_.setRotation(config.rotation);
    write_locked_ = false;
    return true;
  }

  void fillScreen(uint16_t color565) override {
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return;
    }
    tft_.fillScreen(color565);
  }

  bool initDma(bool use_double_buffer) override {
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return false;
    }
    return tft_.initDMA(use_double_buffer);
  }

  bool dmaBusy() const override {
    return const_cast<TFT_eSPI&>(tft_).dmaBusy();
  }

  bool waitDmaComplete(uint32_t timeout_us) override {
    if (!dmaBusy()) {
      return true;
    }
    const uint32_t started_us = micros();
    while (dmaBusy()) {
      if ((micros() - started_us) >= timeout_us) {
        return !dmaBusy();
      }
      delayMicroseconds(20U);
    }
    return true;
  }

  bool startWrite() override {
    if (write_locked_) {
      return true;
    }
    if (!SpiBusManager::instance().lock(250U)) {
      return false;
    }
    tft_.startWrite();
    write_locked_ = true;
    return true;
  }

  void endWrite() override {
    if (!write_locked_) {
      return;
    }
    tft_.endWrite();
    write_locked_ = false;
    SpiBusManager::instance().unlock();
  }

  void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) override {
    if (w <= 0 || h <= 0) {
      return;
    }
    tft_.setAddrWindow(x, y, w, h);
  }

  void pushImageDma(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) override {
    if (pixels == nullptr || w <= 0 || h <= 0) {
      return;
    }

    const uint32_t pixel_count = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
    if (pixel_count == 0U) {
      return;
    }

    if (pixel_count <= 256U) {
      tft_.pushImage(x, y, w, h, const_cast<uint16_t*>(pixels));
      return;
    }

    if (dmaBusy()) {
      waitDmaComplete(2500U);
    }

    constexpr int16_t kDmaChunkLines = 32;
    int16_t y_offset = 0;
    while (y_offset < h) {
      const int16_t chunk_h = ((h - y_offset) > kDmaChunkLines) ? kDmaChunkLines : (h - y_offset);
      const uint16_t* chunk_pixels = pixels + (static_cast<int32_t>(y_offset) * static_cast<int32_t>(w));
      tft_.pushImageDMA(x, static_cast<int16_t>(y + y_offset), w, chunk_h, const_cast<uint16_t*>(chunk_pixels));
      if (!waitDmaComplete(3000U)) {
        tft_.pushImage(x, static_cast<int16_t>(y + y_offset), w, chunk_h, const_cast<uint16_t*>(chunk_pixels));
      }
      y_offset = static_cast<int16_t>(y_offset + chunk_h);
    }
  }

  void pushColors(const uint16_t* pixels, uint32_t count, bool swap_bytes) override {
    if (pixels == nullptr || count == 0U) {
      return;
    }
    tft_.pushColors(const_cast<uint16_t*>(pixels), static_cast<uint32_t>(count), swap_bytes);
  }

  void pushColor(uint16_t color565) override {
    tft_.pushColor(color565);
  }

  bool drawOverlayLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) override {
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)color565;
    return false;
  }

  bool drawOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) override {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color565;
    return false;
  }

  bool fillOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) override {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color565;
    return false;
  }

  bool drawOverlayCircle(int16_t x, int16_t y, int16_t radius, uint16_t color565) override {
    (void)x;
    (void)y;
    (void)radius;
    (void)color565;
    return false;
  }

  bool supportsOverlayText() const override {
    return false;
  }

  int16_t measureOverlayText(const char* text, OverlayFontFace font_face, uint8_t size) override {
    (void)text;
    (void)font_face;
    (void)size;
    return 0;
  }

  bool drawOverlayText(const OverlayTextCommand& command) override {
    (void)command;
    return false;
  }

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const override {
    const uint16_t red = static_cast<uint16_t>((r & 0xF8U) << 8U);
    const uint16_t green = static_cast<uint16_t>((g & 0xFCU) << 3U);
    const uint16_t blue = static_cast<uint16_t>(b >> 3U);
    return static_cast<uint16_t>(red | green | blue);
  }

  DisplayHalBackend backend() const override {
    return DisplayHalBackend::kTftEsPi;
  }

 private:
  TFT_eSPI tft_;
  bool write_locked_ = false;
};

TftEsPiDisplayHal g_tft_backend;
DisplayHal* g_active_backend = &g_tft_backend;
bool g_backend_selected = false;

void selectBackendOnce() {
  if (g_backend_selected) {
    return;
  }
#if UI_FX_BACKEND_LGFX
  DisplayHal* lgfx = createLovyanGfxDisplayHal();
  if (lgfx != nullptr) {
    g_active_backend = lgfx;
    Serial.println("[DISPLAY] backend=lovyangfx");
  } else {
    Serial.println("[DISPLAY] backend=tftespi (lgfx unavailable)");
  }
#else
  Serial.println("[DISPLAY] backend=tftespi");
#endif
  g_backend_selected = true;
}

}  // namespace

DisplayHal& displayHal() {
  selectBackendOnce();
  return *g_active_backend;
}

bool displayHalUsesLovyanGfx() {
  return displayHal().backend() == DisplayHalBackend::kLovyanGfx;
}

void displayHalInvalidateOverlay() {
  // Overlay invalidation is driven by UI layer via lv_obj_invalidate.
}

}  // namespace drivers::display
