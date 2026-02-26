#include "drivers/display/display_hal.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "drivers/display/spi_bus_manager.h"
#include "ui_freenove_config.h"

#ifndef UI_FX_BACKEND_LGFX
#define UI_FX_BACKEND_LGFX 0
#endif

namespace drivers::display {

namespace {

#if UI_FX_BACKEND_LGFX

#if defined(ARDUINO_ARCH_ESP32)
#if FREENOVE_LCD_USE_HSPI
constexpr spi_host_device_t kLgfxSpiHost = SPI3_HOST;
#else
constexpr spi_host_device_t kLgfxSpiHost = SPI2_HOST;
#endif
#endif

class FreenoveLgfxDevice final : public lgfx::LGFX_Device {
 public:
  FreenoveLgfxDevice() {
    {
      auto cfg = bus_.config();
#if defined(ARDUINO_ARCH_ESP32)
      cfg.spi_host = kLgfxSpiHost;
#endif
      cfg.spi_mode = 0;
      cfg.freq_write = SPI_FREQUENCY;
      cfg.freq_read = SPI_READ_FREQUENCY;
      cfg.spi_3wire = false;
      cfg.use_lock = false;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = FREENOVE_TFT_SCK;
      cfg.pin_mosi = FREENOVE_TFT_MOSI;
      cfg.pin_miso = FREENOVE_TFT_MISO;
      cfg.pin_dc = FREENOVE_TFT_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = FREENOVE_TFT_CS;
      cfg.pin_rst = FREENOVE_TFT_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = FREENOVE_LCD_WIDTH;
      cfg.memory_height = FREENOVE_LCD_HEIGHT;
      cfg.panel_width = FREENOVE_LCD_WIDTH;
      cfg.panel_height = FREENOVE_LCD_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = (FREENOVE_TFT_MISO >= 0);
#if defined(TFT_INVERSION_ON)
      cfg.invert = true;
#else
      cfg.invert = false;
#endif
      cfg.rgb_order =
#if defined(TFT_RGB_ORDER) && (TFT_RGB_ORDER == TFT_BGR)
          true;
#else
          false;
#endif
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      panel_.config(cfg);
    }

#if (FREENOVE_TFT_BL >= 0)
    {
      auto cfg = light_.config();
      cfg.pin_bl = FREENOVE_TFT_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
#endif

    setPanel(&panel_);
  }

 private:
  lgfx::Panel_ST7796 panel_;
  lgfx::Bus_SPI bus_;
#if (FREENOVE_TFT_BL >= 0)
  lgfx::Light_PWM light_;
#endif
};

class LovyanGfxDisplayHal final : public DisplayHal {
 public:
  bool begin(const DisplayHalConfig& config) override {
    SpiBusManager::instance().begin();
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return false;
    }
    display_.init();
    display_.setRotation(config.rotation);
    write_locked_ = false;
    return true;
  }

  void fillScreen(uint16_t color565) override {
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return;
    }
    display_.fillScreen(color565);
  }

  bool initDma(bool use_double_buffer) override {
    (void)use_double_buffer;
    SpiBusManager::Guard guard(250U);
    if (!guard.locked()) {
      return false;
    }
    display_.initDMA();
    return true;
  }

  bool dmaBusy() const override {
    return const_cast<FreenoveLgfxDevice&>(display_).dmaBusy();
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
    display_.startWrite();
    write_locked_ = true;
    return true;
  }

  void endWrite() override {
    if (!write_locked_) {
      return;
    }
    display_.endWrite();
    write_locked_ = false;
    SpiBusManager::instance().unlock();
  }

  void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) override {
    display_.setAddrWindow(x, y, w, h);
  }

  void pushImageDma(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) override {
    if (pixels == nullptr || w <= 0 || h <= 0) {
      return;
    }
    // Keep the same RGB565+swap contract as pushColors(..., swap=true).
    display_.setAddrWindow(x, y, w, h);
    const int32_t count = static_cast<int32_t>(w) * static_cast<int32_t>(h);
    display_.writePixelsDMA(pixels, count, true);
  }

  void pushColors(const uint16_t* pixels, uint32_t count, bool swap_bytes) override {
    display_.writePixels(pixels, static_cast<int32_t>(count), swap_bytes);
  }

  void pushColor(uint16_t color565) override {
    display_.writePixels(&color565, 1, false);
  }

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const override {
    const uint16_t red = static_cast<uint16_t>((r & 0xF8U) << 8U);
    const uint16_t green = static_cast<uint16_t>((g & 0xFCU) << 3U);
    const uint16_t blue = static_cast<uint16_t>(b >> 3U);
    return static_cast<uint16_t>(red | green | blue);
  }

  DisplayHalBackend backend() const override {
    return DisplayHalBackend::kLovyanGfx;
  }

 private:
  FreenoveLgfxDevice display_;
  bool write_locked_ = false;
};

LovyanGfxDisplayHal g_lgfx_backend;

#endif  // UI_FX_BACKEND_LGFX

}  // namespace

DisplayHal* createLovyanGfxDisplayHal() {
#if UI_FX_BACKEND_LGFX
  return &g_lgfx_backend;
#else
  return nullptr;
#endif
}

}  // namespace drivers::display
