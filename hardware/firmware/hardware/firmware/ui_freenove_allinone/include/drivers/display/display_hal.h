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

  virtual uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const = 0;
  virtual DisplayHalBackend backend() const = 0;
};

DisplayHal& displayHal();
bool displayHalUsesLovyanGfx();
void displayHalInvalidateOverlay();

}  // namespace drivers::display
