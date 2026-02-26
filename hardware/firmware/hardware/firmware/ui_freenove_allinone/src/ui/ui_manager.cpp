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

constexpr uint16_t kIntroTickMs = 42U;
constexpr uint32_t kUiUpdateFrameMs = 42U;
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
  if (normalized == "fireworks") {
    *out_preset = ui::fx::FxPreset::kFireworks;
    return true;
  }
  if (normalized == "boingball") {
    *out_preset = ui::fx::FxPreset::kBoingball;
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
    case ui::fx::FxPreset::kFireworks:
      return "fireworks";
    case ui::fx::FxPreset::kBoingball:
      return "boingball";
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
  constexpr uint8_t min_opa = 60U;
  constexpr uint8_t max_opa = LV_OPA_COVER;
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
  fx_engine_.begin(fx_cfg);
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

  if (elapsed_ms >= kUiUpdateFrameMs) {
    lv_tick_inc(elapsed_ms);
    last_lvgl_tick_ms_ = now_ms;
  } else {
    if (pending_lvgl_flush_request_ && !flush_busy_now) {
      run_lvgl_draw();
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
  const bool fx_candidate = (intro_active_ || direct_fx_scene_active_) && fx_engine_.enabled();
  if (flush_busy) {
    graphics_stats_.flush_blocked_count += 1U;
    if (fx_candidate) {
      graphics_stats_.fx_skip_flush_busy += 1U;
    }
    pending_lvgl_flush_request_ = true;
    pollAsyncFlush();
    return;
  }
  // Frame order contract: FX (LGFX) -> invalidate LVGL overlay -> lv_timer_handler when bus is free.
  if (fx_candidate) {
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
  run_lvgl_draw();
  pending_lvgl_flush_request_ = false;
  pollAsyncFlush();
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
      "GFX_STATUS depth=%u mode=%s theme256=%u lines=%u double=%u source=%s full_frame=%u dma_req=%u dma_async=%u trans_px=%u trans_lines=%u pending=%u flush=%lu dma=%lu sync=%lu flush_spi_avg=%lu flush_spi_max=%lu draw_lvgl_avg=%lu draw_lvgl_max=%lu fx_fps=%u fx_frames=%lu fx_blit=%lu/%lu/%lu tail=%lu fx_dma_to=%lu fx_fail=%lu fx_skip_busy=%lu block=%lu ovf=%lu stall=%lu recover=%lu async_fallback=%lu",
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
  const bool win_etape_intro_scene = (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 ||
                                      std::strcmp(scene_id, "SCENE_WIN_ETAPE1") == 0 ||
                                      std::strcmp(scene_id, "SCENE_WIN_ETAPE2") == 0);
  const bool direct_fx_scene = isDirectFxSceneId(scene_id);
  const bool is_locked_scene = (std::strcmp(scene_id, "SCENE_LOCKED") == 0);
  const bool qr_scene = (std::strcmp(scene_id, "SCENE_CAMERA_SCAN") == 0 ||
                         std::strcmp(scene_id, "SCENE_QR_DETECTOR") == 0);
  const bool parse_payload_this_frame = static_state_changed || win_etape_intro_scene;
  if (static_state_changed && scene_changed && has_previous_scene) {
    cleanupSceneTransitionAssets(last_scene_id_, scene_id);
  }

  if (static_state_changed && !win_etape_intro_scene && intro_active_) {
    stopIntroAndCleanup();
  }
  if (static_state_changed && !direct_fx_scene) {
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

  String title = "MISSION";
  String subtitle;
  String symbol = "RUN";
  bool show_title = false;
  bool show_subtitle = true;
  bool show_symbol = true;
  SceneEffect effect = SceneEffect::kPulse;
  uint16_t effect_speed_ms = 0U;
  SceneTransition transition = SceneTransition::kFade;
  uint16_t transition_ms = 240U;
  SceneTextAlign title_align = SceneTextAlign::kTop;
  SceneTextAlign subtitle_align = SceneTextAlign::kBottom;
  int16_t frame_dx = 0;
  int16_t frame_dy = 0;
  uint8_t frame_scale_pct = 100U;
  bool frame_split_layout = false;
  SceneScrollMode subtitle_scroll_mode = SceneScrollMode::kNone;
  uint16_t subtitle_scroll_speed_ms = 4200U;
  uint16_t subtitle_scroll_pause_ms = 900U;
  bool subtitle_scroll_loop = true;
  String demo_mode = "standard";
  uint8_t demo_particle_count = 4U;
  uint8_t demo_strobe_level = 65U;
  bool win_etape_fireworks = false;
  bool waveform_enabled = false;
  uint8_t waveform_sample_count = HardwareManager::kMicWaveformCapacity;
  uint8_t waveform_amplitude_pct = 95U;
  bool waveform_jitter = true;
  la_detection_scene_ = false;
  uint32_t bg_rgb = 0x07132AUL;
  uint32_t accent_rgb = 0x2A76FFUL;
  uint32_t text_rgb = 0xE8F1FFUL;

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
    subtitle = "Signal brouille";
    symbol = "ALERT";
    effect = SceneEffect::kBlink;
    bg_rgb = 0x2A0508UL;
    accent_rgb = 0xFF4A45UL;
    text_rgb = 0xFFD5D1UL;
  } else if (std::strcmp(scene_id, "SCENE_WARNING") == 0) {
    title = "ALERTE";
    subtitle = "Signal anormal";
    symbol = "WARN";
    effect = SceneEffect::kBlink;
    bg_rgb = 0x261209UL;
    accent_rgb = 0xFF9A4AUL;
    text_rgb = 0xFFF2E6UL;
  } else if (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0 || std::strcmp(scene_id, "SCENE_SEARCH") == 0) {
    title = "DETECTEUR DE RESONNANCE";
    subtitle = "";
    symbol = "AUDIO";
    effect = SceneEffect::kWave;
    bg_rgb = 0x04141FUL;
    accent_rgb = 0x49D9FFUL;
    text_rgb = 0xE7F6FFUL;
    if (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0) {
      bg_rgb = 0x000000UL;
      la_detection_scene_ = true;
      waveform_enabled = true;
      waveform_sample_count = HardwareManager::kMicWaveformCapacity;
      waveform_amplitude_pct = 100U;
      waveform_jitter = true;
      frame_split_layout = true;
      frame_dy = 8;
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
    effect = SceneEffect::kNone;
    transition = SceneTransition::kFade;
    transition_ms = 220U;
    bg_rgb = 0x000022UL;
    accent_rgb = 0x00FFFFUL;
    text_rgb = 0xFFFFFFUL;
    show_title = true;
    show_subtitle = true;
    show_symbol = false;
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
    bg_rgb = scene_status_.bg_rgb;
    accent_rgb = scene_status_.accent_rgb;
    text_rgb = scene_status_.text_rgb;
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

      effect = parseEffectToken(payload_effect, effect, "scene payload effect");

      const char* payload_bg = document["theme"]["bg"] | document["visual"]["theme"]["bg"] | document["bg"] | "";
      const char* payload_accent =
          document["theme"]["accent"] | document["visual"]["theme"]["accent"] | document["accent"] | "";
      const char* payload_secondary =
          document["theme"]["text"] | document["visual"]["theme"]["text"] | document["text"] | "";
      parseHexRgb(payload_bg, &bg_rgb);
      parseHexRgb(payload_accent, &accent_rgb);
      parseHexRgb(payload_secondary, &text_rgb);

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
  const bool mic_needed = la_detection_scene_ || waveform_enabled;
  if (hardware_ != nullptr) {
    hardware_->setMicRuntimeEnabled(mic_needed);
  }
  configureWaveformOverlay((waveform_snapshot_ref_ != nullptr) ? waveform_snapshot_ref_
                                                               : (waveform_snapshot_valid_ ? &waveform_snapshot_
                                                                                           : nullptr),
                           waveform_enabled,
                           waveform_sample_count,
                          waveform_amplitude_pct,
                          waveform_jitter);
  if (win_etape_intro_scene) {
    if (subtitle.length() == 0U) {
      subtitle = kWinEtapeWaitingSubtitle;
    }
    if (audio_playing) {
      subtitle = "Validation en cours...";
    }
  }
  if (static_state_changed && direct_fx_scene) {
    direct_fx_scene_active_ = fx_engine_.config().lgfx_backend;
    if (direct_fx_scene_active_) {
      direct_fx_scene_preset_ =
          (std::strcmp(scene_id, "SCENE_FIREWORKS") == 0) ? ui::fx::FxPreset::kFireworks : ui::fx::FxPreset::kWinner;
      fx_engine_.setEnabled(true);
      fx_engine_.setPreset(direct_fx_scene_preset_);
      fx_engine_.setMode(ui::fx::FxMode::kClassic);
      fx_engine_.setBpm(125U);
      fx_engine_.setScrollFont(ui::fx::FxScrollFont::kItalic);
      const String fx_scroll_text = asciiFallbackForUiText((subtitle.length() > 0U) ? subtitle.c_str() : title.c_str());
      if (fx_scroll_text.length() > 0U) {
        fx_engine_.setScrollText(fx_scroll_text.c_str());
      } else {
        fx_engine_.setScrollText(nullptr);
      }
    }
  } else if (static_state_changed && !win_etape_intro_scene) {
    direct_fx_scene_active_ = false;
    if (!intro_active_) {
      fx_engine_.setEnabled(false);
    }
  }

  if (static_state_changed) {
    stopSceneAnimations();
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
    applyTextLayout(title_align, subtitle_align);
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
    applySceneFraming(frame_dx, frame_dy, frame_scale_pct, frame_split_layout);
    applySubtitleScroll(subtitle_scroll_mode, subtitle_scroll_speed_ms, subtitle_scroll_pause_ms, subtitle_scroll_loop);
    for (lv_obj_t* particle : scene_particles_) {
      lv_obj_set_style_bg_color(particle, lv_color_hex(text_rgb), LV_PART_MAIN);
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

  applySceneDynamicState(subtitle, show_subtitle, audio_playing, text_rgb);
  const bool subtitle_visible = show_subtitle && subtitle.length() > 0U;
  const String title_ascii = asciiFallbackForUiText(title.c_str());
  const String subtitle_ascii = asciiFallbackForUiText(subtitle.c_str());
  const String symbol_ascii = asciiFallbackForUiText(symbol.c_str());
  scene_status_.valid = true;
  scene_status_.audio_playing = audio_playing;
  scene_status_.show_title = show_title;
  scene_status_.show_subtitle = subtitle_visible;
  scene_status_.show_symbol = show_symbol;
  scene_status_.payload_crc = payload_crc;
  scene_status_.effect_speed_ms = effect_speed_ms_;
  scene_status_.transition_ms = transition_ms;
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
  lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBodyM(), LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontBodyM(), LV_PART_MAIN);
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
