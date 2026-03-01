#pragma once
#include <cstdint>
#include <cstddef>

namespace fx::assets {

struct TextureI8 {
  const uint8_t* pixels = nullptr;  // w*h bytes
  int w = 0;
  int h = 0;
  int stride = 0;
  const uint16_t* palette565 = nullptr; // optional
};

struct FontBitmap {
  const uint8_t* bitmap = nullptr; // glyph atlas, format defined by user
  int w = 0;
  int h = 0;
};

class IAssetManager {
public:
  virtual ~IAssetManager() = default;

  // Returns a NUL-terminated string owned by asset manager (FS cache or PROGMEM pointer).
  virtual const char* getText(const char* textId) = 0;

  // Returns pointer to 256-entry palette (RGB565).
  virtual const uint16_t* getPalette565(const char* paletteId) = 0;

  // Returns texture (8bpp indexed).
  virtual TextureI8 getTextureI8(const char* textureId) = 0;

  virtual FontBitmap getFont(const char* fontId) = 0;
};

} // namespace fx::assets
