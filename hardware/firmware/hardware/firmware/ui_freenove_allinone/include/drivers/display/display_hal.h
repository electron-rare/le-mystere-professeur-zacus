#pragma once

#include <cstdint>

namespace drivers::display {

enum class DisplayHalBackend : uint8_t {
  kTftEsPi = 0,
  kLovyanGfx = 1,
};

struct DisplayHalConfig {
  uint16_t width = 0U;
  uint16_t height = 0U;
  uint8_t rotation = 0U;
};

enum class OverlayFontFace : uint8_t {
  kBuiltinSmall = 0,
  kBuiltinMedium,
  kBuiltinLarge,
  kIbmRegular14,
  kIbmRegular18,
  kIbmBold12,
  kIbmBold16,
  kIbmBold20,
  kIbmBold24,
  kIbmItalic12,
  kIbmItalic16,
  kIbmItalic20,
  kIbmItalic24,
  kInter18,
  kInter24,
  kOrbitron28,
  kBungee24,
  kMonoton24,
  kRubikGlitch24,
};

struct OverlayTextCommand {
  const char* text = nullptr;
  int16_t x = 0;
  int16_t y = 0;
  uint16_t color565 = 0xFFFFU;
  uint16_t bg565 = 0x0000U;
  OverlayFontFace font_face = OverlayFontFace::kBuiltinMedium;
  uint8_t size = 1U;
  bool opaque_bg = false;
};

class DisplayHal {
 public:
  virtual ~DisplayHal() = default;

  virtual bool begin(const DisplayHalConfig& config) = 0;
  virtual void fillScreen(uint16_t color565) = 0;

  virtual bool initDma(bool use_double_buffer) = 0;
  virtual bool dmaBusy() const = 0;
  virtual bool waitDmaComplete(uint32_t timeout_us) = 0;

  virtual bool startWrite() = 0;
  virtual void endWrite() = 0;

  virtual void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  // Contract: both DMA image and pushColors(swap=true) consume the same logical RGB565 pixel format.
  virtual void pushImageDma(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) = 0;
  virtual void pushColors(const uint16_t* pixels, uint32_t count, bool swap_bytes) = 0;
  virtual void pushColor(uint16_t color565) = 0;
  virtual bool drawOverlayLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) = 0;
  virtual bool drawOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) = 0;
  virtual bool fillOverlayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) = 0;
  virtual bool drawOverlayCircle(int16_t x, int16_t y, int16_t radius, uint16_t color565) = 0;
  virtual bool supportsOverlayText() const = 0;
  virtual int16_t measureOverlayText(const char* text, OverlayFontFace font_face, uint8_t size) = 0;
  virtual bool drawOverlayText(const OverlayTextCommand& command) = 0;

  virtual uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const = 0;
  virtual DisplayHalBackend backend() const = 0;
};

DisplayHal& displayHal();
bool displayHalUsesLovyanGfx();
void displayHalInvalidateOverlay();

}  // namespace drivers::display
