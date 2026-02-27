#include "drivers/display/display_hal.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <cstddef>
#include <cstdint>

#include "drivers/display/spi_bus_manager.h"
#include "ui/ui_fonts.h"
#include "ui_freenove_config.h"

#ifndef UI_FX_BACKEND_LGFX
#define UI_FX_BACKEND_LGFX 0
#endif

namespace drivers::display {

namespace {

#if UI_FX_BACKEND_LGFX

constexpr uint8_t kOverlayTextMinSize = 1U;
constexpr uint8_t kOverlayTextMaxSize = 4U;

uint8_t clampTextSize(uint8_t size) {
  if (size < kOverlayTextMinSize) {
    return kOverlayTextMinSize;
  }
  if (size > kOverlayTextMaxSize) {
    return kOverlayTextMaxSize;
  }
  return size;
}

bool isBuiltinFace(OverlayFontFace face) {
  return face == OverlayFontFace::kBuiltinSmall ||
         face == OverlayFontFace::kBuiltinMedium ||
         face == OverlayFontFace::kBuiltinLarge;
}

uint8_t builtinFontId(OverlayFontFace face) {
  switch (face) {
    case OverlayFontFace::kBuiltinSmall:
      return 1U;
    case OverlayFontFace::kBuiltinLarge:
      return 4U;
    case OverlayFontFace::kBuiltinMedium:
    default:
      return 2U;
  }
}

const lv_font_t* resolveOverlayFont(OverlayFontFace face) {
  switch (face) {
    case OverlayFontFace::kIbmRegular14:
      return UiFonts::fontIbmRegular14();
    case OverlayFontFace::kIbmRegular18:
      return UiFonts::fontIbmRegular18();
    case OverlayFontFace::kIbmBold12:
      return UiFonts::fontBold12();
    case OverlayFontFace::kIbmBold16:
      return UiFonts::fontBold16();
    case OverlayFontFace::kIbmBold20:
      return UiFonts::fontBold20();
    case OverlayFontFace::kIbmBold24:
      return UiFonts::fontBold24();
    case OverlayFontFace::kIbmItalic12:
      return UiFonts::fontItalic12();
    case OverlayFontFace::kIbmItalic16:
      return UiFonts::fontItalic16();
    case OverlayFontFace::kIbmItalic20:
      return UiFonts::fontItalic20();
    case OverlayFontFace::kIbmItalic24:
      return UiFonts::fontItalic24();
    case OverlayFontFace::kInter18:
      return UiFonts::fontBodyM();
    case OverlayFontFace::kInter24:
      return UiFonts::fontBodyL();
    case OverlayFontFace::kOrbitron28:
      return UiFonts::fontTitle();
    case OverlayFontFace::kBungee24:
      return UiFonts::fontFunkyBungee();
    case OverlayFontFace::kMonoton24:
      return UiFonts::fontFunkyMonoton();
    case OverlayFontFace::kRubikGlitch24:
      return UiFonts::fontFunkyRubikGlitch();
    case OverlayFontFace::kBuiltinSmall:
    case OverlayFontFace::kBuiltinMedium:
    case OverlayFontFace::kBuiltinLarge:
    default:
      return nullptr;
  }
}

uint8_t glyphPixelAlpha(uint8_t bpp, const uint8_t* bitmap, uint32_t pixel_index) {
  if (bitmap == nullptr || bpp == 0U) {
    return 0U;
  }
  switch (bpp) {
    case 1U: {
      const uint32_t byte_index = pixel_index >> 3U;
      const uint8_t bit_shift = static_cast<uint8_t>(7U - (pixel_index & 0x7U));
      return (bitmap[byte_index] & (1U << bit_shift)) ? 255U : 0U;
    }
    case 2U: {
      const uint32_t byte_index = pixel_index >> 2U;
      const uint8_t shift = static_cast<uint8_t>(6U - ((pixel_index & 0x3U) << 1U));
      const uint8_t level = static_cast<uint8_t>((bitmap[byte_index] >> shift) & 0x03U);
      return static_cast<uint8_t>(level * 85U);
    }
    case 4U: {
      const uint32_t byte_index = pixel_index >> 1U;
      const uint8_t level = ((pixel_index & 0x1U) == 0U) ? static_cast<uint8_t>(bitmap[byte_index] >> 4U)
                                                          : static_cast<uint8_t>(bitmap[byte_index] & 0x0FU);
      return static_cast<uint8_t>(level * 17U);
    }
    case 8U:
      return bitmap[pixel_index];
    default:
      return 0U;
  }
}

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
          false;
#else
          true;
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
      // Overlay pass can arrive right after DMA flush release; wait briefly and retry lock.
      waitDmaComplete(1800U);
      if (!SpiBusManager::instance().lock(2000U)) {
        return false;
      }
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

  bool drawOverlayLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) override {
    display_.drawLine(x0, y0, x1, y1, color565);
    return true;
  }

  bool drawOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) override {
    if (w <= 0 || h <= 0) {
      return false;
    }
    display_.drawRect(x, y, w, h, color565);
    return true;
  }

  bool fillOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) override {
    if (w <= 0 || h <= 0) {
      return false;
    }
    display_.fillRect(x, y, w, h, color565);
    return true;
  }

  bool drawOverlayCircle(int16_t x, int16_t y, int16_t radius, uint16_t color565) override {
    if (radius <= 0) {
      return false;
    }
    display_.drawCircle(x, y, radius, color565);
    return true;
  }

  bool supportsOverlayText() const override {
    return true;
  }

  int16_t measureOverlayText(const char* text, OverlayFontFace font_face, uint8_t size) override {
    if (text == nullptr || text[0] == '\0') {
      return 0;
    }
    const uint8_t effective_size = clampTextSize(size);
    if (isBuiltinFace(font_face)) {
      display_.setTextFont(builtinFontId(font_face));
      display_.setTextSize(effective_size);
      return display_.textWidth(text);
    }

    const lv_font_t* overlay_font = resolveOverlayFont(font_face);
    if (overlay_font == nullptr) {
      display_.setTextFont(builtinFontId(font_face));
      display_.setTextSize(effective_size);
      return display_.textWidth(text);
    }

    int32_t width_px = 0;
    bool has_bitmap_glyph = false;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != '\0'; ++cursor) {
      lv_font_glyph_dsc_t glyph = {};
      uint32_t codepoint = static_cast<uint32_t>(*cursor);
      if (codepoint < 0x20U || codepoint > 0x7EU) {
        codepoint = static_cast<uint32_t>('?');
      }
      if (!lv_font_get_glyph_dsc(overlay_font, &glyph, codepoint, 0U)) {
        if (!lv_font_get_glyph_dsc(overlay_font, &glyph, static_cast<uint32_t>('?'), 0U)) {
          continue;
        }
        codepoint = static_cast<uint32_t>('?');
      }
      const uint8_t* bitmap = lv_font_get_glyph_bitmap(overlay_font, codepoint);
      if (bitmap != nullptr && glyph.box_w > 0U && glyph.box_h > 0U) {
        has_bitmap_glyph = true;
      }
      const int32_t adv_px = static_cast<int32_t>((glyph.adv_w + 8U) >> 4U);
      width_px += adv_px * static_cast<int32_t>(effective_size);
    }
    if (!has_bitmap_glyph) {
      display_.setTextFont(builtinFontId(font_face));
      display_.setTextSize(effective_size);
      return display_.textWidth(text);
    }
    if (width_px < 0) {
      width_px = 0;
    }
    if (width_px > 32767) {
      width_px = 32767;
    }
    return static_cast<int16_t>(width_px);
  }

bool drawOverlayText(const OverlayTextCommand& command) override {
  if (command.text == nullptr || command.text[0] == '\0') {
    return false;
  }
  const uint8_t effective_size = clampTextSize(command.size);
  const uint16_t text_color = (command.color565 == 0U) ? 0xFFFFU : command.color565;
  if (isBuiltinFace(command.font_face)) {
    display_.setTextFont(builtinFontId(command.font_face));
    display_.setTextSize(effective_size);
    if (command.opaque_bg) {
      display_.setTextColor(text_color, command.bg565);
    } else {
      display_.setTextColor(text_color);
    }
    display_.drawString(command.text, command.x, command.y);
    return true;
  }

    const lv_font_t* overlay_font = resolveOverlayFont(command.font_face);
    if (overlay_font == nullptr) {
      display_.setTextFont(builtinFontId(command.font_face));
      display_.setTextSize(effective_size);
      display_.setTextColor(text_color);
      display_.drawString(command.text, command.x, command.y);
      return true;
    }

    if (command.opaque_bg) {
      const int16_t text_w = measureOverlayText(command.text, command.font_face, effective_size);
      const int16_t text_h = static_cast<int16_t>(overlay_font->line_height * effective_size);
      if (text_w > 0 && text_h > 0) {
        display_.fillRect(command.x, command.y, text_w, text_h, command.bg565);
      }
    }

    const int16_t screen_w = static_cast<int16_t>(display_.width());
    const int16_t screen_h = static_cast<int16_t>(display_.height());
    int32_t cursor_x = command.x;
    const int32_t cursor_y = command.y;
    bool glyph_drawn = false;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(command.text); *cursor != '\0'; ++cursor) {
      uint32_t codepoint = static_cast<uint32_t>(*cursor);
      if (codepoint < 0x20U || codepoint > 0x7EU) {
        codepoint = static_cast<uint32_t>('?');
      }

      lv_font_glyph_dsc_t glyph = {};
      if (!lv_font_get_glyph_dsc(overlay_font, &glyph, codepoint, 0U)) {
        if (!lv_font_get_glyph_dsc(overlay_font, &glyph, static_cast<uint32_t>('?'), 0U)) {
          continue;
        }
        codepoint = static_cast<uint32_t>('?');
      }

      const uint8_t* bitmap = lv_font_get_glyph_bitmap(overlay_font, codepoint);
      if (bitmap != nullptr && glyph.box_w > 0U && glyph.box_h > 0U) {
        const int32_t glyph_x = cursor_x + static_cast<int32_t>(glyph.ofs_x) * static_cast<int32_t>(effective_size);
        const int32_t glyph_y =
            cursor_y +
            static_cast<int32_t>((overlay_font->line_height - overlay_font->base_line) - glyph.box_h - glyph.ofs_y) *
                static_cast<int32_t>(effective_size);

        for (uint16_t row = 0U; row < glyph.box_h; ++row) {
          for (uint16_t col = 0U; col < glyph.box_w; ++col) {
            const uint32_t pixel_index = static_cast<uint32_t>(row) * glyph.box_w + col;
            const uint8_t alpha = glyphPixelAlpha(glyph.bpp, bitmap, pixel_index);
            if (alpha < 40U) {
              continue;
            }
            const int16_t px = static_cast<int16_t>(glyph_x + static_cast<int32_t>(col) * effective_size);
            const int16_t py = static_cast<int16_t>(glyph_y + static_cast<int32_t>(row) * effective_size);
            if (px < 0 || py < 0 || px >= screen_w || py >= screen_h) {
              continue;
            }
            if (effective_size == 1U) {
              display_.drawPixel(px, py, text_color);
            } else {
              display_.fillRect(px, py, effective_size, effective_size, text_color);
            }
            glyph_drawn = true;
          }
        }
      }

      const int32_t adv_px = static_cast<int32_t>((glyph.adv_w + 8U) >> 4U);
      cursor_x += adv_px * static_cast<int32_t>(effective_size);
    }
    if (!glyph_drawn) {
      // Safety fallback: if glyph atlas/font mapping fails at runtime, use built-in font so text stays visible.
      display_.setTextFont(builtinFontId(command.font_face));
      display_.setTextSize(effective_size);
      if (command.opaque_bg) {
        display_.setTextColor(text_color, command.bg565);
      } else {
        display_.setTextColor(text_color);
      }
      display_.drawString(command.text, command.x, command.y);
    }
    return true;
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
