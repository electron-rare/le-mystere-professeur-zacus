#pragma once
#include "ui/fx/v9/assets/assets.h"
#include <string>
#include <unordered_map>

namespace fx::assets {

// Very small FS asset manager skeleton (LittleFS/SD etc).
// You must implement file reads for your platform; here it's a placeholder.
// Strategy:
// - PROGMEM: palettes + default fonts
// - FS: long texts + textures
class FsAssetManager : public IAssetManager {
public:
  explicit FsAssetManager(const char* basePath);

  const char* getText(const char* textId) override;
  const uint16_t* getPalette565(const char* paletteId) override;
  TextureI8 getTextureI8(const char* textureId) override;
  FontBitmap getFont(const char* fontId) override;

  // Hooks you can provide from your platform
  using ReadFileFn = bool(*)(const char* path, std::string& out);
  void setReadFileFn(ReadFileFn fn) { readFileFn = fn; }

private:
  std::string base;

  std::unordered_map<std::string, std::string> textCache;
  std::unordered_map<std::string, std::string> binCache; // raw bytes cache (textures)

  ReadFileFn readFileFn = nullptr;

  bool readTextFile(const std::string& path, std::string& out);
  bool readBinFile(const std::string& path, std::string& out);
};

} // namespace fx::assets
