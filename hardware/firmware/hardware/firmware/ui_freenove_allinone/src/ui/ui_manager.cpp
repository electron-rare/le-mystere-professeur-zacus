// ui_manager.cpp - LVGL binding for TFT + keypad events.
#include "ui_manager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <esp_log.h>
#endif

#include "ui_freenove_config.h"
#include "drivers/display/display_hal.h"
#include "resources/screen_scene_registry.h"
#include "runtime/memory/caps_allocator.h"
#include "runtime/memory/safe_size.h"
#include "runtime/perf/perf_monitor.h"
#include "runtime/simd/simd_accel.h"
#include "ui/scene_element.h"
#include "ui/scene_state.h"
#include "ui/fx/fx_engine.h"
#include "ui_fonts.h"

namespace {

#ifndef UI_DEBUG_LOG
#define UI_DEBUG_LOG 0
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CORE_DEBUG_LEVEL) && (CORE_DEBUG_LEVEL > 0)
#define UI_LOGI(fmt, ...) ESP_LOGI("UI", fmt, ##__VA_ARGS__)
#else
#define UI_LOGI(fmt, ...) Serial.printf("[UI] " fmt "\n", ##__VA_ARGS__)
#endif

#if UI_DEBUG_LOG
#define UI_LOGD(fmt, ...) UI_LOGI(fmt, ##__VA_ARGS__)
#else
#define UI_LOGD(fmt, ...) do {} while (0)
#endif

#ifndef UI_COLOR_256
#define UI_COLOR_256 1
#endif

#ifndef UI_COLOR_565
#define UI_COLOR_565 0
#endif

#ifndef UI_FORCE_THEME_256
#define UI_FORCE_THEME_256 1
#endif

#ifndef UI_DRAW_BUF_LINES
#define UI_DRAW_BUF_LINES 40
#endif

#ifndef UI_DRAW_BUF_IN_PSRAM
#if defined(FREENOVE_PSRAM_UI_DRAW_BUFFER)
#define UI_DRAW_BUF_IN_PSRAM FREENOVE_PSRAM_UI_DRAW_BUFFER
#else
#define UI_DRAW_BUF_IN_PSRAM 1
#endif
#endif

#ifndef UI_DMA_TX_IN_DRAM
#define UI_DMA_TX_IN_DRAM 1
#endif

#ifndef UI_DMA_FLUSH_ASYNC
#define UI_DMA_FLUSH_ASYNC 1
#endif

#ifndef UI_DMA_RGB332_ASYNC_EXPERIMENTAL
#define UI_DMA_RGB332_ASYNC_EXPERIMENTAL 0
#endif

#ifndef UI_DMA_TRANS_BUF_LINES
#define UI_DMA_TRANS_BUF_LINES UI_DRAW_BUF_LINES
#endif

#ifndef UI_CONV_LINEBUF_RGB565
#define UI_CONV_LINEBUF_RGB565 1
#endif

#ifndef UI_SIMD_EXPERIMENTAL
#define UI_SIMD_EXPERIMENTAL 0
#endif

#ifndef UI_FULL_FRAME_BENCH
#define UI_FULL_FRAME_BENCH 0
#endif

#ifndef UI_DEMO_AUTORUN_WIN_ETAPE
#define UI_DEMO_AUTORUN_WIN_ETAPE 0
#endif

#ifndef UI_WIN_ETAPE_SIMPLIFIED
#define UI_WIN_ETAPE_SIMPLIFIED 1
#endif

UiManager* g_instance = nullptr;

constexpr uint16_t kDrawLineFallbacks[] = {48U, 40U, 32U, 24U};
constexpr uint16_t kDrawBufLinesRequested = static_cast<uint16_t>(UI_DRAW_BUF_LINES);
constexpr uint16_t kDmaTransBufLinesRequested = static_cast<uint16_t>(UI_DMA_TRANS_BUF_LINES);
constexpr bool kUseColor256Runtime = (UI_COLOR_565 == 0) && (UI_COLOR_256 != 0);
constexpr bool kUseThemeQuantizeRuntime = (UI_FORCE_THEME_256 != 0);
constexpr bool kUseAsyncDmaRuntime = (UI_DMA_FLUSH_ASYNC != 0);
constexpr bool kUsePsramLineBuffersRuntime = (UI_DRAW_BUF_IN_PSRAM != 0);
constexpr bool kUseDmaTxInDramRuntime = (UI_DMA_TX_IN_DRAM != 0);
[[maybe_unused]] constexpr bool kUseRgb332AsyncExperimental = (UI_DMA_RGB332_ASYNC_EXPERIMENTAL != 0);
constexpr bool kUseFullFrameBenchRuntime = (UI_FULL_FRAME_BENCH != 0);
constexpr bool kUseDemoAutorunWinEtapeRuntime = (UI_DEMO_AUTORUN_WIN_ETAPE != 0);
constexpr bool kUseWinEtapeSimplifiedEffects = (UI_WIN_ETAPE_SIMPLIFIED != 0);
constexpr uint32_t kFullFrameBenchMinFreePsram = 256U * 1024U;
constexpr uint32_t kFlushStallTimeoutMs = 240U;
constexpr uint32_t kAsyncFallbackRecoverMs = 1500U;
constexpr uint32_t kLvglFlushDmaWaitUs = 12000U;

int16_t activeDisplayWidth() {
  lv_disp_t* display = lv_disp_get_default();
  if (display != nullptr) {
    return static_cast<int16_t>(lv_disp_get_hor_res(display));
  }
  return ((FREENOVE_LCD_ROTATION & 0x1U) != 0U) ? FREENOVE_LCD_HEIGHT : FREENOVE_LCD_WIDTH;
}

int16_t activeDisplayHeight() {
  lv_disp_t* display = lv_disp_get_default();
  if (display != nullptr) {
    return static_cast<int16_t>(lv_disp_get_ver_res(display));
  }
  return ((FREENOVE_LCD_ROTATION & 0x1U) != 0U) ? FREENOVE_LCD_WIDTH : FREENOVE_LCD_HEIGHT;
}

void copyTextSafe(char* out, size_t out_size, const char* value) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (value == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, value, out_size - 1U);
  out[out_size - 1U] = '\0';
}

void trimAsciiWhitespace(char* text) {
  if (text == nullptr) {
    return;
  }
  size_t start = 0U;
  while (text[start] != '\0' && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  size_t end = std::strlen(text);
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
    --end;
  }
  size_t write = 0U;
  for (size_t index = start; index < end; ++index) {
    text[write++] = text[index];
  }
  text[write] = '\0';
}

uint32_t pseudoRandom32(uint32_t value) {
  value ^= (value << 13U);
  value ^= (value >> 17U);
  value ^= (value << 5U);
  return value;
}

uint32_t toLvKey(uint8_t key, bool long_press) {
  (void)long_press;
  const uint8_t rotation = static_cast<uint8_t>(FREENOVE_LCD_ROTATION & 0x3U);
  switch (key) {
    case 1:
      return LV_KEY_ENTER;
    case 2:
      if (rotation == 0U) {
        return LV_KEY_PREV;
      }
      if (rotation == 1U) {
        return LV_KEY_LEFT;
      }
      if (rotation == 2U) {
        return LV_KEY_NEXT;
      }
      return LV_KEY_RIGHT;
    case 3:
      if (rotation == 0U) {
        return LV_KEY_NEXT;
      }
      if (rotation == 1U) {
        return LV_KEY_RIGHT;
      }
      if (rotation == 2U) {
        return LV_KEY_PREV;
      }
      return LV_KEY_LEFT;
    case 4:
      if (rotation == 0U) {
        return LV_KEY_LEFT;
      }
      if (rotation == 1U) {
        return LV_KEY_NEXT;
      }
      if (rotation == 2U) {
        return LV_KEY_RIGHT;
      }
      return LV_KEY_PREV;
    case 5:
      if (rotation == 0U) {
        return LV_KEY_RIGHT;
      }
      if (rotation == 1U) {
        return LV_KEY_PREV;
      }
      if (rotation == 2U) {
        return LV_KEY_LEFT;
      }
      return LV_KEY_NEXT;
    default:
      return LV_KEY_ENTER;
  }
}

bool parseHexRgb(const char* text, uint32_t* out_rgb) {
  if (text == nullptr || text[0] == '\0' || out_rgb == nullptr) {
    return false;
  }
  const char* begin = text;
  if (begin[0] == '#') {
    ++begin;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(begin, &end, 16);
  if (end == begin || *end != '\0' || value > 0xFFFFFFUL) {
    return false;
  }
  *out_rgb = static_cast<uint32_t>(value);
  return true;
}

String asciiFallbackForUiText(const char* text) {
  String out;
  if (text == nullptr || text[0] == '\0') {
    return out;
  }
  const uint8_t* cursor = reinterpret_cast<const uint8_t*>(text);
  while (*cursor != 0U) {
    const uint8_t c = *cursor;
    if (c < 0x80U) {
      out += static_cast<char>(c);
      ++cursor;
      continue;
    }
    if (c == 0xC2U && cursor[1] != 0U) {
      if (cursor[1] == 0xA0U) {
        out += ' ';
      }
      cursor += 2;
      continue;
    }
    if (c == 0xC3U && cursor[1] != 0U) {
      switch (cursor[1]) {
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
          out += 'A';
          break;
        case 0x87:
          out += 'C';
          break;
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
          out += 'E';
          break;
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
          out += 'I';
          break;
        case 0x91:
          out += 'N';
          break;
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x98:
          out += 'O';
          break;
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
          out += 'U';
          break;
        case 0x9D:
          out += 'Y';
          break;
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
          out += 'a';
          break;
        case 0xA7:
          out += 'c';
          break;
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
          out += 'e';
          break;
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
          out += 'i';
          break;
        case 0xB1:
          out += 'n';
          break;
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB6:
        case 0xB8:
          out += 'o';
          break;
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
          out += 'u';
          break;
        case 0xBD:
        case 0xBF:
          out += 'y';
          break;
        default:
          break;
      }
      cursor += 2;
      continue;
    }
    if (c == 0xC5U && cursor[1] != 0U) {
      if (cursor[1] == 0x92U) {
        out += "OE";
      } else if (cursor[1] == 0x93U) {
        out += "oe";
      }
      cursor += 2;
      continue;
    }
    if (c == 0xE2U && cursor[1] != 0U && cursor[2] != 0U) {
      if (cursor[1] == 0x80U && cursor[2] == 0x99U) {
        out += '\'';
      } else if (cursor[1] == 0x80U && (cursor[2] == 0x93U || cursor[2] == 0x94U)) {
        out += '-';
      } else if (cursor[1] == 0x80U && cursor[2] == 0xA6U) {
        out += "...";
      }
      cursor += 3;
      continue;
    }
    if ((c & 0xE0U) == 0xC0U && cursor[1] != 0U) {
      cursor += 2;
      continue;
    }
    if ((c & 0xF0U) == 0xE0U && cursor[1] != 0U && cursor[2] != 0U) {
      cursor += 3;
      continue;
    }
    if ((c & 0xF8U) == 0xF0U && cursor[1] != 0U && cursor[2] != 0U && cursor[3] != 0U) {
      cursor += 4;
      continue;
    }
    ++cursor;
  }
  return out;
}

uint32_t lerpRgb(uint32_t from_rgb, uint32_t to_rgb, uint16_t progress_per_mille) {
  if (progress_per_mille >= 1000U) {
    return to_rgb;
  }
  const uint32_t from_r = (from_rgb >> 16) & 0xFFU;
  const uint32_t from_g = (from_rgb >> 8) & 0xFFU;
  const uint32_t from_b = from_rgb & 0xFFU;
  const uint32_t to_r = (to_rgb >> 16) & 0xFFU;
  const uint32_t to_g = (to_rgb >> 8) & 0xFFU;
  const uint32_t to_b = to_rgb & 0xFFU;

  const uint32_t out_r = from_r + ((to_r - from_r) * progress_per_mille) / 1000U;
  const uint32_t out_g = from_g + ((to_g - from_g) * progress_per_mille) / 1000U;
  const uint32_t out_b = from_b + ((to_b - from_b) * progress_per_mille) / 1000U;
  return (out_r << 16) | (out_g << 8) | out_b;
}

const char* mapSymbolToken(const char* symbol) {
  if (symbol == nullptr || symbol[0] == '\0') {
    return nullptr;
  }
  if (std::strcmp(symbol, "LOCK") == 0) {
    return LV_SYMBOL_CLOSE;
  }
  if (std::strcmp(symbol, "ALERT") == 0) {
    return LV_SYMBOL_WARNING;
  }
  if (std::strcmp(symbol, "SCAN") == 0) {
    return LV_SYMBOL_EYE_OPEN;
  }
  if (std::strcmp(symbol, "WIN") == 0) {
    return LV_SYMBOL_OK;
  }
  if (std::strcmp(symbol, "READY") == 0) {
    return LV_SYMBOL_POWER;
  }
  if (std::strcmp(symbol, "RUN") == 0) {
    return LV_SYMBOL_PLAY;
  }
  return nullptr;
}

uint32_t mixNoise(uint32_t value, uintptr_t salt) {
  uint32_t x = value ^ static_cast<uint32_t>(salt);
  x ^= (x << 13);
  x ^= (x >> 17);
  x ^= (x << 5);
  return x;
}

int16_t signedNoise(uint32_t value, uintptr_t salt, int16_t amplitude) {
  if (amplitude <= 0) {
    return 0;
  }
  const uint32_t mixed = mixNoise(value * 1103515245UL + 12345UL, salt);
  const int32_t span = static_cast<int32_t>(amplitude) * 2 + 1;
  return static_cast<int16_t>(static_cast<int32_t>(mixed % static_cast<uint32_t>(span)) - amplitude);
}

constexpr const char* kWinEtapeCracktroTitle = "PROFESSEUR ZACUS";
constexpr const char* kWinEtapeCracktroScroll =
    "PROUDLY PRESENTS ... ... NO CLOUD • PURE SIGNAL ...";
constexpr const char* kWinEtapeCracktroBottomScroll =
    "... Le Professeur SAILLANT Franck HOTAMP vous salue bien ...";
constexpr const char* kWinEtapeDemoTitle = "BRAVO Brigade Z";
constexpr const char* kWinEtapeDemoScroll =
    "Vous n’avez plus qu’a valider sur le téléphone qui sonne";
constexpr const char* kWinEtapeWaitingSubtitle = "Validation par reponse au telephone";
constexpr const char* kWinEtapeFxScrollTextA =
    "DEMO MODE - BRAVO BRIGADE Z - LE MYSTERE DU PROFESSEUR ZACUS - ";
constexpr const char* kWinEtapeFxScrollTextB =
    "WINNER MODE - STAGE B - KEEP THE BEAT - ";
constexpr const char* kWinEtapeFxScrollTextC =
    "BOINGBALL MODE - SCENE WIN ETAPE - ";
constexpr uint32_t kWinEtape1CelebrateMs = 20000U;
constexpr uint32_t kWinEtape1WinnerMs = 20000U;
constexpr uint32_t kWinEtape1CreditsStartMs = kWinEtape1CelebrateMs + kWinEtape1WinnerMs;

constexpr uint16_t kIntroTickMs = 42U;
constexpr uint32_t kUiUpdateFrameMs = 42U;
constexpr uint32_t kUiUpdateFrameMsLaDetectorLgfx = 40U;
constexpr uint32_t kIntroCracktroMsDefault = 30000U;
constexpr uint32_t kIntroTransitionMsDefault = 15000U;
constexpr uint32_t kIntroCleanMsDefault = 20000U;
constexpr uint16_t kIntroFxBpmDefault = 125U;
constexpr uint16_t kIntroB1CrashMsDefault = 4000U;
constexpr uint16_t kIntroOutroMs = 400U;
constexpr uint32_t kWinEtapeAutorunLoopMs = 120000U;
constexpr uint32_t kIntroCracktroMsMin = 1000U;
constexpr uint32_t kIntroCracktroMsMax = 120000U;
constexpr uint32_t kIntroTransitionMsMin = 300U;
constexpr uint32_t kIntroTransitionMsMax = 60000U;
constexpr uint32_t kIntroCleanMsMin = 1000U;
constexpr uint32_t kIntroCleanMsMax = 120000U;
constexpr uint16_t kIntroB1CrashMsMin = 3000U;
constexpr uint16_t kIntroB1CrashMsMax = 5000U;
constexpr uint16_t kIntroScrollApxPerSecDefault = 216U;
constexpr uint16_t kIntroScrollBotApxPerSecDefault = 108U;
constexpr uint16_t kIntroScrollCpxPerSecDefault = 72U;
constexpr uint16_t kIntroScrollSpeedMin = 10U;
constexpr uint16_t kIntroScrollSpeedMax = 400U;
constexpr uint16_t kIntroScrollBotSpeedMin = 60U;
constexpr uint16_t kIntroScrollBotSpeedMax = 160U;
constexpr uint8_t kIntroSineAmpApxDefault = 96U;
constexpr uint8_t kIntroSineAmpCpxDefault = 96U;
constexpr uint8_t kIntroSineAmpMin = 8U;
constexpr uint8_t kIntroSineAmpMax = 180U;
constexpr uint16_t kIntroSinePeriodPxDefault = 104U;
constexpr uint16_t kIntroSinePeriodMin = 40U;
constexpr uint16_t kIntroSinePeriodMax = 220U;
constexpr float kIntroSinePhaseSpeedDefault = 1.9f;
constexpr float kIntroSinePhaseSpeedMin = 0.1f;
constexpr float kIntroSinePhaseSpeedMax = 5.0f;
constexpr uint16_t kIntroCubeFov = 156U;
constexpr uint16_t kIntroCubeZOffset = 320U;
constexpr uint16_t kIntroCubeScale = 88U;
constexpr int16_t kIntroBottomScrollMarginPx = 8;
constexpr uint8_t kIntroCenterScrollPadSpaces = 14U;

// Retro key color set used by SCENE_WIN_ETAPE (friendly both for RGB332 and RGB565 displays).
constexpr uint32_t kIntroPaletteRgb[] = {
    0x000020UL,  // 0 bg0
    0x00112FUL,  // 1 bg1
    0x0A2B54UL,  // 2 bg2
    0x00FFFFUL,  // 3 accent cyan
    0xFF55FFUL,  // 4 accent magenta
    0xFFFF55UL,  // 5 accent yellow
    0x005ACCUL,  // 6 accent blue
    0xFFFFFFUL,  // 7 text white
    0x000000UL,  // 8 shadow black
    0x9ED7FFUL,  // 9 text light blue
    0xFFB26BUL,  // 10 warm particle
    0x163255UL,  // 11 dither stripe dark
    0x23456AUL,  // 12 dither stripe mid
    0x0F2D4EUL,  // 13 tunnel stripe dark
    0x1A4E75UL,  // 14 tunnel stripe light
    0xD8EFFFUL,  // 15 star near white-blue
};

constexpr uint8_t kIntroPaletteCount =
    static_cast<uint8_t>(sizeof(kIntroPaletteRgb) / sizeof(kIntroPaletteRgb[0]));

lv_color_t introPaletteColor(uint8_t index) {
  return lv_color_hex(kIntroPaletteRgb[index % kIntroPaletteCount]);
}

template <typename T>
T clampValue(T value, T min_value, T max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

void copyStringBounded(char* dst, size_t dst_size, const char* src) {
  if (dst == nullptr || dst_size == 0U) {
    return;
  }
  dst[0] = '\0';
  if (src == nullptr || src[0] == '\0') {
    return;
  }
  std::strncpy(dst, src, dst_size - 1U);
  dst[dst_size - 1U] = '\0';
}

String trimCopy(String text) {
  text.trim();
  return text;
}

bool parseUint32Text(const String& text, uint32_t* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  if (text.length() == 0U) {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  *out_value = static_cast<uint32_t>(value);
  return true;
}

bool parseInt16Text(const String& text, int16_t* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  if (text.length() == 0U) {
    return false;
  }
  char* end = nullptr;
  const long value = strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  if (value < -32768L || value > 32767L) {
    return false;
  }
  *out_value = static_cast<int16_t>(value);
  return true;
}

bool parseFloatText(const String& text, float* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  if (text.length() == 0U) {
    return false;
  }
  char* end = nullptr;
  const float value = strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  *out_value = value;
  return true;
}

bool parseFxPresetToken(const String& text, ui::fx::FxPreset* out_preset) {
  if (out_preset == nullptr || text.length() == 0U) {
    return false;
  }
  String normalized = trimCopy(text);
  normalized.toLowerCase();
  if (normalized == "demo") {
    *out_preset = ui::fx::FxPreset::kDemo;
    return true;
  }
  if (normalized == "winner") {
    *out_preset = ui::fx::FxPreset::kWinner;
    return true;
  }
  if (normalized == "win_etape1" || normalized == "winetape1") {
    *out_preset = ui::fx::FxPreset::kWinEtape1;
    return true;
  }
  if (normalized == "fireworks") {
    *out_preset = ui::fx::FxPreset::kFireworks;
    return true;
  }
  if (normalized == "boingball") {
    *out_preset = ui::fx::FxPreset::kBoingball;
    return true;
  }
  if (normalized == "uson_proto" || normalized == "u_son_proto") {
    *out_preset = ui::fx::FxPreset::kUsonProto;
    return true;
  }
  if (normalized == "la_detector" || normalized == "ladetector") {
    *out_preset = ui::fx::FxPreset::kLaDetector;
    return true;
  }
  return false;
}

bool parseFxModeToken(const String& text, ui::fx::FxMode* out_mode) {
  if (out_mode == nullptr || text.length() == 0U) {
    return false;
  }
  String normalized = trimCopy(text);
  normalized.toLowerCase();
  if (normalized == "classic") {
    *out_mode = ui::fx::FxMode::kClassic;
    return true;
  }
  if (normalized == "starfield3d" || normalized == "starfield") {
    *out_mode = ui::fx::FxMode::kStarfield3D;
    return true;
  }
  if (normalized == "dotsphere3d" || normalized == "dot_sphere" || normalized == "dotsphere") {
    *out_mode = ui::fx::FxMode::kDotSphere3D;
    return true;
  }
  if (normalized == "voxel" || normalized == "voxellandscape") {
    *out_mode = ui::fx::FxMode::kVoxelLandscape;
    return true;
  }
  if (normalized == "raycorridor" || normalized == "ray") {
    *out_mode = ui::fx::FxMode::kRayCorridor;
    return true;
  }
  return false;
}

bool parseFxScrollFontToken(const String& text, ui::fx::FxScrollFont* out_font) {
  if (out_font == nullptr || text.length() == 0U) {
    return false;
  }
  String normalized = trimCopy(text);
  normalized.toLowerCase();
  if (normalized == "basic") {
    *out_font = ui::fx::FxScrollFont::kBasic;
    return true;
  }
  if (normalized == "bold") {
    *out_font = ui::fx::FxScrollFont::kBold;
    return true;
  }
  if (normalized == "outline") {
    *out_font = ui::fx::FxScrollFont::kOutline;
    return true;
  }
  if (normalized == "italic") {
    *out_font = ui::fx::FxScrollFont::kItalic;
    return true;
  }
  return false;
}

const char* fxPresetToken(ui::fx::FxPreset preset) {
  switch (preset) {
    case ui::fx::FxPreset::kDemo:
      return "demo";
    case ui::fx::FxPreset::kWinner:
      return "winner";
    case ui::fx::FxPreset::kWinEtape1:
      return "win_etape1";
    case ui::fx::FxPreset::kFireworks:
      return "fireworks";
    case ui::fx::FxPreset::kBoingball:
      return "boingball";
    case ui::fx::FxPreset::kUsonProto:
      return "uson_proto";
    case ui::fx::FxPreset::kLaDetector:
      return "la_detector";
    default:
      return "demo";
  }
}

const char* fxModeToken(ui::fx::FxMode mode) {
  switch (mode) {
    case ui::fx::FxMode::kStarfield3D:
      return "starfield3d";
    case ui::fx::FxMode::kDotSphere3D:
      return "dotsphere3d";
    case ui::fx::FxMode::kVoxelLandscape:
      return "voxel";
    case ui::fx::FxMode::kRayCorridor:
      return "raycorridor";
    case ui::fx::FxMode::kClassic:
    default:
      return "classic";
  }
}

const char* fxScrollFontToken(ui::fx::FxScrollFont font) {
  switch (font) {
    case ui::fx::FxScrollFont::kBasic:
      return "basic";
    case ui::fx::FxScrollFont::kBold:
      return "bold";
    case ui::fx::FxScrollFont::kOutline:
      return "outline";
    case ui::fx::FxScrollFont::kItalic:
      return "italic";
    default:
      return "basic";
  }
}

float easeOutBack(float t) {
  const float c1 = 1.70158f;
  const float c3 = c1 + 1.0f;
  const float one_minus = t - 1.0f;
  return 1.0f + c3 * one_minus * one_minus * one_minus + c1 * one_minus * one_minus;
}

}  // namespace

void UiManager::animSetRandomTextOpa(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  uint8_t min_opa = 60U;
  constexpr uint8_t max_opa = LV_OPA_COVER;
  if (g_instance != nullptr) {
    const uint8_t glitch_pct = g_instance->text_glitch_pct_;
    const uint16_t atten = static_cast<uint16_t>(glitch_pct) * 2U;
    if (atten >= 190U) {
      min_opa = 14U;
    } else {
      min_opa = static_cast<uint8_t>(204U - atten);
      if (min_opa < 14U) {
        min_opa = 14U;
      }
    }
    if (target == g_instance->scene_subtitle_label_ && min_opa < 34U) {
      min_opa = 34U;
    }
  }
  const uint32_t mixed =
      mixNoise(static_cast<uint32_t>(value) * 1664525UL + 1013904223UL, reinterpret_cast<uintptr_t>(target) ^ 0x7F4A7C15UL);
  const uint16_t span = static_cast<uint16_t>(max_opa - min_opa);
  const lv_opa_t out = static_cast<lv_opa_t>(min_opa + static_cast<uint16_t>(mixed % (static_cast<uint32_t>(span) + 1U)));
  lv_obj_set_style_text_opa(target, out, LV_PART_MAIN);
  lv_obj_set_style_opa(target, out, LV_PART_MAIN);
}

bool UiManager::begin() {
  if (ready_) {
    return true;
  }

  g_instance = this;
  lv_init();

  drivers::display::DisplayHalConfig display_cfg = {};
  display_cfg.width = FREENOVE_LCD_WIDTH;
  display_cfg.height = FREENOVE_LCD_HEIGHT;
  display_cfg.rotation = FREENOVE_LCD_ROTATION;
  if (!drivers::display::displayHal().begin(display_cfg)) {
    UI_LOGI("display init failed");
    return false;
  }
  drivers::display::displayHal().fillScreen(0x0000U);
  initGraphicsPipeline();
  if (draw_buf1_ == nullptr) {
    UI_LOGI("graphics pipeline init failed");
    return false;
  }

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  if ((FREENOVE_LCD_ROTATION & 0x1U) != 0U) {
    disp_drv.hor_res = FREENOVE_LCD_HEIGHT;
    disp_drv.ver_res = FREENOVE_LCD_WIDTH;
  } else {
    disp_drv.hor_res = FREENOVE_LCD_WIDTH;
    disp_drv.ver_res = FREENOVE_LCD_HEIGHT;
  }
  disp_drv.flush_cb = displayFlushCb;
  disp_drv.draw_buf = &draw_buf_;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t keypad_drv;
  lv_indev_drv_init(&keypad_drv);
  keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
  keypad_drv.read_cb = keypadReadCb;
  lv_indev_drv_register(&keypad_drv);

#if FREENOVE_HAS_TOUCH
  static lv_indev_drv_t touch_drv;
  lv_indev_drv_init(&touch_drv);
  touch_drv.type = LV_INDEV_TYPE_POINTER;
  touch_drv.read_cb = touchReadCb;
  lv_indev_drv_register(&touch_drv);
#endif

  player_ui_.reset();
  UiFonts::init();
  createWidgets();
  ui::fx::FxEngineConfig fx_cfg = {};
  fx_cfg.sprite_width = 160U;
  fx_cfg.sprite_height = 120U;
  fx_cfg.target_fps = 18U;
#ifdef UI_FX_SPRITE_W
  fx_cfg.sprite_width = static_cast<uint16_t>(UI_FX_SPRITE_W);
#endif
#ifdef UI_FX_SPRITE_H
  fx_cfg.sprite_height = static_cast<uint16_t>(UI_FX_SPRITE_H);
#endif
#ifdef UI_FX_TARGET_FPS
  fx_cfg.target_fps = static_cast<uint8_t>(UI_FX_TARGET_FPS);
#endif
  fx_cfg.lgfx_backend = drivers::display::displayHalUsesLovyanGfx();
  bool fx_ready = fx_engine_.begin(fx_cfg);
  if (!fx_ready && fx_cfg.lgfx_backend) {
    // Keep animated scenes alive under memory pressure by retrying with a smaller sprite.
    ui::fx::FxEngineConfig fallback_cfg = fx_cfg;
    fallback_cfg.sprite_width = 128U;
    fallback_cfg.sprite_height = 96U;
    if (fallback_cfg.target_fps > 15U) {
      fallback_cfg.target_fps = 15U;
    }
    UI_LOGI("FX init failed at %ux%u@%u, retry fallback %ux%u@%u",
            static_cast<unsigned int>(fx_cfg.sprite_width),
            static_cast<unsigned int>(fx_cfg.sprite_height),
            static_cast<unsigned int>(fx_cfg.target_fps),
            static_cast<unsigned int>(fallback_cfg.sprite_width),
            static_cast<unsigned int>(fallback_cfg.sprite_height),
            static_cast<unsigned int>(fallback_cfg.target_fps));
    fx_ready = fx_engine_.begin(fallback_cfg);
    if (fx_ready) {
      fx_cfg = fallback_cfg;
    }
  }
  if (!fx_ready) {
    fx_engine_.setEnabled(false);
    UI_LOGI("FX engine disabled: init failed");
  } else {
    UI_LOGI("FX engine ready sprite=%ux%u target_fps=%u",
            static_cast<unsigned int>(fx_cfg.sprite_width),
            static_cast<unsigned int>(fx_cfg.sprite_height),
            static_cast<unsigned int>(fx_cfg.target_fps));
  }
  last_lvgl_tick_ms_ = millis();
  graphics_stats_last_report_ms_ = last_lvgl_tick_ms_;
  ready_ = true;
  UI_LOGI("LVGL + display ready backend=%s", drivers::display::displayHalUsesLovyanGfx() ? "lgfx" : "tftespi");
  if (kUseDemoAutorunWinEtapeRuntime) {
    UI_LOGI("autorun SCENE_WIN_ETAPE enabled");
  }
  dumpGraphicsStatus();
  return true;
}

void UiManager::tick(uint32_t now_ms) {
  (void)now_ms;
  update();
}

void UiManager::setHardwareController(HardwareManager* hardware) {
  hardware_ = hardware;
}

void UiManager::setLaMetrics(const UiLaMetrics& metrics) {
  setLaDetectionState(metrics.locked,
                      metrics.stability_pct,
                      metrics.stable_ms,
                      metrics.stable_target_ms,
                      metrics.gate_elapsed_ms,
                      metrics.gate_timeout_ms);
}

void UiManager::submitSceneFrame(const UiSceneFrame& frame) {
  renderScene(frame.scenario,
              frame.screen_scene_id,
              frame.step_id,
              frame.audio_pack_id,
              frame.audio_playing,
              frame.screen_payload_json);
}

void UiManager::submitInputEvent(const UiInputEvent& event) {
  if (event.type == UiInputEventType::kTouch) {
    handleTouch(event.touch_x, event.touch_y, event.touch_pressed);
    return;
  }
  handleButton(event.key, event.long_press);
}

bool UiManager::consumeRuntimeEvent(char* out_event, size_t capacity) {
  return qr_scene_controller_.consumeRuntimeEvent(out_event, capacity);
}

bool UiManager::simulateQrPayload(const char* payload) {
  return qr_scene_controller_.queueSimulatedPayload(payload);
}

void UiManager::dumpStatus(UiStatusTopic topic) const {
  if (topic == UiStatusTopic::kMemory) {
    dumpMemoryStatus();
    return;
  }
  dumpGraphicsStatus();
}

void UiManager::update() {
  if (!ready_) {
    return;
  }
  const uint32_t now_ms = millis();
  const uint32_t elapsed_ms = now_ms - last_lvgl_tick_ms_;
  const bool lgfx_hard_mode =
      scene_use_lgfx_text_overlay_ && (scene_lgfx_hard_mode_ || la_detection_scene_);
  const bool win_etape_overlay_scene =
      scene_use_lgfx_text_overlay_ && scene_status_.valid && isWinEtapeSceneId(scene_status_.scene_id);
  const uint32_t frame_period_ms =
      (lgfx_hard_mode || win_etape_overlay_scene) ? kUiUpdateFrameMsLaDetectorLgfx : kUiUpdateFrameMs;
  const bool needs_trans_buffer = kUseColor256Runtime || buffer_cfg_.draw_in_psram;
  if (!async_flush_enabled_ &&
      dma_requested_ &&
      dma_available_ &&
      !buffer_cfg_.dma_enabled &&
      !buffer_cfg_.full_frame &&
      async_fallback_until_ms_ != 0U &&
      static_cast<int32_t>(now_ms - async_fallback_until_ms_) >= 0 &&
      !flush_ctx_.pending) {
    if (!needs_trans_buffer || dma_trans_buf_ != nullptr) {
      async_flush_enabled_ = true;
      buffer_cfg_.dma_enabled = true;
      async_fallback_until_ms_ = 0U;
      UI_LOGI("DMA async rearmed after fallback");
    }
  }
  const bool flush_busy_now = isDisplayOutputBusy();
  auto run_lvgl_draw = [this]() {
    if (pending_full_repaint_request_ && lv_scr_act() != nullptr) {
      lv_obj_invalidate(lv_scr_act());
      pending_full_repaint_request_ = false;
    }
    const uint32_t draw_start = micros();
    lv_timer_handler();
    const uint32_t draw_elapsed = micros() - draw_start;
    graphics_stats_.draw_time_total_us += draw_elapsed;
    if (draw_elapsed > graphics_stats_.draw_time_max_us) {
      graphics_stats_.draw_time_max_us = draw_elapsed;
    }
    graphics_stats_.draw_count += 1U;
  };

  if (elapsed_ms >= frame_period_ms) {
    lv_tick_inc(elapsed_ms);
    last_lvgl_tick_ms_ = now_ms;
  } else {
    if (pending_lvgl_flush_request_ && !flush_busy_now) {
      if (!lgfx_hard_mode) {
        run_lvgl_draw();
      }
      pending_lvgl_flush_request_ = false;
    }
    pollAsyncFlush();
    return;
  }
  if (player_ui_.consumeDirty()) {
    updatePageLine();
  }
  renderMicrophoneWaveform();
  qr_scene_controller_.tick(now_ms, &qr_scan_, qr_rules_, scene_subtitle_label_, scene_symbol_label_);
  pollAsyncFlush();
  const bool flush_busy =
      isDisplayOutputBusy();
  if (!intro_active_ && scene_status_.valid && fx_engine_.config().lgfx_backend) {
    const bool la_detector_scene = (std::strcmp(scene_status_.scene_id, "SCENE_LA_DETECTOR") == 0);
    const bool warning_blocks_direct_fx =
        warning_gyrophare_enabled_ && warning_gyrophare_disable_direct_fx_ &&
        (std::strcmp(scene_status_.scene_id, "SCENE_WARNING") == 0);
    const bool wants_direct_fx_scene =
        isDirectFxSceneId(scene_status_.scene_id) && !la_detector_scene && !warning_blocks_direct_fx;
    const bool retry_allowed = (fx_rearm_retry_after_ms_ == 0U) ||
                               (static_cast<int32_t>(now_ms - fx_rearm_retry_after_ms_) >= 0);
    if (wants_direct_fx_scene && retry_allowed && (!direct_fx_scene_active_ || !fx_engine_.enabled())) {
      direct_fx_scene_active_ = true;
      armDirectFxScene(scene_status_.scene_id,
                       std::strcmp(scene_status_.scene_id, "SCENE_TEST_LAB") == 0,
                       scene_status_.title,
                       scene_status_.subtitle);
    }
  }
  if (direct_fx_scene_active_ && scene_status_.valid && std::strcmp(scene_status_.scene_id, "SCENE_WIN_ETAPE1") == 0) {
    const uint32_t scene_elapsed_ms =
        (scene_runtime_started_ms_ == 0U || now_ms < scene_runtime_started_ms_) ? 0U : (now_ms - scene_runtime_started_ms_);
    ui::fx::FxPreset target_preset = ui::fx::FxPreset::kWinEtape1;
    if (scene_elapsed_ms < kWinEtape1CelebrateMs) {
      target_preset = ui::fx::FxPreset::kFireworks;
    } else if (scene_elapsed_ms < kWinEtape1CreditsStartMs) {
      target_preset = ui::fx::FxPreset::kWinner;
    }
    if (fx_engine_.preset() != target_preset) {
      fx_engine_.setPreset(target_preset);
      fx_engine_.setScrollerCentered(false);
      if (target_preset != ui::fx::FxPreset::kWinEtape1) {
        fx_engine_.setScrollText(nullptr);
      }
    }
  }
  const bool fx_candidate = (intro_active_ || direct_fx_scene_active_) && fx_engine_.enabled();
  const bool hold_fx_for_overlay = win_etape_overlay_scene && (overlay_recovery_frames_ > 0U);
  const bool fx_render_this_frame = fx_candidate && !hold_fx_for_overlay;
  if (hold_fx_for_overlay) {
    overlay_recovery_frames_ = static_cast<uint8_t>(overlay_recovery_frames_ - 1U);
    graphics_stats_.fx_skip_flush_busy += 1U;
  }
  if (flush_busy) {
    graphics_stats_.flush_blocked_count += 1U;
    if (fx_render_this_frame) {
      graphics_stats_.fx_skip_flush_busy += 1U;
    }
    pending_lvgl_flush_request_ = true;
    pollAsyncFlush();
    return;
  }
  // Frame order contract: FX background first, then LVGL flush, then LGFX text/scene overlays on top.
  if (fx_render_this_frame) {
    ui::fx::FxScenePhase fx_phase = ui::fx::FxScenePhase::kPhaseC;
    if (intro_active_) {
      fx_phase = ui::fx::FxScenePhase::kIdle;
      switch (intro_state_) {
        case IntroState::PHASE_A_CRACKTRO:
          fx_phase = ui::fx::FxScenePhase::kPhaseA;
          break;
        case IntroState::PHASE_B_TRANSITION:
          fx_phase = ui::fx::FxScenePhase::kPhaseB;
          break;
        case IntroState::PHASE_C_CLEAN:
        case IntroState::PHASE_C_LOOP:
          fx_phase = ui::fx::FxScenePhase::kPhaseC;
          break;
        default:
          fx_phase = ui::fx::FxScenePhase::kIdle;
          break;
      }
    }
    if (fx_engine_.renderFrame(now_ms,
                               drivers::display::displayHal(),
                               static_cast<uint16_t>(activeDisplayWidth()),
                               static_cast<uint16_t>(activeDisplayHeight()),
                               fx_phase)) {
      invalidateFxOverlayObjects();
    }
  }
  if (isDisplayOutputBusy()) {
    graphics_stats_.flush_blocked_count += 1U;
    pending_lvgl_flush_request_ = true;
    pollAsyncFlush();
    if (isDisplayOutputBusy()) {
      return;
    }
  }
  if (!lgfx_hard_mode) {
    run_lvgl_draw();
  }
  pending_lvgl_flush_request_ = false;
  pollAsyncFlush();
  // Overlay text must be drawn after LVGL flush; wait briefly for async DMA completion.
  const bool overlay_needed = scene_use_lgfx_text_overlay_ || la_detection_scene_;
  const uint32_t overlay_wait_budget_us = win_etape_overlay_scene ? 120000U : 50000U;
  const uint32_t overlay_dma_wait_us = win_etape_overlay_scene ? 4200U : 1800U;
  const uint32_t overlay_spin_wait_us = win_etape_overlay_scene ? 60U : 120U;
  const uint32_t overlay_wait_started_us = micros();
  bool overlay_wait_timed_out = false;
  while (isDisplayOutputBusy()) {
    pollAsyncFlush();
    if (!isDisplayOutputBusy()) {
      break;
    }
    drivers::display::displayHal().waitDmaComplete(overlay_dma_wait_us);
    pollAsyncFlush();
    if (!isDisplayOutputBusy()) {
      break;
    }
    if ((micros() - overlay_wait_started_us) >= overlay_wait_budget_us) {
      overlay_wait_timed_out = true;
      break;
    }
    delayMicroseconds(overlay_spin_wait_us);
  }
  if (overlay_wait_timed_out && win_etape_overlay_scene) {
    // Give the text overlay one last chance by draining a lingering DMA transaction.
    drivers::display::displayHal().waitDmaComplete(12000U);
    pollAsyncFlush();
  }
  if (isDisplayOutputBusy()) {
    if (overlay_needed) {
      ++overlay_skip_busy_count_;
    }
    if (win_etape_overlay_scene) {
      overlay_recovery_frames_ = 2U;
      pending_lvgl_flush_request_ = true;
    }
    return;
  }
  overlay_recovery_frames_ = 0U;
  if (scene_use_lgfx_text_overlay_) {
    renderLgfxSceneTextOverlay(now_ms);
  }
  renderLgfxLaDetectorOverlay(now_ms);
}

void UiManager::renderLgfxSceneTextOverlay(uint32_t now_ms) {
  if (!scene_use_lgfx_text_overlay_ || !scene_status_.valid) {
    return;
  }
  if (std::strcmp(scene_status_.scene_id, "SCENE_LA_DETECTOR") == 0) {
    return;
  }
  drivers::display::DisplayHal& display = drivers::display::displayHal();
  if (!display.supportsOverlayText()) {
    ++overlay_draw_fail_count_;
    return;
  }
  bool write_ready = false;
  for (uint8_t attempt = 0U; attempt < 3U; ++attempt) {
    if (display.startWrite()) {
      write_ready = true;
      break;
    }
    display.waitDmaComplete(2200U);
    delayMicroseconds(static_cast<unsigned int>(100U * (attempt + 1U)));
  }
  if (!write_ready) {
    ++overlay_startwrite_fail_count_;
    ++overlay_draw_fail_count_;
    return;
  }

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  if (width <= 0 || height <= 0) {
    display.endWrite();
    return;
  }
  auto to565 = [&display](uint32_t rgb) -> uint16_t {
    return display.color565(static_cast<uint8_t>((rgb >> 16U) & 0xFFU),
                            static_cast<uint8_t>((rgb >> 8U) & 0xFFU),
                            static_cast<uint8_t>(rgb & 0xFFU));
  };
  auto ensureReadableRgb = [](uint32_t rgb, uint32_t fallback) -> uint32_t {
    const uint8_t r = static_cast<uint8_t>((rgb >> 16U) & 0xFFU);
    const uint8_t g = static_cast<uint8_t>((rgb >> 8U) & 0xFFU);
    const uint8_t b = static_cast<uint8_t>(rgb & 0xFFU);
    const uint16_t luma = static_cast<uint16_t>((static_cast<uint16_t>(r) * 30U) +
                                                (static_cast<uint16_t>(g) * 59U) +
                                                (static_cast<uint16_t>(b) * 11U)) /
                          100U;
    if (luma < 70U) {
      return fallback;
    }
    return rgb;
  };
  auto mixRgb = [](uint32_t lhs, uint32_t rhs, uint8_t rhs_pct) -> uint32_t {
    const uint8_t lhs_pct = static_cast<uint8_t>(100U - rhs_pct);
    const uint8_t l_r = static_cast<uint8_t>((lhs >> 16U) & 0xFFU);
    const uint8_t l_g = static_cast<uint8_t>((lhs >> 8U) & 0xFFU);
    const uint8_t l_b = static_cast<uint8_t>(lhs & 0xFFU);
    const uint8_t r_r = static_cast<uint8_t>((rhs >> 16U) & 0xFFU);
    const uint8_t r_g = static_cast<uint8_t>((rhs >> 8U) & 0xFFU);
    const uint8_t r_b = static_cast<uint8_t>(rhs & 0xFFU);
    const uint8_t out_r = static_cast<uint8_t>((static_cast<uint16_t>(l_r) * lhs_pct + static_cast<uint16_t>(r_r) * rhs_pct) / 100U);
    const uint8_t out_g = static_cast<uint8_t>((static_cast<uint16_t>(l_g) * lhs_pct + static_cast<uint16_t>(r_g) * rhs_pct) / 100U);
    const uint8_t out_b = static_cast<uint8_t>((static_cast<uint16_t>(l_b) * lhs_pct + static_cast<uint16_t>(r_b) * rhs_pct) / 100U);
    return (static_cast<uint32_t>(out_r) << 16U) | (static_cast<uint32_t>(out_g) << 8U) | out_b;
  };

  const uint32_t text_rgb = ensureReadableRgb(scene_status_.text_rgb, 0xF5FAFFUL);
  const uint32_t accent_rgb = scene_status_.accent_rgb;
  const uint16_t title_color = to565(text_rgb);
  const uint16_t symbol_color = to565(mixRgb(text_rgb, accent_rgb, 55U));
  const uint16_t subtitle_color = to565(mixRgb(text_rgb, accent_rgb, 30U));
  const bool uson_proto_scene = (std::strcmp(scene_status_.scene_id, "SCENE_U_SON_PROTO") == 0);
  const bool win_etape1_scene = (std::strcmp(scene_status_.scene_id, "SCENE_WIN_ETAPE1") == 0);
  const uint32_t scene_elapsed_ms = (scene_runtime_started_ms_ == 0U || now_ms < scene_runtime_started_ms_)
                                        ? 0U
                                        : (now_ms - scene_runtime_started_ms_);
  const uint8_t glitch_pct = (scene_status_.text_glitch_pct > 100U) ? 100U : scene_status_.text_glitch_pct;
  int16_t jitter_span = (glitch_pct < 8U) ? 0 : static_cast<int16_t>(1 + (glitch_pct / 18U));
  if (uson_proto_scene && jitter_span > 1) {
    jitter_span = static_cast<int16_t>(jitter_span / 2);
    if (jitter_span < 1) {
      jitter_span = 1;
    }
  }
  const uint32_t seed = pseudoRandom32((now_ms / 16U) ^ scene_status_.payload_crc ^ 0xA53F1UL);

  drivers::display::OverlayFontFace title_font = overlay_title_font_face_;
  uint8_t title_size = 1U;
  if (scene_status_.text_size_pct >= 85U) {
    title_size = 3U;
  } else if (scene_status_.text_size_pct >= 60U) {
    title_size = 2U;
  }
  const bool title_font_is_ibm_family =
      title_font == drivers::display::OverlayFontFace::kIbmBold12 ||
      title_font == drivers::display::OverlayFontFace::kIbmBold16 ||
      title_font == drivers::display::OverlayFontFace::kIbmBold20 ||
      title_font == drivers::display::OverlayFontFace::kIbmBold24;
  if (uson_proto_scene && title_font_is_ibm_family) {
    if (scene_status_.text_size_pct >= 85U) {
      title_font = drivers::display::OverlayFontFace::kIbmBold24;
      title_size = 3U;
    } else if (scene_status_.text_size_pct >= 60U) {
      title_font = drivers::display::OverlayFontFace::kIbmBold24;
      title_size = 2U;
    } else if (scene_status_.text_size_pct >= 40U) {
      title_font = drivers::display::OverlayFontFace::kIbmBold16;
      title_size = 2U;
    } else {
      title_font = drivers::display::OverlayFontFace::kIbmBold16;
      title_size = 1U;
    }
  }
  const drivers::display::OverlayFontFace symbol_font = overlay_symbol_font_face_;
  const uint8_t symbol_size = (scene_status_.text_size_pct >= 60U && !uson_proto_scene) ? 2U : 1U;
  const drivers::display::OverlayFontFace subtitle_font = overlay_subtitle_font_face_;
  const uint8_t subtitle_size = 1U;

  auto jitter = [&](uint32_t salt) -> int16_t {
    if (jitter_span == 0) {
      return 0;
    }
    const uint32_t value = pseudoRandom32(seed ^ salt);
    const uint32_t span = static_cast<uint32_t>(jitter_span * 2 + 1);
    return static_cast<int16_t>(static_cast<int32_t>(value % span) - jitter_span);
  };
  auto resolveY = [height](SceneTextAlign align, uint8_t slot) -> int16_t {
    int16_t y = 0;
    switch (align) {
      case SceneTextAlign::kTop:
        y = (slot == 0U) ? 8 : (slot == 1U ? 38 : 72);
        break;
      case SceneTextAlign::kBottom:
        y = (slot == 0U) ? static_cast<int16_t>(height - 130)
                         : (slot == 1U ? static_cast<int16_t>(height - 70) : static_cast<int16_t>(height - 28));
        break;
      case SceneTextAlign::kCenter:
      default:
        y = (slot == 0U) ? static_cast<int16_t>((height / 2) - 76)
                         : (slot == 1U ? static_cast<int16_t>((height / 2) - 10)
                                       : static_cast<int16_t>((height / 2) + 42));
        break;
    }
    if (y < 2) {
      y = 2;
    } else if (y > (height - 20)) {
      y = static_cast<int16_t>(height - 20);
    }
    return y;
  };

  bool text_attempted = false;
  bool text_draw_ok = false;
  auto drawLine = [&](const char* text,
                      SceneTextAlign align,
                      uint8_t slot,
                      drivers::display::OverlayFontFace font_face,
                      uint8_t size,
                      uint16_t color,
                      uint32_t salt) {
    if (text == nullptr || text[0] == '\0') {
      return;
    }
    text_attempted = true;
    uint8_t effective_size = size;
    if (uson_proto_scene && slot == 1U && scene_status_.text_size_pct >= 40U) {
      const uint16_t pulse_window_ms = static_cast<uint16_t>(now_ms % 1800U);
      if (pulse_window_ms < 180U) {
        effective_size = static_cast<uint8_t>(size + 1U);
      }
    }
    const uint8_t max_size = 4U;
    if (effective_size > max_size) {
      effective_size = max_size;
    }
    const int16_t text_w = display.measureOverlayText(text, font_face, effective_size);
    int16_t x = static_cast<int16_t>((width - text_w) / 2);
    int16_t y = static_cast<int16_t>(resolveY(align, slot) + jitter(salt + 2U));

    drivers::display::OverlayTextCommand command = {};
    command.text = text;
    command.font_face = font_face;
    command.size = effective_size;
    command.opaque_bg = false;

    const uint32_t glitch_gate = pseudoRandom32(seed ^ (salt + 3U)) % 100U;
    if (glitch_pct > 12U && glitch_gate < glitch_pct) {
      command.x = static_cast<int16_t>(x + 1 + jitter(salt + 4U));
      command.y = static_cast<int16_t>(y + jitter(salt + 5U));
      command.color565 = to565(accent_rgb);
      display.drawOverlayText(command);
    }

    command.x = x;
    command.y = y;
    uint16_t final_color = color;
    if (uson_proto_scene && slot == 1U && ((seed ^ salt) & 0x1U) != 0U) {
      final_color = symbol_color;
    }
    command.color565 = final_color;
    if (display.drawOverlayText(command)) {
      text_draw_ok = true;
    }
  };

  bool custom_win_etape1_credits = false;
  if (win_etape1_scene && scene_elapsed_ms >= kWinEtape1CreditsStartMs) {
    custom_win_etape1_credits = true;
    text_attempted = true;
    if (!win_etape_credits_loaded_) {
      win_etape_credits_loaded_ = true;
      win_etape_credits_count_ = 0U;
      std::memset(win_etape_credits_lines_, 0, sizeof(win_etape_credits_lines_));
      std::memset(win_etape_credits_size_, 0, sizeof(win_etape_credits_size_));
      std::memset(win_etape_credits_align_, 0, sizeof(win_etape_credits_align_));
      std::memset(win_etape_credits_pause_ms_, 0, sizeof(win_etape_credits_pause_ms_));
      win_etape_credits_scroll_px_per_sec_ = 16U;
      uint8_t current_size_tag = 0U;   // 0=normal 1=big 2=title 3=small
      uint8_t current_align_tag = 0U;  // 0=center 1=left 2=right
      auto append_credit_line = [&](const char* raw_line, uint16_t pause_ms, bool preserve_blank) {
        if (win_etape_credits_count_ >= kWinEtapeCreditsMaxLines) {
          return;
        }
        const String normalized = asciiFallbackForUiText(raw_line != nullptr ? raw_line : "");
        char cleaned[kWinEtapeCreditsMaxLineChars] = {0};
        normalized.toCharArray(cleaned, sizeof(cleaned));
        trimAsciiWhitespace(cleaned);
        if (cleaned[0] == '\0' && !preserve_blank) {
          if (win_etape_credits_count_ == 0U) {
            return;
          }
          const uint8_t prev = static_cast<uint8_t>(win_etape_credits_count_ - 1U);
          const bool prev_blank = (win_etape_credits_lines_[prev][0] == '\0') ||
                                  (win_etape_credits_lines_[prev][0] == ' ' && win_etape_credits_lines_[prev][1] == '\0');
          if (prev_blank && win_etape_credits_pause_ms_[prev] == 0U) {
            return;
          }
          copyTextSafe(win_etape_credits_lines_[win_etape_credits_count_], kWinEtapeCreditsMaxLineChars, " ");
        } else {
          copyTextSafe(win_etape_credits_lines_[win_etape_credits_count_], kWinEtapeCreditsMaxLineChars, cleaned);
        }
        win_etape_credits_size_[win_etape_credits_count_] = current_size_tag;
        win_etape_credits_align_[win_etape_credits_count_] = current_align_tag;
        win_etape_credits_pause_ms_[win_etape_credits_count_] = pause_ms;
        ++win_etape_credits_count_;
      };
      const char* const credits_paths[] = {
          "/ui/fx/texts/credits.txt",
          "/ui/fx/texts/credits_01.txt",
          "/ui/scene_win_etape.txt",
      };
      bool stop_from_directive = false;
      for (const char* path : credits_paths) {
        if (path == nullptr || path[0] == '\0' || !LittleFS.exists(path)) {
          continue;
        }
        File file = LittleFS.open(path, "r");
        if (!file) {
          continue;
        }
        while (file.available() && win_etape_credits_count_ < kWinEtapeCreditsMaxLines) {
          String line = file.readStringUntil('\n');
          line.replace('\r', ' ');
          char trimmed[kWinEtapeCreditsMaxLineChars] = {0};
          line.toCharArray(trimmed, sizeof(trimmed));
          trimAsciiWhitespace(trimmed);
          if (trimmed[0] == '[') {
            size_t len = std::strlen(trimmed);
            if (len > 2U && trimmed[len - 1U] == ']') {
              trimmed[len - 1U] = '\0';
              char* command = trimmed + 1;
              while (*command != '\0' && std::isspace(static_cast<unsigned char>(*command)) != 0) {
                ++command;
              }
              char* arg = command;
              while (*arg != '\0' && std::isspace(static_cast<unsigned char>(*arg)) == 0) {
                *arg = static_cast<char>(std::toupper(static_cast<unsigned char>(*arg)));
                ++arg;
              }
              while (*arg != '\0' && std::isspace(static_cast<unsigned char>(*arg)) != 0) {
                *arg = '\0';
                ++arg;
              }
              for (char* p = arg; *p != '\0'; ++p) {
                *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
              }
              if (std::strcmp(command, "SPEED") == 0) {
                const unsigned long speed = std::strtoul(arg, nullptr, 10);
                win_etape_credits_scroll_px_per_sec_ = static_cast<uint16_t>(std::min<unsigned long>(72UL, std::max<unsigned long>(6UL, speed)));
                continue;
              }
              if (std::strcmp(command, "ALIGN") == 0) {
                if (std::strcmp(arg, "LEFT") == 0) {
                  current_align_tag = 1U;
                } else if (std::strcmp(arg, "RIGHT") == 0) {
                  current_align_tag = 2U;
                } else {
                  current_align_tag = 0U;
                }
                continue;
              }
              if (std::strcmp(command, "SIZE") == 0) {
                if (std::strcmp(arg, "BIG") == 0) {
                  current_size_tag = 1U;
                } else if (std::strcmp(arg, "TITLE") == 0) {
                  current_size_tag = 2U;
                } else if (std::strcmp(arg, "SMALL") == 0) {
                  current_size_tag = 3U;
                } else {
                  current_size_tag = 0U;
                }
                continue;
              }
              if (std::strcmp(command, "SPACE") == 0) {
                const unsigned long blanks = std::strtoul(arg, nullptr, 10);
                const uint8_t blank_count = static_cast<uint8_t>(std::min<unsigned long>(6UL, std::max<unsigned long>(1UL, blanks)));
                for (uint8_t idx = 0U; idx < blank_count; ++idx) {
                  append_credit_line(" ", 0U, true);
                }
                continue;
              }
              if (std::strcmp(command, "PAUSE") == 0) {
                const unsigned long pause_ms = std::strtoul(arg, nullptr, 10);
                append_credit_line(" ", static_cast<uint16_t>(std::min<unsigned long>(12000UL, pause_ms)), true);
                continue;
              }
              if (std::strcmp(command, "END") == 0) {
                stop_from_directive = true;
                break;
              }
            }
          }
          append_credit_line(trimmed, 0U, false);
        }
        file.close();
        if (stop_from_directive) {
          break;
        }
        if (win_etape_credits_count_ > 0U) {
          break;
        }
      }
      if (win_etape_credits_count_ == 0U) {
        static constexpr const char* kCreditsFallback[] = {
            "CODE + INTEGRATION",
            "TEAM ZACUS",
            " ",
            "GRAPHICS + FX",
            "FREENOVE UI CREW",
            " ",
            "HOT-LINE AUDIO",
            "RTC A252 CREW",
            " ",
            "SPECIAL THANKS",
            "BRIGADE Z",
        };
        for (size_t i = 0U; i < (sizeof(kCreditsFallback) / sizeof(kCreditsFallback[0])) &&
                            win_etape_credits_count_ < kWinEtapeCreditsMaxLines;
             ++i) {
          copyTextSafe(win_etape_credits_lines_[win_etape_credits_count_],
                       kWinEtapeCreditsMaxLineChars,
                       kCreditsFallback[i]);
          win_etape_credits_size_[win_etape_credits_count_] = 0U;
          win_etape_credits_align_[win_etape_credits_count_] = 0U;
          win_etape_credits_pause_ms_[win_etape_credits_count_] = 0U;
          ++win_etape_credits_count_;
        }
      }
    }

    drivers::display::OverlayTextCommand header = {};
    header.text = "CREDITS";
    header.font_face = drivers::display::OverlayFontFace::kIbmBold24;
    header.size = 1U;
    header.color565 = symbol_color;
    const int16_t header_w = display.measureOverlayText(header.text, header.font_face, header.size);
    header.x = static_cast<int16_t>((width - header_w) / 2);
    header.y = 6;
    if (display.drawOverlayText(header)) {
      text_draw_ok = true;
    }

    if (win_etape_credits_count_ > 0U) {
      const uint32_t credits_elapsed_ms = scene_elapsed_ms - kWinEtape1CreditsStartMs;
      int32_t line_offsets[kWinEtapeCreditsMaxLines] = {0};
      int32_t total_height = 0;
      for (uint8_t idx = 0U; idx < win_etape_credits_count_; ++idx) {
        line_offsets[idx] = total_height;
        int16_t line_height = 16;
        int16_t line_gap = 6;
        if (win_etape_credits_size_[idx] == 1U) {
          line_height = 20;
          line_gap = 8;
        } else if (win_etape_credits_size_[idx] == 2U) {
          line_height = 24;
          line_gap = 10;
        } else if (win_etape_credits_size_[idx] == 3U) {
          line_height = 12;
          line_gap = 4;
        }
        total_height += static_cast<int32_t>(line_height + line_gap);
        total_height += static_cast<int32_t>((static_cast<uint32_t>(win_etape_credits_pause_ms_[idx]) *
                                              static_cast<uint32_t>(win_etape_credits_scroll_px_per_sec_)) /
                                             1000U);
      }
      int32_t loop_span = total_height + static_cast<int32_t>(height) + 28;
      if (loop_span < 1) {
        loop_span = 1;
      }
      const int32_t scroll_px =
          static_cast<int32_t>((static_cast<uint64_t>(credits_elapsed_ms) * win_etape_credits_scroll_px_per_sec_) / 1000ULL);
      const int32_t offset = scroll_px % loop_span;
      const int32_t base_y = static_cast<int32_t>(height + 14) - offset;
      for (uint8_t line_index = 0U; line_index < win_etape_credits_count_; ++line_index) {
        const char* line = win_etape_credits_lines_[line_index];
        if (line == nullptr) {
          continue;
        }
        int16_t line_height = 16;
        drivers::display::OverlayFontFace line_font = drivers::display::OverlayFontFace::kIbmBold16;
        if (win_etape_credits_size_[line_index] == 1U) {
          line_height = 20;
          line_font = drivers::display::OverlayFontFace::kIbmBold20;
        } else if (win_etape_credits_size_[line_index] == 2U) {
          line_height = 24;
          line_font = drivers::display::OverlayFontFace::kIbmBold24;
        } else if (win_etape_credits_size_[line_index] == 3U) {
          line_height = 12;
          line_font = drivers::display::OverlayFontFace::kIbmBold12;
        }
        const int32_t y32 = base_y + line_offsets[line_index];
        if (y32 < -line_height || y32 > (height + 6)) {
          continue;
        }
        if (line[0] == '\0' || (line[0] == ' ' && line[1] == '\0')) {
          continue;
        }
        drivers::display::OverlayTextCommand line_cmd = {};
        line_cmd.text = line;
        line_cmd.font_face = line_font;
        line_cmd.size = 1U;
        line_cmd.color565 = (win_etape_credits_size_[line_index] >= 2U)
                                ? symbol_color
                                : (((line_index & 0x01U) == 0U) ? title_color : subtitle_color);
        const int16_t text_w = display.measureOverlayText(line_cmd.text, line_cmd.font_face, line_cmd.size);
        if (win_etape_credits_align_[line_index] == 1U) {
          line_cmd.x = 8;
        } else if (win_etape_credits_align_[line_index] == 2U) {
          line_cmd.x = static_cast<int16_t>(width - text_w - 8);
        } else {
          line_cmd.x = static_cast<int16_t>((width - text_w) / 2);
        }
        line_cmd.y = static_cast<int16_t>(y32);
        if (display.drawOverlayText(line_cmd)) {
          text_draw_ok = true;
        }
      }
    }
  }

  if (!custom_win_etape1_credits && scene_status_.show_symbol) {
    drawLine(scene_status_.symbol, overlay_symbol_align_, 0U, symbol_font, symbol_size, symbol_color, 0x1000U);
  }
  if (!custom_win_etape1_credits && scene_status_.show_title) {
    drawLine(scene_status_.title, overlay_title_align_, 1U, title_font, title_size, title_color, 0x2000U);
  }
  if (!custom_win_etape1_credits && scene_status_.show_subtitle) {
    drawLine(scene_status_.subtitle, overlay_subtitle_align_, 2U, subtitle_font, subtitle_size, subtitle_color, 0x3000U);
  }

  if (text_attempted) {
    if (text_draw_ok) {
      ++overlay_draw_ok_count_;
    } else {
      ++overlay_draw_fail_count_;
    }
  }
  display.endWrite();
}

void UiManager::renderLgfxLaDetectorOverlay(uint32_t now_ms) {
  if (!scene_status_.valid) {
    return;
  }
  if (!la_detection_scene_ || !scene_use_lgfx_text_overlay_) {
    return;
  }
  if (std::strcmp(scene_status_.scene_id, "SCENE_LA_DETECTOR") != 0) {
    return;
  }

  const HardwareManager::Snapshot* active_snapshot = waveform_snapshot_ref_;
  if (active_snapshot == nullptr && waveform_snapshot_valid_) {
    active_snapshot = &waveform_snapshot_;
  }

  drivers::display::DisplayHal& display = drivers::display::displayHal();
  bool write_ready = false;
  for (uint8_t attempt = 0U; attempt < 3U; ++attempt) {
    if (display.startWrite()) {
      write_ready = true;
      break;
    }
    display.waitDmaComplete(2200U);
    delayMicroseconds(static_cast<unsigned int>(100U * (attempt + 1U)));
  }
  if (!write_ready) {
    ++overlay_startwrite_fail_count_;
    ++overlay_draw_fail_count_;
    return;
  }

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  if (width <= 0 || height <= 0) {
    display.endWrite();
    return;
  }

  auto clamp_i16 = [](int16_t value, int16_t lo, int16_t hi) -> int16_t {
    if (value < lo) {
      return lo;
    }
    if (value > hi) {
      return hi;
    }
    return value;
  };
  auto color565 = [&display](uint32_t rgb) -> uint16_t {
    return display.color565(static_cast<uint8_t>((rgb >> 16U) & 0xFFU),
                            static_cast<uint8_t>((rgb >> 8U) & 0xFFU),
                            static_cast<uint8_t>(rgb & 0xFFU));
  };

  const uint16_t osc_main = color565(0x44FF6EUL);
  const uint16_t osc_head = color565(0xA8FFC0UL);
  const uint16_t osc_ring = color565(0x1E5138UL);
  const uint16_t marker = color565(0x6CC9FFUL);
  bool text_attempted = false;
  bool text_draw_ok = false;

  const int16_t jitter_x = 0;
  const int16_t jitter_y = 0;

  constexpr float kTau = 6.28318530718f;
  constexpr float kHalfPi = 1.57079632679f;

  const uint8_t stability_pct = (la_detection_stability_pct_ > 100U) ? 100U : la_detection_stability_pct_;
  const uint32_t gate_elapsed_ms =
      (la_detection_gate_elapsed_ms_ > la_detection_gate_timeout_ms_) ? la_detection_gate_timeout_ms_
                                                                       : la_detection_gate_elapsed_ms_;
  float gate_remain = 1.0f;
  if (la_detection_gate_timeout_ms_ > 0U) {
    gate_remain = 1.0f - (static_cast<float>(gate_elapsed_ms) / static_cast<float>(la_detection_gate_timeout_ms_));
  }
  if (gate_remain < 0.0f) {
    gate_remain = 0.0f;
  } else if (gate_remain > 1.0f) {
    gate_remain = 1.0f;
  }
  // Hourglass visual timeout is intentionally faster than real gate timeout (80% duration).
  float hourglass_gate_remain = gate_remain;
  if (la_detection_gate_timeout_ms_ > 0U) {
    constexpr float kHourglassTimeoutScale = 0.80f;
    const float visual_timeout_ms =
        std::max<float>(1.0f, static_cast<float>(la_detection_gate_timeout_ms_) * kHourglassTimeoutScale);
    hourglass_gate_remain = 1.0f - (static_cast<float>(gate_elapsed_ms) / visual_timeout_ms);
    if (hourglass_gate_remain < 0.0f) {
      hourglass_gate_remain = 0.0f;
    } else if (hourglass_gate_remain > 1.0f) {
      hourglass_gate_remain = 1.0f;
    }
  }
  const uint8_t mic_level_pct =
      (active_snapshot != nullptr) ? std::min<uint8_t>(active_snapshot->mic_level_percent, 100U) : 0U;
  const float mic_level = static_cast<float>(mic_level_pct) / 100.0f;
  uint32_t dt_ms = 16U;
  if (la_bg_last_ms_ != 0U && now_ms >= la_bg_last_ms_) {
    dt_ms = now_ms - la_bg_last_ms_;
    if (dt_ms > 1000U) {
      dt_ms = 1000U;
    }
  }
  la_bg_last_ms_ = now_ms;
  float mic_target = (active_snapshot != nullptr) ? mic_level : 0.15f;
  if (la_bg_sync_ == LaBackgroundSync::kFixed) {
    mic_target = 0.15f;
  }
  float alpha = static_cast<float>(dt_ms) / (180.0f + static_cast<float>(dt_ms));
  if (alpha < 0.02f) {
    alpha = 0.02f;
  } else if (alpha > 0.35f) {
    alpha = 0.35f;
  }
  la_bg_mic_lpf_ += alpha * (mic_target - la_bg_mic_lpf_);
  if (la_bg_mic_lpf_ < 0.0f) {
    la_bg_mic_lpf_ = 0.0f;
  } else if (la_bg_mic_lpf_ > 1.0f) {
    la_bg_mic_lpf_ = 1.0f;
  }
  float mic_drive = 0.15f;
  if (la_bg_sync_ == LaBackgroundSync::kMicDirect) {
    mic_drive = mic_target;
  } else if (la_bg_sync_ == LaBackgroundSync::kMicSmoothed) {
    mic_drive = la_bg_mic_lpf_;
  }
  if (mic_drive < 0.0f) {
    mic_drive = 0.0f;
  } else if (mic_drive > 1.0f) {
    mic_drive = 1.0f;
  }
  const float bg_intensity = static_cast<float>(la_bg_intensity_pct_) / 100.0f;
  auto scale_rgb = [](uint32_t rgb, float scale) -> uint32_t {
    if (scale < 0.0f) {
      scale = 0.0f;
    } else if (scale > 1.0f) {
      scale = 1.0f;
    }
    const uint8_t r = static_cast<uint8_t>(std::min<int>(255, static_cast<int>(((rgb >> 16U) & 0xFFU) * scale)));
    const uint8_t g = static_cast<uint8_t>(std::min<int>(255, static_cast<int>(((rgb >> 8U) & 0xFFU) * scale)));
    const uint8_t b = static_cast<uint8_t>(std::min<int>(255, static_cast<int>((rgb & 0xFFU) * scale)));
    return (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) | static_cast<uint32_t>(b);
  };
  const float palette_scale = 0.45f + (0.55f * bg_intensity);
  const uint16_t la_bg = color565(scale_rgb(0x060F18UL, palette_scale));
  const uint16_t la_bg_mid = color565(scale_rgb(0x0A1A26UL, palette_scale));
  const bool uses_fullscreen_sprite_bg = (la_bg_preset_ == LaBackgroundPreset::kHourglassDemosceneUltra);
  if (!uses_fullscreen_sprite_bg) {
    display.fillOverlayRect(0, 0, width, height, la_bg);
    display.fillOverlayRect(0,
                            static_cast<int16_t>(height / 4),
                            width,
                            static_cast<int16_t>(height / 2),
                            la_bg_mid);
  }

  if (la_bg_preset_ == LaBackgroundPreset::kHourglassDemosceneUltra) {
    size_t bg_pixels = 0U;
    const bool bg_area_ok =
        runtime::memory::safeMulSize(static_cast<size_t>(width), static_cast<size_t>(height), &bg_pixels);
    if (bg_area_ok && bg_pixels > 0U) {
      const bool needs_new_buffer = (la_bg_sprite_buf_ == nullptr) || (la_bg_sprite_pixels_ < bg_pixels) ||
                                    (la_bg_sprite_w_ != width) || (la_bg_sprite_h_ != height);
      if (needs_new_buffer) {
        if (la_bg_sprite_buf_ != nullptr) {
          runtime::memory::CapsAllocator::release(la_bg_sprite_buf_);
          la_bg_sprite_buf_ = nullptr;
          la_bg_sprite_pixels_ = 0U;
          la_bg_sprite_w_ = 0;
          la_bg_sprite_h_ = 0;
        }
        size_t bg_bytes = 0U;
        if (runtime::memory::safeMulSize(bg_pixels, sizeof(uint16_t), &bg_bytes)) {
          la_bg_sprite_buf_ =
              static_cast<uint16_t*>(runtime::memory::CapsAllocator::allocPsram(bg_bytes, "la_hg_sprite"));
          if (la_bg_sprite_buf_ != nullptr) {
            la_bg_sprite_pixels_ = bg_pixels;
            la_bg_sprite_w_ = width;
            la_bg_sprite_h_ = height;
          }
        }
      }
    }

    if (la_bg_sprite_buf_ != nullptr && la_bg_sprite_w_ == width && la_bg_sprite_h_ == height) {
      auto hg_xorshift = [this]() -> uint32_t {
        uint32_t x = la_hg_rng_;
        x ^= (x << 13U);
        x ^= (x >> 17U);
        x ^= (x << 5U);
        la_hg_rng_ = x;
        return x;
      };
      auto clamp_i32 = [](int32_t value, int32_t lo, int32_t hi) -> int32_t {
        if (value < lo) {
          return lo;
        }
        if (value > hi) {
          return hi;
        }
        return value;
      };
      auto shade565 = [&clamp_i32](uint16_t color, int delta) -> uint16_t {
        int r = (color >> 11U) & 0x1FU;
        int g = (color >> 5U) & 0x3FU;
        int b = color & 0x1FU;
        r = clamp_i32(r + delta, 0, 31);
        g = clamp_i32(g + (delta * 2), 0, 63);
        b = clamp_i32(b + delta, 0, 31);
        return static_cast<uint16_t>((r << 11U) | (g << 5U) | b);
      };
      auto darken565 = [](uint16_t color, uint8_t amount) -> uint16_t {
        const uint16_t r = (color >> 11U) & 0x1FU;
        const uint16_t g = (color >> 5U) & 0x3FU;
        const uint16_t b = color & 0x1FU;
        const uint16_t scale = static_cast<uint16_t>(255U - amount);
        const uint16_t rr = static_cast<uint16_t>((static_cast<uint32_t>(r) * scale) / 255U);
        const uint16_t gg = static_cast<uint16_t>((static_cast<uint32_t>(g) * scale) / 255U);
        const uint16_t bb = static_cast<uint16_t>((static_cast<uint32_t>(b) * scale) / 255U);
        return static_cast<uint16_t>((rr << 11U) | (gg << 5U) | bb);
      };
      auto lerpChannel = [](uint8_t a, uint8_t b, uint16_t t, uint16_t den) -> uint8_t {
        const uint32_t aa = static_cast<uint32_t>(a) * static_cast<uint32_t>(den - t);
        const uint32_t bb = static_cast<uint32_t>(b) * static_cast<uint32_t>(t);
        return static_cast<uint8_t>((aa + bb) / den);
      };

      const uint16_t grid_w =
          static_cast<uint16_t>(std::min<int>(kLaHourglassGridWMax, std::max<int>(56, width / 3)));
      const uint16_t grid_h =
          static_cast<uint16_t>(std::min<int>(kLaHourglassGridHMax, std::max<int>(42, height / 3)));
      if (la_hg_grid_w_ != grid_w || la_hg_grid_h_ != grid_h) {
        la_hg_grid_w_ = grid_w;
        la_hg_grid_h_ = grid_h;
        la_hg_ready_ = false;
      }
      const uint16_t active_grid_w = (la_hg_grid_w_ == 0U) ? 56U : la_hg_grid_w_;
      const uint16_t active_grid_h = (la_hg_grid_h_ == 0U) ? 42U : la_hg_grid_h_;
      auto hg_idx = [&](uint16_t x, uint16_t y) -> size_t {
        return static_cast<size_t>(y) * static_cast<size_t>(active_grid_w) + static_cast<size_t>(x);
      };
      auto is_source_half = [&](int y) -> bool {
        const int mid = static_cast<int>(active_grid_h / 2U);
        return (la_hg_orient_ == 0U) ? (y < mid) : (y >= mid);
      };
      auto seed_source = [&]() {
        std::memset(la_hg_sand_, 0, sizeof(la_hg_sand_));
        const int mid = static_cast<int>(active_grid_h / 2U);
        for (uint16_t y = 0U; y < active_grid_h; ++y) {
          if (!is_source_half(static_cast<int>(y))) {
            continue;
          }
          if ((la_hg_orient_ == 0U && static_cast<int>(y) > mid - 4) ||
              (la_hg_orient_ != 0U && static_cast<int>(y) < mid + 3)) {
            continue;
          }
          for (uint16_t x = 0U; x < active_grid_w; ++x) {
            const size_t i = hg_idx(x, y);
            if (la_hg_mask_[i] == 0U || la_hg_outline_[i] != 0U) {
              continue;
            }
            const uint32_t r = hg_xorshift();
            uint8_t density = 6U;
            if ((la_hg_orient_ == 0U && y < 6U) || (la_hg_orient_ != 0U && y > active_grid_h - 7U)) {
              density = 7U;
            }
            if ((r & 0x7U) < density) {
              la_hg_sand_[i] = 1U;
            }
          }
        }
      };

      if (!la_hg_ready_) {
        std::memset(la_hg_mask_, 0, sizeof(la_hg_mask_));
        std::memset(la_hg_outline_, 0, sizeof(la_hg_outline_));
        std::memset(la_hg_depth_, 0, sizeof(la_hg_depth_));
        std::memset(la_hg_halfw_, 0, sizeof(la_hg_halfw_));
        const int cx = static_cast<int>(active_grid_w / 2U);
        const float mid = (static_cast<float>(active_grid_h) - 1.0f) * 0.5f;
        const int max_half = std::max<int>(6, static_cast<int>(active_grid_w / 2U) - 2);
        const int min_half = 4;
        const int throat_y0 = static_cast<int>(active_grid_h / 2U) - 1;
        const int throat_y1 = static_cast<int>(active_grid_h / 2U);
        const int throat_half = 2;
        for (uint16_t y = 0U; y < active_grid_h; ++y) {
          float d = std::fabs(static_cast<float>(y) - mid) / ((mid > 0.0f) ? mid : 1.0f);
          const float w = static_cast<float>(min_half) +
                          (static_cast<float>(max_half - min_half) * std::pow(d, 1.35f));
          int half = static_cast<int>(w + 0.5f);
          if (static_cast<int>(y) == throat_y0 || static_cast<int>(y) == throat_y1) {
            half = throat_half;
          }
          half = static_cast<int>(clamp_i32(half, 1, max_half));
          la_hg_halfw_[y] = static_cast<uint8_t>(half);
          const int x0 = cx - half;
          const int x1 = cx + half;
          for (int x = x0; x <= x1; ++x) {
            if (x >= 0 && x < static_cast<int>(active_grid_w)) {
              la_hg_mask_[hg_idx(static_cast<uint16_t>(x), y)] = 1U;
            }
          }
        }
        for (uint16_t y = 1U; y + 1U < active_grid_h; ++y) {
          for (uint16_t x = 1U; x + 1U < active_grid_w; ++x) {
            const size_t i = hg_idx(x, y);
            if (la_hg_mask_[i] == 0U) {
              continue;
            }
            const bool solid = (la_hg_mask_[hg_idx(static_cast<uint16_t>(x - 1U), y)] != 0U) &&
                               (la_hg_mask_[hg_idx(static_cast<uint16_t>(x + 1U), y)] != 0U) &&
                               (la_hg_mask_[hg_idx(x, static_cast<uint16_t>(y - 1U))] != 0U) &&
                               (la_hg_mask_[hg_idx(x, static_cast<uint16_t>(y + 1U))] != 0U);
            if (!solid) {
              la_hg_outline_[i] = 1U;
            }
          }
        }
        for (uint16_t y = 0U; y < active_grid_h; ++y) {
          const int half = static_cast<int>(la_hg_halfw_[y]);
          for (uint16_t x = 0U; x < active_grid_w; ++x) {
            const size_t i = hg_idx(x, y);
            if (la_hg_mask_[i] == 0U) {
              continue;
            }
            const int ax = static_cast<int>(x) - static_cast<int>(active_grid_w / 2U);
            float rad = (half > 0) ? (std::fabs(static_cast<float>(ax)) / static_cast<float>(half)) : 1.0f;
            if (rad > 1.0f) {
              rad = 1.0f;
            }
            const float depth = 1.0f - (rad * rad);
            int s = static_cast<int>(depth * 18.0f - 9.0f);
            s += (ax < 0) ? 1 : 0;
            s += (y < active_grid_h / 2U) ? 1 : 0;
            if (rad > 0.86f) {
              s += 2;
            }
            la_hg_depth_[i] = static_cast<int8_t>(clamp_i32(s, -16, 16));
          }
        }
        la_hg_theta_ = 0.0f;
        la_hg_omega_ = 0.0008f;
        la_hg_timeout_latched_ = false;
        la_hg_prev_gate_elapsed_ms_ = gate_elapsed_ms;
        la_hg_prev_gate_valid_ = true;
        seed_source();
        la_hg_ready_ = true;
      }

      auto count_source = [&]() -> int {
        int count = 0;
        for (uint16_t y = 0U; y < active_grid_h; ++y) {
          const bool source_half = is_source_half(static_cast<int>(y));
          for (uint16_t x = 0U; x < active_grid_w; ++x) {
            const size_t i = hg_idx(x, y);
            if (source_half && la_hg_sand_[i] != 0U) {
              ++count;
            }
          }
        }
        return count;
      };
      auto count_total = [&]() -> int {
        int count = 0;
        const size_t limit = static_cast<size_t>(active_grid_w) * static_cast<size_t>(active_grid_h);
        for (size_t i = 0U; i < limit; ++i) {
          count += (la_hg_sand_[i] != 0U) ? 1 : 0;
        }
        return count;
      };
      auto physics_step = [&]() {
        const int gd = (la_hg_orient_ == 0U) ? 1 : -1;
        int bias = 0;
        if (la_hg_theta_ > 0.02f) {
          bias = 1;
        } else if (la_hg_theta_ < -0.02f) {
          bias = -1;
        }
        if (la_hg_orient_ != 0U) {
          bias = -bias;
        }
        const int y_start = (gd > 0) ? static_cast<int>(active_grid_h - 2U) : 1;
        const int y_end = (gd > 0) ? -1 : static_cast<int>(active_grid_h);
        const int y_step = (gd > 0) ? -1 : 1;
        for (int y = y_start; y != y_end; y += y_step) {
          for (uint16_t x = 0U; x < active_grid_w; ++x) {
            const size_t i = hg_idx(x, static_cast<uint16_t>(y));
            if (la_hg_sand_[i] == 0U) {
              continue;
            }
            const int yn = y + gd;
            if (yn < 0 || yn >= static_cast<int>(active_grid_h)) {
              continue;
            }
            const size_t id = hg_idx(x, static_cast<uint16_t>(yn));
            if (la_hg_mask_[id] != 0U && la_hg_sand_[id] == 0U) {
              la_hg_sand_[i] = 0U;
              la_hg_sand_[id] = 1U;
              continue;
            }
            const int x1 = static_cast<int>(x) + bias;
            const int x2 = static_cast<int>(x) - bias;
            if (bias != 0 && x1 >= 0 && x1 < static_cast<int>(active_grid_w)) {
              const size_t id1 = hg_idx(static_cast<uint16_t>(x1), static_cast<uint16_t>(yn));
              if (la_hg_mask_[id1] != 0U && la_hg_sand_[id1] == 0U) {
                la_hg_sand_[i] = 0U;
                la_hg_sand_[id1] = 1U;
                continue;
              }
            }
            if (x2 >= 0 && x2 < static_cast<int>(active_grid_w)) {
              const size_t id2 = hg_idx(static_cast<uint16_t>(x2), static_cast<uint16_t>(yn));
              if (la_hg_mask_[id2] != 0U && la_hg_sand_[id2] == 0U) {
                la_hg_sand_[i] = 0U;
                la_hg_sand_[id2] = 1U;
                continue;
              }
            }
          }
        }
      };

      bool timeout_reset_flip = false;
      if (la_hg_prev_gate_valid_) {
        const uint32_t min_reset_progress_ms = std::max<uint32_t>(900U, la_detection_gate_timeout_ms_ / 10U);
        const bool gate_reset_to_zero = (gate_elapsed_ms <= 80U) &&
                                        (la_hg_prev_gate_elapsed_ms_ >= min_reset_progress_ms) &&
                                        (la_hg_prev_gate_elapsed_ms_ > (gate_elapsed_ms + 400U));
        if (gate_reset_to_zero && !la_hg_flipping_) {
          timeout_reset_flip = true;
          la_hg_timeout_latched_ = false;
        }
      } else {
        la_hg_prev_gate_valid_ = true;
      }
      la_hg_prev_gate_elapsed_ms_ = gate_elapsed_ms;

      if (hourglass_gate_remain > 0.02f) {
        la_hg_timeout_latched_ = false;
      }
      const bool hourglass_timeout_reached = (hourglass_gate_remain <= 0.001f);
      if (timeout_reset_flip) {
        la_hg_flipping_ = true;
        la_hg_flip_started_ms_ = now_ms;
      }
      if (la_hg_flip_on_timeout_ && !la_hg_flipping_ && !la_hg_timeout_latched_ && hourglass_timeout_reached) {
        la_hg_flipping_ = true;
        la_hg_flip_started_ms_ = now_ms;
        la_hg_timeout_latched_ = true;
      }
      const float dt_s = static_cast<float>(dt_ms) * 0.001f;
      const float kHourglassFlipDurationMs = static_cast<float>(std::max<uint32_t>(500U, la_hg_flip_duration_ms_));
      constexpr float kSwingK = 0.080f;       // slower baseline swing
      constexpr float kSwingDamp = 0.460f;    // much stronger damping
      constexpr float kSwingFlipDamp = 0.620f;
      const float max_theta = 0.06109f;  // ~3.5deg max for softer motion
      float flip_rad = 0.0f;
      if (la_hg_flipping_) {
        const float p = static_cast<float>(now_ms - la_hg_flip_started_ms_) / kHourglassFlipDurationMs;
        float e = p;
        if (e < 0.0f) {
          e = 0.0f;
        } else if (e > 1.0f) {
          e = 1.0f;
        }
        e = e * e * (3.0f - 2.0f * e);
        flip_rad = e * 3.14159265359f;
        la_hg_omega_ += ((-kSwingK * la_hg_theta_) - (kSwingFlipDamp * la_hg_omega_)) * dt_s;
        la_hg_theta_ += la_hg_omega_ * dt_s;
        if (p >= 1.0f) {
          la_hg_flipping_ = false;
          la_hg_orient_ ^= 1U;
          la_hg_omega_ += ((hg_xorshift() & 1U) != 0U) ? 0.0015f : -0.0015f;
          seed_source();
        }
      } else {
        const bool freeze_sand = (!la_hg_flip_on_timeout_ && hourglass_timeout_reached);
        const int total_grains = std::max<int>(1, count_total());
        const int source_now = count_source();
        const int source_target =
            static_cast<int>(std::round(static_cast<float>(total_grains) * hourglass_gate_remain));
        if (!freeze_sand) {
          int need_move = source_now - source_target;
          if (need_move > 0) {
            need_move = std::min<int>(need_move, 10);
            const int cx = static_cast<int>(active_grid_w / 2U);
            const int mid = static_cast<int>(active_grid_h / 2U);
            const int gd = (la_hg_orient_ == 0U) ? 1 : -1;
            const int y_from = (gd > 0) ? (mid - 2) : (mid + 1);
            const int y_to = (gd > 0) ? (mid + 1) : (mid - 2);
            int bias = 0;
            if (la_hg_theta_ > 0.02f) {
              bias = 1;
            } else if (la_hg_theta_ < -0.02f) {
              bias = -1;
            }
            if (la_hg_orient_ != 0U) {
              bias = -bias;
            }
            for (int moved = 0; moved < need_move; ++moved) {
              bool done = false;
              for (int radius = 0; radius <= 6 && !done; ++radius) {
                for (int dx = -radius; dx <= radius && !done; ++dx) {
                  const int x = cx + dx;
                  if (x < 0 || x >= static_cast<int>(active_grid_w)) {
                    continue;
                  }
                  const size_t from = hg_idx(static_cast<uint16_t>(x), static_cast<uint16_t>(y_from));
                  if (la_hg_mask_[from] == 0U || la_hg_sand_[from] == 0U) {
                    continue;
                  }
                  for (int ddx = -radius; ddx <= radius; ++ddx) {
                    const int xb = x + ddx + bias;
                    if (xb < 0 || xb >= static_cast<int>(active_grid_w)) {
                      continue;
                    }
                    const size_t to = hg_idx(static_cast<uint16_t>(xb), static_cast<uint16_t>(y_to));
                    if (la_hg_mask_[to] == 0U || la_hg_sand_[to] != 0U) {
                      continue;
                    }
                    la_hg_sand_[from] = 0U;
                    la_hg_sand_[to] = 1U;
                    done = true;
                    break;
                  }
                }
              }
              if (!done) {
                break;
              }
            }
          }
        }
        la_hg_omega_ += (((hg_xorshift() & 1023U) - 512U) * 0.000000010f);
        la_hg_omega_ += ((-kSwingK * la_hg_theta_) - (kSwingDamp * la_hg_omega_)) * dt_s;
        la_hg_theta_ += la_hg_omega_ * dt_s;
        if (la_hg_theta_ > max_theta) {
          la_hg_theta_ = max_theta;
          if (la_hg_omega_ > 0.0f) {
            la_hg_omega_ *= -0.10f;
          }
        } else if (la_hg_theta_ < -max_theta) {
          la_hg_theta_ = -max_theta;
          if (la_hg_omega_ < 0.0f) {
            la_hg_omega_ *= -0.10f;
          }
        }
        if (!freeze_sand) {
          for (uint8_t step = 0U; step < 1U; ++step) {
            physics_step();
          }
        }
      }

      const uint8_t top_r = static_cast<uint8_t>(2 + (la_bg_intensity_pct_ / 8U));
      const uint8_t top_g = static_cast<uint8_t>(7 + (la_bg_intensity_pct_ / 6U));
      const uint8_t top_b = static_cast<uint8_t>(11 + (la_bg_intensity_pct_ / 5U));
      const uint8_t bot_r = static_cast<uint8_t>(9 + (la_bg_intensity_pct_ / 5U));
      const uint8_t bot_g = static_cast<uint8_t>(20 + (la_bg_intensity_pct_ / 4U));
      const uint8_t bot_b = static_cast<uint8_t>(26 + (la_bg_intensity_pct_ / 4U));
      const uint16_t den = static_cast<uint16_t>(std::max<int16_t>(1, height - 1));
      for (int16_t y = 0; y < height; ++y) {
        const uint16_t t = static_cast<uint16_t>(y);
        const uint8_t rr = lerpChannel(top_r, bot_r, t, den);
        const uint8_t gg = lerpChannel(top_g, bot_g, t, den);
        const uint8_t bb = lerpChannel(top_b, bot_b, t, den);
        const uint16_t row_color = display.color565(rr, gg, bb);
        uint16_t* row = la_bg_sprite_buf_ + (static_cast<size_t>(y) * static_cast<size_t>(width));
        runtime::simd::simd_rgb565_fill(row, row_color, static_cast<size_t>(width));
      }

      const uint16_t glass_base = color565(0x4A9BDAUL);
      const uint16_t glass_edge = color565(0x9BE3FFUL);
      const uint16_t sand_base = color565(0xF2D463UL);
      const uint16_t sand_glow = color565(0xFFF2A9UL);
      const float target_w = (la_hg_target_width_px_ > 0U)
                                 ? static_cast<float>(la_hg_target_width_px_)
                                 : (static_cast<float>(width) * 0.2f);
      const float target_h = (la_hg_target_height_px_ > 0U)
                                 ? static_cast<float>(la_hg_target_height_px_)
                                 : (static_cast<float>(height) * (0.9f / 1.33f) * 0.8f);
      float center_x = (static_cast<float>(width) * 0.7f) + static_cast<float>(la_hg_x_offset_px_);
      const float center_y = static_cast<float>(height) * 0.51f;
      const float min_center_x = target_w * 0.58f;
      const float max_center_x = static_cast<float>(width) - (target_w * 0.58f);
      if (center_x < min_center_x) {
        center_x = min_center_x;
      } else if (center_x > max_center_x) {
        center_x = max_center_x;
      }
      const float scale_x = target_w / static_cast<float>(active_grid_w);
      const float scale_y = target_h / static_cast<float>(active_grid_h);
      const float base_angle = (la_hg_orient_ == 0U) ? 0.0f : 3.14159265359f;
      const float angle = base_angle + la_hg_theta_ + flip_rad;
      const float cs = std::cos(angle);
      const float sn = std::sin(angle);
      const int block = std::max<int>(1, static_cast<int>(std::round(std::min(scale_x, scale_y) * 0.85f)));
      const uint8_t glint_phase = static_cast<uint8_t>((now_ms / 1800U) & 0x7FU);
      for (uint16_t y = 0U; y < active_grid_h; ++y) {
        for (uint16_t x = 0U; x < active_grid_w; ++x) {
          const size_t i = hg_idx(x, y);
          if (la_hg_mask_[i] == 0U) {
            continue;
          }
          uint16_t color = shade565(glass_base, static_cast<int>(la_hg_depth_[i]) - 6);
          if (la_hg_sand_[i] != 0U) {
            color = shade565(sand_base, static_cast<int>(la_hg_depth_[i]) / 3);
          }
          if (la_hg_outline_[i] != 0U) {
            color = shade565(glass_edge, static_cast<int>(la_hg_depth_[i]) / 2 + 2);
          }
          if (la_hg_outline_[i] != 0U && ((((x * 3U) + (y * 5U) + glint_phase) & 127U) == 0U)) {
            color = shade565(sand_glow, 1);
          }
          const float lx = (static_cast<float>(x) - static_cast<float>(active_grid_w) * 0.5f) * scale_x;
          // Render each logical row with 3 sub-lines for denser, smoother hourglass lines.
          for (uint8_t sub_line = 0U; sub_line < 3U; ++sub_line) {
            const float y_sub = static_cast<float>(y) + (static_cast<float>(sub_line) / 3.0f);
            const float ly = (y_sub - static_cast<float>(active_grid_h) * 0.5f) * scale_y;
            const int16_t px = static_cast<int16_t>(std::round(center_x + (lx * cs) - (ly * sn)));
            const int16_t py = static_cast<int16_t>(std::round(center_y + (lx * sn) + (ly * cs)));
            for (int dy = 0; dy < block; ++dy) {
              const int16_t yy = static_cast<int16_t>(py + dy - block / 2);
              if (yy < 0 || yy >= height) {
                continue;
              }
              uint16_t* row = la_bg_sprite_buf_ + (static_cast<size_t>(yy) * static_cast<size_t>(width));
              for (int dx = 0; dx < block; ++dx) {
                const int16_t xx = static_cast<int16_t>(px + dx - block / 2);
                if (xx < 0 || xx >= width) {
                  continue;
                }
                row[xx] = color;
              }
            }
          }
        }
      }

      const int16_t top_guard = std::min<int16_t>(44, static_cast<int16_t>(height / 4));
      const int16_t bottom_guard_start = std::max<int16_t>(0, static_cast<int16_t>(height - 34));
      for (int16_t y = 0; y < top_guard; ++y) {
        uint16_t* row = la_bg_sprite_buf_ + (static_cast<size_t>(y) * static_cast<size_t>(width));
        for (int16_t x = 0; x < width; ++x) {
          row[x] = darken565(row[x], 86U);
        }
      }
      for (int16_t y = bottom_guard_start; y < height; ++y) {
        uint16_t* row = la_bg_sprite_buf_ + (static_cast<size_t>(y) * static_cast<size_t>(width));
        for (int16_t x = 0; x < width; ++x) {
          row[x] = darken565(row[x], 72U);
        }
      }

      const uint32_t pixel_count = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
      // Avoid full-frame DMA from PSRAM in loopTask: stream via CPU path for RTOS stability.
      display.setAddrWindow(0, 0, width, height);
      display.pushColors(la_bg_sprite_buf_, pixel_count, true);
    } else {
      // Fallback background if sprite allocation failed.
      display.fillOverlayRect(0, 0, width, height, la_bg);
      display.fillOverlayRect(0,
                              static_cast<int16_t>(height / 4),
                              width,
                              static_cast<int16_t>(height / 2),
                              la_bg_mid);
    }
  } else if (la_bg_preset_ == LaBackgroundPreset::kWirecubeRotozoomSubtle) {
    const uint16_t rz_dark = color565(scale_rgb(0x0B1824UL, palette_scale));
    const uint16_t rz_mid = color565(scale_rgb(0x102536UL, palette_scale));
    const uint16_t cube_dim = color565(scale_rgb(0x1A3245UL, palette_scale));
    const uint16_t cube_lit = color565(scale_rgb(0x26506AUL, palette_scale));
    const int16_t fx_cx = static_cast<int16_t>(width / 2);
    const int16_t fx_cy = static_cast<int16_t>((height / 2) + 8);
    const int16_t fx_top_guard = 44;
    const int16_t fx_bottom_guard = static_cast<int16_t>(height - 30);
    int16_t fx_extent = static_cast<int16_t>(std::min<int16_t>(width / 2 - 10, height / 2 - 18));
    if (fx_extent < 26) {
      fx_extent = 26;
    }
    if ((fx_cy - fx_extent) < fx_top_guard) {
      fx_extent = static_cast<int16_t>(fx_cy - fx_top_guard);
    }
    if ((fx_cy + fx_extent) > fx_bottom_guard) {
      fx_extent = static_cast<int16_t>(fx_bottom_guard - fx_cy);
    }
    if (fx_extent < 18) {
      fx_extent = 18;
    }
    auto draw_safe_line = [&](int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
      display.drawOverlayLine(clamp_i16(x0, 1, static_cast<int16_t>(width - 2)),
                              clamp_i16(y0, 1, static_cast<int16_t>(height - 2)),
                              clamp_i16(x1, 1, static_cast<int16_t>(width - 2)),
                              clamp_i16(y1, 1, static_cast<int16_t>(height - 2)),
                              color);
    };
    auto draw_roto_line = [&](float x0, float y0, float x1, float y1, float c, float s, float z, uint16_t color) {
      const float rx0 = ((x0 * c) - (y0 * s)) * z;
      const float ry0 = ((x0 * s) + (y0 * c)) * z;
      const float rx1 = ((x1 * c) - (y1 * s)) * z;
      const float ry1 = ((x1 * s) + (y1 * c)) * z;
      draw_safe_line(static_cast<int16_t>(fx_cx + rx0),
                     static_cast<int16_t>(fx_cy + ry0),
                     static_cast<int16_t>(fx_cx + rx1),
                     static_cast<int16_t>(fx_cy + ry1),
                     color);
    };

    const float t = static_cast<float>(now_ms) * 0.001f;
    const float rz_speed = 0.18f + (0.20f * mic_drive);
    const float rz_angle = t * rz_speed;
    const float rz_cos = std::cos(rz_angle);
    const float rz_sin = std::sin(rz_angle);
    const float rz_zoom = 1.0f + (0.05f + 0.07f * mic_drive) * std::sin((t * 0.92f) + (mic_drive * 2.0f));
    int16_t grid_step = static_cast<int16_t>(11 - static_cast<int16_t>(mic_drive * 4.0f));
    if (grid_step < 6) {
      grid_step = 6;
    }
    for (int16_t offset = static_cast<int16_t>(-fx_extent); offset <= fx_extent;
         offset = static_cast<int16_t>(offset + grid_step)) {
      const uint16_t color = (((offset / grid_step) & 0x01) == 0) ? rz_dark : rz_mid;
      draw_roto_line(static_cast<float>(offset),
                     static_cast<float>(-fx_extent),
                     static_cast<float>(offset),
                     static_cast<float>(fx_extent),
                     rz_cos,
                     rz_sin,
                     rz_zoom,
                     color);
      draw_roto_line(static_cast<float>(-fx_extent),
                     static_cast<float>(offset),
                     static_cast<float>(fx_extent),
                     static_cast<float>(offset),
                     rz_cos,
                     rz_sin,
                     rz_zoom,
                     color);
    }

    const float yaw = t * (0.40f + mic_drive * 0.24f);
    const float pitch = t * (0.30f + mic_drive * 0.16f);
    const float roll = 0.20f * std::sin((t * 0.70f) + (mic_drive * 1.20f));
    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);
    const float sp = std::sin(pitch);
    const float cp = std::cos(pitch);
    const float sr = std::sin(roll);
    const float cr = std::cos(roll);
    const float cube_radius = static_cast<float>(std::max<int16_t>(18, fx_extent - 8));
    const float cube_scale = cube_radius * (0.72f + mic_drive * 0.15f);
    constexpr float kCubeVerts[8][3] = {{-1.0f, -1.0f, -1.0f},
                                        {1.0f, -1.0f, -1.0f},
                                        {1.0f, 1.0f, -1.0f},
                                        {-1.0f, 1.0f, -1.0f},
                                        {-1.0f, -1.0f, 1.0f},
                                        {1.0f, -1.0f, 1.0f},
                                        {1.0f, 1.0f, 1.0f},
                                        {-1.0f, 1.0f, 1.0f}};
    constexpr uint8_t kCubeEdges[12][2] = {{0U, 1U},
                                           {1U, 2U},
                                           {2U, 3U},
                                           {3U, 0U},
                                           {4U, 5U},
                                           {5U, 6U},
                                           {6U, 7U},
                                           {7U, 4U},
                                           {0U, 4U},
                                           {1U, 5U},
                                           {2U, 6U},
                                           {3U, 7U}};
    int16_t px[8] = {0};
    int16_t py[8] = {0};
    float pz[8] = {0.0f};
    for (uint8_t i = 0U; i < 8U; ++i) {
      float x = kCubeVerts[i][0];
      float y = kCubeVerts[i][1];
      float z = kCubeVerts[i][2];
      const float x1 = (x * cy) - (z * sy);
      const float z1 = (x * sy) + (z * cy);
      const float y2 = (y * cp) - (z1 * sp);
      const float z2 = (y * sp) + (z1 * cp);
      const float x3 = (x1 * cr) - (y2 * sr);
      const float y3 = (x1 * sr) + (y2 * cr);
      const float depth = z2 + 3.2f + (mic_drive * 0.6f);
      const float proj = cube_scale / depth;
      px[i] = static_cast<int16_t>(fx_cx + (x3 * proj));
      py[i] = static_cast<int16_t>(fx_cy + (y3 * proj));
      pz[i] = z2;
    }
    for (uint8_t edge = 0U; edge < 12U; ++edge) {
      const uint8_t a = kCubeEdges[edge][0];
      const uint8_t b = kCubeEdges[edge][1];
      const float z_mix = (pz[a] + pz[b]) * 0.5f;
      const uint16_t color = (z_mix > 0.0f) ? cube_lit : cube_dim;
      draw_safe_line(px[a], py[a], px[b], py[b], color);
    }
  }

  if (scene_status_.show_title && scene_status_.title[0] != '\0') {
    text_attempted = true;
    drivers::display::OverlayTextCommand title_cmd = {};
    title_cmd.text = scene_status_.title;
    title_cmd.font_face = drivers::display::OverlayFontFace::kIbmBold24;
    title_cmd.size = (scene_status_.text_size_pct >= 70U) ? 2U : 1U;
    title_cmd.color565 = color565(0xEBFFF4UL);
    const int16_t title_w = display.measureOverlayText(title_cmd.text, title_cmd.font_face, title_cmd.size);
    title_cmd.x = static_cast<int16_t>((width - title_w) / 2);
    title_cmd.y = 6;
    if (display.drawOverlayText(title_cmd)) {
      text_draw_ok = true;
    }
  }
  if (scene_status_.show_subtitle && scene_status_.subtitle[0] != '\0') {
    text_attempted = true;
    drivers::display::OverlayTextCommand subtitle_cmd = {};
    subtitle_cmd.text = scene_status_.subtitle;
    subtitle_cmd.font_face = drivers::display::OverlayFontFace::kIbmBold16;
    subtitle_cmd.size = 1U;
    subtitle_cmd.color565 = color565(0x9BE7D5UL);
    const int16_t subtitle_w = display.measureOverlayText(subtitle_cmd.text, subtitle_cmd.font_face, subtitle_cmd.size);
    subtitle_cmd.x = static_cast<int16_t>((width - subtitle_w) / 2);
    subtitle_cmd.y = 34;
    if (display.drawOverlayText(subtitle_cmd)) {
      text_draw_ok = true;
    }
  }

  if (la_bg_preset_ == LaBackgroundPreset::kLegacyHourglass && la_overlay_show_hourglass_) {
    int16_t hg_w = static_cast<int16_t>((width * 38) / 100);
    hg_w = std::max<int16_t>(56, std::min<int16_t>(hg_w, static_cast<int16_t>(width - 24)));
    int16_t hg_h = static_cast<int16_t>((height * 62) / 100);
    hg_h = std::max<int16_t>(72, std::min<int16_t>(hg_h, static_cast<int16_t>(height - 36)));
    if (la_hg_target_width_px_ > 0U) {
      hg_w = static_cast<int16_t>(la_hg_target_width_px_);
    }
    if (la_hg_target_height_px_ > 0U) {
      hg_h = static_cast<int16_t>(la_hg_target_height_px_);
    }
    hg_w = std::max<int16_t>(36, std::min<int16_t>(hg_w, static_cast<int16_t>(width - 8)));
    hg_h = std::max<int16_t>(72, std::min<int16_t>(hg_h, static_cast<int16_t>(height - 8)));
    int16_t hg_x = static_cast<int16_t>(((width - hg_w) / 2) + la_hg_x_offset_px_);
    hg_x = clamp_i16(hg_x, 2, static_cast<int16_t>(width - hg_w - 2));
    const int16_t hg_y = static_cast<int16_t>((height - hg_h) / 2);
    const int16_t hg_mid_x = static_cast<int16_t>(hg_x + (hg_w / 2));
    const int16_t hg_mid_y = static_cast<int16_t>(hg_y + (hg_h / 2));
    const int16_t hg_depth = la_overlay_hourglass_modern_ ? 4 : 2;
    const uint16_t hg_bg = color565(0x070E16UL);
    const uint16_t hg_inner_bg = color565(0x0A1721UL);
    const uint16_t hg_far = color565(0x1A3B52UL);
    const uint16_t hg_near = color565(0x7BE2FFUL);
    const uint16_t hg_link = color565(0x3F89AEUL);
    const uint16_t sand_base = color565(0x3DD9B4UL);
    const uint16_t sand_high = color565(0xB6FFF0UL);
    const float roto_t = static_cast<float>(now_ms) * 0.0014f;
    const int16_t rot_dx = static_cast<int16_t>(std::sin(roto_t) * 5.0f);
    const int16_t rot_dy = static_cast<int16_t>(std::cos(roto_t * 0.73f) * 3.0f);
    const int16_t back_x_max = static_cast<int16_t>(std::max<int>(1, static_cast<int>(width - hg_w - 2)));
    const int16_t back_y_max = static_cast<int16_t>(std::max<int>(1, static_cast<int>(height - hg_h - 2)));
    const int16_t hg_back_x = clamp_i16(static_cast<int16_t>(hg_x + rot_dx - hg_depth), 1, back_x_max);
    const int16_t hg_back_y = clamp_i16(static_cast<int16_t>(hg_y + rot_dy - hg_depth), 1, back_y_max);
    const int16_t hg_back_mid_x = static_cast<int16_t>(hg_back_x + (hg_w / 2));
    const int16_t hg_back_mid_y = static_cast<int16_t>(hg_back_y + (hg_h / 2));

    auto draw_clamped_line = [&](int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
      display.drawOverlayLine(clamp_i16(x0, 1, static_cast<int16_t>(width - 2)),
                              clamp_i16(y0, 1, static_cast<int16_t>(height - 2)),
                              clamp_i16(x1, 1, static_cast<int16_t>(width - 2)),
                              clamp_i16(y1, 1, static_cast<int16_t>(height - 2)),
                              color);
    };

    display.fillOverlayRect(hg_x, hg_y, hg_w, hg_h, hg_bg);
    display.fillOverlayRect(static_cast<int16_t>(hg_x + 2),
                            static_cast<int16_t>(hg_y + 2),
                            static_cast<int16_t>(hg_w - 4),
                            static_cast<int16_t>(hg_h - 4),
                            hg_inner_bg);
    display.drawOverlayRect(hg_back_x, hg_back_y, hg_w, hg_h, hg_far);
    display.drawOverlayRect(hg_x, hg_y, hg_w, hg_h, hg_near);
    draw_clamped_line(hg_x, hg_y, hg_back_x, hg_back_y, hg_link);
    draw_clamped_line(static_cast<int16_t>(hg_x + hg_w - 1),
                      hg_y,
                      static_cast<int16_t>(hg_back_x + hg_w - 1),
                      hg_back_y,
                      hg_link);
    draw_clamped_line(hg_x,
                      static_cast<int16_t>(hg_y + hg_h - 1),
                      hg_back_x,
                      static_cast<int16_t>(hg_back_y + hg_h - 1),
                      hg_link);
    draw_clamped_line(static_cast<int16_t>(hg_x + hg_w - 1),
                      static_cast<int16_t>(hg_y + hg_h - 1),
                      static_cast<int16_t>(hg_back_x + hg_w - 1),
                      static_cast<int16_t>(hg_back_y + hg_h - 1),
                      hg_link);

    draw_clamped_line(static_cast<int16_t>(hg_x + 2), static_cast<int16_t>(hg_y + 2), hg_mid_x, hg_mid_y, hg_near);
    draw_clamped_line(static_cast<int16_t>(hg_x + hg_w - 3), static_cast<int16_t>(hg_y + 2), hg_mid_x, hg_mid_y, hg_near);
    draw_clamped_line(static_cast<int16_t>(hg_x + 2),
                      static_cast<int16_t>(hg_y + hg_h - 3),
                      hg_mid_x,
                      hg_mid_y,
                      hg_near);
    draw_clamped_line(static_cast<int16_t>(hg_x + hg_w - 3),
                      static_cast<int16_t>(hg_y + hg_h - 3),
                      hg_mid_x,
                      hg_mid_y,
                      hg_near);

    draw_clamped_line(static_cast<int16_t>(hg_back_x + 2),
                      static_cast<int16_t>(hg_back_y + 2),
                      hg_back_mid_x,
                      hg_back_mid_y,
                      hg_far);
    draw_clamped_line(static_cast<int16_t>(hg_back_x + hg_w - 3),
                      static_cast<int16_t>(hg_back_y + 2),
                      hg_back_mid_x,
                      hg_back_mid_y,
                      hg_far);
    draw_clamped_line(static_cast<int16_t>(hg_back_x + 2),
                      static_cast<int16_t>(hg_back_y + hg_h - 3),
                      hg_back_mid_x,
                      hg_back_mid_y,
                      hg_far);
    draw_clamped_line(static_cast<int16_t>(hg_back_x + hg_w - 3),
                      static_cast<int16_t>(hg_back_y + hg_h - 3),
                      hg_back_mid_x,
                      hg_back_mid_y,
                      hg_far);
    draw_clamped_line(hg_mid_x, hg_mid_y, hg_back_mid_x, hg_back_mid_y, hg_link);

    const int16_t chamber_h = static_cast<int16_t>((hg_h - 12) / 2);
    const int16_t inner_half = static_cast<int16_t>((hg_w / 2) - (la_overlay_hourglass_modern_ ? 8 : 6));
    const int16_t top_start_y = static_cast<int16_t>(hg_y + 5);
    int16_t top_fill = static_cast<int16_t>(hourglass_gate_remain * static_cast<float>(chamber_h));
    if (top_fill < 0) {
      top_fill = 0;
    } else if (top_fill > chamber_h) {
      top_fill = chamber_h;
    }
    int16_t bottom_fill = static_cast<int16_t>(chamber_h - top_fill);
    if (bottom_fill < 0) {
      bottom_fill = 0;
    } else if (bottom_fill > chamber_h) {
      bottom_fill = chamber_h;
    }

    auto drawSandRows = [&](bool top_chamber, int16_t rows) {
      if (rows <= 0 || inner_half <= 0) {
        return;
      }
      for (int16_t row = 0; row < rows; ++row) {
        const float t = static_cast<float>(row) / static_cast<float>((chamber_h > 1) ? (chamber_h - 1) : 1);
        int16_t half = static_cast<int16_t>((1.0f - t) * static_cast<float>(inner_half));
        if (half < 1) {
          half = 1;
        }
        const int16_t y = top_chamber ? static_cast<int16_t>(top_start_y + row)
                                      : static_cast<int16_t>(hg_y + hg_h - 6 - row);
        const uint16_t sand = (t < 0.2f || t > 0.82f) ? sand_high : sand_base;
        display.drawOverlayLine(static_cast<int16_t>(hg_mid_x - half), y, static_cast<int16_t>(hg_mid_x + half), y, sand);
      }
    };

    drawSandRows(true, top_fill);
    drawSandRows(false, bottom_fill);

    const int16_t stream_x = static_cast<int16_t>(hg_mid_x + static_cast<int16_t>(std::sin(roto_t * 2.2f) * 1.0f));
    const int16_t stream_len = static_cast<int16_t>(5 + ((100U - stability_pct) / 9U));
    draw_clamped_line(stream_x, static_cast<int16_t>(hg_mid_y - 2), stream_x, static_cast<int16_t>(hg_mid_y + stream_len), sand_high);
    const int16_t chamber_span = std::max<int16_t>(1, static_cast<int16_t>(chamber_h - 1));
    const int16_t stream_phase = static_cast<int16_t>((now_ms / 36U) % static_cast<uint32_t>(chamber_span));
    for (uint8_t bead = 0U; bead < 3U; ++bead) {
      const int16_t bead_y = static_cast<int16_t>(hg_mid_y + 2 +
                                                  ((stream_phase + static_cast<int16_t>(bead * (chamber_span / 3 + 1))) %
                                                   chamber_span));
      display.fillOverlayRect(stream_x, bead_y, 1, 1, sand_base);
    }
  }

  auto draw_thick_line = [&](int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    display.drawOverlayLine(x0, y0, x1, y1, color);
    display.drawOverlayLine(clamp_i16(static_cast<int16_t>(x0 + 1), 2, static_cast<int16_t>(width - 3)),
                            y0,
                            clamp_i16(static_cast<int16_t>(x1 + 1), 2, static_cast<int16_t>(width - 3)),
                            y1,
                            color);
    display.drawOverlayLine(x0,
                            clamp_i16(static_cast<int16_t>(y0 + 1), 2, static_cast<int16_t>(height - 3)),
                            x1,
                            clamp_i16(static_cast<int16_t>(y1 + 1), 2, static_cast<int16_t>(height - 3)),
                            color);
  };

  if (active_snapshot != nullptr) {
    uint8_t count = active_snapshot->mic_waveform_count;
    if (count > HardwareManager::kMicWaveformCapacity) {
      count = HardwareManager::kMicWaveformCapacity;
    }
    // Oscilloscope mode: shorter acquisition window for audio-player style responsiveness.
    uint8_t max_points = 12U;
    if (la_waveform_audio_player_mode_) {
      uint8_t dynamic_points = static_cast<uint8_t>(la_waveform_window_ms_ / 20U);
      if (dynamic_points < 6U) {
        dynamic_points = 6U;
      } else if (dynamic_points > 18U) {
        dynamic_points = 18U;
      }
      max_points = dynamic_points;
    }
    uint8_t points = (count < max_points) ? count : max_points;
    const int16_t wave_left = 2;
    int16_t wave_w = static_cast<int16_t>((width * (la_waveform_audio_player_mode_ ? 54 : 60)) / 100);
    const int16_t wave_w_min = std::min<int16_t>(96, static_cast<int16_t>(width - 8));
    const int16_t wave_w_max =
        std::max<int16_t>(wave_w_min, static_cast<int16_t>(width - wave_left - 2));
    wave_w = clamp_i16(wave_w, wave_w_min, wave_w_max);
    const int16_t wave_cy = static_cast<int16_t>(height / 2);
    int16_t wave_h = static_cast<int16_t>((height * 50) / 100);
    const int16_t wave_h_min = std::min<int16_t>(56, static_cast<int16_t>(height - 8));
    const int16_t wave_h_max = std::max<int16_t>(wave_h_min, static_cast<int16_t>(height - 8));
    wave_h = clamp_i16(wave_h, wave_h_min, wave_h_max);
    int16_t wave_half_h = static_cast<int16_t>(wave_h / 2 - 4);
    if (wave_half_h < 16) {
      wave_half_h = 16;
    }
    const int16_t wave_box_y = clamp_i16(static_cast<int16_t>(wave_cy - wave_half_h - 4),
                                         2,
                                         static_cast<int16_t>(std::max<int16_t>(2, height - 10)));
    const int16_t wave_box_h = clamp_i16(static_cast<int16_t>((wave_half_h * 2) + 8),
                                         8,
                                         static_cast<int16_t>(std::max<int16_t>(8, height - wave_box_y - 2)));
    const uint16_t wave_bg_line_a = color565(0x091810UL);
    const uint16_t wave_bg_line_b = color565(0x0D1F16UL);
    const uint16_t wave_grid_major = color565(0x1D3A2CUL);
    const uint16_t wave_grid_minor = color565(0x12271EUL);
    const uint16_t wave_axis = color565(0x2DC46EUL);
    if (wave_w > 32) {
      // Pseudo-alpha oscilloscope background: sparse dark scanlines, keep LA backdrop visible.
      const int16_t wave_x0 = static_cast<int16_t>(wave_left + 1);
      const int16_t wave_x1 = static_cast<int16_t>(wave_left + wave_w - 2);
      const int16_t wave_y0 = static_cast<int16_t>(wave_box_y + 1);
      const int16_t wave_y1 = static_cast<int16_t>(wave_box_y + wave_box_h - 2);
      for (int16_t y = wave_y0; y <= wave_y1; y = static_cast<int16_t>(y + 2)) {
        const uint16_t c = (((y - wave_y0) / 2) & 0x01) ? wave_bg_line_b : wave_bg_line_a;
        display.drawOverlayLine(wave_x0, y, wave_x1, y, c);
      }

      const uint32_t osc_window_ms =
          std::max<uint32_t>(120U, la_waveform_audio_player_mode_ ? la_waveform_window_ms_ : 2000U);
      constexpr uint32_t kOscDivCount = 10U;
      const uint32_t osc_ms_per_div = std::max<uint32_t>(10U, osc_window_ms / kOscDivCount);
      for (uint32_t div = 0U; div <= kOscDivCount; ++div) {
        const uint32_t t_ms = div * osc_ms_per_div;
        const int16_t gx = clamp_i16(static_cast<int16_t>(
                                         wave_left +
                                         (static_cast<int32_t>(t_ms) * static_cast<int32_t>(wave_w - 1)) /
                                             static_cast<int32_t>(osc_window_ms)),
                                     wave_x0,
                                     wave_x1);
        display.drawOverlayLine(gx, wave_y0, gx, wave_y1, wave_grid_major);
      }

      // Voltage base: 1 V/div emulated on 8 vertical divisions.
      constexpr int16_t kOscVoltDivisions = 8;
      for (int16_t div = 0; div <= kOscVoltDivisions; ++div) {
        const int16_t gy = clamp_i16(static_cast<int16_t>(wave_box_y + 1 +
                                                           (static_cast<int32_t>(div) *
                                                            static_cast<int32_t>(wave_box_h - 2)) /
                                                               kOscVoltDivisions),
                                     wave_y0,
                                     wave_y1);
        const uint16_t c =
            (div == (kOscVoltDivisions / 2)) ? wave_axis : (((div & 0x01) == 0) ? wave_grid_major : wave_grid_minor);
        display.drawOverlayLine(wave_x0, gy, wave_x1, gy, c);
      }

      display.drawOverlayRect(wave_left, wave_box_y, wave_w, wave_box_h, osc_ring);
      display.drawOverlayLine(wave_left,
                              wave_cy,
                              static_cast<int16_t>(wave_left + wave_w - 1),
                              wave_cy,
                              wave_axis);
      if (wave_w >= 140) {
        char scope_info[28] = {0};
        std::snprintf(scope_info, sizeof(scope_info), "%lums/div  1V/div", static_cast<unsigned long>(osc_ms_per_div));
        drivers::display::OverlayTextCommand osc_text = {};
        osc_text.text = scope_info;
        osc_text.font_face = drivers::display::OverlayFontFace::kBuiltinSmall;
        osc_text.size = 1U;
        osc_text.color565 = color565(0x4EF88DUL);
        osc_text.x = static_cast<int16_t>(wave_left + 4);
        osc_text.y = static_cast<int16_t>(wave_box_y + 3);
        display.drawOverlayText(osc_text);
      }
    }
    if (la_overlay_show_progress_ring_) {
      int16_t gauge_w = static_cast<int16_t>((width * 30) / 100);
      int16_t gauge_h = static_cast<int16_t>((height * 50) / 100);
      const int16_t gauge_w_min = std::min<int16_t>(56, static_cast<int16_t>(width - 8));
      const int16_t gauge_h_min = std::min<int16_t>(56, static_cast<int16_t>(height - 8));
      gauge_w = clamp_i16(gauge_w, gauge_w_min, static_cast<int16_t>(std::max<int16_t>(gauge_w_min, width - 8)));
      gauge_h = clamp_i16(gauge_h, gauge_h_min, static_cast<int16_t>(std::max<int16_t>(gauge_h_min, height - 8)));
      int16_t gauge_radius = static_cast<int16_t>(std::min<int16_t>(gauge_w, gauge_h) / 2);
      if (gauge_radius < 12) {
        gauge_radius = 12;
      }
      const int16_t gauge_margin = std::max<int16_t>(6, static_cast<int16_t>(width / 42));
      const int16_t gauge_cx = clamp_i16(static_cast<int16_t>(width - gauge_margin - gauge_radius + (jitter_x / 2)),
                                         static_cast<int16_t>(gauge_radius + 2),
                                         static_cast<int16_t>(width - gauge_radius - 3));
      const int16_t gauge_cy = clamp_i16(static_cast<int16_t>(wave_cy + (jitter_y / 2)),
                                         static_cast<int16_t>(gauge_radius + 2),
                                         static_cast<int16_t>(height - gauge_radius - 3));
      const int16_t ring_thickness = std::max<int16_t>(3, static_cast<int16_t>(gauge_radius / 6));
      int16_t timeout_outer = gauge_radius;
      int16_t timeout_inner = static_cast<int16_t>(timeout_outer - ring_thickness);
      int16_t stability_outer = static_cast<int16_t>(timeout_inner - 3);
      int16_t stability_inner = static_cast<int16_t>(stability_outer - ring_thickness);
      if (timeout_inner >= timeout_outer) {
        timeout_inner = static_cast<int16_t>(std::max<int16_t>(4, timeout_outer - 3));
      }
      if (stability_outer >= timeout_inner) {
        stability_outer = static_cast<int16_t>(timeout_inner - 2);
      }
      if (stability_inner >= stability_outer) {
        stability_inner = static_cast<int16_t>(std::max<int16_t>(4, stability_outer - 3));
      }

      const uint16_t timeout_bg = color565(0x1A2F3AUL);
      const uint16_t timeout_fg = (gate_remain > 0.25f) ? color565(0x58D8FFUL) : color565(0xFFA46AUL);
      const uint16_t timeout_tip = color565(0xEFFFFFUL);
      const uint16_t stability_bg = color565(0x1E362EUL);
      const uint16_t stability_fg = color565(0x76FFB2UL);
      const uint16_t stability_tip = color565(0xE8FFF4UL);
      constexpr uint8_t kTimeoutSegments = 96U;
      constexpr uint8_t kStabilitySegments = 84U;
      uint8_t timeout_active =
          static_cast<uint8_t>(std::round(gate_remain * static_cast<float>(kTimeoutSegments)));
      if (timeout_active > kTimeoutSegments) {
        timeout_active = kTimeoutSegments;
      }
      const uint8_t stability_active =
          static_cast<uint8_t>((static_cast<uint16_t>(stability_pct) * kStabilitySegments) / 100U);
      auto drawRing = [&](uint8_t segment_count,
                          uint8_t active_segments,
                          int16_t ring_inner,
                          int16_t ring_outer,
                          uint16_t active_color,
                          uint16_t inactive_color,
                          uint16_t tip_color) {
        for (uint8_t segment = 0U; segment < segment_count; ++segment) {
          const float phase = static_cast<float>(segment) / static_cast<float>(segment_count);
          const float angle = (-kHalfPi) + (phase * kTau);
          const int16_t x0 =
              clamp_i16(static_cast<int16_t>(gauge_cx + std::cos(angle) * static_cast<float>(ring_inner)),
                        2,
                        static_cast<int16_t>(width - 3));
          const int16_t y0 =
              clamp_i16(static_cast<int16_t>(gauge_cy + std::sin(angle) * static_cast<float>(ring_inner)),
                        2,
                        static_cast<int16_t>(height - 3));
          const int16_t x1 =
              clamp_i16(static_cast<int16_t>(gauge_cx + std::cos(angle) * static_cast<float>(ring_outer)),
                        2,
                        static_cast<int16_t>(width - 3));
          const int16_t y1 =
              clamp_i16(static_cast<int16_t>(gauge_cy + std::sin(angle) * static_cast<float>(ring_outer)),
                        2,
                        static_cast<int16_t>(height - 3));
          uint16_t ring_color = (segment < active_segments) ? active_color : inactive_color;
          if (segment == active_segments && active_segments > 0U && active_segments < segment_count) {
            ring_color = tip_color;
          }
          display.drawOverlayLine(x0, y0, x1, y1, ring_color);
        }
      };
      drawRing(kTimeoutSegments,
               timeout_active,
               timeout_inner,
               timeout_outer,
               timeout_fg,
               timeout_bg,
               timeout_tip);
      drawRing(kStabilitySegments,
               stability_active,
               stability_inner,
               stability_outer,
               stability_fg,
               stability_bg,
               stability_tip);
    }
    if (points >= 2U && wave_w > 32) {
      const uint8_t head = active_snapshot->mic_waveform_head;
      const uint16_t start = (head >= points) ? static_cast<uint16_t>(head - points)
                                              : static_cast<uint16_t>(head + HardwareManager::kMicWaveformCapacity - points);
      int16_t prev_x = wave_left;
      int16_t prev_y = wave_cy;
      int16_t prev_centered = 0;
      for (uint8_t index = 0U; index < points; ++index) {
        const uint16_t sample_index = static_cast<uint16_t>(start + index) % HardwareManager::kMicWaveformCapacity;
        uint8_t sample = active_snapshot->mic_waveform[sample_index];
        if (sample > 100U) {
          sample = 100U;
        }
        int16_t x = static_cast<int16_t>(wave_left +
                                         (static_cast<int32_t>(index) * static_cast<int32_t>(wave_w - 1)) /
                                             static_cast<int32_t>((points > 1U) ? (points - 1U) : 1U));
        int16_t centered = static_cast<int16_t>(sample) - 50;
        if (la_waveform_audio_player_mode_) {
          const int16_t delta = static_cast<int16_t>(centered - prev_centered);
          centered = static_cast<int16_t>(centered + (delta * 2));
          if (centered < -50) {
            centered = -50;
          } else if (centered > 50) {
            centered = 50;
          }
          centered = static_cast<int16_t>((centered * 130) / 100);
        }
        const int16_t amp = static_cast<int16_t>((centered * wave_half_h) / 50);
        int16_t y = static_cast<int16_t>(wave_cy - amp);
        x = clamp_i16(x, 2, static_cast<int16_t>(width - 3));
        y = clamp_i16(y, static_cast<int16_t>(wave_cy - wave_half_h), static_cast<int16_t>(wave_cy + wave_half_h));
        if (index > 0U) {
          const uint16_t seg_color = ((index + 3U) >= points) ? osc_head : osc_main;
          draw_thick_line(prev_x, prev_y, x, y, seg_color);
        }
        prev_x = x;
        prev_y = y;
        prev_centered = centered;
      }
      display.fillOverlayRect(clamp_i16(static_cast<int16_t>(prev_x - 1), 0, static_cast<int16_t>(width - 1)),
                              clamp_i16(static_cast<int16_t>(prev_y - 1), 0, static_cast<int16_t>(height - 1)),
                              3,
                              3,
                              osc_head);
    }

    constexpr uint8_t kFftVisualBandCount = 60U;
    constexpr uint8_t kA4VisualBand = kFftVisualBandCount / 2U;
    const int16_t fft_bottom = la_overlay_meter_bottom_horizontal_ ? static_cast<int16_t>(height - 28)
                                                                    : static_cast<int16_t>(height - 18);
    const int16_t fft_max_h = 54;
    constexpr int16_t kFftMarginX = 2;
    const int16_t fft_start_x = kFftMarginX;
    int16_t fft_end_x = static_cast<int16_t>(width - 1 - kFftMarginX);
    if (fft_end_x < fft_start_x) {
      fft_end_x = fft_start_x;
    }
    const int32_t fft_span = std::max<int32_t>(
        1, static_cast<int32_t>(fft_end_x) - static_cast<int32_t>(fft_start_x) + 1);
    auto sampleFftBand = [&](uint8_t visual_index) -> uint8_t {
      constexpr uint8_t kSourceBands = HardwareManager::kMicSpectrumBinCount;
      if (kSourceBands == 0U) {
        return 0U;
      }
      if (kSourceBands == 1U || kFftVisualBandCount <= 1U) {
        return active_snapshot->mic_spectrum[0];
      }
      const float centered = static_cast<float>(visual_index) - static_cast<float>(kA4VisualBand);
      const float half_visual = static_cast<float>((kFftVisualBandCount > 2U) ? (kA4VisualBand) : 1U);
      const float normalized = centered / ((half_visual > 0.0f) ? half_visual : 1.0f);
      const float source_center = static_cast<float>(kSourceBands - 1U) / 2.0f;
      const float source_span = source_center;
      const float pos = source_center + (normalized * source_span);
      uint8_t left = static_cast<uint8_t>(pos);
      if (left >= kSourceBands) {
        left = static_cast<uint8_t>(kSourceBands - 1U);
      }
      uint8_t right = (left + 1U < kSourceBands) ? static_cast<uint8_t>(left + 1U) : left;
      const float frac = pos - static_cast<float>(left);
      const float blended = (static_cast<float>(active_snapshot->mic_spectrum[left]) * (1.0f - frac)) +
                            (static_cast<float>(active_snapshot->mic_spectrum[right]) * frac);
      int value = static_cast<int>(std::round(blended));
      if (value < 0) {
        value = 0;
      } else if (value > 100) {
        value = 100;
      }
      return static_cast<uint8_t>(value);
    };
    auto spectrumGradientColor = [&](uint8_t y_pct) -> uint16_t {
      const uint8_t clamped = (y_pct > 100U) ? 100U : y_pct;
      uint8_t r = 0U;
      uint8_t g = 0U;
      uint8_t b = 0U;
      if (la_bargraph_blue_palette_) {
        r = static_cast<uint8_t>(18U + (clamped / 4U));
        g = static_cast<uint8_t>(70U + ((static_cast<uint16_t>(clamped) * 165U) / 100U));
        b = static_cast<uint8_t>(168U + ((static_cast<uint16_t>(clamped) * 86U) / 100U));
      } else {
        if (clamped <= 50U) {
          r = static_cast<uint8_t>((static_cast<uint16_t>(clamped) * 255U) / 50U);
          g = 255U;
        } else {
          r = 255U;
          g = static_cast<uint8_t>((static_cast<uint16_t>(100U - clamped) * 255U) / 50U);
        }
        b = 18U;
      }
      return display.color565(r, g, b);
    };
    const uint16_t fft_edge = color565(la_bargraph_blue_palette_ ? 0xC8ECFFUL : 0xD7F4E8UL);
    const uint16_t fft_peak = color565(la_bargraph_blue_palette_ ? 0xEEFAFFUL : 0xFFF2C7UL);
    static uint8_t s_fft_peak_level[kFftVisualBandCount] = {};
    static uint32_t s_fft_peak_hold_until_ms[kFftVisualBandCount] = {};
    static uint32_t s_fft_peak_last_ms = 0U;
    if (s_fft_peak_last_ms == 0U || now_ms < s_fft_peak_last_ms || (now_ms - s_fft_peak_last_ms) > 3500U) {
      std::memset(s_fft_peak_level, 0, sizeof(s_fft_peak_level));
      std::memset(s_fft_peak_hold_until_ms, 0, sizeof(s_fft_peak_hold_until_ms));
      s_fft_peak_last_ms = now_ms;
    }
    uint32_t peak_dt_ms = (now_ms >= s_fft_peak_last_ms) ? (now_ms - s_fft_peak_last_ms) : 0U;
    if (peak_dt_ms > 500U) {
      peak_dt_ms = 500U;
    }
    s_fft_peak_last_ms = now_ms;
    uint8_t peak_decay_step =
        static_cast<uint8_t>((static_cast<uint32_t>(la_bargraph_decay_per_s_) * peak_dt_ms) / 1000U);
    if (peak_decay_step == 0U) {
      peak_decay_step = 1U;
    }

    for (uint8_t index = 0U; index < kFftVisualBandCount; ++index) {
      const uint8_t band_raw = sampleFftBand(index);
      const uint8_t band = static_cast<uint8_t>(
          std::min<int>(100, (static_cast<int>(band_raw) * 220) / 100 + (static_cast<int>(mic_level_pct) / 3)));
      int16_t h = static_cast<int16_t>(4 + (static_cast<int32_t>(fft_max_h) * band) / 100);
      if (h < 4) {
        h = 4;
      }
      if (h > fft_max_h) {
        h = fft_max_h;
      }
      const int32_t band_x0_raw =
          static_cast<int32_t>(fft_start_x) +
          (static_cast<int32_t>(index) * fft_span) / static_cast<int32_t>(kFftVisualBandCount);
      const int32_t band_x1_raw =
          static_cast<int32_t>(fft_start_x) +
          (static_cast<int32_t>(index + 1U) * fft_span) / static_cast<int32_t>(kFftVisualBandCount) - 1;
      int16_t x0 = clamp_i16(static_cast<int16_t>(band_x0_raw), fft_start_x, fft_end_x);
      int16_t x1 = clamp_i16(static_cast<int16_t>(band_x1_raw), fft_start_x, fft_end_x);
      if (x1 < x0) {
        x1 = x0;
      }
      const int16_t bar_w = std::max<int16_t>(1, static_cast<int16_t>(x1 - x0 + 1));
      const int16_t y = static_cast<int16_t>(fft_bottom - h);
      const int16_t x_end = static_cast<int16_t>(x0 + bar_w - 1);
      uint8_t peak_level = s_fft_peak_level[index];
      if (band >= peak_level) {
        peak_level = band;
        s_fft_peak_hold_until_ms[index] = now_ms + la_bargraph_peak_hold_ms_;
      } else if (now_ms >= s_fft_peak_hold_until_ms[index]) {
        peak_level = (peak_level > peak_decay_step) ? static_cast<uint8_t>(peak_level - peak_decay_step) : 0U;
        if (band > peak_level) {
          peak_level = band;
        }
      }
      s_fft_peak_level[index] = peak_level;
      for (int16_t row = 0; row < h; ++row) {
        const uint8_t y_pct = static_cast<uint8_t>(
            (static_cast<uint32_t>(h - 1 - row) * 100U) / static_cast<uint32_t>(std::max<int16_t>(1, h - 1)));
        const uint16_t c = spectrumGradientColor(y_pct);
        const int16_t yy = static_cast<int16_t>(y + row);
        display.drawOverlayLine(x0, yy, x_end, yy, c);
      }
      display.drawOverlayRect(x0, y, bar_w, h, fft_edge);
      if (peak_level > 0U) {
        const int16_t peak_h = static_cast<int16_t>(4 + (static_cast<int32_t>(fft_max_h) * peak_level) / 100);
        const int16_t peak_y = static_cast<int16_t>(fft_bottom - peak_h);
        if (peak_y >= static_cast<int16_t>(fft_bottom - fft_max_h - 1) && peak_y < fft_bottom) {
          display.drawOverlayLine(x0, peak_y, x_end, peak_y, fft_peak);
        }
      }
    }

    const int16_t marker_x = static_cast<int16_t>(width / 2);
    display.drawOverlayLine(marker_x, static_cast<int16_t>(fft_bottom - fft_max_h - 4), marker_x, fft_bottom, marker);

    if (la_overlay_show_pitch_text_) {
      text_attempted = true;
      char pitch_line[44] = {0};
      std::snprintf(pitch_line,
                    sizeof(pitch_line),
                    "A4 440Hz  %3uHz  %+dc",
                    static_cast<unsigned int>(active_snapshot->mic_freq_hz),
                    static_cast<int>(active_snapshot->mic_pitch_cents));
      drivers::display::OverlayTextCommand text_cmd = {};
      text_cmd.text = pitch_line;
      text_cmd.x = static_cast<int16_t>(marker_x - 58);
      text_cmd.y = static_cast<int16_t>(height - 24);
      text_cmd.font_face = drivers::display::OverlayFontFace::kBuiltinSmall;
      text_cmd.size = 1U;
      text_cmd.color565 = marker;
      if (display.drawOverlayText(text_cmd)) {
        text_draw_ok = true;
      }
    }
  }

  if (la_overlay_show_caption_ && la_overlay_caption_[0] != '\0') {
    text_attempted = true;
    drivers::display::OverlayTextCommand caption_cmd = {};
    caption_cmd.text = la_overlay_caption_;
    caption_cmd.font_face = la_overlay_caption_font_;
    caption_cmd.size = la_overlay_caption_size_;
    caption_cmd.color565 = color565(0xC8FCE9UL);
    const int16_t caption_width = display.measureOverlayText(caption_cmd.text, caption_cmd.font_face, caption_cmd.size);
    int16_t caption_x = static_cast<int16_t>((width - caption_width) / 2);
    caption_cmd.x = caption_x;
    caption_cmd.y = la_overlay_meter_bottom_horizontal_ ? static_cast<int16_t>(height - 28)
                                                        : static_cast<int16_t>(height - 12);
    if (display.drawOverlayText(caption_cmd)) {
      text_draw_ok = true;
    }
  }

  if (text_attempted) {
    if (text_draw_ok) {
      ++overlay_draw_ok_count_;
    } else {
      ++overlay_draw_fail_count_;
    }
  }
  display.endWrite();
}


// Keep split sections in the original translation unit so they can use
// internal helpers and constants defined above.
#define UI_MANAGER_SPLIT_IMPL 1
#include "ui_manager_display.cpp"
#include "ui_manager_intro.cpp"
#include "ui_manager_effects.cpp"
#undef UI_MANAGER_SPLIT_IMPL

void UiManager::dumpGraphicsStatus() const {
  const uint32_t flush_avg_us = (graphics_stats_.flush_count == 0U)
                                  ? 0U
                                  : (graphics_stats_.flush_time_total_us / graphics_stats_.flush_count);
  const uint32_t draw_avg_us =
      (graphics_stats_.draw_count == 0U) ? 0U : (graphics_stats_.draw_time_total_us / graphics_stats_.draw_count);
  const ui::fx::FxEngineStats fx_stats = fx_engine_.stats();
  UI_LOGI(
      "GFX_STATUS depth=%u mode=%s theme256=%u lines=%u double=%u source=%s full_frame=%u dma_req=%u dma_async=%u trans_px=%u trans_lines=%u pending=%u flush=%lu dma=%lu sync=%lu flush_spi_avg=%lu flush_spi_max=%lu draw_lvgl_avg=%lu draw_lvgl_max=%lu fx_enabled=%u fx_scene=%u fx_fps=%u fx_frames=%lu fx_blit=%lu/%lu/%lu tail=%lu fx_dma_to=%lu fx_fail=%lu fx_skip_busy=%lu block=%lu ovf=%lu stall=%lu recover=%lu async_fallback=%lu",
      static_cast<unsigned int>(LV_COLOR_DEPTH),
      kUseColor256Runtime ? "RGB332" : "RGB565",
      kUseThemeQuantizeRuntime ? 1U : 0U,
      static_cast<unsigned int>(buffer_cfg_.lines),
      buffer_cfg_.double_buffer ? 1U : 0U,
      buffer_cfg_.draw_in_psram ? "PSRAM" : "SRAM_DMA",
      buffer_cfg_.full_frame ? 1U : 0U,
      dma_requested_ ? 1U : 0U,
      async_flush_enabled_ ? 1U : 0U,
      static_cast<unsigned int>(dma_trans_buf_pixels_),
      static_cast<unsigned int>(buffer_cfg_.selected_trans_lines),
      flush_ctx_.pending ? 1U : 0U,
      static_cast<unsigned long>(graphics_stats_.flush_count),
      static_cast<unsigned long>(graphics_stats_.dma_flush_count),
      static_cast<unsigned long>(graphics_stats_.sync_flush_count),
      static_cast<unsigned long>(flush_avg_us),
      static_cast<unsigned long>(graphics_stats_.flush_time_max_us),
      static_cast<unsigned long>(draw_avg_us),
      static_cast<unsigned long>(graphics_stats_.draw_time_max_us),
      fx_engine_.enabled() ? 1U : 0U,
      direct_fx_scene_active_ ? 1U : 0U,
      static_cast<unsigned int>(fx_stats.fps),
      static_cast<unsigned long>(fx_stats.frame_count),
      static_cast<unsigned long>(fx_stats.blit_cpu_us),
      static_cast<unsigned long>(fx_stats.blit_dma_submit_us),
      static_cast<unsigned long>(fx_stats.blit_dma_wait_us),
      static_cast<unsigned long>(fx_stats.dma_tail_wait_us),
      static_cast<unsigned long>(fx_stats.dma_timeout_count),
      static_cast<unsigned long>(fx_stats.blit_fail_busy),
      static_cast<unsigned long>(graphics_stats_.fx_skip_flush_busy),
      static_cast<unsigned long>(graphics_stats_.flush_blocked_count),
      static_cast<unsigned long>(graphics_stats_.flush_overflow_count),
      static_cast<unsigned long>(graphics_stats_.flush_stall_count),
      static_cast<unsigned long>(graphics_stats_.flush_recover_count),
      static_cast<unsigned long>(graphics_stats_.async_fallback_count));
}

UiMemorySnapshot UiManager::memorySnapshot() const {
  UiMemorySnapshot snapshot = {};

#if LV_USE_MEM_MONITOR
  lv_mem_monitor_t monitor = {};
  lv_mem_monitor(&monitor);
  snapshot.lv_mem_used = monitor.total_size - monitor.free_size;
  snapshot.lv_mem_free = monitor.free_size;
  snapshot.lv_mem_frag_pct = monitor.frag_pct;
  snapshot.lv_mem_max_used = monitor.max_used;
#endif

#if defined(ARDUINO_ARCH_ESP32)
  snapshot.heap_internal_free = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  snapshot.heap_dma_free = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_DMA));
  snapshot.heap_psram_free = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  snapshot.heap_largest_dma_block = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
#endif

  snapshot.alloc_failures = runtime::memory::CapsAllocator::failureCount();
  snapshot.draw_lines = buffer_cfg_.lines;
  snapshot.draw_in_psram = buffer_cfg_.draw_in_psram;
  snapshot.full_frame = buffer_cfg_.full_frame;
  snapshot.dma_async_enabled = async_flush_enabled_;

  const size_t width = static_cast<size_t>(activeDisplayWidth());
  const size_t height = static_cast<size_t>(activeDisplayHeight());
  size_t draw_pixels = 0U;
  if (buffer_cfg_.full_frame) {
    runtime::memory::safeMulSize(width, height, &draw_pixels);
  } else {
    runtime::memory::safeMulSize(width, static_cast<size_t>(buffer_cfg_.lines), &draw_pixels);
  }
  size_t draw_bytes = 0U;
  runtime::memory::safeMulSize(draw_pixels, sizeof(lv_color_t), &draw_bytes);
  snapshot.draw_buffer_bytes = static_cast<uint32_t>((draw_bytes > 0xFFFFFFFFULL) ? 0xFFFFFFFFULL : draw_bytes);

  size_t trans_bytes = 0U;
  runtime::memory::safeMulSize(dma_trans_buf_pixels_, sizeof(uint16_t), &trans_bytes);
  snapshot.trans_buffer_bytes = static_cast<uint32_t>((trans_bytes > 0xFFFFFFFFULL) ? 0xFFFFFFFFULL : trans_bytes);
  snapshot.selected_trans_lines = buffer_cfg_.selected_trans_lines;
  snapshot.async_fallback_count = graphics_stats_.async_fallback_count;
  const ui::fx::FxEngineStats fx_stats = fx_engine_.stats();
  snapshot.fx_fps = fx_stats.fps;
  snapshot.fx_frame_count = fx_stats.frame_count;
  snapshot.fx_blit_cpu_us = fx_stats.blit_cpu_us;
  snapshot.fx_blit_submit_us = fx_stats.blit_dma_submit_us;
  snapshot.fx_blit_wait_us = fx_stats.blit_dma_wait_us;
  snapshot.fx_blit_tail_wait_us = fx_stats.dma_tail_wait_us;
  snapshot.fx_dma_timeout_count = fx_stats.dma_timeout_count;
  snapshot.fx_blit_fail_busy = fx_stats.blit_fail_busy;
  snapshot.fx_skip_flush_busy = graphics_stats_.fx_skip_flush_busy;
  snapshot.flush_blocked = graphics_stats_.flush_blocked_count;
  snapshot.flush_overflow = graphics_stats_.flush_overflow_count;
  snapshot.flush_stall = graphics_stats_.flush_stall_count;
  snapshot.flush_recover = graphics_stats_.flush_recover_count;
  snapshot.draw_flush_stall = graphics_stats_.flush_stall_count;
  const uint32_t fx_pixels = static_cast<uint32_t>(activeDisplayWidth()) * static_cast<uint32_t>(activeDisplayHeight());
  snapshot.conv_pixels_per_ms = 0U;
  if (fx_pixels != 0U && fx_stats.blit_cpu_us != 0U) {
    const uint32_t px_per_ms = (fx_pixels * 1000U) / fx_stats.blit_cpu_us;
    snapshot.conv_pixels_per_ms = static_cast<uint16_t>((px_per_ms > 0xFFFFU) ? 0xFFFFU : px_per_ms);
  }
  if (graphics_stats_.flush_count > 0U) {
    snapshot.flush_time_avg_us = graphics_stats_.flush_time_total_us / graphics_stats_.flush_count;
  } else {
    snapshot.flush_time_avg_us = 0U;
  }
  snapshot.flush_time_max_us = graphics_stats_.flush_time_max_us;
  if (graphics_stats_.draw_count > 0U) {
    snapshot.draw_time_avg_us = graphics_stats_.draw_time_total_us / graphics_stats_.draw_count;
  } else {
    snapshot.draw_time_avg_us = 0U;
  }
  snapshot.draw_time_max_us = graphics_stats_.draw_time_max_us;
  snapshot.flush_spi_us = snapshot.flush_time_avg_us;
  snapshot.draw_lvgl_us = snapshot.draw_time_avg_us;
  return snapshot;
}

UiSceneStatusSnapshot UiManager::sceneStatusSnapshot() const {
  return scene_status_;
}

void UiManager::dumpMemoryStatus() const {
  const UiMemorySnapshot snapshot = memorySnapshot();
#if LV_USE_MEM_MONITOR
  UI_LOGI("LV_MEM used=%u free=%u frag=%u%% max_used=%u",
          static_cast<unsigned int>(snapshot.lv_mem_used),
          static_cast<unsigned int>(snapshot.lv_mem_free),
          static_cast<unsigned int>(snapshot.lv_mem_frag_pct),
          static_cast<unsigned int>(snapshot.lv_mem_max_used));
#else
  UI_LOGI("LV_MEM monitor disabled at compile-time");
#endif
#if defined(ARDUINO_ARCH_ESP32)
  UI_LOGI("HEAP internal=%u dma=%u psram=%u largest_dma=%u",
          static_cast<unsigned int>(snapshot.heap_internal_free),
          static_cast<unsigned int>(snapshot.heap_dma_free),
          static_cast<unsigned int>(snapshot.heap_psram_free),
          static_cast<unsigned int>(snapshot.heap_largest_dma_block));
#endif
  UI_LOGI("MEM_SNAPSHOT draw_lines=%u draw_psram=%u full_frame=%u dma_async=%u draw_bytes=%u trans_bytes=%u trans_lines=%u alloc_fail=%lu draw_lvgl=%lu flush_spi=%lu draw_stall=%lu conv_px_ms=%u async_fb=%lu fx_blit=%lu/%lu/%lu tail=%lu",
          static_cast<unsigned int>(snapshot.draw_lines),
          snapshot.draw_in_psram ? 1U : 0U,
          snapshot.full_frame ? 1U : 0U,
          snapshot.dma_async_enabled ? 1U : 0U,
          static_cast<unsigned int>(snapshot.draw_buffer_bytes),
          static_cast<unsigned int>(snapshot.trans_buffer_bytes),
          static_cast<unsigned int>(snapshot.selected_trans_lines),
          static_cast<unsigned long>(snapshot.alloc_failures),
          static_cast<unsigned long>(snapshot.draw_lvgl_us),
          static_cast<unsigned long>(snapshot.flush_spi_us),
          static_cast<unsigned long>(snapshot.draw_flush_stall),
          static_cast<unsigned int>(snapshot.conv_pixels_per_ms),
          static_cast<unsigned long>(snapshot.async_fallback_count),
          static_cast<unsigned long>(snapshot.fx_blit_cpu_us),
          static_cast<unsigned long>(snapshot.fx_blit_submit_us),
          static_cast<unsigned long>(snapshot.fx_blit_wait_us),
          static_cast<unsigned long>(snapshot.fx_blit_tail_wait_us));
}

void UiManager::setHardwareSnapshot(const HardwareManager::Snapshot& snapshot) {
  waveform_snapshot_ref_ = nullptr;
  waveform_snapshot_ = snapshot;
  waveform_snapshot_valid_ = true;
}

void UiManager::setHardwareSnapshotRef(const HardwareManager::Snapshot* snapshot) {
  waveform_snapshot_ref_ = snapshot;
  waveform_snapshot_valid_ = (snapshot != nullptr);
  if (snapshot != nullptr) {
    waveform_snapshot_ = *snapshot;
  }
}

void UiManager::setLaDetectionState(bool locked,
                                    uint8_t stability_pct,
                                    uint32_t stable_ms,
                                    uint32_t stable_target_ms,
                                    uint32_t gate_elapsed_ms,
                                    uint32_t gate_timeout_ms) {
  la_detection_locked_ = locked;
  if (stability_pct > 100U) {
    stability_pct = 100U;
  }
  la_detection_stability_pct_ = stability_pct;
  la_detection_stable_ms_ = stable_ms;
  la_detection_stable_target_ms_ = stable_target_ms;
  la_detection_gate_elapsed_ms_ = gate_elapsed_ms;
  la_detection_gate_timeout_ms_ = gate_timeout_ms;
}

void UiManager::renderScene(const ScenarioDef* scenario,
                            const char* screen_scene_id,
                            const char* step_id,
                            const char* audio_pack_id,
                            bool audio_playing,
                            const char* screen_payload_json) {
  if (!ready_) {
    return;
  }

  const char* scenario_id = (scenario != nullptr && scenario->id != nullptr) ? scenario->id : "N/A";
  const char* raw_scene_id =
      (screen_scene_id != nullptr && screen_scene_id[0] != '\0') ? screen_scene_id : "SCENE_READY";
  if (kUseDemoAutorunWinEtapeRuntime) {
    raw_scene_id = "SCENE_WIN_ETAPE";
  }
  const char* normalized_scene_id = storyNormalizeScreenSceneId(raw_scene_id);
  const char* step_id_for_log = (step_id != nullptr && step_id[0] != '\0') ? step_id : "N/A";
  const char* step_id_for_ui = (step_id != nullptr && step_id[0] != '\0') ? step_id : "";
  const char* audio_pack_id_for_ui = (audio_pack_id != nullptr && audio_pack_id[0] != '\0') ? audio_pack_id : "";
  if (normalized_scene_id == nullptr) {
    scene_status_.valid = false;
    copyTextSafe(scene_status_.scenario_id, sizeof(scene_status_.scenario_id), scenario_id);
    copyTextSafe(scene_status_.step_id, sizeof(scene_status_.step_id), step_id_for_ui);
    copyTextSafe(scene_status_.scene_id, sizeof(scene_status_.scene_id), raw_scene_id);
    copyTextSafe(scene_status_.audio_pack_id, sizeof(scene_status_.audio_pack_id), audio_pack_id_for_ui);
    UI_LOGI("unknown scene id '%s' in scenario=%s step=%s", raw_scene_id, scenario_id, step_id_for_log);
    return;
  }
  if (normalized_scene_id != nullptr && std::strcmp(raw_scene_id, normalized_scene_id) != 0) {
    UI_LOGI("scene alias normalized: %s -> %s", raw_scene_id, normalized_scene_id);
  }
  const char* scene_id = normalized_scene_id;
  const bool scene_changed = (std::strcmp(last_scene_id_, scene_id) != 0);
  const uint32_t payload_crc = hashScenePayload(screen_payload_json);
  const bool static_state_changed = shouldApplySceneStaticState(scene_id, screen_payload_json, scene_changed);
  const bool has_previous_scene = (last_scene_id_[0] != '\0');
  const bool win_etape_intro_scene = false;
  const bool la_detector_lgfx_only_scene = (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0);
  const bool direct_fx_scene_runtime = isDirectFxSceneId(scene_id) && !la_detector_lgfx_only_scene;
  const bool test_lab_scene = (std::strcmp(scene_id, "SCENE_TEST_LAB") == 0);
  const bool is_locked_scene = (std::strcmp(scene_id, "SCENE_LOCKED") == 0);
  const bool qr_scene = (std::strcmp(scene_id, "SCENE_CAMERA_SCAN") == 0 ||
                         std::strcmp(scene_id, "SCENE_QR_DETECTOR") == 0);
  const bool parse_payload_this_frame = static_state_changed || win_etape_intro_scene;
  if (scene_changed && has_previous_scene) {
    cleanupSceneTransitionAssets(last_scene_id_, scene_id);
  }

  if (static_state_changed && !win_etape_intro_scene && intro_active_) {
    stopIntroAndCleanup();
  }
  if (static_state_changed && !direct_fx_scene_runtime) {
    direct_fx_scene_active_ = false;
  }

  auto parseEffectToken = [](const char* token, SceneEffect fallback, const char* source) -> SceneEffect {
    if (token == nullptr || token[0] == '\0') {
      return fallback;
    }
    char normalized[24] = {0};
    std::strncpy(normalized, token, sizeof(normalized) - 1U);
    for (size_t index = 0U; normalized[index] != '\0'; ++index) {
      normalized[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[index])));
    }
    if (std::strcmp(normalized, "none") == 0 || std::strcmp(normalized, "steady") == 0) {
      return SceneEffect::kNone;
    }
    if (std::strcmp(normalized, "pulse") == 0) {
      return SceneEffect::kPulse;
    }
    if (std::strcmp(normalized, "scan") == 0) {
      return SceneEffect::kScan;
    }
    if (std::strcmp(normalized, "radar") == 0) {
      return SceneEffect::kRadar;
    }
    if (std::strcmp(normalized, "wave") == 0) {
      return SceneEffect::kWave;
    }
    if (std::strcmp(normalized, "blink") == 0) {
      return SceneEffect::kBlink;
    }
    if (std::strcmp(normalized, "glitch") == 0 || std::strcmp(normalized, "camera_flash") == 0) {
      return SceneEffect::kGlitch;
    }
    if (std::strcmp(normalized, "text_glitch") == 0 || std::strcmp(normalized, "glitch_text") == 0 ||
        std::strcmp(normalized, "textglitch") == 0) {
      return SceneEffect::kGlitch;
    }
    if (std::strcmp(normalized, "celebrate") == 0 || std::strcmp(normalized, "reward") == 0) {
      return SceneEffect::kCelebrate;
    }
    UI_LOGD("unknown effect token '%s' in %s, fallback", token, source);
    return SceneEffect::kPulse;
  };

  auto parseTransitionToken = [](const char* token, SceneTransition fallback, const char* source) -> SceneTransition {
    if (token == nullptr || token[0] == '\0') {
      return fallback;
    }
    char normalized[28] = {0};
    std::strncpy(normalized, token, sizeof(normalized) - 1U);
    for (size_t index = 0U; normalized[index] != '\0'; ++index) {
      normalized[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[index])));
      if (normalized[index] == '-') {
        normalized[index] = '_';
      }
    }
    if (std::strcmp(normalized, "none") == 0 || std::strcmp(normalized, "off") == 0) {
      return SceneTransition::kNone;
    }
    if (std::strcmp(normalized, "fade") == 0 || std::strcmp(normalized, "crossfade") == 0) {
      return SceneTransition::kFade;
    }
    if (std::strcmp(normalized, "slide_left") == 0 || std::strcmp(normalized, "left") == 0) {
      return SceneTransition::kSlideLeft;
    }
    if (std::strcmp(normalized, "wipe") == 0) {
      return SceneTransition::kSlideLeft;
    }
    if (std::strcmp(normalized, "slide_right") == 0 || std::strcmp(normalized, "right") == 0) {
      return SceneTransition::kSlideRight;
    }
    if (std::strcmp(normalized, "slide_up") == 0 || std::strcmp(normalized, "up") == 0) {
      return SceneTransition::kSlideUp;
    }
    if (std::strcmp(normalized, "slide_down") == 0 || std::strcmp(normalized, "down") == 0) {
      return SceneTransition::kSlideDown;
    }
    if (std::strcmp(normalized, "zoom") == 0 || std::strcmp(normalized, "zoom_in") == 0) {
      return SceneTransition::kZoom;
    }
    if (std::strcmp(normalized, "glitch") == 0 || std::strcmp(normalized, "flash") == 0 ||
        std::strcmp(normalized, "camera_flash") == 0) {
      return SceneTransition::kGlitch;
    }
    UI_LOGD("unknown transition token '%s' in %s, fallback", token, source);
    return fallback;
  };

  auto effectToToken = [](SceneEffect value) -> const char* {
    switch (value) {
      case SceneEffect::kNone:
        return "none";
      case SceneEffect::kPulse:
        return "pulse";
      case SceneEffect::kScan:
        return "scan";
      case SceneEffect::kRadar:
        return "radar";
      case SceneEffect::kWave:
        return "wave";
      case SceneEffect::kBlink:
        return "blink";
      case SceneEffect::kGlitch:
        return "glitch";
      case SceneEffect::kCelebrate:
        return "celebrate";
      default:
        return "none";
    }
  };

  auto transitionToToken = [](SceneTransition value) -> const char* {
    switch (value) {
      case SceneTransition::kNone:
        return "none";
      case SceneTransition::kFade:
        return "fade";
      case SceneTransition::kSlideLeft:
        return "slide_left";
      case SceneTransition::kSlideRight:
        return "slide_right";
      case SceneTransition::kSlideUp:
        return "slide_up";
      case SceneTransition::kSlideDown:
        return "slide_down";
      case SceneTransition::kZoom:
        return "zoom";
      case SceneTransition::kGlitch:
        return "glitch";
      default:
        return "none";
    }
  };

  auto parseAlignToken = [](const char* token, SceneTextAlign fallback) -> SceneTextAlign {
    if (token == nullptr || token[0] == '\0') {
      return fallback;
    }
    char normalized[20] = {0};
    std::strncpy(normalized, token, sizeof(normalized) - 1U);
    for (size_t index = 0U; normalized[index] != '\0'; ++index) {
      normalized[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[index])));
    }
    if (std::strcmp(normalized, "top") == 0) {
      return SceneTextAlign::kTop;
    }
    if (std::strcmp(normalized, "center") == 0 || std::strcmp(normalized, "middle") == 0) {
      return SceneTextAlign::kCenter;
    }
    if (std::strcmp(normalized, "bottom") == 0) {
      return SceneTextAlign::kBottom;
    }
    return fallback;
  };

  auto applyTextCase = [](const char* mode, String value) -> String {
    if (mode == nullptr || mode[0] == '\0') {
      return value;
    }
    char normalized[16] = {0};
    std::strncpy(normalized, mode, sizeof(normalized) - 1U);
    for (size_t index = 0U; normalized[index] != '\0'; ++index) {
      normalized[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[index])));
    }
    if (std::strcmp(normalized, "upper") == 0) {
      value.toUpperCase();
    } else if (std::strcmp(normalized, "lower") == 0) {
      value.toLowerCase();
    }
    return value;
  };

  auto parseOverlayFontFace = [](const char* token,
                                 drivers::display::OverlayFontFace fallback) -> drivers::display::OverlayFontFace {
    if (token == nullptr || token[0] == '\0') {
      return fallback;
    }
    char normalized[40] = {0};
    std::strncpy(normalized, token, sizeof(normalized) - 1U);
    for (size_t index = 0U; normalized[index] != '\0'; ++index) {
      const char ch = normalized[index];
      normalized[index] = (ch == '-' || ch == ' ') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (std::strcmp(normalized, "builtin_small") == 0) {
      return drivers::display::OverlayFontFace::kBuiltinSmall;
    }
    if (std::strcmp(normalized, "builtin_medium") == 0) {
      return drivers::display::OverlayFontFace::kBuiltinMedium;
    }
    if (std::strcmp(normalized, "builtin_large") == 0) {
      return drivers::display::OverlayFontFace::kBuiltinLarge;
    }
    if (std::strcmp(normalized, "ibm_regular_14") == 0) {
      return drivers::display::OverlayFontFace::kIbmRegular14;
    }
    if (std::strcmp(normalized, "ibm_regular_18") == 0) {
      return drivers::display::OverlayFontFace::kIbmRegular18;
    }
    if (std::strcmp(normalized, "ibm_bold_12") == 0) {
      return drivers::display::OverlayFontFace::kIbmBold12;
    }
    if (std::strcmp(normalized, "ibm_bold_16") == 0) {
      return drivers::display::OverlayFontFace::kIbmBold16;
    }
    if (std::strcmp(normalized, "ibm_bold_20") == 0) {
      return drivers::display::OverlayFontFace::kIbmBold20;
    }
    if (std::strcmp(normalized, "ibm_bold_24") == 0) {
      return drivers::display::OverlayFontFace::kIbmBold24;
    }
    if (std::strcmp(normalized, "ibm_italic_12") == 0) {
      return drivers::display::OverlayFontFace::kIbmItalic12;
    }
    if (std::strcmp(normalized, "ibm_italic_16") == 0) {
      return drivers::display::OverlayFontFace::kIbmItalic16;
    }
    if (std::strcmp(normalized, "ibm_italic_20") == 0) {
      return drivers::display::OverlayFontFace::kIbmItalic20;
    }
    if (std::strcmp(normalized, "ibm_italic_24") == 0) {
      return drivers::display::OverlayFontFace::kIbmItalic24;
    }
    if (std::strcmp(normalized, "inter_18") == 0) {
      return drivers::display::OverlayFontFace::kInter18;
    }
    if (std::strcmp(normalized, "inter_24") == 0) {
      return drivers::display::OverlayFontFace::kInter24;
    }
    if (std::strcmp(normalized, "orbitron_28") == 0) {
      return drivers::display::OverlayFontFace::kOrbitron28;
    }
    if (std::strcmp(normalized, "bungee_24") == 0) {
      return drivers::display::OverlayFontFace::kBungee24;
    }
    if (std::strcmp(normalized, "monoton_24") == 0) {
      return drivers::display::OverlayFontFace::kMonoton24;
    }
    if (std::strcmp(normalized, "rubik_glitch_24") == 0) {
      return drivers::display::OverlayFontFace::kRubikGlitch24;
    }
    return fallback;
  };

  String title = "MISSION";
  String subtitle;
  String symbol = "RUN";
  // Keep titles visible by default so payload misses cannot silently blank scene text.
  bool show_title = true;
  bool show_subtitle = true;
  bool show_symbol = true;
  SceneEffect effect = SceneEffect::kPulse;
  uint16_t effect_speed_ms = 0U;
  SceneTransition transition = SceneTransition::kFade;
  uint16_t transition_ms = 240U;
  SceneTextAlign title_align = SceneTextAlign::kTop;
  SceneTextAlign subtitle_align = SceneTextAlign::kBottom;
  SceneTextAlign symbol_align = SceneTextAlign::kCenter;
  const char* symbol_align_token = "";
  bool use_lgfx_text_overlay = false;
  bool lgfx_hard_mode = false;
  bool disable_lvgl_text = false;
  int16_t frame_dx = 0;
  int16_t frame_dy = 0;
  uint8_t frame_scale_pct = 100U;
  bool frame_split_layout = false;
  SceneScrollMode subtitle_scroll_mode = SceneScrollMode::kNone;
  uint16_t subtitle_scroll_speed_ms = 4200U;
  uint16_t subtitle_scroll_pause_ms = 900U;
  bool subtitle_scroll_loop = true;
  uint8_t text_glitch_pct = text_glitch_pct_;
  uint8_t text_size_pct = text_size_pct_;
  drivers::display::OverlayFontFace title_font_face = drivers::display::OverlayFontFace::kIbmBold24;
  drivers::display::OverlayFontFace subtitle_font_face = drivers::display::OverlayFontFace::kIbmBold16;
  drivers::display::OverlayFontFace symbol_font_face = drivers::display::OverlayFontFace::kBuiltinLarge;
  String demo_mode = "standard";
  uint8_t demo_particle_count = 4U;
  uint8_t demo_strobe_level = 65U;
  bool win_etape_fireworks = false;
  bool waveform_enabled = false;
  uint8_t waveform_sample_count = HardwareManager::kMicWaveformCapacity;
  uint8_t waveform_amplitude_pct = 95U;
  bool waveform_jitter = true;
  bool la_overlay_show_progress_ring = true;
  bool la_overlay_show_hourglass = true;
  bool la_overlay_show_caption = true;
  bool la_overlay_show_pitch_text = true;
  drivers::display::OverlayFontFace la_overlay_caption_font = drivers::display::OverlayFontFace::kBuiltinSmall;
  uint8_t la_overlay_caption_size = 1U;
  String la_overlay_caption = "Recherche d'accordance";
  bool la_overlay_meter_bottom_horizontal = true;
  bool la_overlay_hourglass_modern = true;
  LaBackgroundPreset la_bg_preset = LaBackgroundPreset::kLegacyHourglass;
  LaBackgroundSync la_bg_sync = LaBackgroundSync::kMicSmoothed;
  uint8_t la_bg_intensity_pct = 32U;
  bool la_hg_flip_on_timeout = true;
  uint32_t la_hg_reset_flip_ms = 10000U;
  int16_t la_hg_x_offset_px = 0;
  uint16_t la_hg_height_px = 0U;
  uint16_t la_hg_width_px = 0U;
  bool la_bargraph_blue_palette = false;
  uint16_t la_bargraph_peak_hold_ms = 320U;
  uint16_t la_bargraph_decay_per_s = 120U;
  bool la_waveform_audio_player_mode = false;
  uint16_t la_waveform_window_ms = 300U;
  bool warning_gyrophare_enabled = false;
  bool warning_gyrophare_disable_direct_fx = false;
  bool warning_lgfx_only = false;
  bool warning_siren = false;
  uint8_t warning_gyrophare_fps = 25U;
  uint16_t warning_gyrophare_speed_deg_per_sec = 180U;
  uint16_t warning_gyrophare_beam_width_deg = 70U;
  String warning_gyrophare_message = "SIGNAL ANORMAL";
  la_detection_scene_ = false;
  uint32_t bg_rgb = 0x07132AUL;
  uint32_t accent_rgb = 0x2A76FFUL;
  uint32_t text_rgb = 0xE8F1FFUL;
  const bool uson_proto_scene = (std::strcmp(scene_id, "SCENE_U_SON_PROTO") == 0);
  if (uson_proto_scene) {
    use_lgfx_text_overlay = fx_engine_.config().lgfx_backend;
    lgfx_hard_mode = true;
    disable_lvgl_text = use_lgfx_text_overlay;
    title_align = SceneTextAlign::kCenter;
    subtitle_align = SceneTextAlign::kBottom;
    symbol_align = SceneTextAlign::kTop;
    symbol_align_token = "top";
    title_font_face = drivers::display::OverlayFontFace::kIbmBold24;
    subtitle_font_face = drivers::display::OverlayFontFace::kIbmBold16;
    symbol_font_face = drivers::display::OverlayFontFace::kOrbitron28;
  }

  if (is_locked_scene) {
    title = "Module U-SON PROTO";
    subtitle = "VERIFICATION EN COURS";
    symbol = "";
    effect = SceneEffect::kGlitch;
    show_title = true;
    show_subtitle = true;
    show_symbol = false;
    waveform_enabled = true;
    waveform_sample_count = HardwareManager::kMicWaveformCapacity;
    waveform_amplitude_pct = 100U;
    waveform_jitter = true;
    demo_mode = "standard";
    bg_rgb = 0x07070FUL;
    accent_rgb = 0xFFB74EUL;
    text_rgb = 0xF6FBFFUL;
  } else if (std::strcmp(scene_id, "SCENE_BROKEN") == 0 || std::strcmp(scene_id, "SCENE_U_SON_PROTO") == 0) {
    title = "PROTO U-SON";
    subtitle = "Signal brouille / transmission active";
    symbol = "ALERT";
    effect = SceneEffect::kNone;
    bg_rgb = 0x06040BUL;
    accent_rgb = 0x6FD8FFUL;
    text_rgb = 0xF6FAFFUL;
    title_align = SceneTextAlign::kCenter;
    subtitle_align = SceneTextAlign::kBottom;
    symbol_align = SceneTextAlign::kTop;
    symbol_align_token = "top";
  } else if (std::strcmp(scene_id, "SCENE_TEST_LAB") == 0) {
    title = "MIRE COULEUR";
    subtitle = "NOIR | BLANC | ROUGE | VERT | BLEU | CYAN | MAGENTA | JAUNE";
    symbol = "";
    effect = SceneEffect::kNone;
    show_title = true;
    show_subtitle = true;
    show_symbol = false;
    bg_rgb = 0x000000UL;
    accent_rgb = 0x888888UL;
    text_rgb = 0xFFFFFFUL;
    transition = SceneTransition::kNone;
    transition_ms = 0U;
    waveform_enabled = false;
    demo_mode = "standard";
    demo_particle_count = 0U;
    demo_strobe_level = 0U;
    la_detection_scene_ = false;
  } else if (std::strcmp(scene_id, "SCENE_WARNING") == 0) {
    title = "ALERTE";
    subtitle = "Signal anormal";
    symbol = "WARN";
    effect = SceneEffect::kBlink;
    bg_rgb = 0x261209UL;
    accent_rgb = 0xFF9A4AUL;
    text_rgb = 0xFFFFFFUL;
    warning_lgfx_only = true;
    warning_siren = true;
    warning_gyrophare_enabled = true;
    warning_gyrophare_disable_direct_fx = true;
  } else if (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0 || std::strcmp(scene_id, "SCENE_SEARCH") == 0) {
    title = "recherche d'accordance";
    subtitle = "Balayage en cours";
    symbol = "";
    effect = SceneEffect::kWave;
    bg_rgb = 0x04141FUL;
    accent_rgb = 0x4ABEFFUL;
    text_rgb = 0xFFFFFFUL;
    if (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0) {
      bg_rgb = 0x000000UL;
      la_detection_scene_ = true;
      waveform_enabled = true;
      waveform_sample_count = HardwareManager::kMicWaveformCapacity;
      waveform_amplitude_pct = 100U;
      waveform_jitter = true;
      frame_split_layout = true;
      frame_dy = 8;
      use_lgfx_text_overlay = fx_engine_.config().lgfx_backend;
      disable_lvgl_text = use_lgfx_text_overlay;
      show_title = true;
      show_subtitle = true;
      show_symbol = false;
      symbol = "";
    }
  } else if (std::strcmp(scene_id, "SCENE_LEFOU_DETECTOR") == 0) {
    title = "DETECTEUR LEFOU";
    subtitle = "Analyse en cours";
    symbol = "AUDIO";
    effect = SceneEffect::kWave;
    bg_rgb = 0x071B1AUL;
    accent_rgb = 0x46E6C8UL;
    text_rgb = 0xE9FFF9UL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
  } else if (std::strcmp(scene_id, "SCENE_CAMERA_SCAN") == 0 || std::strcmp(scene_id, "SCENE_QR_DETECTOR") == 0) {
    title = "ZACUS QR VALIDATION";
    subtitle = "Scan du QR final";
    symbol = "QR";
    effect = SceneEffect::kNone;
    transition = SceneTransition::kFade;
    transition_ms = 180U;
    bg_rgb = 0x102040UL;
    accent_rgb = 0x5CA3FFUL;
    text_rgb = 0xF3F7FFUL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
    waveform_enabled = false;
  } else if (std::strcmp(scene_id, "SCENE_MEDIA_MANAGER") == 0) {
    title = "MEDIA MANAGER";
    subtitle = "PHOTO / MP3 / STORY";
    symbol = "MEDIA";
    effect = SceneEffect::kRadar;
    bg_rgb = 0x081A34UL;
    accent_rgb = 0x8BC4FFUL;
    text_rgb = 0xEAF6FFUL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
  } else if (std::strcmp(scene_id, "SCENE_PHOTO_MANAGER") == 0) {
    title = "PHOTO MANAGER";
    subtitle = "Capture JPEG";
    symbol = "PHOTO";
    effect = SceneEffect::kNone;
    bg_rgb = 0x0B1A2EUL;
    accent_rgb = 0x86CCFFUL;
    text_rgb = 0xEEF6FFUL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
  } else if (std::strcmp(scene_id, "SCENE_SIGNAL_SPIKE") == 0) {
    title = "PIC DE SIGNAL";
    subtitle = "Interference detectee";
    symbol = "ALERT";
    effect = SceneEffect::kWave;
    bg_rgb = 0x24090CUL;
    accent_rgb = 0xFF6A52UL;
    text_rgb = 0xFFF2EBUL;
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 ||
             std::strcmp(scene_id, "SCENE_REWARD") == 0 ||
             std::strcmp(scene_id, "SCENE_WINNER") == 0) {
    title = "VICTOIRE";
    symbol = "WIN";
    effect = (std::strcmp(scene_id, "SCENE_WINNER") == 0) ? SceneEffect::kNone : SceneEffect::kCelebrate;
    bg_rgb = 0x231038UL;
    accent_rgb = 0xF4CB4AUL;
    text_rgb = 0xFFF6C7UL;
    subtitle = (std::strcmp(scene_id, "SCENE_WINNER") == 0) ? "Mode Winner actif" : "Etape validee";
  } else if (std::strcmp(scene_id, "SCENE_FIREWORKS") == 0) {
    title = "FIREWORKS";
    subtitle = "Mode celebration";
    symbol = "WIN";
    effect = SceneEffect::kNone;
    bg_rgb = 0x120825UL;
    accent_rgb = 0xFFB65CUL;
    text_rgb = 0xFFF4E6UL;
    demo_mode = "fireworks";
  } else if (std::strcmp(scene_id, "SCENE_MP3_PLAYER") == 0) {
    title = "LECTEUR MP3";
    subtitle = "AmigaAMP";
    symbol = "PLAY";
    effect = SceneEffect::kNone;
    bg_rgb = 0x101A36UL;
    accent_rgb = 0x66B4FFUL;
    text_rgb = 0xF3F9FFUL;
    show_symbol = false;
  } else if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 ||
             std::strcmp(scene_id, "SCENE_WIN_ETAPE1") == 0 ||
             std::strcmp(scene_id, "SCENE_WIN_ETAPE2") == 0) {
    title = "BRAVO!";
    subtitle = audio_playing ? "Validation en cours..." : kWinEtapeWaitingSubtitle;
    symbol = "WIN";
    effect = (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0) ? SceneEffect::kCelebrate : SceneEffect::kNone;
    transition = SceneTransition::kFade;
    transition_ms = 220U;
    bg_rgb = 0x000022UL;
    accent_rgb = 0x00FFFFUL;
    text_rgb = 0xFFFFFFUL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
    win_etape_fireworks = false;
    subtitle_scroll_mode = SceneScrollMode::kNone;
  } else if (std::strcmp(scene_id, "SCENE_FINAL_WIN") == 0) {
    title = "FINAL WIN";
    subtitle = "Mission accomplie";
    symbol = "WIN";
    effect = SceneEffect::kCelebrate;
    bg_rgb = 0x1C0C2EUL;
    accent_rgb = 0xFFCC5CUL;
    text_rgb = 0xFFF7E4UL;
    show_title = true;
    show_subtitle = true;
    show_symbol = true;
  } else if (std::strcmp(scene_id, "SCENE_READY") == 0 || std::strcmp(scene_id, "SCENE_MEDIA_ARCHIVE") == 0) {
    title = "PRET";
    subtitle = "Scenario termine";
    symbol = "READY";
    effect = SceneEffect::kWave;
    bg_rgb = 0x0F2A12UL;
    accent_rgb = 0x6CD96BUL;
    text_rgb = 0xE8FFE7UL;
  }

  if (!parse_payload_this_frame && scene_status_.valid &&
      scene_status_.payload_crc == payload_crc &&
      std::strcmp(scene_status_.scene_id, scene_id) == 0) {
    title = scene_status_.title;
    subtitle = scene_status_.subtitle;
    symbol = scene_status_.symbol;
    show_title = scene_status_.show_title;
    show_subtitle = scene_status_.show_subtitle;
    show_symbol = scene_status_.show_symbol;
    effect = parseEffectToken(scene_status_.effect, effect, "scene status cache");
    effect_speed_ms = scene_status_.effect_speed_ms;
    transition = parseTransitionToken(scene_status_.transition, transition, "scene status cache");
    transition_ms = scene_status_.transition_ms;
    text_glitch_pct = scene_status_.text_glitch_pct;
    text_size_pct = scene_status_.text_size_pct;
    use_lgfx_text_overlay = (std::strcmp(scene_status_.text_backend, "lgfx_overlay") == 0);
    disable_lvgl_text = scene_status_.lvgl_text_disabled;
    bg_rgb = scene_status_.bg_rgb;
    accent_rgb = scene_status_.accent_rgb;
    text_rgb = scene_status_.text_rgb;
    title_font_face = overlay_title_font_face_;
    subtitle_font_face = overlay_subtitle_font_face_;
    symbol_font_face = overlay_symbol_font_face_;
  }

  if (static_state_changed) {
    resetSceneTimeline();
  }

  if (static_state_changed) {
    qr_rules_.clear();
  }

  if (parse_payload_this_frame && screen_payload_json != nullptr && screen_payload_json[0] != '\0') {
    DynamicJsonDocument document(4096);
    const DeserializationError error = deserializeJson(document, screen_payload_json);
    if (!error) {
      if (qr_scene && static_state_changed) {
        qr_rules_.configureFromPayload(document.as<JsonVariantConst>());
      }
      const char* payload_title = document["title"] | document["content"]["title"] | document["visual"]["title"] | "";
      const char* payload_subtitle =
          document["subtitle"] | document["content"]["subtitle"] | document["visual"]["subtitle"] | "";
      const char* payload_symbol = document["symbol"] | document["content"]["symbol"] | document["visual"]["symbol"] | "";
      const char* payload_effect = document["effect"] | document["visual"]["effect"] | document["content"]["effect"] | "";
      if (payload_title[0] != '\0') {
        title = payload_title;
      }
      if (payload_subtitle[0] != '\0') {
        subtitle = payload_subtitle;
      }
      if (payload_symbol[0] != '\0') {
        symbol = payload_symbol;
      }
      if (document["show_title"].is<bool>()) {
        show_title = document["show_title"].as<bool>();
      } else if (document["visual"]["show_title"].is<bool>()) {
        show_title = document["visual"]["show_title"].as<bool>();
      } else if (document["content"]["show_title"].is<bool>()) {
        show_title = document["content"]["show_title"].as<bool>();
      }
      if (document["text"]["show_title"].is<bool>()) {
        show_title = document["text"]["show_title"].as<bool>();
      }
      if (document["show_subtitle"].is<bool>()) {
        show_subtitle = document["show_subtitle"].as<bool>();
      } else if (document["visual"]["show_subtitle"].is<bool>()) {
        show_subtitle = document["visual"]["show_subtitle"].as<bool>();
      } else if (document["text"]["show_subtitle"].is<bool>()) {
        show_subtitle = document["text"]["show_subtitle"].as<bool>();
      }
      if (document["show_symbol"].is<bool>()) {
        show_symbol = document["show_symbol"].as<bool>();
      } else if (document["visual"]["show_symbol"].is<bool>()) {
        show_symbol = document["visual"]["show_symbol"].as<bool>();
      } else if (document["content"]["show_symbol"].is<bool>()) {
        show_symbol = document["content"]["show_symbol"].as<bool>();
      }
      if (document["text"]["show_symbol"].is<bool>()) {
        show_symbol = document["text"]["show_symbol"].as<bool>();
      }

      const char* title_case = document["text"]["title_case"] | "";
      const char* subtitle_case = document["text"]["subtitle_case"] | "";
      title = applyTextCase(title_case, title);
      subtitle = applyTextCase(subtitle_case, subtitle);
      title_align = parseAlignToken(document["text"]["title_align"] | "", title_align);
      subtitle_align = parseAlignToken(document["text"]["subtitle_align"] | "", subtitle_align);
      symbol_align_token = document["text"]["symbol_align"] | "";
      symbol_align = parseAlignToken(symbol_align_token, symbol_align);
      title_font_face = parseOverlayFontFace(document["text"]["title_font_face"] | "", title_font_face);
      subtitle_font_face = parseOverlayFontFace(document["text"]["subtitle_font_face"] | "", subtitle_font_face);
      symbol_font_face = parseOverlayFontFace(document["text"]["symbol_font_face"] | "", symbol_font_face);

      effect = parseEffectToken(payload_effect, effect, "scene payload effect");

      const char* payload_bg = document["theme"]["bg"] | document["visual"]["theme"]["bg"] | document["bg"] | "";
      const char* payload_accent =
          document["theme"]["accent"] | document["visual"]["theme"]["accent"] | document["accent"] | "";
      const char* payload_secondary =
          document["theme"]["text"] | document["visual"]["theme"]["text"] | document["text"] | "";
      parseHexRgb(payload_bg, &bg_rgb);
      parseHexRgb(payload_accent, &accent_rgb);
      parseHexRgb(payload_secondary, &text_rgb);

      const char* text_backend =
          document["render"]["text_backend"] | document["render"]["text"]["backend"] | document["text_backend"] | "";
      if (text_backend[0] != '\0') {
        if (std::strcmp(text_backend, "lgfx_overlay") == 0 || std::strcmp(text_backend, "lgfx") == 0) {
          use_lgfx_text_overlay = true;
        } else if (std::strcmp(text_backend, "lvgl") == 0) {
          use_lgfx_text_overlay = false;
        }
      }
      if (document["render"]["disable_lvgl_text"].is<bool>()) {
        disable_lvgl_text = document["render"]["disable_lvgl_text"].as<bool>();
      }
      if (document["render"]["lgfx_hard_mode"].is<bool>()) {
        lgfx_hard_mode = document["render"]["lgfx_hard_mode"].as<bool>();
      }
      if (document["render"]["wave"].is<bool>() && !document["render"]["wave"].as<bool>()) {
        subtitle_scroll_mode = SceneScrollMode::kNone;
      }
      if (document["render"]["warning"]["gyrophare"].is<JsonObjectConst>()) {
        const JsonObjectConst gyro_render = document["render"]["warning"]["gyrophare"].as<JsonObjectConst>();
        if (gyro_render["enabled"].is<bool>()) {
          warning_gyrophare_enabled = gyro_render["enabled"].as<bool>();
        }
        if (gyro_render["disable_direct_fx"].is<bool>()) {
          warning_gyrophare_disable_direct_fx = gyro_render["disable_direct_fx"].as<bool>();
        }
        if (gyro_render["fps"].is<unsigned int>()) {
          warning_gyrophare_fps = static_cast<uint8_t>(gyro_render["fps"].as<unsigned int>());
        }
        if (gyro_render["speed_deg_per_sec"].is<unsigned int>()) {
          warning_gyrophare_speed_deg_per_sec = static_cast<uint16_t>(gyro_render["speed_deg_per_sec"].as<unsigned int>());
        }
        if (gyro_render["beam_width_deg"].is<unsigned int>()) {
          warning_gyrophare_beam_width_deg = static_cast<uint16_t>(gyro_render["beam_width_deg"].as<unsigned int>());
        }
        const char* message = gyro_render["message"] | "";
        if (message[0] != '\0') {
          warning_gyrophare_message = message;
        }
      }
      if (document["render"]["warning"]["lgfx_only"].is<bool>()) {
        warning_lgfx_only = document["render"]["warning"]["lgfx_only"].as<bool>();
      }
      if (document["render"]["warning"]["siren"].is<bool>()) {
        warning_siren = document["render"]["warning"]["siren"].as<bool>();
      }
      if (document["render"]["la_detector"].is<JsonObjectConst>()) {
        const JsonObjectConst la_render = document["render"]["la_detector"].as<JsonObjectConst>();
        const char* caption = la_render["caption"] | "";
        if (caption[0] != '\0') {
          la_overlay_caption = caption;
        }
        if (la_render["show_progress_ring"].is<bool>()) {
          la_overlay_show_progress_ring = la_render["show_progress_ring"].as<bool>();
        }
        if (la_render["show_hourglass"].is<bool>()) {
          la_overlay_show_hourglass = la_render["show_hourglass"].as<bool>();
        }
        if (la_render["show_caption"].is<bool>()) {
          la_overlay_show_caption = la_render["show_caption"].as<bool>();
        }
        if (la_render["show_pitch_text"].is<bool>()) {
          la_overlay_show_pitch_text = la_render["show_pitch_text"].as<bool>();
        }
        const char* meter_layout = la_render["meter_layout"] | "";
        if (meter_layout[0] != '\0') {
          char token[24] = {0};
          std::strncpy(token, meter_layout, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          la_overlay_meter_bottom_horizontal = (std::strcmp(token, "bottom_horizontal") == 0);
        }
        const char* progress_layout = la_render["progress_layout"] | "";
        if (progress_layout[0] != '\0') {
          char token[16] = {0};
          std::strncpy(token, progress_layout, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          la_overlay_show_progress_ring = (std::strcmp(token, "ring") == 0);
        }
        const char* hourglass_style = la_render["hourglass_style"] | "";
        if (hourglass_style[0] != '\0') {
          char token[16] = {0};
          std::strncpy(token, hourglass_style, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          la_overlay_hourglass_modern = (std::strcmp(token, "modern") == 0);
        }
        const char* background_preset = la_render["background_preset"] | "";
        if (background_preset[0] != '\0') {
          char token[40] = {0};
          std::strncpy(token, background_preset, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          if (std::strcmp(token, "wirecube_rotozoom_subtle") == 0) {
            la_bg_preset = LaBackgroundPreset::kWirecubeRotozoomSubtle;
          } else if (std::strcmp(token, "hourglass_demoscene_ultra") == 0) {
            la_bg_preset = LaBackgroundPreset::kHourglassDemosceneUltra;
          } else {
            la_bg_preset = LaBackgroundPreset::kLegacyHourglass;
          }
        }
        const char* background_sync = la_render["background_sync"] | "";
        if (background_sync[0] != '\0') {
          char token[24] = {0};
          std::strncpy(token, background_sync, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          if (std::strcmp(token, "fixed") == 0) {
            la_bg_sync = LaBackgroundSync::kFixed;
          } else if (std::strcmp(token, "mic_direct") == 0) {
            la_bg_sync = LaBackgroundSync::kMicDirect;
          } else {
            la_bg_sync = LaBackgroundSync::kMicSmoothed;
          }
        }
        if (la_render["background_intensity_pct"].is<unsigned int>()) {
          la_bg_intensity_pct = static_cast<uint8_t>(la_render["background_intensity_pct"].as<unsigned int>());
        }
        if (la_render["flip_on_timeout"].is<bool>()) {
          la_hg_flip_on_timeout = la_render["flip_on_timeout"].as<bool>();
        }
        if (la_render["reset_flip_ms"].is<unsigned int>()) {
          la_hg_reset_flip_ms = la_render["reset_flip_ms"].as<unsigned int>();
        }
        if (la_render["hourglass_x_offset_px"].is<int>()) {
          la_hg_x_offset_px = static_cast<int16_t>(la_render["hourglass_x_offset_px"].as<int>());
        }
        if (la_render["hourglass_height_px"].is<unsigned int>()) {
          la_hg_height_px = static_cast<uint16_t>(la_render["hourglass_height_px"].as<unsigned int>());
        }
        if (la_render["hourglass_width_px"].is<unsigned int>()) {
          la_hg_width_px = static_cast<uint16_t>(la_render["hourglass_width_px"].as<unsigned int>());
        }
        const char* bargraph_palette = la_render["bargraph_palette"] | "";
        if (bargraph_palette[0] != '\0') {
          char token[16] = {0};
          std::strncpy(token, bargraph_palette, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          la_bargraph_blue_palette = (std::strcmp(token, "blue") == 0 ||
                                      std::strcmp(token, "blue_cyan") == 0 ||
                                      std::strcmp(token, "cyan") == 0);
        }
        if (la_render["bargraph_peak_hold_ms"].is<unsigned int>()) {
          la_bargraph_peak_hold_ms = static_cast<uint16_t>(la_render["bargraph_peak_hold_ms"].as<unsigned int>());
        }
        if (la_render["bargraph_decay_per_s"].is<unsigned int>()) {
          la_bargraph_decay_per_s = static_cast<uint16_t>(la_render["bargraph_decay_per_s"].as<unsigned int>());
        }
        const char* waveform_mode = la_render["waveform_mode"] | "";
        if (waveform_mode[0] != '\0') {
          char token[24] = {0};
          std::strncpy(token, waveform_mode, sizeof(token) - 1U);
          for (size_t i = 0U; token[i] != '\0'; ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
          }
          la_waveform_audio_player_mode = (std::strcmp(token, "audio_player") == 0 ||
                                           std::strcmp(token, "audio") == 0 ||
                                           std::strcmp(token, "player") == 0);
        }
        if (la_render["waveform_window_ms"].is<unsigned int>()) {
          la_waveform_window_ms = static_cast<uint16_t>(la_render["waveform_window_ms"].as<unsigned int>());
        }
        if (la_render["caption_font"].is<unsigned int>()) {
          const uint8_t legacy_font = static_cast<uint8_t>(la_render["caption_font"].as<unsigned int>());
          if (legacy_font <= 1U) {
            la_overlay_caption_font = drivers::display::OverlayFontFace::kBuiltinSmall;
          } else if (legacy_font >= 4U) {
            la_overlay_caption_font = drivers::display::OverlayFontFace::kBuiltinLarge;
          } else {
            la_overlay_caption_font = drivers::display::OverlayFontFace::kBuiltinMedium;
          }
        } else {
          const char* caption_font = la_render["caption_font_face"] | "";
          la_overlay_caption_font = parseOverlayFontFace(caption_font, la_overlay_caption_font);
        }
        if (la_render["caption_size"].is<unsigned int>()) {
          la_overlay_caption_size = static_cast<uint8_t>(la_render["caption_size"].as<unsigned int>());
        }
      }

      if (document["effect_speed_ms"].is<unsigned int>()) {
        effect_speed_ms = static_cast<uint16_t>(document["effect_speed_ms"].as<unsigned int>());
      } else if (document["visual"]["effect_speed_ms"].is<unsigned int>()) {
        effect_speed_ms = static_cast<uint16_t>(document["visual"]["effect_speed_ms"].as<unsigned int>());
      }

      const char* transition_token =
          document["transition"]["effect"] | document["transition"]["type"] | document["visual"]["transition"] | "";
      transition = parseTransitionToken(transition_token, transition, "scene payload transition");
      if (document["transition"]["duration_ms"].is<unsigned int>()) {
        transition_ms = static_cast<uint16_t>(document["transition"]["duration_ms"].as<unsigned int>());
      } else if (document["transition"]["ms"].is<unsigned int>()) {
        transition_ms = static_cast<uint16_t>(document["transition"]["ms"].as<unsigned int>());
      } else if (document["visual"]["transition_ms"].is<unsigned int>()) {
        transition_ms = static_cast<uint16_t>(document["visual"]["transition_ms"].as<unsigned int>());
      }

      const char* framing_preset = document["framing"]["preset"] | "";
      if (std::strcmp(framing_preset, "focus_top") == 0) {
        frame_dy -= 18;
      } else if (std::strcmp(framing_preset, "focus_bottom") == 0) {
        frame_dy += 20;
      } else if (std::strcmp(framing_preset, "split") == 0) {
        frame_split_layout = true;
      }
      if (document["framing"]["x_offset"].is<int>()) {
        frame_dx = static_cast<int16_t>(document["framing"]["x_offset"].as<int>());
      }
      if (document["framing"]["y_offset"].is<int>()) {
        frame_dy = static_cast<int16_t>(frame_dy + document["framing"]["y_offset"].as<int>());
      }
      if (document["framing"]["scale_pct"].is<unsigned int>()) {
        frame_scale_pct = static_cast<uint8_t>(document["framing"]["scale_pct"].as<unsigned int>());
      }
      if (frame_scale_pct < 60U) {
        frame_scale_pct = 60U;
      } else if (frame_scale_pct > 140U) {
        frame_scale_pct = 140U;
      }

      const char* scroll_mode = document["scroll"]["mode"] | "";
      if (std::strcmp(scroll_mode, "marquee") == 0 || std::strcmp(scroll_mode, "ticker") == 0 ||
          std::strcmp(scroll_mode, "crawl") == 0) {
        subtitle_scroll_mode = SceneScrollMode::kMarquee;
      } else {
        subtitle_scroll_mode = SceneScrollMode::kNone;
      }
      if (document["scroll"]["speed_ms"].is<unsigned int>()) {
        subtitle_scroll_speed_ms = static_cast<uint16_t>(document["scroll"]["speed_ms"].as<unsigned int>());
      }
      if (subtitle_scroll_speed_ms < 600U) {
        subtitle_scroll_speed_ms = 600U;
      }
      if (document["scroll"]["pause_ms"].is<unsigned int>()) {
        subtitle_scroll_pause_ms = static_cast<uint16_t>(document["scroll"]["pause_ms"].as<unsigned int>());
      }
      if (document["scroll"]["loop"].is<bool>()) {
        subtitle_scroll_loop = document["scroll"]["loop"].as<bool>();
      }

      if (document["text"]["glitch"].is<unsigned int>()) {
        text_glitch_pct = static_cast<uint8_t>(document["text"]["glitch"].as<unsigned int>());
      } else if (document["text"]["glitch_pct"].is<unsigned int>()) {
        text_glitch_pct = static_cast<uint8_t>(document["text"]["glitch_pct"].as<unsigned int>());
      } else if (document["text_glitch"].is<unsigned int>()) {
        text_glitch_pct = static_cast<uint8_t>(document["text_glitch"].as<unsigned int>());
      }
      if (text_glitch_pct > 100U) {
        text_glitch_pct = 100U;
      }

      if (document["text"]["size"].is<unsigned int>()) {
        text_size_pct = static_cast<uint8_t>(document["text"]["size"].as<unsigned int>());
      } else if (document["text"]["size_pct"].is<unsigned int>()) {
        text_size_pct = static_cast<uint8_t>(document["text"]["size_pct"].as<unsigned int>());
      } else if (document["text_size"].is<unsigned int>()) {
        text_size_pct = static_cast<uint8_t>(document["text_size"].as<unsigned int>());
      }
      if (text_size_pct > 100U) {
        text_size_pct = 100U;
      }

      if (document["demo"]["particle_count"].is<unsigned int>()) {
        demo_particle_count = static_cast<uint8_t>(document["demo"]["particle_count"].as<unsigned int>());
      }
      if (demo_particle_count > 4U) {
        demo_particle_count = 4U;
      }
      const char* parsed_demo_mode = document["demo"]["mode"] | "";
      if (parsed_demo_mode[0] != '\0') {
        demo_mode = parsed_demo_mode;
        demo_mode.toLowerCase();
      }
      if (document["demo"]["strobe_level"].is<unsigned int>()) {
        demo_strobe_level = static_cast<uint8_t>(document["demo"]["strobe_level"].as<unsigned int>());
      }
      if (demo_strobe_level > 100U) {
        demo_strobe_level = 100U;
      }
      if (document["visual"]["waveform"].is<JsonObjectConst>()) {
        const JsonObjectConst waveform = document["visual"]["waveform"].as<JsonObjectConst>();
        if (waveform["enabled"].is<bool>()) {
          waveform_enabled = waveform["enabled"].as<bool>();
        }
        if (waveform["sample_count"].is<unsigned int>()) {
          waveform_sample_count = static_cast<uint8_t>(waveform["sample_count"].as<unsigned int>());
        }
        if (waveform["amplitude_pct"].is<unsigned int>()) {
          waveform_amplitude_pct = static_cast<uint8_t>(waveform["amplitude_pct"].as<unsigned int>());
        }
        if (waveform["jitter"].is<bool>()) {
          waveform_jitter = waveform["jitter"].as<bool>();
        }
      }
      if (document["waveform"].is<JsonObjectConst>()) {
        const JsonObjectConst waveform = document["waveform"].as<JsonObjectConst>();
        if (waveform["enabled"].is<bool>()) {
          waveform_enabled = waveform["enabled"].as<bool>();
        }
        if (waveform["sample_count"].is<unsigned int>()) {
          waveform_sample_count = static_cast<uint8_t>(waveform["sample_count"].as<unsigned int>());
        }
        if (waveform["amplitude_pct"].is<unsigned int>()) {
          waveform_amplitude_pct = static_cast<uint8_t>(waveform["amplitude_pct"].as<unsigned int>());
        }
        if (waveform["jitter"].is<bool>()) {
          waveform_jitter = waveform["jitter"].as<bool>();
        }
      }

      JsonArrayConst timeline_nodes;
      bool timeline_loop = true;
      uint16_t timeline_duration_override = 0U;
      if (document["timeline"].is<JsonArrayConst>()) {
        timeline_nodes = document["timeline"].as<JsonArrayConst>();
      } else if (document["timeline"].is<JsonObjectConst>()) {
        JsonObjectConst timeline_obj = document["timeline"].as<JsonObjectConst>();
        if (timeline_obj["keyframes"].is<JsonArrayConst>()) {
          timeline_nodes = timeline_obj["keyframes"].as<JsonArrayConst>();
        } else if (timeline_obj["frames"].is<JsonArrayConst>()) {
          timeline_nodes = timeline_obj["frames"].as<JsonArrayConst>();
        }
        if (timeline_obj["loop"].is<bool>()) {
          timeline_loop = timeline_obj["loop"].as<bool>();
        }
        if (timeline_obj["duration_ms"].is<unsigned int>()) {
          timeline_duration_override = static_cast<uint16_t>(timeline_obj["duration_ms"].as<unsigned int>());
        }
      } else if (document["visual"]["timeline"].is<JsonArrayConst>()) {
        timeline_nodes = document["visual"]["timeline"].as<JsonArrayConst>();
      } else if (document["visual"]["timeline"].is<JsonObjectConst>()) {
        JsonObjectConst timeline_obj = document["visual"]["timeline"].as<JsonObjectConst>();
        if (timeline_obj["keyframes"].is<JsonArrayConst>()) {
          timeline_nodes = timeline_obj["keyframes"].as<JsonArrayConst>();
        } else if (timeline_obj["frames"].is<JsonArrayConst>()) {
          timeline_nodes = timeline_obj["frames"].as<JsonArrayConst>();
        }
        if (timeline_obj["loop"].is<bool>()) {
          timeline_loop = timeline_obj["loop"].as<bool>();
        }
        if (timeline_obj["duration_ms"].is<unsigned int>()) {
          timeline_duration_override = static_cast<uint16_t>(timeline_obj["duration_ms"].as<unsigned int>());
        }
      }
      if (!timeline_nodes.isNull() && timeline_nodes.size() > 0U) {
        SceneTimelineKeyframe base;
        base.at_ms = 0U;
        base.effect = effect;
        base.speed_ms = effect_speed_ms;
        base.bg_rgb = bg_rgb;
        base.accent_rgb = accent_rgb;
        base.text_rgb = text_rgb;
        timeline_keyframes_[0] = base;
        timeline_keyframe_count_ = 1U;
        SceneTimelineKeyframe previous = base;
        uint16_t previous_at_ms = 0U;

        for (JsonVariantConst frame_node : timeline_nodes) {
          if (timeline_keyframe_count_ >= kMaxTimelineKeyframes) {
            break;
          }
          if (!frame_node.is<JsonObjectConst>()) {
            continue;
          }
          JsonObjectConst frame = frame_node.as<JsonObjectConst>();
          SceneTimelineKeyframe candidate = previous;
          uint16_t at_ms = static_cast<uint16_t>(previous_at_ms + 420U);
          if (frame["at_ms"].is<unsigned int>()) {
            at_ms = static_cast<uint16_t>(frame["at_ms"].as<unsigned int>());
          } else if (frame["time_ms"].is<unsigned int>()) {
            at_ms = static_cast<uint16_t>(frame["time_ms"].as<unsigned int>());
          } else if (frame["t"].is<unsigned int>()) {
            at_ms = static_cast<uint16_t>(frame["t"].as<unsigned int>());
          }
          if (at_ms < previous_at_ms) {
            at_ms = previous_at_ms;
          }
          candidate.at_ms = at_ms;
          candidate.effect = parseEffectToken(frame["effect"] | frame["fx"] | "", candidate.effect, "timeline frame effect");

          if (frame["speed_ms"].is<unsigned int>()) {
            candidate.speed_ms = static_cast<uint16_t>(frame["speed_ms"].as<unsigned int>());
          } else if (frame["effect_speed_ms"].is<unsigned int>()) {
            candidate.speed_ms = static_cast<uint16_t>(frame["effect_speed_ms"].as<unsigned int>());
          } else if (frame["speed"].is<unsigned int>()) {
            candidate.speed_ms = static_cast<uint16_t>(frame["speed"].as<unsigned int>());
          }

          const char* frame_bg = frame["theme"]["bg"] | frame["bg"] | "";
          const char* frame_accent = frame["theme"]["accent"] | frame["accent"] | "";
          const char* frame_text = frame["theme"]["text"] | frame["text"] | "";
          parseHexRgb(frame_bg, &candidate.bg_rgb);
          parseHexRgb(frame_accent, &candidate.accent_rgb);
          parseHexRgb(frame_text, &candidate.text_rgb);

          if (timeline_keyframe_count_ == 1U && candidate.at_ms == 0U) {
            timeline_keyframes_[0] = candidate;
          } else {
            timeline_keyframes_[timeline_keyframe_count_] = candidate;
            ++timeline_keyframe_count_;
          }
          previous = candidate;
          previous_at_ms = candidate.at_ms;
        }
        if (timeline_keyframe_count_ > 1U) {
          timeline_duration_ms_ = timeline_keyframes_[timeline_keyframe_count_ - 1U].at_ms;
          if (timeline_duration_override > timeline_duration_ms_) {
            timeline_duration_ms_ = timeline_duration_override;
          }
          if (timeline_duration_ms_ < 100U) {
            timeline_duration_ms_ = 100U;
          }
          timeline_loop_ = timeline_loop;
        } else {
          resetSceneTimeline();
        }
      }
    } else {
      UI_LOGD("invalid scene payload (%s)", error.c_str());
    }
  }

  if (is_locked_scene && effect == SceneEffect::kGlitch && effect_speed_ms == 0U) {
    const uint32_t speed_entropy =
        mixNoise(static_cast<uint32_t>(lv_tick_get()), reinterpret_cast<uintptr_t>(this) ^ 0xA5A37UL);
    effect_speed_ms = static_cast<uint16_t>(80U + (speed_entropy % 141U));
  }
  if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 && effect == SceneEffect::kNone) {
    effect = SceneEffect::kCelebrate;
    if (effect_speed_ms == 0U) {
      effect_speed_ms = 320U;
    }
  }

  if (waveform_sample_count == 0U) {
    waveform_sample_count = HardwareManager::kMicWaveformCapacity;
  } else if (waveform_sample_count > HardwareManager::kMicWaveformCapacity) {
    waveform_sample_count = HardwareManager::kMicWaveformCapacity;
  }
  if (waveform_sample_count < 2U) {
    waveform_sample_count = 2U;
  }
  if (waveform_amplitude_pct > 100U) {
    waveform_amplitude_pct = 100U;
  }
  if (la_overlay_caption_size < 1U) {
    la_overlay_caption_size = 1U;
  } else if (la_overlay_caption_size > 3U) {
    la_overlay_caption_size = 3U;
  }
  if (la_bg_intensity_pct > 100U) {
    la_bg_intensity_pct = 100U;
  }
  if (la_hg_reset_flip_ms < 500U) {
    la_hg_reset_flip_ms = 500U;
  } else if (la_hg_reset_flip_ms > 20000U) {
    la_hg_reset_flip_ms = 20000U;
  }
  const int16_t width_px = std::max<int16_t>(1, activeDisplayWidth());
  const int16_t height_px = std::max<int16_t>(1, activeDisplayHeight());
  if (la_hg_x_offset_px < static_cast<int16_t>(-width_px)) {
    la_hg_x_offset_px = static_cast<int16_t>(-width_px);
  } else if (la_hg_x_offset_px > width_px) {
    la_hg_x_offset_px = width_px;
  }
  if (la_hg_height_px > 0U) {
    const uint16_t max_h = static_cast<uint16_t>(std::max<int16_t>(72, height_px - 12));
    if (la_hg_height_px < 72U) {
      la_hg_height_px = 72U;
    } else if (la_hg_height_px > max_h) {
      la_hg_height_px = max_h;
    }
  }
  if (la_hg_width_px > 0U) {
    const uint16_t max_w = static_cast<uint16_t>(std::max<int16_t>(36, width_px - 8));
    if (la_hg_width_px < 36U) {
      la_hg_width_px = 36U;
    } else if (la_hg_width_px > max_w) {
      la_hg_width_px = max_w;
    }
  }
  if (la_bargraph_peak_hold_ms < 120U) {
    la_bargraph_peak_hold_ms = 120U;
  } else if (la_bargraph_peak_hold_ms > 3500U) {
    la_bargraph_peak_hold_ms = 3500U;
  }
  if (la_bargraph_decay_per_s < 10U) {
    la_bargraph_decay_per_s = 10U;
  } else if (la_bargraph_decay_per_s > 600U) {
    la_bargraph_decay_per_s = 600U;
  }
  if (la_waveform_window_ms < 80U) {
    la_waveform_window_ms = 80U;
  } else if (la_waveform_window_ms > 1200U) {
    la_waveform_window_ms = 1200U;
  }
  if (warning_gyrophare_fps < 10U) {
    warning_gyrophare_fps = 10U;
  } else if (warning_gyrophare_fps > 60U) {
    warning_gyrophare_fps = 60U;
  }
  if (warning_gyrophare_speed_deg_per_sec < 30U) {
    warning_gyrophare_speed_deg_per_sec = 30U;
  } else if (warning_gyrophare_speed_deg_per_sec > 600U) {
    warning_gyrophare_speed_deg_per_sec = 600U;
  }
  if (warning_gyrophare_beam_width_deg < 20U) {
    warning_gyrophare_beam_width_deg = 20U;
  } else if (warning_gyrophare_beam_width_deg > 120U) {
    warning_gyrophare_beam_width_deg = 120U;
  }
  if (la_overlay_caption.isEmpty()) {
    la_overlay_caption = "Recherche d'accordance";
  }
  if (warning_lgfx_only && std::strcmp(scene_id, "SCENE_WARNING") == 0) {
    use_lgfx_text_overlay = fx_engine_.config().lgfx_backend;
    disable_lvgl_text = use_lgfx_text_overlay;
    warning_gyrophare_enabled = false;
    warning_gyrophare_disable_direct_fx = true;
  }
  const bool mic_needed = la_detection_scene_ || waveform_enabled;
  if (hardware_ != nullptr) {
    // Runtime mic ownership is centralized in main.cpp resource policy.
    // UI can request ON for waveform scenes but must not force OFF for other scenes (e.g. U_SON proto).
    if (mic_needed) {
      hardware_->setMicRuntimeEnabled(true);
    }
  }
  configureWaveformOverlay((waveform_snapshot_ref_ != nullptr) ? waveform_snapshot_ref_
                                                               : (waveform_snapshot_valid_ ? &waveform_snapshot_
                                                                                           : nullptr),
                           waveform_enabled,
                           waveform_sample_count,
                          waveform_amplitude_pct,
                          waveform_jitter);
  if (scene_runtime_lgfx_lock_ && scene_id != nullptr && std::strncmp(scene_id, "SCENE_", 6U) == 0) {
    use_lgfx_text_overlay = true;
    disable_lvgl_text = true;
    la_overlay_caption_font = drivers::display::OverlayFontFace::kIbmBold16;
  }
  if (win_etape_intro_scene) {
    if (subtitle.length() == 0U) {
      subtitle = kWinEtapeWaitingSubtitle;
    }
    if (audio_playing) {
      subtitle = "Validation en cours...";
    }
  }
  if (use_lgfx_text_overlay && !fx_engine_.config().lgfx_backend) {
    use_lgfx_text_overlay = false;
  }
  if (use_lgfx_text_overlay) {
    disable_lvgl_text = true;
    subtitle_scroll_mode = SceneScrollMode::kNone;
    effect = SceneEffect::kNone;
  }
  const bool test_lab_lgfx_scroller = test_lab_scene;
  const bool warning_blocks_direct_fx =
      warning_gyrophare_enabled && warning_gyrophare_disable_direct_fx && (std::strcmp(scene_id, "SCENE_WARNING") == 0);
  const bool wants_direct_fx = (direct_fx_scene_runtime && !warning_blocks_direct_fx) || test_lab_lgfx_scroller;
  const bool can_use_direct_fx_backend = fx_engine_.config().lgfx_backend;
  const uint32_t now_tick_ms = millis();
  const bool fx_retry_allowed = (fx_rearm_retry_after_ms_ == 0U) ||
                                (static_cast<int32_t>(now_tick_ms - fx_rearm_retry_after_ms_) >= 0);
  const bool should_rearm_direct_fx =
      wants_direct_fx && can_use_direct_fx_backend && fx_retry_allowed &&
      (static_state_changed || !direct_fx_scene_active_ || !fx_engine_.enabled());
  if (should_rearm_direct_fx) {
    direct_fx_scene_active_ = can_use_direct_fx_backend;
    if (direct_fx_scene_active_) {
      armDirectFxScene(scene_id, test_lab_lgfx_scroller, title.c_str(), subtitle.c_str());
    }
  } else if (static_state_changed && !win_etape_intro_scene) {
    direct_fx_scene_active_ = false;
    if (!intro_active_) {
      fx_engine_.setEnabled(false);
      fx_engine_.setScrollerCentered(false);
    }
  }

  if (static_state_changed) {
    fx_rearm_retry_after_ms_ = 0U;
    scene_runtime_started_ms_ = millis();
    overlay_draw_ok_count_ = 0U;
    overlay_draw_fail_count_ = 0U;
    overlay_startwrite_fail_count_ = 0U;
    overlay_skip_busy_count_ = 0U;
    overlay_recovery_frames_ = 0U;
    win_etape_credits_loaded_ = false;
    win_etape_credits_count_ = 0U;
    std::memset(win_etape_credits_lines_, 0, sizeof(win_etape_credits_lines_));
    stopSceneAnimations();
    scene_use_lgfx_text_overlay_ = use_lgfx_text_overlay;
    scene_lgfx_hard_mode_ = lgfx_hard_mode;
    scene_disable_lvgl_text_ = disable_lvgl_text && scene_use_lgfx_text_overlay_;
    overlay_title_align_ = title_align;
    overlay_subtitle_align_ = subtitle_align;
    overlay_symbol_align_ = symbol_align;
    overlay_title_font_face_ = title_font_face;
    overlay_subtitle_font_face_ = subtitle_font_face;
    overlay_symbol_font_face_ = symbol_font_face;
    la_overlay_show_progress_ring_ = la_overlay_show_progress_ring;
    la_overlay_show_hourglass_ = la_overlay_show_hourglass;
    la_overlay_show_caption_ = la_overlay_show_caption;
    la_overlay_show_pitch_text_ = la_overlay_show_pitch_text;
    la_overlay_meter_bottom_horizontal_ = la_overlay_meter_bottom_horizontal;
    la_overlay_hourglass_modern_ = la_overlay_hourglass_modern;
    la_bg_preset_ = la_bg_preset;
    la_bg_sync_ = la_bg_sync;
    la_bg_intensity_pct_ = la_bg_intensity_pct;
    la_hg_flip_on_timeout_ = la_hg_flip_on_timeout;
    la_hg_flip_duration_ms_ = la_hg_reset_flip_ms;
    la_hg_x_offset_px_ = la_hg_x_offset_px;
    la_hg_target_height_px_ = la_hg_height_px;
    la_hg_target_width_px_ = la_hg_width_px;
    la_bargraph_blue_palette_ = la_bargraph_blue_palette;
    la_bargraph_peak_hold_ms_ = la_bargraph_peak_hold_ms;
    la_bargraph_decay_per_s_ = la_bargraph_decay_per_s;
    la_waveform_audio_player_mode_ = la_waveform_audio_player_mode;
    la_waveform_window_ms_ = la_waveform_window_ms;
    la_bg_mic_lpf_ = 0.15f;
    la_bg_last_ms_ = 0U;
    la_overlay_caption_font_ = la_overlay_caption_font;
    la_overlay_caption_size_ = la_overlay_caption_size;
    copyTextSafe(la_overlay_caption_, sizeof(la_overlay_caption_), la_overlay_caption.c_str());
    warning_gyrophare_enabled_ = warning_gyrophare_enabled && (std::strcmp(scene_id, "SCENE_WARNING") == 0);
    warning_gyrophare_disable_direct_fx_ = warning_gyrophare_disable_direct_fx;
    warning_lgfx_only_ = warning_lgfx_only && (std::strcmp(scene_id, "SCENE_WARNING") == 0);
    warning_siren_enabled_ = warning_siren && (std::strcmp(scene_id, "SCENE_WARNING") == 0);
    if (warning_lgfx_only_) {
      warning_gyrophare_enabled_ = false;
    }
    warning_gyrophare_fps_ = warning_gyrophare_fps;
    warning_gyrophare_speed_deg_per_sec_ = warning_gyrophare_speed_deg_per_sec;
    warning_gyrophare_beam_width_deg_ = warning_gyrophare_beam_width_deg;
    copyTextSafe(warning_gyrophare_message_, sizeof(warning_gyrophare_message_), warning_gyrophare_message.c_str());
    warning_gyrophare_.destroy();
    if (warning_gyrophare_enabled_ && scene_root_ != nullptr) {
      ui::effects::SceneGyrophareConfig gyro_config = {};
      gyro_config.fps = warning_gyrophare_fps_;
      gyro_config.speed_deg_per_sec = warning_gyrophare_speed_deg_per_sec_;
      gyro_config.beam_width_deg = warning_gyrophare_beam_width_deg_;
      gyro_config.message = warning_gyrophare_message_;
      const bool created = warning_gyrophare_.create(scene_root_, activeDisplayWidth(), activeDisplayHeight(), gyro_config);
      if (!created) {
        warning_gyrophare_enabled_ = false;
        warning_gyrophare_disable_direct_fx_ = false;
      }
    }
    const bool show_base_scene_fx = (!test_lab_scene && effect != SceneEffect::kNone && !scene_use_lgfx_text_overlay_);
    setBaseSceneFxVisible(show_base_scene_fx);
    text_glitch_pct_ = text_glitch_pct;
    text_size_pct_ = text_size_pct;
    demo_particle_count_ = demo_particle_count;
    demo_strobe_level_ = demo_strobe_level;
    if (demo_mode == "cinematic") {
      if (demo_particle_count_ > 2U) {
        demo_particle_count_ = 2U;
      }
      if (transition_ms < 300U) {
        transition_ms = 300U;
      }
    } else if (demo_mode == "arcade") {
      if (transition_ms < 140U) {
        transition_ms = 140U;
      }
      if (effect_speed_ms < 240U && effect_speed_ms != 0U) {
        effect_speed_ms = 240U;
      }
    } else if (demo_mode == "fireworks") {
      if (demo_particle_count_ < 3U) {
        demo_particle_count_ = 3U;
      }
      if (demo_strobe_level_ < 82U) {
        demo_strobe_level_ = 82U;
      }
      if (effect_speed_ms == 0U || effect_speed_ms > 460U) {
        effect_speed_ms = 300U;
      }
      if (transition_ms < 200U) {
        transition_ms = 200U;
      }
    }
    current_effect_ = effect;
    effect_speed_ms_ = effect_speed_ms;
    if (effect_speed_ms_ == 0U && demo_mode == "arcade") {
      effect_speed_ms_ = 240U;
    }
    win_etape_fireworks_mode_ = win_etape_fireworks;
    applyThemeColors(bg_rgb, accent_rgb, text_rgb);
    const String title_ui = asciiFallbackForUiText(title.c_str());
    const String subtitle_ui = asciiFallbackForUiText(subtitle.c_str());
    const bool lvgl_text_enabled = !scene_disable_lvgl_text_;
    if (lvgl_text_enabled) {
      lv_label_set_text(scene_title_label_, title_ui.c_str());
      lv_label_set_text(scene_subtitle_label_, subtitle_ui.c_str());
      const char* symbol_glyph = mapSymbolToken(symbol.c_str());
      lv_label_set_text(scene_symbol_label_, (symbol_glyph != nullptr) ? symbol_glyph : LV_SYMBOL_PLAY);
      if (show_title) {
        lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
      }
      if (show_symbol) {
        lv_obj_clear_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
      }
      if (show_subtitle && subtitle.length() > 0U) {
        lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
      }
      const lv_font_t* title_font = UiFonts::fontBold24();
      if (text_size_pct_ <= 20U) {
        title_font = UiFonts::fontBold12();
      } else if (text_size_pct_ <= 45U) {
        title_font = UiFonts::fontBold16();
      } else if (text_size_pct_ <= 70U) {
        title_font = UiFonts::fontBold20();
      }
      if (title_font == nullptr) {
        title_font = &lv_font_montserrat_14;
      }
      const lv_font_t* subtitle_font = UiFonts::fontItalic12();
      if (uson_proto_scene) {
        if (text_size_pct_ <= 20U) {
          subtitle_font = UiFonts::fontBold12();
        } else if (text_size_pct_ <= 60U) {
          subtitle_font = UiFonts::fontBold16();
        } else {
          subtitle_font = UiFonts::fontBold20();
        }
      }
      if (subtitle_font == nullptr) {
        subtitle_font = &lv_font_montserrat_14;
      }
      const lv_font_t* symbol_font = UiFonts::fontTitle();
      if (symbol_font == nullptr) {
        symbol_font = &lv_font_montserrat_14;
      }
      if (scene_title_label_ != nullptr) {
        lv_obj_set_style_text_font(scene_title_label_, title_font, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_color(scene_title_label_, lv_color_hex(text_rgb), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_bg_opa(scene_title_label_, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_left(scene_title_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_right(scene_title_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_top(scene_title_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_bottom(scene_title_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_transform_angle(scene_title_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
      }
      if (scene_subtitle_label_ != nullptr) {
        lv_obj_set_style_text_font(scene_subtitle_label_, subtitle_font, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(text_rgb), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_bg_opa(scene_subtitle_label_, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_left(scene_subtitle_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_right(scene_subtitle_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_top(scene_subtitle_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_bottom(scene_subtitle_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_transform_angle(scene_subtitle_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_width(scene_subtitle_label_, activeDisplayWidth() - 32);
        lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_ANY);
      }
      if (scene_symbol_label_ != nullptr) {
        lv_obj_set_style_text_font(scene_symbol_label_, symbol_font, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_color(scene_symbol_label_, lv_color_hex(text_rgb), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_opa(scene_symbol_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_opa(scene_symbol_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_transform_angle(scene_symbol_label_, 0, LV_PART_MAIN | LV_STATE_ANY);
      }
      applyTextLayout(title_align, subtitle_align, symbol_align);
      if (scene_title_label_ != nullptr && !lv_obj_has_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(scene_title_label_);
        lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_transform_angle(scene_title_label_, 0, LV_PART_MAIN);
      }
      if (scene_subtitle_label_ != nullptr && !lv_obj_has_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(scene_subtitle_label_);
        lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_transform_angle(scene_subtitle_label_, 0, LV_PART_MAIN);
      }
    } else {
      if (scene_title_label_ != nullptr) {
        lv_obj_add_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
      }
      if (scene_subtitle_label_ != nullptr) {
        lv_obj_add_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
      }
      if (scene_symbol_label_ != nullptr) {
        lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
      }
      if (page_label_ != nullptr) {
        lv_obj_add_flag(page_label_, LV_OBJ_FLAG_HIDDEN);
      }
    }
    applySceneFraming(frame_dx, frame_dy, frame_scale_pct, frame_split_layout);
    if (lvgl_text_enabled) {
      applySubtitleScroll(subtitle_scroll_mode, subtitle_scroll_speed_ms, subtitle_scroll_pause_ms, subtitle_scroll_loop);
    } else {
      applySubtitleScroll(SceneScrollMode::kNone, subtitle_scroll_speed_ms, subtitle_scroll_pause_ms, false);
    }
    for (lv_obj_t* particle : scene_particles_) {
      lv_obj_set_style_bg_color(particle, lv_color_hex(text_rgb), LV_PART_MAIN);
    }
    if (test_lab_scene) {
      constexpr uint32_t kTestLabPaletteInputRgb[] = {
          0x000000UL,  // noir
          0xFFFFFFUL,  // blanc
          0xFF0000UL,  // rouge
          0x00FF00UL,  // vert
          0x0000FFUL,  // bleu
          0x00FFFFUL,  // cyan
          0xFF00FFUL,  // magenta
          0xFFFF00UL,  // jaune
      };
      const uint8_t palette_count = static_cast<uint8_t>(sizeof(kTestLabPaletteInputRgb) / sizeof(kTestLabPaletteInputRgb[0]));
      const int16_t width_px = activeDisplayWidth();
      const int16_t height_px = activeDisplayHeight();
      for (uint8_t index = 0U; index < kCracktroBarCount; ++index) {
        lv_obj_t* bar = scene_cracktro_bars_[index];
        if (bar == nullptr) {
          continue;
        }
        if (index >= palette_count) {
          lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
          continue;
        }
        const int16_t x0 = static_cast<int16_t>((static_cast<int32_t>(width_px) * index) / palette_count);
        const int16_t x1 = static_cast<int16_t>((static_cast<int32_t>(width_px) * (index + 1U)) / palette_count);
        int16_t bar_width = static_cast<int16_t>(x1 - x0);
        if (bar_width < 1) {
          bar_width = 1;
        }
        lv_obj_set_pos(bar, x0, 0);
        lv_obj_set_size(bar, static_cast<lv_coord_t>(bar_width + 1), height_px);
        lv_obj_set_style_bg_color(bar, lv_color_hex(kTestLabPaletteInputRgb[index]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_translate_y(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
      }
      if (scene_title_label_ != nullptr) {
        lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(scene_title_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_color(scene_title_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_bg_opa(scene_title_label_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_left(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 6);
        lv_obj_move_foreground(scene_title_label_);
      }
      if (scene_subtitle_label_ != nullptr) {
        lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(scene_subtitle_label_, activeDisplayWidth() - 20);
        lv_obj_set_style_text_font(scene_subtitle_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_bg_opa(scene_subtitle_label_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_left(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_move_foreground(scene_subtitle_label_);
      }
      if (scene_subtitle_label_ != nullptr) {
        lv_anim_t subtitle_wave;
        lv_anim_init(&subtitle_wave);
        lv_anim_set_var(&subtitle_wave, scene_subtitle_label_);
        lv_anim_set_exec_cb(&subtitle_wave, animSetSineTranslateY);
        lv_anim_set_values(&subtitle_wave, 0, 4095);
        lv_anim_set_time(&subtitle_wave, resolveAnimMs(2400U));
        lv_anim_set_repeat_count(&subtitle_wave, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&subtitle_wave);
      }
    } else {
      if (scene_title_label_ != nullptr) {
        lv_obj_set_style_bg_opa(scene_title_label_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_left(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(scene_title_label_, 0, LV_PART_MAIN);
      }
      if (scene_subtitle_label_ != nullptr) {
        lv_obj_set_style_bg_opa(scene_subtitle_label_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_left(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(scene_subtitle_label_, 0, LV_PART_MAIN);
      }
    }

    if (scene_use_lgfx_text_overlay_) {
      resetSceneTimeline();
      current_effect_ = SceneEffect::kNone;
      effect_speed_ms_ = 0U;
    }

    if (timeline_keyframe_count_ > 1U && timeline_duration_ms_ > 0U) {
      timeline_effect_index_ = -1;
      onTimelineTick(0U);

      lv_anim_t timeline_anim;
      lv_anim_init(&timeline_anim);
      lv_anim_set_var(&timeline_anim, scene_root_);
      lv_anim_set_exec_cb(&timeline_anim, animTimelineTickCb);
      lv_anim_set_values(&timeline_anim, 0, timeline_duration_ms_);
      lv_anim_set_time(&timeline_anim, timeline_duration_ms_);
      lv_anim_set_repeat_count(&timeline_anim, timeline_loop_ ? LV_ANIM_REPEAT_INFINITE : 0U);
      lv_anim_set_playback_time(&timeline_anim, 0);
      lv_anim_start(&timeline_anim);
    } else {
      applySceneEffect(effect);
    }
    if (scene_changed && has_previous_scene) {
      applySceneTransition(transition, transition_ms);
    }
  }

  if (static_state_changed && is_locked_scene && show_title && scene_title_label_ != nullptr) {
    lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(scene_title_label_);
    const bool title_bounce_inverted =
        ((mixNoise(static_cast<uint32_t>(effect_speed_ms), reinterpret_cast<uintptr_t>(scene_title_label_)) & 1U) != 0U);
    lv_anim_t title_bounce;
    lv_anim_init(&title_bounce);
    lv_anim_set_var(&title_bounce, scene_title_label_);
    lv_anim_set_exec_cb(&title_bounce, animSetSineTranslateY);
    lv_anim_set_values(&title_bounce, title_bounce_inverted ? 4095 : 0, title_bounce_inverted ? 0 : 4095);
    lv_anim_set_time(&title_bounce, resolveAnimMs(980U));
    lv_anim_set_playback_time(&title_bounce, resolveAnimMs(980U));
    lv_anim_set_repeat_count(&title_bounce, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&title_bounce);
    lv_anim_t title_lock_opa;
    lv_anim_init(&title_lock_opa);
    lv_anim_set_var(&title_lock_opa, scene_title_label_);
    lv_anim_set_exec_cb(&title_lock_opa, animSetRandomTextOpa);
    lv_anim_set_values(&title_lock_opa, 0, 4095);
    lv_anim_set_time(&title_lock_opa, resolveAnimMs(72U));
    lv_anim_set_repeat_count(&title_lock_opa, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&title_lock_opa);
    lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_angle(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(scene_title_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN);
  }
  if (static_state_changed && is_locked_scene && show_subtitle && subtitle.length() > 0U &&
      scene_subtitle_label_ != nullptr) {
    lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(scene_subtitle_label_);
    lv_anim_t subtitle_lock_jitter_x;
    lv_anim_init(&subtitle_lock_jitter_x);
    lv_anim_set_var(&subtitle_lock_jitter_x, scene_subtitle_label_);
    lv_anim_set_exec_cb(&subtitle_lock_jitter_x, animSetRandomTranslateX);
    lv_anim_set_values(&subtitle_lock_jitter_x, 0, 4095);
    lv_anim_set_time(&subtitle_lock_jitter_x, resolveAnimMs(66U));
    lv_anim_set_repeat_count(&subtitle_lock_jitter_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&subtitle_lock_jitter_x);

    lv_anim_t subtitle_lock_jitter_y;
    lv_anim_init(&subtitle_lock_jitter_y);
    lv_anim_set_var(&subtitle_lock_jitter_y, scene_subtitle_label_);
    lv_anim_set_exec_cb(&subtitle_lock_jitter_y, animSetRandomTranslateY);
    lv_anim_set_values(&subtitle_lock_jitter_y, 0, 4095);
    lv_anim_set_time(&subtitle_lock_jitter_y, resolveAnimMs(58U));
    lv_anim_set_repeat_count(&subtitle_lock_jitter_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&subtitle_lock_jitter_y);
    lv_anim_t subtitle_lock_opa;
    lv_anim_init(&subtitle_lock_opa);
    lv_anim_set_var(&subtitle_lock_opa, scene_subtitle_label_);
    lv_anim_set_exec_cb(&subtitle_lock_opa, animSetRandomTextOpa);
    lv_anim_set_values(&subtitle_lock_opa, 0, 4095);
    lv_anim_set_time(&subtitle_lock_opa, resolveAnimMs(56U));
    lv_anim_set_repeat_count(&subtitle_lock_opa, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&subtitle_lock_opa);
    lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_angle(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN);
  }
  if (static_state_changed && is_locked_scene && !show_symbol && scene_symbol_label_ != nullptr) {
    lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(scene_symbol_label_, "");
  }

  if (static_state_changed) {
    if (qr_scene) {
      qr_scene_controller_.onSceneEnter(&qr_scan_, scene_subtitle_label_);
    } else {
      qr_scene_controller_.onSceneExit(&qr_scan_);
    }
  }

  const String title_ascii = asciiFallbackForUiText(title.c_str());
  const String subtitle_ascii = asciiFallbackForUiText(subtitle.c_str());
  const String symbol_ascii = asciiFallbackForUiText(symbol.c_str());
  applySceneDynamicState(subtitle, show_subtitle, audio_playing, text_rgb);
  if (test_lab_scene) {
    if (scene_title_label_ != nullptr) {
      const char* current_title = lv_label_get_text(scene_title_label_);
      if (current_title == nullptr || std::strcmp(current_title, title_ascii.c_str()) != 0) {
        lv_label_set_text(scene_title_label_, title_ascii.c_str());
      }
      lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_font(scene_title_label_, &lv_font_montserrat_14, LV_PART_MAIN);
      lv_obj_set_style_text_color(scene_title_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN);
      lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(scene_title_label_, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_pad_left(scene_title_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_right(scene_title_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_top(scene_title_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_bottom(scene_title_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(scene_title_label_, 0, LV_PART_MAIN);
      lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 6);
      lv_obj_move_foreground(scene_title_label_);
    }
    if (scene_subtitle_label_ != nullptr) {
      const char* current_subtitle = lv_label_get_text(scene_subtitle_label_);
      if (current_subtitle == nullptr || std::strcmp(current_subtitle, subtitle_ascii.c_str()) != 0) {
        lv_label_set_text(scene_subtitle_label_, subtitle_ascii.c_str());
      }
      lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(scene_subtitle_label_, activeDisplayWidth() - 20);
      lv_obj_set_style_text_font(scene_subtitle_label_, &lv_font_montserrat_14, LV_PART_MAIN);
      lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(0xFFFFFFUL), LV_PART_MAIN);
      lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(scene_subtitle_label_, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_pad_left(scene_subtitle_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_right(scene_subtitle_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_top(scene_subtitle_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_bottom(scene_subtitle_label_, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(scene_subtitle_label_, 0, LV_PART_MAIN);
      lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -6);
      lv_obj_move_foreground(scene_subtitle_label_);
    }
  }
  const bool subtitle_visible = show_subtitle && subtitle.length() > 0U;
  scene_status_.valid = true;
  scene_status_.audio_playing = audio_playing;
  scene_status_.show_title = show_title;
  scene_status_.show_subtitle = subtitle_visible;
  scene_status_.show_symbol = show_symbol;
  scene_status_.lvgl_text_disabled = scene_disable_lvgl_text_;
  scene_status_.payload_crc = payload_crc;
  scene_status_.effect_speed_ms = effect_speed_ms_;
  scene_status_.text_glitch_pct = text_glitch_pct_;
  scene_status_.text_size_pct = text_size_pct_;
  scene_status_.transition_ms = transition_ms;
  scene_status_.overlay_draw_ok = overlay_draw_ok_count_;
  scene_status_.overlay_draw_fail = overlay_draw_fail_count_;
  scene_status_.overlay_startwrite_fail = overlay_startwrite_fail_count_;
  scene_status_.overlay_skip_busy = overlay_skip_busy_count_;
  if (theme_cache_valid_) {
    scene_status_.bg_rgb = theme_cache_bg_;
    scene_status_.accent_rgb = theme_cache_accent_;
    scene_status_.text_rgb = theme_cache_text_;
  } else {
    scene_status_.bg_rgb = bg_rgb;
    scene_status_.accent_rgb = accent_rgb;
    scene_status_.text_rgb = text_rgb;
  }
  copyTextSafe(scene_status_.scenario_id, sizeof(scene_status_.scenario_id), scenario_id);
  copyTextSafe(scene_status_.step_id, sizeof(scene_status_.step_id), step_id_for_ui);
  copyTextSafe(scene_status_.scene_id, sizeof(scene_status_.scene_id), scene_id);
  copyTextSafe(scene_status_.audio_pack_id, sizeof(scene_status_.audio_pack_id), audio_pack_id_for_ui);
  copyTextSafe(scene_status_.title, sizeof(scene_status_.title), title_ascii.c_str());
  copyTextSafe(scene_status_.subtitle, sizeof(scene_status_.subtitle), subtitle_ascii.c_str());
  copyTextSafe(scene_status_.symbol, sizeof(scene_status_.symbol), symbol_ascii.c_str());
  copyTextSafe(scene_status_.symbol_align, sizeof(scene_status_.symbol_align), symbol_align_token);
  copyTextSafe(scene_status_.text_backend,
               sizeof(scene_status_.text_backend),
               scene_use_lgfx_text_overlay_ ? "lgfx_overlay" : "lvgl");
  copyTextSafe(scene_status_.effect, sizeof(scene_status_.effect), effectToToken(effect));
  copyTextSafe(scene_status_.transition, sizeof(scene_status_.transition), transitionToToken(transition));
  std::strncpy(last_scene_id_, scene_id, sizeof(last_scene_id_) - 1U);
  last_scene_id_[sizeof(last_scene_id_) - 1U] = '\0';
  last_payload_crc_ = payload_crc;
  if (static_state_changed) {
    updatePageLine();
    UI_LOGI("scene=%s effect=%u speed=%u title=%u symbol=%u scenario=%s audio=%u timeline=%u transition=%u:%u",
            scene_id,
            static_cast<unsigned int>(effect),
            static_cast<unsigned int>(effect_speed_ms_),
            show_title ? 1U : 0U,
            show_symbol ? 1U : 0U,
            scenario_id,
            audio_playing ? 1U : 0U,
            static_cast<unsigned int>(timeline_keyframe_count_),
            static_cast<unsigned int>(transition),
            static_cast<unsigned int>(transition_ms));
    if (win_etape_intro_scene) {
      startIntroIfNeeded(static_state_changed);
    }
  }
}

void UiManager::handleButton(uint8_t key, bool long_press) {
  UiAction action;
  action.source = long_press ? UiActionSource::kKeyLong : UiActionSource::kKeyShort;
  action.key = key;
  player_ui_.applyAction(action);

  pending_key_code_ = toLvKey(key, long_press);
  key_press_pending_ = true;
}

void UiManager::handleTouch(int16_t x, int16_t y, bool touched) {
  touch_x_ = x;
  touch_y_ = y;
  touch_pressed_ = touched;
}

void UiManager::createWidgets() {
  lv_obj_t* root = lv_scr_act();
  lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), LV_PART_MAIN);

  scene_root_ = lv_obj_create(root);
  lv_obj_remove_style_all(scene_root_);
  lv_obj_set_size(scene_root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(scene_root_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_root_, lv_color_hex(0x07132A), LV_PART_MAIN);
  lv_obj_clear_flag(scene_root_, LV_OBJ_FLAG_SCROLLABLE);

  for (lv_obj_t*& bar : scene_cracktro_bars_) {
    bar = lv_obj_create(scene_root_);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, activeDisplayWidth(), 20);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x28143A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t index = 0U; index < kStarfieldCount; ++index) {
    lv_obj_t*& star = scene_starfield_[index];
    star = lv_obj_create(scene_root_);
    lv_obj_remove_style_all(star);
    lv_obj_set_size(star, 3, 3);
    lv_obj_set_style_radius(star, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(star, lv_color_hex(0xE9F6FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(star, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
  }

  scene_ring_outer_ = lv_obj_create(scene_root_);
  SceneElement::initCircle(scene_ring_outer_,
                           lv_color_hex(0x000000),
                           LV_OPA_TRANSP,
                           lv_color_hex(0x2A76FF),
                           3,
                           LV_OPA_70);

  scene_ring_inner_ = lv_obj_create(scene_root_);
  SceneElement::initCircle(scene_ring_inner_,
                           lv_color_hex(0x000000),
                           LV_OPA_TRANSP,
                           lv_color_hex(0xC8DCFF),
                           2,
                           LV_OPA_80);

  scene_core_ = lv_obj_create(scene_root_);
  SceneElement::initCircle(scene_core_,
                           lv_color_hex(0x2A76FF),
                           LV_OPA_90,
                           lv_color_hex(0xE8F1FF),
                           2,
                           LV_OPA_COVER);

  scene_fx_bar_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(scene_fx_bar_);
  lv_obj_set_style_radius(scene_fx_bar_, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_fx_bar_, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_fx_bar_, lv_color_hex(0x2A76FF), LV_PART_MAIN);

  for (lv_obj_t*& particle : scene_particles_) {
    particle = lv_obj_create(scene_root_);
    lv_obj_remove_style_all(particle);
    lv_obj_set_size(particle, 10, 10);
    lv_obj_set_style_radius(particle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(particle, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(particle, LV_OPA_90, LV_PART_MAIN);
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
  }

  scene_waveform_outer_ = lv_line_create(scene_root_);
  lv_obj_add_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_line_color(scene_waveform_outer_, lv_color_hex(0x4AEAFF), LV_PART_MAIN);
  lv_obj_set_style_line_width(scene_waveform_outer_, 1, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(scene_waveform_outer_, true, LV_PART_MAIN);
  lv_obj_set_style_opa(scene_waveform_outer_, LV_OPA_60, LV_PART_MAIN);

  scene_waveform_ = lv_line_create(scene_root_);
  lv_obj_add_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_line_color(scene_waveform_, lv_color_hex(0xA9FFCF), LV_PART_MAIN);
  lv_obj_set_style_line_width(scene_waveform_, 2, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(scene_waveform_, true, LV_PART_MAIN);

  scene_la_needle_ = lv_line_create(scene_root_);
  lv_obj_add_flag(scene_la_needle_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_line_color(scene_la_needle_, lv_color_hex(0xA9FFCF), LV_PART_MAIN);
  lv_obj_set_style_line_width(scene_la_needle_, 3, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(scene_la_needle_, true, LV_PART_MAIN);
  lv_obj_set_style_opa(scene_la_needle_, LV_OPA_90, LV_PART_MAIN);

  scene_la_meter_bg_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(scene_la_meter_bg_);
  lv_obj_set_size(scene_la_meter_bg_, activeDisplayWidth() - 52, 10);
  lv_obj_set_style_radius(scene_la_meter_bg_, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_la_meter_bg_, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_la_meter_bg_, lv_color_hex(0x1B3C56), LV_PART_MAIN);
  lv_obj_set_style_border_width(scene_la_meter_bg_, 1, LV_PART_MAIN);
  lv_obj_set_style_border_opa(scene_la_meter_bg_, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_la_meter_bg_, lv_color_hex(0x53A5CC), LV_PART_MAIN);
  lv_obj_align(scene_la_meter_bg_, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_add_flag(scene_la_meter_bg_, LV_OBJ_FLAG_HIDDEN);

  scene_la_meter_fill_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(scene_la_meter_fill_);
  lv_obj_set_size(scene_la_meter_fill_, 12, 6);
  lv_obj_set_style_radius(scene_la_meter_fill_, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_la_meter_fill_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_la_meter_fill_, lv_color_hex(0x4AD0FF), LV_PART_MAIN);
  lv_obj_add_flag(scene_la_meter_fill_, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t index = 0U; index < kLaAnalyzerBarCount; ++index) {
    scene_la_analyzer_bars_[index] = lv_obj_create(scene_root_);
    lv_obj_remove_style_all(scene_la_analyzer_bars_[index]);
    lv_obj_set_size(scene_la_analyzer_bars_[index], 8, 8);
    lv_obj_set_style_radius(scene_la_analyzer_bars_[index], 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(scene_la_analyzer_bars_[index], lv_color_hex(0x3CCBFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scene_la_analyzer_bars_[index], LV_OPA_70, LV_PART_MAIN);
    lv_obj_add_flag(scene_la_analyzer_bars_[index], LV_OBJ_FLAG_HIDDEN);
  }

  page_label_ = lv_label_create(scene_root_);
  lv_obj_add_flag(page_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_opa(page_label_, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_text_color(page_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  scene_title_label_ = lv_label_create(scene_root_);
  scene_subtitle_label_ = lv_label_create(scene_root_);
  scene_symbol_label_ = lv_label_create(scene_root_);
  scene_la_status_label_ = lv_label_create(scene_root_);
  scene_la_pitch_label_ = lv_label_create(scene_root_);
  scene_la_timer_label_ = lv_label_create(scene_root_);
  scene_la_timeout_label_ = lv_label_create(scene_root_);
  lv_obj_set_style_text_color(scene_title_label_, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_symbol_label_, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_la_status_label_, lv_color_hex(0x86CCFF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_la_pitch_label_, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_la_timer_label_, lv_color_hex(0x9AD6FF), LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_la_timeout_label_, lv_color_hex(0x84CFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBold24(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontItalic12(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_symbol_label_, UiFonts::fontTitle(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_status_label_, UiFonts::fontMono(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_pitch_label_, UiFonts::fontBodyM(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_timer_label_, UiFonts::fontMono(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_timeout_label_, UiFonts::fontMono(), LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_symbol_label_, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_la_status_label_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_la_pitch_label_, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_la_timer_label_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_opa(scene_la_timeout_label_, LV_OPA_90, LV_PART_MAIN);
  lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_align(scene_symbol_label_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(scene_la_status_label_, LV_ALIGN_TOP_RIGHT, -8, 8);
  lv_obj_align(scene_la_timer_label_, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_obj_align(scene_la_timeout_label_, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_align(scene_la_pitch_label_, LV_ALIGN_BOTTOM_MID, 0, -30);
  lv_obj_set_style_text_align(scene_la_status_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_set_style_text_align(scene_la_pitch_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_align(scene_la_timer_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_set_style_text_align(scene_la_timeout_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(scene_la_pitch_label_, activeDisplayWidth() - 26);
  lv_obj_set_width(scene_subtitle_label_, activeDisplayWidth() - 32);
  lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(scene_title_label_, "MISSION");
  lv_label_set_text(scene_subtitle_label_, "");
  lv_label_set_text(scene_symbol_label_, LV_SYMBOL_PLAY);
  lv_label_set_text(scene_la_status_label_, "");
  lv_label_set_text(scene_la_pitch_label_, "");
  lv_label_set_text(scene_la_timer_label_, "");
  lv_label_set_text(scene_la_timeout_label_, "");
  lv_obj_add_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scene_la_status_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scene_la_pitch_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scene_la_timer_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scene_la_timeout_label_, LV_OBJ_FLAG_HIDDEN);

  stopSceneAnimations();
}

void UiManager::animSetY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_y(static_cast<lv_obj_t*>(obj), value);
}

void UiManager::animSetX(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_x(static_cast<lv_obj_t*>(obj), value);
}

void UiManager::animSetStyleTranslateX(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj), static_cast<int16_t>(value), LV_PART_MAIN);
}

void UiManager::animSetStyleTranslateY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), static_cast<int16_t>(value), LV_PART_MAIN);
}

void UiManager::animSetStyleRotate(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  const int16_t angle = static_cast<int16_t>(value);
  lv_obj_set_style_transform_angle(target, angle, LV_PART_MAIN);
}

void UiManager::animSetOpa(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(value), LV_PART_MAIN);
}

void UiManager::animSetSize(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  if (value < 24) {
    value = 24;
  }
  lv_obj_set_size(static_cast<lv_obj_t*>(obj), value, value);
}

void UiManager::animSetParticleSize(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  if (value < 4) {
    value = 4;
  } else if (value > 24) {
    value = 24;
  }
  lv_obj_set_size(static_cast<lv_obj_t*>(obj), static_cast<int16_t>(value), static_cast<int16_t>(value));
}

void UiManager::animSetWidth(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  if (value < 16) {
    value = 16;
  }
  lv_obj_set_width(static_cast<lv_obj_t*>(obj), value);
}

void UiManager::animSetRandomTranslateX(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  int16_t amplitude = 12;
  if (g_instance != nullptr) {
    if (target == g_instance->scene_fx_bar_) {
      amplitude = 62;
    } else if (target == g_instance->scene_core_) {
      amplitude = 30;
    } else if (target == g_instance->scene_title_label_) {
      amplitude = static_cast<int16_t>(2 + (static_cast<uint16_t>(g_instance->text_glitch_pct_) * 18U) / 100U);
    } else if (target == g_instance->scene_subtitle_label_) {
      amplitude = static_cast<int16_t>(1 + (static_cast<uint16_t>(g_instance->text_glitch_pct_) * 14U) / 100U);
    } else if (target == g_instance->scene_symbol_label_) {
      amplitude = 18;
    } else if (target == g_instance->scene_ring_outer_ || target == g_instance->scene_ring_inner_) {
      amplitude = 16;
    } else {
      for (lv_obj_t* particle : g_instance->scene_particles_) {
        if (target == particle) {
          amplitude = 42;
          break;
        }
      }
    }
  }
  const int16_t jitter = signedNoise(static_cast<uint32_t>(value), reinterpret_cast<uintptr_t>(target) ^ 0x6A09E667UL, amplitude);
  lv_obj_set_style_translate_x(target, jitter, LV_PART_MAIN);
}

void UiManager::animSetRandomTranslateY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  int16_t amplitude = 10;
  if (g_instance != nullptr) {
    if (target == g_instance->scene_fx_bar_) {
      amplitude = 34;
    } else if (target == g_instance->scene_core_) {
      amplitude = 24;
    } else if (target == g_instance->scene_title_label_) {
      amplitude = static_cast<int16_t>(1 + (static_cast<uint16_t>(g_instance->text_glitch_pct_) * 12U) / 100U);
    } else if (target == g_instance->scene_subtitle_label_) {
      amplitude = static_cast<int16_t>(1 + (static_cast<uint16_t>(g_instance->text_glitch_pct_) * 10U) / 100U);
    } else if (target == g_instance->scene_symbol_label_) {
      amplitude = 14;
    } else if (target == g_instance->scene_ring_outer_ || target == g_instance->scene_ring_inner_) {
      amplitude = 12;
    } else {
      for (lv_obj_t* particle : g_instance->scene_particles_) {
        if (target == particle) {
          amplitude = 30;
          break;
        }
      }
    }
  }
  const int16_t jitter = signedNoise(static_cast<uint32_t>(value), reinterpret_cast<uintptr_t>(target) ^ 0xBB67AE85UL, amplitude);
  lv_obj_set_style_translate_y(target, jitter, LV_PART_MAIN);
}

void UiManager::animSetRandomOpa(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  lv_opa_t min_opa = 14;
  lv_opa_t max_opa = LV_OPA_COVER;
  if (g_instance != nullptr) {
    if (target == g_instance->scene_root_) {
      min_opa = (g_instance->demo_strobe_level_ >= 90U) ? 4 : 12;
      max_opa = LV_OPA_COVER;
    } else if (target == g_instance->scene_fx_bar_) {
      min_opa = 12;
    } else if (target == g_instance->scene_symbol_label_) {
      min_opa = 8;
    } else {
      for (lv_obj_t* particle : g_instance->scene_particles_) {
        if (target == particle) {
          min_opa = 4;
          break;
        }
      }
    }
  }
  const uint32_t mixed =
      mixNoise(static_cast<uint32_t>(value) * 1664525UL + 1013904223UL, reinterpret_cast<uintptr_t>(target) ^ 0x3C6EF372UL);
  const uint16_t span = static_cast<uint16_t>(max_opa - min_opa);
  const lv_opa_t out = static_cast<lv_opa_t>(min_opa + static_cast<lv_opa_t>(mixed % (span + 1U)));
  lv_obj_set_style_opa(target, out, LV_PART_MAIN);
}

void UiManager::animSetFireworkTranslateX(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  constexpr int16_t kFireworkX[4] = {-48, 52, -24, 30};
  const uint8_t index = g_instance != nullptr ? g_instance->particleIndexForObj(target) : 4U;
  if (index >= 4U) {
    return;
  }
  const int32_t clamped = (value < 0) ? 0 : (value > 4095) ? 4095 : value;
  const int32_t phase = (clamped <= 2047) ? clamped : (4095 - clamped);
  const int16_t x = static_cast<int16_t>((static_cast<int32_t>(kFireworkX[index]) * phase) / 2047);
  const int16_t jitter = signedNoise(static_cast<uint32_t>(value) + 77U, reinterpret_cast<uintptr_t>(target) ^ 0x9E3779B9UL, 3);
  lv_obj_set_style_translate_x(target, static_cast<int16_t>(x + jitter), LV_PART_MAIN);
}

void UiManager::animSetFireworkTranslateY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(obj);
  constexpr int16_t kFireworkY[4] = {-62, -34, 52, 64};
  const uint8_t index = g_instance != nullptr ? g_instance->particleIndexForObj(target) : 4U;
  if (index >= 4U) {
    return;
  }
  const int32_t clamped = (value < 0) ? 0 : (value > 4095) ? 4095 : value;
  const int32_t phase = (clamped <= 2047) ? clamped : (4095 - clamped);
  const int16_t y = static_cast<int16_t>((static_cast<int32_t>(kFireworkY[index]) * phase) / 2047);
  const int16_t jitter = signedNoise(static_cast<uint32_t>(value) + 143U, reinterpret_cast<uintptr_t>(target) ^ 0xBB67AE85UL, 4);
  lv_obj_set_style_translate_y(target, static_cast<int16_t>(y + jitter), LV_PART_MAIN);
}

void UiManager::animTimelineTickCb(void* obj, int32_t value) {
  (void)obj;
  if (g_instance == nullptr || value < 0) {
    return;
  }
  g_instance->onTimelineTick(static_cast<uint16_t>(value));
}

void UiManager::animWinEtapeShowcaseTickCb(void* obj, int32_t value) {
  (void)obj;
  if (g_instance == nullptr || value < 0) {
    return;
  }
  g_instance->onWinEtapeShowcaseTick(static_cast<uint16_t>(value));
}

void UiManager::animSetWinTitleReveal(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  constexpr const char* kTitle = kWinEtapeDemoTitle;
  constexpr size_t kMaxChars = 48U;
  char buffer[kMaxChars] = {0};
  size_t count = (value < 0) ? 0U : static_cast<size_t>(value);
  const size_t max_len = std::strlen(kTitle);
  if (count > max_len) {
    count = max_len;
  }
  if (count > (kMaxChars - 1U)) {
    count = kMaxChars - 1U;
  }
  if (count > 0U) {
    std::memcpy(buffer, kTitle, count);
  }
  lv_label_set_text(static_cast<lv_obj_t*>(obj), buffer);
}

void UiManager::animSetSineTranslateY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  constexpr float kTau = 6.28318530718f;
  const int32_t phase = (value < 0) ? 0 : (value % 4096);
  const float radians = (static_cast<float>(phase) / 4095.0f) * kTau;
  const int16_t offset = static_cast<int16_t>(std::sin(radians) * 6.0f);
  lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), offset, LV_PART_MAIN);
}

void UiManager::keypadReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  if (g_instance == nullptr) {
    data->state = LV_INDEV_STATE_REL;
    data->key = LV_KEY_ENTER;
    return;
  }

  data->key = g_instance->pending_key_code_;
  if (g_instance->key_press_pending_) {
    data->state = LV_INDEV_STATE_PR;
    g_instance->key_press_pending_ = false;
    g_instance->key_release_pending_ = true;
    return;
  }
  if (g_instance->key_release_pending_) {
    data->state = LV_INDEV_STATE_REL;
    g_instance->key_release_pending_ = false;
    return;
  }
  data->state = LV_INDEV_STATE_REL;
}

void UiManager::touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  if (g_instance == nullptr) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  data->point.x = g_instance->touch_x_;
  data->point.y = g_instance->touch_y_;
  data->state = g_instance->touch_pressed_ ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}
