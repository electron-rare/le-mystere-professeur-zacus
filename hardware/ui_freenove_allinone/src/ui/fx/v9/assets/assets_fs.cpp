#include "ui/fx/v9/assets/assets_fs.h"
#include "ui/fx/v9/assets/palette_gray565.h"

#include <LittleFS.h>

#include <cstring>

namespace fx::assets {

FsAssetManager::FsAssetManager(const char* basePath) : base(basePath ? basePath : "") {}

bool FsAssetManager::readTextFile(const std::string& path, std::string& out)
{
  if (readFileFn != nullptr) {
    return readFileFn(path.c_str(), out);
  }
  File file = LittleFS.open(path.c_str(), "r");
  if (!file) {
    return false;
  }
  String text = file.readString();
  file.close();
  out.assign(text.c_str(), text.length());
  return true;
}

bool FsAssetManager::readBinFile(const std::string& path, std::string& out)
{
  if (readFileFn != nullptr) {
    return readFileFn(path.c_str(), out);
  }
  File file = LittleFS.open(path.c_str(), "r");
  if (!file) {
    return false;
  }
  const size_t size = file.size();
  out.resize(size);
  if (size > 0U) {
    const size_t read = file.readBytes(&out[0], size);
    if (read != size) {
      file.close();
      out.clear();
      return false;
    }
  }
  file.close();
  return true;
}

const char* FsAssetManager::getText(const char* textId)
{
  if (!textId) return "";
  auto it = textCache.find(textId);
  if (it != textCache.end()) return it->second.c_str();

  std::string path = base + "/texts/" + textId + ".txt";
  std::string data;
  if (!readTextFile(path, data)) {
    if (std::strcmp(textId, "greetz_01") == 0) {
      textCache[textId] = "GREETZ FROM ZACUS DEMOSCENE!";
    } else if (std::strcmp(textId, "credits_01") == 0) {
      textCache[textId] = "CODE + MUSIC + FX: TEAM ZACUS";
    } else {
      textCache[textId] = "";
    }
  } else {
    textCache[textId] = data;
  }
  return textCache[textId].c_str();
}

const uint16_t* FsAssetManager::getPalette565(const char* paletteId)
{
  if (paletteId == nullptr || paletteId[0] == '\0') {
    return palette_gray565;
  }
  if (std::strcmp(paletteId, "gray") == 0 ||
      std::strcmp(paletteId, "plasma_cyan") == 0 ||
      std::strcmp(paletteId, "default") == 0) {
    return palette_gray565;
  }
  return palette_gray565;
}

TextureI8 FsAssetManager::getTextureI8(const char* textureId)
{
  TextureI8 t{};
  if (!textureId) return t;

  auto it = binCache.find(textureId);
  if (it == binCache.end()) {
    std::string path = base + "/textures/" + textureId + ".bin";
    std::string data;
    if (readBinFile(path, data)) {
      binCache[textureId] = data;
      it = binCache.find(textureId);
    } else {
      return t;
    }
  }

  // Interpret as a user-defined format; placeholder.
  // Recommended: custom header {w,h,stride} then pixels.
  return t;
}

FontBitmap FsAssetManager::getFont(const char* fontId)
{
  (void)fontId;
  return {};
}

} // namespace fx::assets
