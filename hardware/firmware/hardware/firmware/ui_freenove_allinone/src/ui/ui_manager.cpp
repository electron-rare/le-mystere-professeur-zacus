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
#endif

#include "ui_freenove_config.h"
#include "drivers/display/display_hal.h"
#include "resources/screen_scene_registry.h"
#include "runtime/memory/caps_allocator.h"
#include "runtime/memory/safe_size.h"
#include "runtime/perf/perf_monitor.h"
#include "ui/scene_element.h"
#include "ui/scene_state.h"
#include "ui/fx/fx_engine.h"
#include "ui_fonts.h"

namespace {

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

#ifndef UI_DMA_TRANS_BUF_LINES
#define UI_DMA_TRANS_BUF_LINES UI_DRAW_BUF_LINES
#endif

#ifndef UI_FULL_FRAME_BENCH
#define UI_FULL_FRAME_BENCH 0
#endif

#ifndef UI_DEMO_AUTORUN_WIN_ETAPE
#define UI_DEMO_AUTORUN_WIN_ETAPE 0
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
constexpr bool kUseFullFrameBenchRuntime = (UI_FULL_FRAME_BENCH != 0);
constexpr bool kUseDemoAutorunWinEtapeRuntime = (UI_DEMO_AUTORUN_WIN_ETAPE != 0);
constexpr uint32_t kFullFrameBenchMinFreePsram = 256U * 1024U;

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

constexpr uint16_t kIntroTickMs = 42U;
constexpr uint32_t kIntroCracktroMsDefault = 30000U;
constexpr uint32_t kIntroTransitionMsDefault = 15000U;
constexpr uint32_t kIntroCleanMsDefault = 20000U;
constexpr uint16_t kIntroB1CrashMsDefault = 4000U;
constexpr uint16_t kIntroOutroMs = 400U;
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

// Retro key color set (RGB332-friendly) used by SCENE_WIN_ETAPE only.
constexpr uint32_t kIntroPaletteRgb[] = {
    0x000022UL,  // 0 bg0
    0x001144UL,  // 1 bg1
    0x002E6AUL,  // 2 bg2
    0x00FFFFUL,  // 3 accent cyan
    0xFF00FFUL,  // 4 accent magenta
    0xFFFF00UL,  // 5 accent yellow
    0x0044AAUL,  // 6 accent blue
    0xFFFFFFUL,  // 7 text white
    0x000000UL,  // 8 shadow black
    0x7FD7FFUL,  // 9 text light blue
    0xFFAA55UL,  // 10 warm particle
    0x1A3355UL,  // 11 dither stripe dark
    0x23466FUL,  // 12 dither stripe mid
    0x10233FUL,  // 13 tunnel stripe dark
    0x18365DUL,  // 14 tunnel stripe light
    0xD4F2FFUL,  // 15 star near white-blue
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

float easeOutBack(float t) {
  const float c1 = 1.70158f;
  const float c3 = c1 + 1.0f;
  const float one_minus = t - 1.0f;
  return 1.0f + c3 * one_minus * one_minus * one_minus + c1 * one_minus * one_minus;
}

}  // namespace

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
    Serial.println("[UI] display init failed");
    return false;
  }
  drivers::display::displayHal().fillScreen(0x0000U);
  initGraphicsPipeline();
  if (draw_buf1_ == nullptr) {
    Serial.println("[UI] graphics pipeline init failed");
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
  Serial.printf("[UI] LVGL + display ready backend=%s\n",
                drivers::display::displayHalUsesLovyanGfx() ? "lgfx" : "tftespi");
  if (kUseDemoAutorunWinEtapeRuntime) {
    Serial.println("[UI] autorun SCENE_WIN_ETAPE enabled");
  }
  dumpGraphicsStatus();
  return true;
}

void UiManager::tick(uint32_t now_ms) {
  (void)now_ms;
  update();
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
  if (elapsed_ms > 0U) {
    lv_tick_inc(elapsed_ms);
    last_lvgl_tick_ms_ = now_ms;
  }
  if (player_ui_.consumeDirty()) {
    updatePageLine();
  }
  renderMicrophoneWaveform();
  pollAsyncFlush();
  lv_timer_handler();
  pollAsyncFlush();
}

void UiManager::initGraphicsPipeline() {
  flush_ctx_ = {};
  buffer_cfg_ = {};
  graphics_stats_ = {};

  if (draw_buf1_owned_ && draw_buf1_ != nullptr) {
    runtime::memory::CapsAllocator::release(draw_buf1_);
  }
  if (draw_buf2_owned_ && draw_buf2_ != nullptr) {
    runtime::memory::CapsAllocator::release(draw_buf2_);
  }
  if (dma_trans_buf_owned_ && dma_trans_buf_ != nullptr) {
    runtime::memory::CapsAllocator::release(dma_trans_buf_);
  }
  if (full_frame_buf_owned_ && full_frame_buf_ != nullptr) {
    runtime::memory::CapsAllocator::release(full_frame_buf_);
  }

  draw_buf1_ = nullptr;
  draw_buf2_ = nullptr;
  draw_buf1_owned_ = false;
  draw_buf2_owned_ = false;
  dma_trans_buf_ = nullptr;
  dma_trans_buf_pixels_ = 0U;
  dma_trans_buf_owned_ = false;
  full_frame_buf_ = nullptr;
  full_frame_buf_owned_ = false;
  color_lut_ready_ = false;
  dma_requested_ = false;
  dma_available_ = false;
  async_flush_enabled_ = false;

  if (kUseColor256Runtime) {
    for (uint16_t value = 0; value < 256U; ++value) {
      const uint8_t r3 = static_cast<uint8_t>((value >> 5U) & 0x07U);
      const uint8_t g3 = static_cast<uint8_t>((value >> 2U) & 0x07U);
      const uint8_t b2 = static_cast<uint8_t>(value & 0x03U);
      const uint8_t r5 = static_cast<uint8_t>((r3 * 31U + 3U) / 7U);
      const uint8_t g6 = static_cast<uint8_t>((g3 * 63U + 3U) / 7U);
      const uint8_t b5 = static_cast<uint8_t>((b2 * 31U + 1U) / 3U);
      rgb332_to_565_lut_[value] =
          static_cast<uint16_t>((static_cast<uint16_t>(r5) << 11U) |
                                (static_cast<uint16_t>(g6) << 5U) |
                                static_cast<uint16_t>(b5));
    }
    color_lut_ready_ = true;
  }

  if (!allocateDrawBuffers()) {
    Serial.println("[UI] draw buffer allocation failed");
    return;
  }
  initDmaEngine();

  const uint16_t width = static_cast<uint16_t>(activeDisplayWidth());
  uint32_t draw_pixels = static_cast<uint32_t>(width) * static_cast<uint32_t>(buffer_cfg_.lines);
  if (buffer_cfg_.full_frame) {
    const uint16_t height = static_cast<uint16_t>(activeDisplayHeight());
    draw_pixels = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
  }
  lv_disp_draw_buf_init(&draw_buf_, draw_buf1_, draw_buf2_, draw_pixels);
}

bool UiManager::allocateDrawBuffers() {
  const uint16_t width = static_cast<uint16_t>(activeDisplayWidth());
  const uint16_t height = static_cast<uint16_t>(activeDisplayHeight());
  if (width == 0U || height == 0U) {
    return false;
  }

  const uint8_t bpp = static_cast<uint8_t>(sizeof(lv_color_t) * 8U);
  buffer_cfg_.bpp = bpp;
  buffer_cfg_.draw_in_psram = false;
  buffer_cfg_.full_frame = false;
  buffer_cfg_.double_buffer = false;

  if (kUseFullFrameBenchRuntime) {
    size_t full_pixels = 0U;
    size_t full_bytes = 0U;
    if (!runtime::memory::safeMulSize(static_cast<size_t>(width), static_cast<size_t>(height), &full_pixels) ||
        !runtime::memory::safeMulSize(full_pixels, sizeof(lv_color_t), &full_bytes)) {
      Serial.println("[UI] full-frame size overflow, fallback to line buffers");
      full_pixels = 0U;
      full_bytes = 0U;
    }
    lv_color_t* full = nullptr;
#if defined(ARDUINO_ARCH_ESP32)
    if (full_bytes > 0U) {
      const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      if (free_psram > (full_bytes + kFullFrameBenchMinFreePsram)) {
        full = static_cast<lv_color_t*>(
            runtime::memory::CapsAllocator::allocPsram(full_bytes, "ui.full_frame_bench"));
      }
    }
#else
    if (full_bytes > 0U) {
      full = static_cast<lv_color_t*>(
          runtime::memory::CapsAllocator::allocDefault(full_bytes, "ui.full_frame_bench"));
    }
#endif
    if (full != nullptr) {
      full_frame_buf_ = full;
      full_frame_buf_owned_ = true;
      draw_buf1_ = full_frame_buf_;
      draw_buf1_owned_ = false;
      draw_buf2_ = nullptr;
      draw_buf2_owned_ = false;
      buffer_cfg_.lines = height;
      buffer_cfg_.full_frame = true;
      buffer_cfg_.double_buffer = false;
      buffer_cfg_.draw_in_psram = true;
      Serial.printf("[UI] draw buffer full-frame bench enabled bytes=%u\n",
                    static_cast<unsigned int>(full_bytes));
      return true;
    }
    Serial.println("[UI] full-frame bench requested but unavailable, fallback to line buffers");
  }

  uint16_t line_candidates[12] = {0};
  uint8_t candidate_count = 0U;
  auto add_line_candidate = [&](uint16_t lines) {
    if (lines == 0U) {
      return;
    }
    if (lines > height) {
      lines = height;
    }
    for (uint8_t index = 0U; index < candidate_count; ++index) {
      if (line_candidates[index] == lines) {
        return;
      }
    }
    if (candidate_count < static_cast<uint8_t>(sizeof(line_candidates) / sizeof(line_candidates[0]))) {
      line_candidates[candidate_count++] = lines;
    }
  };
  add_line_candidate(kDrawBufLinesRequested != 0U ? kDrawBufLinesRequested : 40U);
  for (uint8_t index = 0U; index < (sizeof(kDrawLineFallbacks) / sizeof(kDrawLineFallbacks[0])); ++index) {
    add_line_candidate(kDrawLineFallbacks[index]);
  }
  add_line_candidate(20U);
  add_line_candidate(16U);
  add_line_candidate(12U);
  add_line_candidate(8U);
  add_line_candidate(6U);
  add_line_candidate(4U);
  add_line_candidate(2U);
  add_line_candidate(1U);

  auto allocate_buffer = [&](size_t bytes, bool psram, const char* tag) -> lv_color_t* {
    if (bytes == 0U) {
      return nullptr;
    }
    if (psram) {
      return static_cast<lv_color_t*>(
          runtime::memory::CapsAllocator::allocPsram(bytes, tag));
    }
    return static_cast<lv_color_t*>(
        runtime::memory::CapsAllocator::allocInternalDma(bytes, tag));
  };

  auto try_allocate_draw = [&](bool draw_in_psram) -> bool {
    for (uint8_t index = 0U; index < candidate_count; ++index) {
      const uint16_t lines = line_candidates[index];
      if (lines == 0U) {
        continue;
      }
      size_t pixels = 0U;
      size_t bytes = 0U;
      if (!runtime::memory::safeMulSize(static_cast<size_t>(width), static_cast<size_t>(lines), &pixels) ||
          !runtime::memory::safeMulSize(pixels, sizeof(lv_color_t), &bytes)) {
        Serial.printf("[UI] draw buffer size overflow lines=%u\n", static_cast<unsigned int>(lines));
        continue;
      }
      lv_color_t* first = allocate_buffer(bytes, draw_in_psram, "ui.draw.first");
      if (first == nullptr) {
        continue;
      }
      lv_color_t* second = allocate_buffer(bytes, draw_in_psram, "ui.draw.second");
      if (second != nullptr) {
        draw_buf1_ = first;
        draw_buf2_ = second;
        draw_buf1_owned_ = true;
        draw_buf2_owned_ = true;
        buffer_cfg_.lines = lines;
        buffer_cfg_.double_buffer = true;
        buffer_cfg_.draw_in_psram = draw_in_psram;
        Serial.printf("[UI] draw buffers ready lines=%u bytes=%u source=%s double=1\n",
                      static_cast<unsigned int>(lines),
                      static_cast<unsigned int>(bytes),
                      draw_in_psram ? "PSRAM" : "SRAM_DMA");
        return true;
      }

      draw_buf1_ = first;
      draw_buf2_ = nullptr;
      draw_buf1_owned_ = true;
      draw_buf2_owned_ = false;
      buffer_cfg_.lines = lines;
      buffer_cfg_.double_buffer = false;
      buffer_cfg_.draw_in_psram = draw_in_psram;
      Serial.printf("[UI] draw buffer fallback mono lines=%u bytes=%u source=%s\n",
                    static_cast<unsigned int>(lines),
                    static_cast<unsigned int>(bytes),
                    draw_in_psram ? "PSRAM" : "SRAM_DMA");
      return true;
    }
    return false;
  };

  const bool prefer_psram_for_trans = kUseColor256Runtime || kUsePsramLineBuffersRuntime;
  const bool preferred_psram = prefer_psram_for_trans;
  bool allocated = try_allocate_draw(preferred_psram);
  if (!allocated) {
    allocated = try_allocate_draw(!preferred_psram);
    if (allocated) {
      Serial.printf("[UI] draw buffer source fallback=%s\n",
                    (!preferred_psram) ? "PSRAM" : "SRAM_DMA");
    }
  }
  if (!allocated) {
    return false;
  }

  const bool needs_trans_buffer = kUseColor256Runtime || buffer_cfg_.draw_in_psram;
  if (needs_trans_buffer) {
    uint16_t trans_line_candidates[12] = {0};
    uint8_t trans_candidate_count = 0U;
    auto add_trans_candidate = [&](uint16_t lines) {
      if (lines == 0U) {
        return;
      }
      if (lines > height) {
        lines = height;
      }
      for (uint8_t i = 0U; i < trans_candidate_count; ++i) {
        if (trans_line_candidates[i] == lines) {
          return;
        }
      }
      if (trans_candidate_count <
          static_cast<uint8_t>(sizeof(trans_line_candidates) / sizeof(trans_line_candidates[0]))) {
        trans_line_candidates[trans_candidate_count++] = lines;
      }
    };
    const uint16_t requested_trans_lines = (kDmaTransBufLinesRequested != 0U)
                                               ? kDmaTransBufLinesRequested
                                               : buffer_cfg_.lines;
    add_trans_candidate(requested_trans_lines);
    add_trans_candidate(buffer_cfg_.lines);
    add_trans_candidate(24U);
    add_trans_candidate(16U);
    add_trans_candidate(12U);
    add_trans_candidate(8U);
    add_trans_candidate(6U);
    add_trans_candidate(4U);
    add_trans_candidate(2U);
    add_trans_candidate(1U);

    uint16_t selected_trans_lines = 0U;
#if defined(ARDUINO_ARCH_ESP32)
    for (uint8_t index = 0U; index < trans_candidate_count; ++index) {
      const uint16_t trans_lines = trans_line_candidates[index];
      size_t trans_pixels = 0U;
      size_t trans_bytes = 0U;
      if (!runtime::memory::safeMulSize(static_cast<size_t>(width), static_cast<size_t>(trans_lines), &trans_pixels) ||
          !runtime::memory::safeMulSize(trans_pixels, sizeof(uint16_t), &trans_bytes)) {
        Serial.printf("[UI] trans buffer size overflow lines=%u\n",
                      static_cast<unsigned int>(trans_lines));
        continue;
      }
      dma_trans_buf_ = static_cast<uint16_t*>(
          kUseDmaTxInDramRuntime
              ? runtime::memory::CapsAllocator::allocInternalDma(trans_bytes, "ui.trans")
              : runtime::memory::CapsAllocator::allocDefault(trans_bytes, "ui.trans"));
      if (dma_trans_buf_ != nullptr) {
        dma_trans_buf_owned_ = true;
        dma_trans_buf_pixels_ = trans_pixels;
        selected_trans_lines = trans_lines;
        break;
      }
    }
#else
    if (trans_candidate_count > 0U) {
      const uint16_t trans_lines = trans_line_candidates[0];
      size_t trans_pixels = 0U;
      size_t trans_bytes = 0U;
      if (!runtime::memory::safeMulSize(static_cast<size_t>(width), static_cast<size_t>(trans_lines), &trans_pixels) ||
          !runtime::memory::safeMulSize(trans_pixels, sizeof(uint16_t), &trans_bytes)) {
        Serial.printf("[UI] trans buffer size overflow lines=%u\n",
                      static_cast<unsigned int>(trans_lines));
      } else {
        dma_trans_buf_ = static_cast<uint16_t*>(
            runtime::memory::CapsAllocator::allocDefault(trans_bytes, "ui.trans"));
      }
      if (dma_trans_buf_ != nullptr) {
        dma_trans_buf_owned_ = true;
        dma_trans_buf_pixels_ = trans_pixels;
        selected_trans_lines = trans_lines;
      }
    }
#endif
    if (dma_trans_buf_ != nullptr && selected_trans_lines > 0U) {
      Serial.printf("[UI] trans buffer ready lines=%u pixels=%u source=%s\n",
                    static_cast<unsigned int>(selected_trans_lines),
                    static_cast<unsigned int>(dma_trans_buf_pixels_),
                    kUseDmaTxInDramRuntime ? "INTERNAL_DMA" : "DEFAULT");
      if (kUseAsyncDmaRuntime && !kUseColor256Runtime && selected_trans_lines < buffer_cfg_.lines) {
        Serial.printf("[UI] draw lines reduced for trans buffer: %u -> %u\n",
                      static_cast<unsigned int>(buffer_cfg_.lines),
                      static_cast<unsigned int>(selected_trans_lines));
        buffer_cfg_.lines = selected_trans_lines;
      }
    } else {
      dma_trans_buf_owned_ = false;
      dma_trans_buf_pixels_ = 0U;
      Serial.println("[UI] trans buffer unavailable; async DMA may be disabled");
    }
  }

  return draw_buf1_ != nullptr;
}

bool UiManager::initDmaEngine() {
  dma_requested_ = kUseAsyncDmaRuntime;
  dma_available_ = false;
  async_flush_enabled_ = false;
  if (!dma_requested_) {
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  dma_available_ = drivers::display::displayHal().initDma(false);
  if (!dma_available_) {
    Serial.println("[UI] DMA engine unavailable, keeping sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  if (kUseColor256Runtime) {
    Serial.println("[UI] DMA async disabled in RGB332 mode (stability guard)");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  const bool needs_trans_buffer = kUseColor256Runtime || buffer_cfg_.draw_in_psram;
  if (needs_trans_buffer && dma_trans_buf_ == nullptr) {
    Serial.println("[UI] DMA enabled but trans buffer missing, keeping sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  if (buffer_cfg_.full_frame) {
    Serial.println("[UI] full-frame bench forces sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  async_flush_enabled_ = true;
  buffer_cfg_.dma_enabled = true;
  Serial.println("[UI] DMA async flush enabled");
  return true;
}

void UiManager::pollAsyncFlush() {
  if (!flush_ctx_.pending) {
    return;
  }
  if (flush_ctx_.using_dma && dma_available_ && drivers::display::displayHal().dmaBusy()) {
    graphics_stats_.flush_busy_poll_count += 1U;
    return;
  }
  completePendingFlush();
}

void UiManager::completePendingFlush() {
  if (!flush_ctx_.pending) {
    return;
  }
  if (flush_ctx_.using_dma) {
    drivers::display::displayHal().endWrite();
  }
  if (flush_ctx_.disp != nullptr) {
    lv_disp_flush_ready(flush_ctx_.disp);
  }

  const uint32_t elapsed_us = micros() - flush_ctx_.started_ms;
  graphics_stats_.flush_count += 1U;
  if (flush_ctx_.using_dma) {
    graphics_stats_.dma_flush_count += 1U;
  } else {
    graphics_stats_.sync_flush_count += 1U;
  }
  graphics_stats_.flush_time_total_us += elapsed_us;
  if (elapsed_us > graphics_stats_.flush_time_max_us) {
    graphics_stats_.flush_time_max_us = elapsed_us;
  }
  perfMonitor().noteUiFlush(flush_ctx_.using_dma, elapsed_us);
  flush_ctx_ = {};
}

uint16_t UiManager::convertLineRgb332ToRgb565(const lv_color_t* src,
                                              uint16_t* dst,
                                              uint32_t px_count) const {
  if (src == nullptr || dst == nullptr || px_count == 0U || !color_lut_ready_) {
    return 0U;
  }
  for (uint32_t index = 0U; index < px_count; ++index) {
#if LV_COLOR_DEPTH == 8
    const uint8_t rgb332 = src[index].full;
    dst[index] = rgb332_to_565_lut_[rgb332];
#else
    dst[index] = src[index].full;
#endif
  }
  return static_cast<uint16_t>((px_count > 0xFFFFU) ? 0xFFFFU : px_count);
}

lv_color_t UiManager::quantize565ToTheme256(lv_color_t color) const {
  if (!kUseThemeQuantizeRuntime) {
    return color;
  }
#if LV_COLOR_DEPTH == 16
  lv_color32_t c32 = {};
  c32.full = lv_color_to32(color);
  const uint8_t r3 = static_cast<uint8_t>((static_cast<uint16_t>(c32.ch.red) * 7U + 127U) / 255U);
  const uint8_t g3 = static_cast<uint8_t>((static_cast<uint16_t>(c32.ch.green) * 7U + 127U) / 255U);
  const uint8_t b2 = static_cast<uint8_t>((static_cast<uint16_t>(c32.ch.blue) * 3U + 127U) / 255U);
  const uint8_t rq = static_cast<uint8_t>((static_cast<uint16_t>(r3) * 255U) / 7U);
  const uint8_t gq = static_cast<uint8_t>((static_cast<uint16_t>(g3) * 255U) / 7U);
  const uint8_t bq = static_cast<uint8_t>((static_cast<uint16_t>(b2) * 255U) / 3U);
  return lv_color_make(rq, gq, bq);
#else
  return color;
#endif
}

void UiManager::dumpGraphicsStatus() const {
  const uint32_t avg_us = (graphics_stats_.flush_count == 0U)
                              ? 0U
                              : (graphics_stats_.flush_time_total_us / graphics_stats_.flush_count);
  Serial.printf(
      "[UI] GFX_STATUS depth=%u mode=%s theme256=%u lines=%u double=%u source=%s full_frame=%u dma_req=%u dma_async=%u trans_px=%u pending=%u flush=%lu dma=%lu sync=%lu avg_us=%lu max_us=%lu\n",
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
      flush_ctx_.pending ? 1U : 0U,
      static_cast<unsigned long>(graphics_stats_.flush_count),
      static_cast<unsigned long>(graphics_stats_.dma_flush_count),
      static_cast<unsigned long>(graphics_stats_.sync_flush_count),
      static_cast<unsigned long>(avg_us),
      static_cast<unsigned long>(graphics_stats_.flush_time_max_us));
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
  return snapshot;
}

void UiManager::dumpMemoryStatus() const {
  const UiMemorySnapshot snapshot = memorySnapshot();
#if LV_USE_MEM_MONITOR
  Serial.printf("[UI] LV_MEM used=%u free=%u frag=%u%% max_used=%u\n",
                static_cast<unsigned int>(snapshot.lv_mem_used),
                static_cast<unsigned int>(snapshot.lv_mem_free),
                static_cast<unsigned int>(snapshot.lv_mem_frag_pct),
                static_cast<unsigned int>(snapshot.lv_mem_max_used));
#else
  Serial.println("[UI] LV_MEM monitor disabled at compile-time");
#endif
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[UI] HEAP internal=%u dma=%u psram=%u largest_dma=%u\n",
                static_cast<unsigned int>(snapshot.heap_internal_free),
                static_cast<unsigned int>(snapshot.heap_dma_free),
                static_cast<unsigned int>(snapshot.heap_psram_free),
                static_cast<unsigned int>(snapshot.heap_largest_dma_block));
#endif
  Serial.printf("[UI] MEM_SNAPSHOT draw_lines=%u draw_psram=%u full_frame=%u dma_async=%u draw_bytes=%u trans_bytes=%u alloc_fail=%lu\n",
                static_cast<unsigned int>(snapshot.draw_lines),
                snapshot.draw_in_psram ? 1U : 0U,
                snapshot.full_frame ? 1U : 0U,
                snapshot.dma_async_enabled ? 1U : 0U,
                static_cast<unsigned int>(snapshot.draw_buffer_bytes),
                static_cast<unsigned int>(snapshot.trans_buffer_bytes),
                static_cast<unsigned long>(snapshot.alloc_failures));
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

void UiManager::resetIntroConfigDefaults() {
  copyStringBounded(intro_config_.logo_text, sizeof(intro_config_.logo_text), "Professeur ZACUS");
  copyStringBounded(intro_config_.crack_scroll,
                    sizeof(intro_config_.crack_scroll),
                    kWinEtapeCracktroScroll);
  copyStringBounded(intro_config_.crack_bottom_scroll,
                    sizeof(intro_config_.crack_bottom_scroll),
                    kWinEtapeCracktroBottomScroll);
  copyStringBounded(intro_config_.clean_title, sizeof(intro_config_.clean_title), kWinEtapeDemoTitle);
  copyStringBounded(intro_config_.clean_scroll, sizeof(intro_config_.clean_scroll), kWinEtapeDemoScroll);
  intro_config_.a_duration_ms = kIntroCracktroMsDefault;
  intro_config_.b_duration_ms = kIntroTransitionMsDefault;
  intro_config_.c_duration_ms = kIntroCleanMsDefault;
  intro_config_.b1_crash_ms = kIntroB1CrashMsDefault;
  intro_config_.scroll_a_px_per_sec = kIntroScrollApxPerSecDefault;
  intro_config_.scroll_bot_a_px_per_sec = kIntroScrollBotApxPerSecDefault;
  intro_config_.scroll_c_px_per_sec = kIntroScrollCpxPerSecDefault;
  intro_config_.sine_amp_a_px = kIntroSineAmpApxDefault;
  intro_config_.sine_amp_c_px = kIntroSineAmpCpxDefault;
  intro_config_.sine_period_px = kIntroSinePeriodPxDefault;
  intro_config_.sine_phase_speed = kIntroSinePhaseSpeedDefault;
  intro_config_.stars_override = -1;
  copyStringBounded(intro_config_.fx_3d, sizeof(intro_config_.fx_3d), "rotozoom");
  copyStringBounded(intro_config_.fx_3d_quality, sizeof(intro_config_.fx_3d_quality), "auto");
  copyStringBounded(intro_config_.font_mode, sizeof(intro_config_.font_mode), "orbitron");
}

void UiManager::parseSceneWinEtapeTxtOverrides(const char* payload) {
  if (payload == nullptr || payload[0] == '\0') {
    return;
  }
  String text(payload);
  int32_t start = 0;
  while (start <= text.length()) {
    int32_t end = text.indexOf('\n', start);
    if (end < 0) {
      end = text.length();
    }
    String line = text.substring(start, end);
    start = end + 1;
    line = trimCopy(line);
    if (line.length() == 0U || line.startsWith("#")) {
      continue;
    }
    const int32_t comment_pos = line.indexOf('#');
    if (comment_pos >= 0) {
      line = trimCopy(line.substring(0, comment_pos));
    }
    if (line.length() == 0U) {
      continue;
    }

    const int32_t sep = line.indexOf('=');
    if (sep <= 0) {
      continue;
    }
    String key = trimCopy(line.substring(0, sep));
    String value = trimCopy(line.substring(sep + 1));
    key.toUpperCase();

    if (key == "LOGO_TEXT") {
      copyStringBounded(intro_config_.logo_text, sizeof(intro_config_.logo_text), value.c_str());
      continue;
    }
    if (key == "A_SCROLL" || key == "MID_A_SCROLL" || key == "CRACK_SCROLL") {
      copyStringBounded(intro_config_.crack_scroll, sizeof(intro_config_.crack_scroll), value.c_str());
      continue;
    }
    if (key == "A_SCROLL_BOTTOM" || key == "BOTTOM_SCROLL" || key == "BOT_A_SCROLL") {
      copyStringBounded(intro_config_.crack_bottom_scroll,
                        sizeof(intro_config_.crack_bottom_scroll),
                        value.c_str());
      continue;
    }
    if (key == "C_TITLE" || key == "CLEAN_TITLE") {
      copyStringBounded(intro_config_.clean_title, sizeof(intro_config_.clean_title), value.c_str());
      continue;
    }
    if (key == "C_SCROLL" || key == "CLEAN_SCROLL") {
      copyStringBounded(intro_config_.clean_scroll, sizeof(intro_config_.clean_scroll), value.c_str());
      continue;
    }

    uint32_t parsed_u32 = 0U;
    if (key == "A_MS" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.a_duration_ms = parsed_u32;
      continue;
    }
    if (key == "B_MS" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.b_duration_ms = parsed_u32;
      continue;
    }
    if (key == "C_MS" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.c_duration_ms = parsed_u32;
      continue;
    }
    if ((key == "B1_MS" || key == "B1_CRASH_MS") && parseUint32Text(value, &parsed_u32)) {
      intro_config_.b1_crash_ms = static_cast<uint16_t>(parsed_u32);
      continue;
    }
    if ((key == "SPEED_A" || key == "SPEED_MID_A") && parseUint32Text(value, &parsed_u32)) {
      intro_config_.scroll_a_px_per_sec = static_cast<uint16_t>(parsed_u32);
      continue;
    }
    if ((key == "SPEED_BOT_A" || key == "SPEED_A_BOTTOM") && parseUint32Text(value, &parsed_u32)) {
      intro_config_.scroll_bot_a_px_per_sec = static_cast<uint16_t>(parsed_u32);
      continue;
    }
    if (key == "SPEED_C" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.scroll_c_px_per_sec = static_cast<uint16_t>(parsed_u32);
      continue;
    }
    if (key == "SINE_AMP_A" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.sine_amp_a_px = static_cast<uint8_t>(parsed_u32);
      continue;
    }
    if (key == "SINE_AMP_C" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.sine_amp_c_px = static_cast<uint8_t>(parsed_u32);
      continue;
    }
    if (key == "SINE_PERIOD" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.sine_period_px = static_cast<uint16_t>(parsed_u32);
      continue;
    }

    float parsed_float = 0.0f;
    if (key == "SINE_PHASE_SPEED" && parseFloatText(value, &parsed_float)) {
      intro_config_.sine_phase_speed = parsed_float;
      continue;
    }

    if (key == "STARS") {
      String normalized = value;
      normalized.toLowerCase();
      if (normalized == "auto") {
        intro_config_.stars_override = -1;
      } else {
        int16_t parsed_i16 = 0;
        if (parseInt16Text(value, &parsed_i16)) {
          intro_config_.stars_override = parsed_i16;
        }
      }
      continue;
    }

    if (key == "FX_3D") {
      copyStringBounded(intro_config_.fx_3d, sizeof(intro_config_.fx_3d), value.c_str());
      continue;
    }
    if (key == "FX_3D_QUALITY") {
      copyStringBounded(intro_config_.fx_3d_quality,
                        sizeof(intro_config_.fx_3d_quality),
                        value.c_str());
      continue;
    }
    if (key == "FONT_MODE") {
      copyStringBounded(intro_config_.font_mode, sizeof(intro_config_.font_mode), value.c_str());
      continue;
    }
  }
}

void UiManager::parseSceneWinEtapeJsonOverrides(const char* payload, const char* path_for_log) {
  if (payload == nullptr || payload[0] == '\0') {
    return;
  }
  DynamicJsonDocument doc(4096);
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[UI] intro overrides parse error path=%s err=%s defaults\n",
                  (path_for_log != nullptr) ? path_for_log : "n/a",
                  err.c_str());
    return;
  }

  const char* logo_text = doc["logo_text"] | doc["LOGO_TEXT"] | doc["logoText"] | "";
  const char* crack_scroll =
      doc["crack_scroll"] | doc["CRACK_SCROLL"] | doc["A_SCROLL"] | doc["MID_A_SCROLL"] |
      doc["mid_a_scroll"] |
      doc["crackScroll"] | "";
  const char* crack_bottom_scroll =
      doc["crack_bottom_scroll"] | doc["BOTTOM_SCROLL"] | doc["A_SCROLL_BOTTOM"] |
      doc["bot_a_scroll"] |
      doc["BOT_A_SCROLL"] | "";
  const char* clean_title =
      doc["clean_title"] | doc["CLEAN_TITLE"] | doc["C_TITLE"] | doc["cleanTitle"] | "";
  const char* clean_scroll =
      doc["clean_scroll"] | doc["CLEAN_SCROLL"] | doc["C_SCROLL"] | doc["cleanScroll"] | "";

  if (logo_text[0] != '\0') {
    copyStringBounded(intro_config_.logo_text, sizeof(intro_config_.logo_text), logo_text);
  }
  if (crack_scroll[0] != '\0') {
    copyStringBounded(intro_config_.crack_scroll, sizeof(intro_config_.crack_scroll), crack_scroll);
  }
  if (crack_bottom_scroll[0] != '\0') {
    copyStringBounded(intro_config_.crack_bottom_scroll,
                      sizeof(intro_config_.crack_bottom_scroll),
                      crack_bottom_scroll);
  }
  if (clean_title[0] != '\0') {
    copyStringBounded(intro_config_.clean_title, sizeof(intro_config_.clean_title), clean_title);
  }
  if (clean_scroll[0] != '\0') {
    copyStringBounded(intro_config_.clean_scroll, sizeof(intro_config_.clean_scroll), clean_scroll);
  }

  if (doc["A_MS"].is<unsigned int>()) {
    intro_config_.a_duration_ms = doc["A_MS"].as<unsigned int>();
  } else if (doc["a_ms"].is<unsigned int>()) {
    intro_config_.a_duration_ms = doc["a_ms"].as<unsigned int>();
  }
  if (doc["B_MS"].is<unsigned int>()) {
    intro_config_.b_duration_ms = doc["B_MS"].as<unsigned int>();
  } else if (doc["b_ms"].is<unsigned int>()) {
    intro_config_.b_duration_ms = doc["b_ms"].as<unsigned int>();
  }
  if (doc["C_MS"].is<unsigned int>()) {
    intro_config_.c_duration_ms = doc["C_MS"].as<unsigned int>();
  } else if (doc["c_ms"].is<unsigned int>()) {
    intro_config_.c_duration_ms = doc["c_ms"].as<unsigned int>();
  }
  if (doc["B1_MS"].is<unsigned int>()) {
    intro_config_.b1_crash_ms = static_cast<uint16_t>(doc["B1_MS"].as<unsigned int>());
  } else if (doc["b1_ms"].is<unsigned int>()) {
    intro_config_.b1_crash_ms = static_cast<uint16_t>(doc["b1_ms"].as<unsigned int>());
  }
  if (doc["SPEED_MID_A"].is<unsigned int>()) {
    intro_config_.scroll_a_px_per_sec = static_cast<uint16_t>(doc["SPEED_MID_A"].as<unsigned int>());
  } else if (doc["speed_mid_a"].is<unsigned int>()) {
    intro_config_.scroll_a_px_per_sec = static_cast<uint16_t>(doc["speed_mid_a"].as<unsigned int>());
  } else if (doc["SPEED_A"].is<unsigned int>()) {
    intro_config_.scroll_a_px_per_sec = static_cast<uint16_t>(doc["SPEED_A"].as<unsigned int>());
  } else if (doc["speed_a"].is<unsigned int>()) {
    intro_config_.scroll_a_px_per_sec = static_cast<uint16_t>(doc["speed_a"].as<unsigned int>());
  }
  if (doc["SPEED_BOT_A"].is<unsigned int>()) {
    intro_config_.scroll_bot_a_px_per_sec = static_cast<uint16_t>(doc["SPEED_BOT_A"].as<unsigned int>());
  } else if (doc["speed_bot_a"].is<unsigned int>()) {
    intro_config_.scroll_bot_a_px_per_sec = static_cast<uint16_t>(doc["speed_bot_a"].as<unsigned int>());
  }
  if (doc["SPEED_C"].is<unsigned int>()) {
    intro_config_.scroll_c_px_per_sec = static_cast<uint16_t>(doc["SPEED_C"].as<unsigned int>());
  } else if (doc["speed_c"].is<unsigned int>()) {
    intro_config_.scroll_c_px_per_sec = static_cast<uint16_t>(doc["speed_c"].as<unsigned int>());
  }
  if (doc["SINE_AMP_A"].is<unsigned int>()) {
    intro_config_.sine_amp_a_px = static_cast<uint8_t>(doc["SINE_AMP_A"].as<unsigned int>());
  } else if (doc["sine_amp_a"].is<unsigned int>()) {
    intro_config_.sine_amp_a_px = static_cast<uint8_t>(doc["sine_amp_a"].as<unsigned int>());
  }
  if (doc["SINE_AMP_C"].is<unsigned int>()) {
    intro_config_.sine_amp_c_px = static_cast<uint8_t>(doc["SINE_AMP_C"].as<unsigned int>());
  } else if (doc["sine_amp_c"].is<unsigned int>()) {
    intro_config_.sine_amp_c_px = static_cast<uint8_t>(doc["sine_amp_c"].as<unsigned int>());
  }
  if (doc["SINE_PERIOD"].is<unsigned int>()) {
    intro_config_.sine_period_px = static_cast<uint16_t>(doc["SINE_PERIOD"].as<unsigned int>());
  } else if (doc["sine_period"].is<unsigned int>()) {
    intro_config_.sine_period_px = static_cast<uint16_t>(doc["sine_period"].as<unsigned int>());
  }
  if (doc["SINE_PHASE_SPEED"].is<float>()) {
    intro_config_.sine_phase_speed = doc["SINE_PHASE_SPEED"].as<float>();
  } else if (doc["sine_phase_speed"].is<float>()) {
    intro_config_.sine_phase_speed = doc["sine_phase_speed"].as<float>();
  }

  if (doc["STARS"].is<const char*>()) {
    const char* stars_text = doc["STARS"].as<const char*>();
    if (stars_text != nullptr && std::strcmp(stars_text, "auto") == 0) {
      intro_config_.stars_override = -1;
    }
  } else if (doc["stars"].is<const char*>()) {
    const char* stars_text = doc["stars"].as<const char*>();
    if (stars_text != nullptr && std::strcmp(stars_text, "auto") == 0) {
      intro_config_.stars_override = -1;
    }
  }
  if (doc["STARS"].is<int>()) {
    intro_config_.stars_override = static_cast<int16_t>(doc["STARS"].as<int>());
  } else if (doc["stars"].is<int>()) {
    intro_config_.stars_override = static_cast<int16_t>(doc["stars"].as<int>());
  }

  const char* fx_3d = doc["FX_3D"] | doc["fx_3d"] | "";
  if (fx_3d[0] != '\0') {
    copyStringBounded(intro_config_.fx_3d, sizeof(intro_config_.fx_3d), fx_3d);
  }
  const char* fx_3d_quality = doc["FX_3D_QUALITY"] | doc["fx_3d_quality"] | "";
  if (fx_3d_quality[0] != '\0') {
    copyStringBounded(intro_config_.fx_3d_quality,
                      sizeof(intro_config_.fx_3d_quality),
                      fx_3d_quality);
  }
  const char* font_mode = doc["FONT_MODE"] | doc["font_mode"] | doc["fontMode"] | "";
  if (font_mode[0] != '\0') {
    copyStringBounded(intro_config_.font_mode, sizeof(intro_config_.font_mode), font_mode);
  }

  Serial.printf("[UI] intro overrides loaded from %s\n",
                (path_for_log != nullptr) ? path_for_log : "json");
}

void UiManager::loadSceneWinEtapeOverrides() {
  resetIntroConfigDefaults();
  const char* const candidates[] = {
      "/ui/scene_win_etape.json",
      "/SCENE_WIN_ETAPE.json",
      "/ui/SCENE_WIN_ETAPE.json",
      "/ui/scene_win_etape.txt",
  };

  String payload;
  String loaded_path;

  for (const char* path : candidates) {
    if (path == nullptr || path[0] == '\0') {
      continue;
    }
    if (!LittleFS.exists(path)) {
      continue;
    }
    File file = LittleFS.open(path, "r");
    if (!file) {
      continue;
    }
    payload = file.readString();
    file.close();
    if (payload.isEmpty()) {
      continue;
    }
    loaded_path = path;
    break;
  }

  if (!payload.isEmpty()) {
    String lower_path = loaded_path;
    lower_path.toLowerCase();
    if (lower_path.endsWith(".txt")) {
      parseSceneWinEtapeTxtOverrides(payload.c_str());
      Serial.printf("[UI] intro overrides loaded from %s\n", loaded_path.c_str());
    } else {
      parseSceneWinEtapeJsonOverrides(payload.c_str(), loaded_path.c_str());
    }
  } else {
    Serial.println("[UI] intro overrides: no file, defaults");
  }

  intro_config_.a_duration_ms =
      clampValue<uint32_t>(intro_config_.a_duration_ms, kIntroCracktroMsMin, kIntroCracktroMsMax);
  intro_config_.b_duration_ms =
      clampValue<uint32_t>(intro_config_.b_duration_ms, kIntroTransitionMsMin, kIntroTransitionMsMax);
  intro_config_.c_duration_ms =
      clampValue<uint32_t>(intro_config_.c_duration_ms, kIntroCleanMsMin, kIntroCleanMsMax);
  intro_config_.b1_crash_ms =
      clampValue<uint16_t>(intro_config_.b1_crash_ms, kIntroB1CrashMsMin, kIntroB1CrashMsMax);
  intro_config_.scroll_a_px_per_sec =
      clampValue<uint16_t>(intro_config_.scroll_a_px_per_sec, kIntroScrollSpeedMin, kIntroScrollSpeedMax);
  intro_config_.scroll_bot_a_px_per_sec =
      clampValue<uint16_t>(intro_config_.scroll_bot_a_px_per_sec, kIntroScrollBotSpeedMin, kIntroScrollBotSpeedMax);
  intro_config_.scroll_c_px_per_sec =
      clampValue<uint16_t>(intro_config_.scroll_c_px_per_sec, kIntroScrollSpeedMin, kIntroScrollSpeedMax);
  intro_config_.sine_amp_a_px =
      clampValue<uint8_t>(intro_config_.sine_amp_a_px, kIntroSineAmpMin, kIntroSineAmpMax);
  intro_config_.sine_amp_c_px =
      clampValue<uint8_t>(intro_config_.sine_amp_c_px, kIntroSineAmpMin, kIntroSineAmpMax);
  intro_config_.sine_period_px =
      clampValue<uint16_t>(intro_config_.sine_period_px, kIntroSinePeriodMin, kIntroSinePeriodMax);
  intro_config_.sine_phase_speed =
      clampValue<float>(intro_config_.sine_phase_speed, kIntroSinePhaseSpeedMin, kIntroSinePhaseSpeedMax);

  if (intro_config_.stars_override < 0) {
    intro_config_.stars_override = -1;
  } else {
    intro_config_.stars_override =
        clampValue<int16_t>(intro_config_.stars_override, static_cast<int16_t>(60), static_cast<int16_t>(220));
  }
  if (intro_config_.fx_3d[0] == '\0') {
    copyStringBounded(intro_config_.fx_3d, sizeof(intro_config_.fx_3d), "rotozoom");
  }
  if (intro_config_.fx_3d_quality[0] == '\0') {
    copyStringBounded(intro_config_.fx_3d_quality, sizeof(intro_config_.fx_3d_quality), "auto");
  }
  if (intro_config_.font_mode[0] == '\0') {
    copyStringBounded(intro_config_.font_mode, sizeof(intro_config_.font_mode), "orbitron");
  }

  intro_b1_crash_ms_ = intro_config_.b1_crash_ms;
  intro_scroll_mid_a_px_per_sec_ = intro_config_.scroll_a_px_per_sec;
  intro_scroll_bot_a_px_per_sec_ = intro_config_.scroll_bot_a_px_per_sec;
}

void UiManager::ensureIntroCreated() {
  if (intro_created_ || scene_root_ == nullptr) {
    return;
  }

  intro_root_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(intro_root_);
  lv_obj_set_size(intro_root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(intro_root_, introPaletteColor(0), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(intro_root_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(intro_root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(intro_root_, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t i = 0U; i < 4U; ++i) {
    intro_gradient_layers_[i] = lv_obj_create(intro_root_);
    lv_obj_remove_style_all(intro_gradient_layers_[i]);
    lv_obj_set_size(intro_gradient_layers_[i], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(intro_gradient_layers_[i], LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(intro_gradient_layers_[i], LV_OBJ_FLAG_HIDDEN);
  }

  for (lv_obj_t* bar : scene_cracktro_bars_) {
    if (bar == nullptr) {
      continue;
    }
    lv_obj_set_parent(bar, intro_root_);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }

  for (lv_obj_t* star : scene_starfield_) {
    if (star == nullptr) {
      continue;
    }
    lv_obj_set_parent(star, intro_root_);
    lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
  }

  intro_logo_shadow_label_ = lv_label_create(intro_root_);
  intro_logo_label_ = lv_label_create(intro_root_);
  intro_crack_scroll_label_ = lv_label_create(intro_root_);
  intro_bottom_scroll_label_ = lv_label_create(intro_root_);
  intro_clean_title_shadow_label_ = lv_label_create(intro_root_);
  intro_clean_title_label_ = lv_label_create(intro_root_);
  intro_clean_scroll_label_ = lv_label_create(intro_root_);
  intro_debug_label_ = lv_label_create(intro_root_);

  if (intro_logo_shadow_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_logo_shadow_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_logo_shadow_label_, introPaletteColor(8), LV_PART_MAIN);
    lv_obj_set_style_text_opa(intro_logo_shadow_label_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(intro_logo_shadow_label_, 2, LV_PART_MAIN);
    lv_obj_add_flag(intro_logo_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_logo_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_logo_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_logo_label_, introPaletteColor(7), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(intro_logo_label_, 2, LV_PART_MAIN);
    lv_obj_add_flag(intro_logo_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_crack_scroll_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_crack_scroll_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_crack_scroll_label_, introPaletteColor(7), LV_PART_MAIN);
    lv_obj_add_flag(intro_crack_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_bottom_scroll_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_bottom_scroll_label_, UiFonts::fontMono(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_bottom_scroll_label_, introPaletteColor(5), LV_PART_MAIN);
    lv_obj_set_style_text_opa(intro_bottom_scroll_label_, LV_OPA_90, LV_PART_MAIN);
    lv_obj_add_flag(intro_bottom_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_shadow_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_clean_title_shadow_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_clean_title_shadow_label_, introPaletteColor(8), LV_PART_MAIN);
    lv_obj_set_style_text_opa(intro_clean_title_shadow_label_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(intro_clean_title_shadow_label_, 1, LV_PART_MAIN);
    lv_obj_add_flag(intro_clean_title_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_clean_title_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_clean_title_label_, introPaletteColor(7), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(intro_clean_title_label_, 1, LV_PART_MAIN);
    lv_obj_add_flag(intro_clean_title_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_scroll_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_clean_scroll_label_, UiFonts::fontTitleXL(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_clean_scroll_label_, introPaletteColor(7), LV_PART_MAIN);
    lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_debug_label_ != nullptr) {
    lv_obj_set_style_text_font(intro_debug_label_, UiFonts::fontBodyS(), LV_PART_MAIN);
    lv_obj_set_style_text_color(intro_debug_label_, introPaletteColor(9), LV_PART_MAIN);
    lv_obj_set_style_text_opa(intro_debug_label_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_align(intro_debug_label_, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_add_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
    IntroGlyphSlot& slot = intro_wave_slots_[i];
    slot.shadow = lv_label_create(intro_root_);
    slot.glyph = lv_label_create(intro_root_);
    if (slot.shadow != nullptr) {
      lv_obj_set_style_text_font(slot.shadow, UiFonts::fontTitleXL(), LV_PART_MAIN);
      lv_obj_set_style_text_color(slot.shadow, introPaletteColor(8), LV_PART_MAIN);
      lv_obj_set_style_text_opa(slot.shadow, LV_OPA_50, LV_PART_MAIN);
      lv_label_set_text(slot.shadow, " ");
      lv_obj_add_flag(slot.shadow, LV_OBJ_FLAG_HIDDEN);
    }
    if (slot.glyph != nullptr) {
      lv_obj_set_style_text_font(slot.glyph, UiFonts::fontTitleXL(), LV_PART_MAIN);
      lv_obj_set_style_text_color(slot.glyph, introPaletteColor(7), LV_PART_MAIN);
      lv_label_set_text(slot.glyph, " ");
      lv_obj_add_flag(slot.glyph, LV_OBJ_FLAG_HIDDEN);
    }
  }

  for (uint8_t i = 0U; i < kIntroWireEdgeCount; ++i) {
    intro_wire_points_[i][0] = {0, 0};
    intro_wire_points_[i][1] = {0, 0};
    intro_wire_lines_[i] = lv_line_create(intro_root_);
    lv_line_set_points(intro_wire_lines_[i], intro_wire_points_[i], 2);
    lv_obj_set_style_line_width(intro_wire_lines_[i], 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(intro_wire_lines_[i], introPaletteColor(3), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(intro_wire_lines_[i], true, LV_PART_MAIN);
    lv_obj_set_style_opa(intro_wire_lines_[i], LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_size(intro_wire_lines_[i], LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(intro_wire_lines_[i], LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
    intro_roto_stripes_[i] = lv_obj_create(intro_root_);
    lv_obj_remove_style_all(intro_roto_stripes_[i]);
    lv_obj_set_size(intro_roto_stripes_[i], 20, 3);
    lv_obj_set_style_bg_color(intro_roto_stripes_[i], introPaletteColor(11), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(intro_roto_stripes_[i], LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t i = 0U; i < 72U; ++i) {
    intro_firework_particles_[i] = lv_obj_create(intro_root_);
    lv_obj_remove_style_all(intro_firework_particles_[i]);
    lv_obj_set_size(intro_firework_particles_[i], 3, 3);
    lv_obj_set_style_bg_opa(intro_firework_particles_[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(intro_firework_particles_[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(intro_firework_particles_[i], introPaletteColor(7), LV_PART_MAIN);
    lv_obj_add_flag(intro_firework_particles_[i], LV_OBJ_FLAG_HIDDEN);
    intro_firework_states_[i] = {};
  }

  intro_created_ = true;
  resetIntroConfigDefaults();
}

uint32_t UiManager::nextIntroRandom() {
  intro_rng_state_ = pseudoRandom32(intro_rng_state_ + 0x9E3779B9UL);
  return intro_rng_state_;
}

void UiManager::createCopperBars(uint8_t count) {
  createCopperWavyRings(count);
}

void UiManager::updateCopperBars(uint32_t t_ms) {
  updateCopperWavyRings(t_ms);
}

void UiManager::createCopperWavyRings(uint8_t count) {
  count = clampValue<uint8_t>(count, 0U, kCracktroBarCount);
  intro_copper_count_ = count;
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const int16_t min_dim = static_cast<int16_t>((width < height) ? width : height);
  const int16_t base_d = static_cast<int16_t>(min_dim / 4);
  const int16_t max_d = static_cast<int16_t>(min_dim - 10);
  int16_t spacing = (count > 0U) ? static_cast<int16_t>((max_d - base_d) / static_cast<int16_t>(count + 1U)) : 6;
  if (spacing < 4) {
    spacing = 4;
  }

  for (uint8_t i = 0U; i < kCracktroBarCount; ++i) {
    lv_obj_t* bar = scene_cracktro_bars_[i];
    if (bar == nullptr) {
      continue;
    }
    lv_anim_del(bar, nullptr);
    if (i < intro_copper_count_) {
      int16_t diameter = static_cast<int16_t>(base_d + static_cast<int16_t>(i * spacing));
      if (diameter > max_d) {
        diameter = max_d;
      }
      lv_obj_set_size(bar, diameter, diameter);
      lv_obj_set_pos(bar,
                     static_cast<lv_coord_t>((width - diameter) / 2),
                     static_cast<lv_coord_t>((height - diameter) / 2));
      lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, introPaletteColor(3), LV_PART_MAIN);
    lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(bar, 0, LV_PART_MAIN);
  }
}

void UiManager::updateCopperWavyRings(uint32_t t_ms) {
  if (intro_copper_count_ == 0U) {
    return;
  }
  static constexpr uint8_t kPaletteIdx[] = {3U, 4U, 5U, 7U};
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const int16_t min_dim = static_cast<int16_t>((width < height) ? width : height);
  const int16_t base_d = static_cast<int16_t>(min_dim / 4);
  const int16_t max_d = static_cast<int16_t>(min_dim - 10);
  int16_t spacing =
      static_cast<int16_t>((max_d - base_d) / static_cast<int16_t>(intro_copper_count_ + 1U));
  if (spacing < 4) {
    spacing = 4;
  }
  const float t = static_cast<float>(t_ms) * 0.001f;
  const float phase_speed = 1.35f;
  for (uint8_t i = 0U; i < intro_copper_count_; ++i) {
    lv_obj_t* bar = scene_cracktro_bars_[i];
    if (bar == nullptr) {
      continue;
    }
    const float phase = (t * phase_speed) + static_cast<float>(i) * 0.44f;
    int16_t diameter = static_cast<int16_t>(base_d + static_cast<int16_t>(i * spacing));
    diameter = static_cast<int16_t>(diameter + static_cast<int16_t>(std::sin(phase * 1.25f) * 8.0f));
    diameter = clampValue<int16_t>(diameter, 18, max_d);
    const int16_t x = static_cast<int16_t>((width - diameter) / 2 +
                                           static_cast<int16_t>(std::sin(phase * 0.83f) * 7.0f));
    const int16_t y = static_cast<int16_t>((height - diameter) / 2 +
                                           static_cast<int16_t>(std::cos(phase * 0.91f) * 6.0f));
    const uint8_t palette_index =
        static_cast<uint8_t>((i + static_cast<uint8_t>((t_ms / 220U) % 4U)) % 4U);
    const float pulse = (std::sin(phase * 2.2f) + 1.0f) * 0.5f;
    lv_opa_t opa = static_cast<lv_opa_t>(80 + static_cast<uint8_t>(pulse * 130.0f));
    const uint8_t border_width = static_cast<uint8_t>(2U + static_cast<uint8_t>(pulse * 3.0f));
    if (intro_state_ == IntroState::PHASE_B_TRANSITION && intro_b1_done_) {
      opa = static_cast<lv_opa_t>(40 + static_cast<uint8_t>(pulse * 90.0f));
    }
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, diameter, diameter);
    lv_obj_set_style_border_width(bar, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, introPaletteColor(kPaletteIdx[palette_index]), LV_PART_MAIN);
    lv_obj_set_style_border_opa(bar, opa, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
  }
}

void UiManager::createStarfield(uint8_t count, uint8_t layers) {
  if (layers == 0U) {
    layers = 1U;
  }
  count = clampValue<uint8_t>(count, 0U, kStarfieldCount);
  intro_star_count_ = count;
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const bool clean_mode = (intro_state_ == IntroState::PHASE_C_CLEAN || intro_state_ == IntroState::PHASE_C_LOOP);
  const int16_t speeds_fast[3] = {54, 116, 198};
  const int16_t speeds_clean[3] = {26, 74, 154};

  const uint16_t layer0_end = static_cast<uint16_t>((count * 50U) / 100U);
  const uint16_t layer1_end = static_cast<uint16_t>((count * 80U) / 100U);

  for (uint8_t i = 0U; i < kStarfieldCount; ++i) {
    lv_obj_t* star = scene_starfield_[i];
    if (star == nullptr) {
      continue;
    }
    lv_anim_del(star, nullptr);
    if (i >= intro_star_count_) {
      lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    uint8_t layer = 0U;
    if (layers >= 3U) {
      if (i < layer0_end) {
        layer = 0U;
      } else if (i < layer1_end) {
        layer = 1U;
      } else {
        layer = 2U;
      }
    } else {
      layer = static_cast<uint8_t>(i % layers);
    }

    IntroStarState& state = intro_star_states_[i];
    state.layer = layer;
    state.size_px = static_cast<uint8_t>(1U + layer);
    const int16_t base_speed = clean_mode ? speeds_clean[layer] : speeds_fast[layer];
    state.speed_px_per_s = base_speed;
    state.x_q8 = static_cast<int32_t>(nextIntroRandom() % static_cast<uint32_t>(width)) << 8;
    state.y_q8 = static_cast<int32_t>(nextIntroRandom() % static_cast<uint32_t>(height)) << 8;

    lv_obj_set_size(star, state.size_px, state.size_px);
    lv_obj_set_style_radius(star, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(star, introPaletteColor(layer == 2U ? 7U : 15U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(star,
                            (layer == 0U) ? LV_OPA_30 : ((layer == 1U) ? LV_OPA_60 : LV_OPA_COVER),
                            LV_PART_MAIN);
    lv_obj_set_pos(star,
                   static_cast<lv_coord_t>(state.x_q8 >> 8),
                   static_cast<lv_coord_t>(state.y_q8 >> 8));
    lv_obj_clear_flag(star, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::updateStarfield(uint32_t dt_ms) {
  if (intro_star_count_ == 0U || dt_ms == 0U) {
    return;
  }
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();

  for (uint8_t i = 0U; i < intro_star_count_; ++i) {
    lv_obj_t* star = scene_starfield_[i];
    if (star == nullptr || lv_obj_has_flag(star, LV_OBJ_FLAG_HIDDEN)) {
      continue;
    }

    IntroStarState& state = intro_star_states_[i];
    state.y_q8 += static_cast<int32_t>((static_cast<uint32_t>(state.speed_px_per_s) * dt_ms * 256U) / 1000U);
    if (state.y_q8 > ((static_cast<int32_t>(height) + 4) << 8)) {
      state.y_q8 = -static_cast<int32_t>((nextIntroRandom() % 36U) << 8);
      state.x_q8 = static_cast<int32_t>(nextIntroRandom() % static_cast<uint32_t>(width)) << 8;
    }

    if ((nextIntroRandom() & 0x7U) == 0U) {
      const lv_opa_t twinkle = static_cast<lv_opa_t>(96U + (nextIntroRandom() % 160U));
      lv_obj_set_style_bg_opa(star, twinkle, LV_PART_MAIN);
    }
    lv_obj_set_pos(star,
                   static_cast<lv_coord_t>(state.x_q8 >> 8),
                   static_cast<lv_coord_t>(state.y_q8 >> 8));
  }
}

void UiManager::createLogoLabel(const char* text) {
  if (intro_logo_label_ == nullptr || intro_logo_shadow_label_ == nullptr) {
    return;
  }
  copyStringBounded(intro_logo_ascii_, sizeof(intro_logo_ascii_), asciiFallbackForUiText(text).c_str());
  lv_label_set_text(intro_logo_label_, intro_logo_ascii_);
  lv_label_set_text(intro_logo_shadow_label_, intro_logo_ascii_);
  lv_obj_align(intro_logo_shadow_label_, LV_ALIGN_TOP_MID, 1, 23);
  lv_obj_align(intro_logo_label_, LV_ALIGN_TOP_MID, 0, 22);
  lv_obj_set_style_translate_y(intro_logo_shadow_label_, -80, LV_PART_MAIN);
  lv_obj_set_style_translate_y(intro_logo_label_, -80, LV_PART_MAIN);
  lv_obj_clear_flag(intro_logo_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(intro_logo_label_, LV_OBJ_FLAG_HIDDEN);
}

void UiManager::configureWavySineScroller(const char* text,
                                          uint16_t speed_px_per_sec,
                                          uint8_t amp_px,
                                          uint16_t period_px,
                                          bool ping_pong,
                                          int16_t base_y,
                                          bool large_text,
                                          bool limit_to_half_width) {
  String padded = asciiFallbackForUiText(text);
  // Keep some visual breathing room so the scroller does not look clipped on screen edges.
  String pad;
  for (uint8_t i = 0U; i < kIntroCenterScrollPadSpaces; ++i) {
    pad += " ";
  }
  padded = pad + padded + pad;
  copyStringBounded(intro_wave_text_ascii_, sizeof(intro_wave_text_ascii_), padded.c_str());
  intro_wave_text_len_ = static_cast<uint16_t>(std::strlen(intro_wave_text_ascii_));
  intro_wave_pingpong_mode_ = ping_pong;
  intro_wave_speed_px_per_sec_ = speed_px_per_sec;
  intro_wave_period_px_ = period_px;
  intro_wave_phase_speed_ = intro_config_.sine_phase_speed;
  intro_wave_base_y_ = base_y;
  intro_wave_phase_ = 0.0f;
  intro_wave_head_index_ = 0U;
  intro_wave_dir_ = -1;
  intro_wave_half_height_mode_ = false;
  intro_wave_band_top_ = 0;
  intro_wave_band_bottom_ = activeDisplayHeight();
  intro_wave_use_pixel_font_ = false;

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  String font_mode = intro_config_.font_mode;
  font_mode.toLowerCase();
  const bool force_pixel = (font_mode == "pixel");
  const lv_font_t* wave_font = UiFonts::fontBodyM();
  if (large_text) {
    if (force_pixel) {
      wave_font = UiFonts::fontPixel();
      intro_wave_use_pixel_font_ = true;
    } else {
      wave_font = UiFonts::fontTitleXL();
      intro_wave_use_pixel_font_ = false;
    }
  }
  intro_wave_font_line_height_ = lv_font_get_line_height(wave_font);
  const float width_ratio = large_text ? 0.62f : 0.56f;
  intro_wave_char_width_ =
      static_cast<int16_t>(clampValue<int32_t>(static_cast<int32_t>(intro_wave_font_line_height_ * width_ratio),
                                               8,
                                               30));
  if (!large_text && intro_wave_char_width_ < 9) {
    intro_wave_char_width_ = 9;
  }
  intro_wave_amp_px_ = amp_px;
  if (large_text) {
    intro_wave_amp_px_ = resolveCenterWaveAmplitudePx(wave_font);
    intro_wave_base_y_ = static_cast<int16_t>(height / 2);
  }

  if (intro_wave_text_len_ == 0U) {
    intro_wave_glyph_count_ = 0U;
    for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
      if (intro_wave_slots_[i].glyph != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].glyph, LV_OBJ_FLAG_HIDDEN);
      }
      if (intro_wave_slots_[i].shadow != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].shadow, LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  if (ping_pong) {
    intro_wave_glyph_count_ =
        clampValue<uint8_t>(static_cast<uint8_t>(intro_wave_text_len_), 12U, kIntroWaveGlyphMax);
  } else {
    const uint8_t desired = static_cast<uint8_t>((width / intro_wave_char_width_) + 6);
    intro_wave_glyph_count_ = clampValue<uint8_t>(desired, 24U, kIntroWaveGlyphMax);
  }

  const int32_t text_width = static_cast<int32_t>(intro_wave_text_len_) * intro_wave_char_width_;
  int32_t pingpong_min_x = (static_cast<int32_t>(width) - text_width - 8);
  int32_t pingpong_max_x = 8;
  if (limit_to_half_width && ping_pong) {
    const int32_t half_band = width / 2;
    const int32_t band_left = (static_cast<int32_t>(width) - half_band) / 2;
    pingpong_max_x = band_left + 8;
    pingpong_min_x = band_left + half_band - text_width - 8;
  }
  if (pingpong_min_x > pingpong_max_x) {
    pingpong_min_x = static_cast<int32_t>((width - text_width) / 2);
    pingpong_max_x = pingpong_min_x;
  }
  intro_wave_pingpong_max_x_q8_ = pingpong_max_x << 8;
  intro_wave_pingpong_min_x_q8_ = pingpong_min_x << 8;
  if (intro_wave_pingpong_min_x_q8_ > intro_wave_pingpong_max_x_q8_) {
    const int32_t centered = static_cast<int32_t>((width - text_width) / 2);
    intro_wave_pingpong_min_x_q8_ = centered << 8;
    intro_wave_pingpong_max_x_q8_ = centered << 8;
  }
  intro_wave_pingpong_x_q8_ = ping_pong ? intro_wave_pingpong_max_x_q8_ : 0;

  for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
    IntroGlyphSlot& slot = intro_wave_slots_[i];
    if (slot.glyph == nullptr || slot.shadow == nullptr) {
      continue;
    }
    lv_obj_set_style_text_font(slot.glyph, wave_font, LV_PART_MAIN);
    lv_obj_set_style_text_font(slot.shadow, wave_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(slot.glyph, introPaletteColor(7), LV_PART_MAIN);
    lv_obj_set_style_text_color(slot.shadow, introPaletteColor(8), LV_PART_MAIN);
    lv_obj_set_style_text_opa(slot.shadow, LV_OPA_60, LV_PART_MAIN);
    if (i < intro_wave_glyph_count_) {
      lv_obj_clear_flag(slot.glyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(slot.shadow, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(slot.glyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(slot.shadow, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

uint8_t UiManager::resolveCenterWaveAmplitudePx(const lv_font_t* wave_font) const {
  const int16_t height = activeDisplayHeight();
  const uint8_t fallback = clampValue<uint8_t>(intro_wave_amp_px_, 8U, kIntroSineAmpMax);
  if (wave_font == nullptr || height <= 0) {
    return fallback;
  }
  const int16_t font_h = static_cast<int16_t>(lv_font_get_line_height(wave_font));
  int16_t target = static_cast<int16_t>((height / 4) - (font_h / 2));
  if (target < 80) {
    target = 80;
  }
  if (target > static_cast<int16_t>(kIntroSineAmpMax)) {
    target = static_cast<int16_t>(kIntroSineAmpMax);
  }
  const uint8_t cfg_amp = clampValue<uint8_t>(intro_wave_amp_px_, 8U, kIntroSineAmpMax);
  if (cfg_amp > static_cast<uint8_t>(target)) {
    return cfg_amp;
  }
  return static_cast<uint8_t>(target);
}

void UiManager::clampWaveYToBand(int16_t* y) const {
  if (y == nullptr || !intro_wave_half_height_mode_) {
    return;
  }
  int16_t y_max = static_cast<int16_t>(intro_wave_band_bottom_ - intro_wave_font_line_height_);
  if (y_max < intro_wave_band_top_) {
    y_max = intro_wave_band_top_;
  }
  if (*y < intro_wave_band_top_) {
    *y = intro_wave_band_top_;
  } else if (*y > y_max) {
    *y = y_max;
  }
}

void UiManager::updateWavySineScroller(uint32_t dt_ms, uint32_t now_ms) {
  if (intro_wave_glyph_count_ == 0U || intro_wave_text_len_ == 0U) {
    return;
  }
  const int16_t width = activeDisplayWidth();

  if (dt_ms > 0U) {
    const float dt_s = static_cast<float>(dt_ms) * 0.001f;
    intro_wave_phase_ += intro_wave_phase_speed_ * dt_s;

    if (intro_wave_pingpong_mode_) {
      const int32_t delta = static_cast<int32_t>((static_cast<uint32_t>(intro_wave_speed_px_per_sec_) * dt_ms * 256U) / 1000U);
      intro_wave_pingpong_x_q8_ += static_cast<int32_t>(intro_wave_dir_) * delta;
      if (intro_wave_pingpong_x_q8_ < intro_wave_pingpong_min_x_q8_) {
        intro_wave_pingpong_x_q8_ = intro_wave_pingpong_min_x_q8_;
        intro_wave_dir_ = 1;
      } else if (intro_wave_pingpong_x_q8_ > intro_wave_pingpong_max_x_q8_) {
        intro_wave_pingpong_x_q8_ = intro_wave_pingpong_max_x_q8_;
        intro_wave_dir_ = -1;
      }
    } else {
      intro_wave_pingpong_x_q8_ +=
          static_cast<int32_t>((static_cast<uint32_t>(intro_wave_speed_px_per_sec_) * dt_ms * 256U) /
                               1000U);
      const int16_t char_width_q8 = static_cast<int16_t>(intro_wave_char_width_ << 8);
      while (intro_wave_pingpong_x_q8_ >= char_width_q8) {
        intro_wave_pingpong_x_q8_ -= char_width_q8;
        intro_wave_head_index_ = static_cast<uint16_t>((intro_wave_head_index_ + 1U) % intro_wave_text_len_);
      }
    }
  }

  const float phase = intro_wave_phase_ + (static_cast<float>(now_ms & 0x3FFU) * 0.0008f);
  for (uint8_t i = 0U; i < intro_wave_glyph_count_; ++i) {
    IntroGlyphSlot& slot = intro_wave_slots_[i];
    if (slot.glyph == nullptr || slot.shadow == nullptr) {
      continue;
    }

    uint16_t char_index = 0U;
    int16_t x = 0;
    if (intro_wave_pingpong_mode_) {
      char_index = static_cast<uint16_t>(i % intro_wave_text_len_);
      x = static_cast<int16_t>((intro_wave_pingpong_x_q8_ >> 8) + static_cast<int16_t>(i * intro_wave_char_width_));
    } else {
      char_index = static_cast<uint16_t>((intro_wave_head_index_ + i) % intro_wave_text_len_);
      x = static_cast<int16_t>(i * intro_wave_char_width_ - (intro_wave_pingpong_x_q8_ >> 8));
    }

    char glyph_text[2] = {intro_wave_text_ascii_[char_index], '\0'};
    lv_label_set_text(slot.glyph, glyph_text);
    lv_label_set_text(slot.shadow, glyph_text);

    const float radians =
        phase + (static_cast<float>(x) * 6.28318530718f / static_cast<float>(intro_wave_period_px_));
    const int16_t y_offset = static_cast<int16_t>(std::sin(radians) * static_cast<float>(intro_wave_amp_px_));
    int16_t y = static_cast<int16_t>(intro_wave_base_y_ + y_offset);
    clampWaveYToBand(&y);

    lv_obj_set_pos(slot.shadow, static_cast<lv_coord_t>(x + 1), static_cast<lv_coord_t>(y + 1));
    lv_obj_set_pos(slot.glyph, static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y));

    const bool visible = (x > -intro_wave_char_width_ * 3) && (x < width + intro_wave_char_width_ * 3);
    if (visible) {
      lv_obj_clear_flag(slot.shadow, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(slot.glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(slot.shadow, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(slot.glyph, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void UiManager::configureBottomRollbackScroller(const char* text) {
  if (intro_bottom_scroll_label_ == nullptr) {
    return;
  }
  copyStringBounded(intro_crack_bottom_scroll_ascii_,
                    sizeof(intro_crack_bottom_scroll_ascii_),
                    asciiFallbackForUiText(text).c_str());
  lv_label_set_text(intro_bottom_scroll_label_, intro_crack_bottom_scroll_ascii_);
  lv_obj_set_width(intro_bottom_scroll_label_, LV_SIZE_CONTENT);
  lv_obj_clear_flag(intro_bottom_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(intro_bottom_scroll_label_);

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const int16_t text_width = lv_obj_get_width(intro_bottom_scroll_label_);
  intro_bottom_scroll_base_y_ = static_cast<int16_t>(height - 26);
  intro_bottom_scroll_max_x_q8_ = kIntroBottomScrollMarginPx << 8;
  intro_bottom_scroll_min_x_q8_ =
      (static_cast<int32_t>(width - text_width - kIntroBottomScrollMarginPx)) << 8;
  if (intro_bottom_scroll_min_x_q8_ > intro_bottom_scroll_max_x_q8_) {
    const int32_t centered = static_cast<int32_t>((width - text_width) / 2);
    intro_bottom_scroll_min_x_q8_ = centered << 8;
    intro_bottom_scroll_max_x_q8_ = centered << 8;
  }
  intro_bottom_scroll_x_q8_ = intro_bottom_scroll_max_x_q8_;
  intro_bottom_scroll_dir_ = -1;
  intro_bottom_scroll_speed_px_per_sec_ = intro_scroll_bot_a_px_per_sec_;
  lv_obj_set_pos(intro_bottom_scroll_label_, intro_bottom_scroll_x_q8_ >> 8, intro_bottom_scroll_base_y_);
}

void UiManager::updateBottomRollbackScroller(uint32_t dt_ms) {
  if (intro_bottom_scroll_label_ == nullptr || lv_obj_has_flag(intro_bottom_scroll_label_, LV_OBJ_FLAG_HIDDEN) ||
      dt_ms == 0U) {
    return;
  }
  const int32_t delta = static_cast<int32_t>((static_cast<uint32_t>(intro_bottom_scroll_speed_px_per_sec_) * dt_ms * 256U) / 1000U);
  intro_bottom_scroll_x_q8_ += static_cast<int32_t>(intro_bottom_scroll_dir_) * delta;
  if (intro_bottom_scroll_x_q8_ < intro_bottom_scroll_min_x_q8_) {
    intro_bottom_scroll_x_q8_ = intro_bottom_scroll_min_x_q8_;
    intro_bottom_scroll_dir_ = 1;
  } else if (intro_bottom_scroll_x_q8_ > intro_bottom_scroll_max_x_q8_) {
    intro_bottom_scroll_x_q8_ = intro_bottom_scroll_max_x_q8_;
    intro_bottom_scroll_dir_ = -1;
  }
  lv_obj_set_pos(intro_bottom_scroll_label_, intro_bottom_scroll_x_q8_ >> 8, intro_bottom_scroll_base_y_);
}

void UiManager::createWireCube() {
  for (uint8_t i = 0U; i < kIntroWireEdgeCount; ++i) {
    if (intro_wire_lines_[i] == nullptr) {
      continue;
    }
    lv_obj_clear_flag(intro_wire_lines_[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(intro_wire_lines_[i], LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_line_width(intro_wire_lines_[i], 1, LV_PART_MAIN);
  }
}

void UiManager::updateWireCube(uint32_t dt_ms, bool crash_boost) {
  static bool lut_ready = false;
  static int16_t sin_lut_q14[256] = {};
  if (!lut_ready) {
    for (uint16_t i = 0U; i < 256U; ++i) {
      const float radians = (static_cast<float>(i) * 6.28318530718f) / 256.0f;
      sin_lut_q14[i] = static_cast<int16_t>(std::sin(radians) * 16384.0f);
    }
    lut_ready = true;
  }

  auto sin_q14 = [&](uint8_t angle) -> int32_t { return sin_lut_q14[angle]; };
  auto cos_q14 = [&](uint8_t angle) -> int32_t { return sin_lut_q14[static_cast<uint8_t>(angle + 64U)]; };

  const uint16_t speed_mul = crash_boost ? 3U : 1U;
  intro_cube_yaw_ = static_cast<uint16_t>((intro_cube_yaw_ + (2U * speed_mul)) & 0xFFU);
  intro_cube_pitch_ = static_cast<uint16_t>((intro_cube_pitch_ + (1U * speed_mul)) & 0xFFU);
  intro_cube_roll_ = static_cast<uint16_t>((intro_cube_roll_ + speed_mul) & 0xFFU);
  if (intro_cube_morph_enabled_) {
    const float phase_step =
        static_cast<float>(dt_ms) * 0.001f * intro_cube_morph_speed_ * (crash_boost ? 1.8f : 1.0f);
    intro_cube_morph_phase_ += phase_step;
    if (intro_cube_morph_phase_ > 6.28318530718f) {
      intro_cube_morph_phase_ = std::fmod(intro_cube_morph_phase_, 6.28318530718f);
    }
  }
  float morph = intro_cube_morph_enabled_ ? (0.5f * (1.0f - std::cos(intro_cube_morph_phase_))) : 0.0f;
  if (crash_boost) {
    morph = clampValue<float>(morph + 0.25f, 0.0f, 1.0f);
  }

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const int16_t cx = static_cast<int16_t>(width / 2);
  const int16_t cy = (intro_state_ == IntroState::PHASE_A_CRACKTRO ||
                      intro_state_ == IntroState::PHASE_B_TRANSITION)
                         ? static_cast<int16_t>((height / 2) - 24)
                         : static_cast<int16_t>((height / 2) + 4);

  const int16_t base[8][3] = {
      {-static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale)},
      {static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale)},
      {static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale)},
      {-static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale)},
      {-static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale)},
      {static_cast<int16_t>(kIntroCubeScale), -static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale)},
      {static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale)},
      {-static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale), static_cast<int16_t>(kIntroCubeScale)},
  };
  const uint8_t edges[12][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  int16_t projected[8][2] = {};
  const int32_t sy = sin_q14(static_cast<uint8_t>(intro_cube_yaw_));
  const int32_t cy_q14 = cos_q14(static_cast<uint8_t>(intro_cube_yaw_));
  const int32_t sx = sin_q14(static_cast<uint8_t>(intro_cube_pitch_));
  const int32_t cx_q14 = cos_q14(static_cast<uint8_t>(intro_cube_pitch_));
  const int32_t sz = sin_q14(static_cast<uint8_t>(intro_cube_roll_));
  const int32_t cz_q14 = cos_q14(static_cast<uint8_t>(intro_cube_roll_));

  for (uint8_t i = 0U; i < 8U; ++i) {
    const float cube_x = static_cast<float>(base[i][0]);
    const float cube_y = static_cast<float>(base[i][1]);
    const float cube_z = static_cast<float>(base[i][2]);
    const float length = std::sqrt((cube_x * cube_x) + (cube_y * cube_y) + (cube_z * cube_z));
    const float sphere_scale = (length > 0.01f) ? (static_cast<float>(kIntroCubeScale) / length) : 1.0f;
    const float sphere_x = cube_x * sphere_scale;
    const float sphere_y = cube_y * sphere_scale;
    const float sphere_z = cube_z * sphere_scale;
    const float blended_x = cube_x + ((sphere_x - cube_x) * morph);
    const float blended_y = cube_y + ((sphere_y - cube_y) * morph);
    const float blended_z = cube_z + ((sphere_z - cube_z) * morph);

    int32_t x = static_cast<int32_t>(blended_x);
    int32_t y = static_cast<int32_t>(blended_y);
    int32_t z = static_cast<int32_t>(blended_z);

    const int32_t x1 = (x * cy_q14 + z * sy) >> 14;
    const int32_t z1 = (-x * sy + z * cy_q14) >> 14;
    const int32_t y2 = (y * cx_q14 - z1 * sx) >> 14;
    const int32_t z2 = (y * sx + z1 * cx_q14) >> 14;
    const int32_t x3 = (x1 * cz_q14 - y2 * sz) >> 14;
    const int32_t y3 = (x1 * sz + y2 * cz_q14) >> 14;

    int32_t zproj = z2 + static_cast<int32_t>(kIntroCubeZOffset);
    if (zproj < 64) {
      zproj = 64;
    }

    const int16_t out_x = static_cast<int16_t>(cx + ((x3 * static_cast<int32_t>(kIntroCubeFov)) / zproj));
    const int16_t out_y = static_cast<int16_t>(cy + ((y3 * static_cast<int32_t>(kIntroCubeFov)) / zproj));
    projected[i][0] = out_x;
    projected[i][1] = out_y;
  }

  lv_opa_t base_opa = LV_OPA_70;
  if (intro_3d_quality_resolved_ == Intro3DQuality::kHigh) {
    base_opa = LV_OPA_80;
  } else if (intro_3d_quality_resolved_ == Intro3DQuality::kLow) {
    base_opa = LV_OPA_60;
  }
  if (crash_boost) {
    base_opa = LV_OPA_COVER;
  }

  for (uint8_t e = 0U; e < kIntroWireEdgeCount; ++e) {
    lv_obj_t* line = intro_wire_lines_[e];
    if (line == nullptr) {
      continue;
    }
    const uint8_t a = edges[e][0];
    const uint8_t b = edges[e][1];
    intro_wire_points_[e][0].x = projected[a][0];
    intro_wire_points_[e][0].y = projected[a][1];
    intro_wire_points_[e][1].x = projected[b][0];
    intro_wire_points_[e][1].y = projected[b][1];
    lv_line_set_points(line, intro_wire_points_[e], 2);
    lv_obj_set_style_line_color(line,
                                introPaletteColor((e % 2U) == 0U ? 3U : 4U),
                                LV_PART_MAIN);
    lv_obj_set_style_opa(line, base_opa, LV_PART_MAIN);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
  }
  (void)dt_ms;
}

void UiManager::createRotoZoom() {
  const bool force_b2_interlude = (intro_state_ == IntroState::PHASE_B_TRANSITION && intro_b1_done_);
  const bool enable_roto =
      force_b2_interlude ||
      (intro_3d_mode_ == Intro3DMode::kRotoZoom || intro_3d_mode_ == Intro3DMode::kTunnel ||
       intro_3d_mode_ == Intro3DMode::kPerspectiveStarfield);
  for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
    if (intro_roto_stripes_[i] == nullptr) {
      continue;
    }
    if (enable_roto) {
      lv_obj_clear_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void UiManager::updateRotoZoom(uint32_t dt_ms) {
  const bool force_b2_interlude = (intro_state_ == IntroState::PHASE_B_TRANSITION && intro_b1_done_);
  const bool enable_roto =
      force_b2_interlude ||
      (intro_3d_mode_ == Intro3DMode::kRotoZoom || intro_3d_mode_ == Intro3DMode::kTunnel ||
       intro_3d_mode_ == Intro3DMode::kPerspectiveStarfield);
  if (!enable_roto) {
    for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
      if (intro_roto_stripes_[i] != nullptr) {
        lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  uint8_t active_count = 12U;
  if (intro_3d_quality_resolved_ == Intro3DQuality::kLow) {
    active_count = 8U;
  } else if (intro_3d_quality_resolved_ == Intro3DQuality::kHigh) {
    active_count = 16U;
  }
  active_count = clampValue<uint8_t>(active_count, 4U, kIntroRotoStripeMax);

  intro_roto_phase_ += static_cast<float>(dt_ms) * 0.0028f;

  for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
    lv_obj_t* stripe = intro_roto_stripes_[i];
    if (stripe == nullptr) {
      continue;
    }
    if (i >= active_count) {
      lv_obj_add_flag(stripe, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    const float depth = static_cast<float>(i + 1U) / static_cast<float>(active_count);
    const float curve = depth * depth;
    const int16_t stripe_h = static_cast<int16_t>(2 + (intro_3d_quality_resolved_ == Intro3DQuality::kHigh ? 2 : 1));
    const int16_t stripe_w =
        static_cast<int16_t>(static_cast<float>(width) * (0.24f + depth * 0.92f));
    const float sway = std::sin(intro_roto_phase_ * 0.9f + depth * 6.8f);
    const int16_t cx = static_cast<int16_t>((width / 2) + sway * (static_cast<float>(width) * 0.20f * (1.0f - depth)));
    const int16_t y = static_cast<int16_t>(height - 18 - curve * (static_cast<float>(height) * 0.72f));
    const int16_t x = static_cast<int16_t>(cx - stripe_w / 2);

    lv_obj_set_pos(stripe, x, y);
    lv_obj_set_size(stripe, stripe_w, stripe_h);
    const bool checker = ((i + static_cast<uint8_t>(intro_roto_phase_ * 3.0f)) & 1U) == 0U;
    lv_obj_set_style_bg_color(stripe,
                              introPaletteColor(checker ? 12U : 13U),
                              LV_PART_MAIN);
    const lv_opa_t opa = static_cast<lv_opa_t>(20 + static_cast<uint8_t>(depth * 90.0f));
    lv_obj_set_style_bg_opa(stripe, opa, LV_PART_MAIN);
    lv_obj_clear_flag(stripe, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::resolveIntro3DModeAndQuality() {
  String mode = intro_config_.fx_3d;
  mode.toLowerCase();
  if (mode == "wirecube" || mode.indexOf("cube") >= 0 || mode.indexOf("boing") >= 0 ||
      mode.indexOf("ball") >= 0) {
    intro_3d_mode_ = Intro3DMode::kWireCube;
  } else if (mode == "tunnel") {
    intro_3d_mode_ = Intro3DMode::kTunnel;
  } else if (mode == "starfield3d") {
    intro_3d_mode_ = Intro3DMode::kPerspectiveStarfield;
  } else {
    intro_3d_mode_ = Intro3DMode::kRotoZoom;
  }

  String quality = intro_config_.fx_3d_quality;
  quality.toLowerCase();
  if (quality == "low") {
    intro_3d_quality_ = Intro3DQuality::kLow;
  } else if (quality == "med" || quality == "medium") {
    intro_3d_quality_ = Intro3DQuality::kMed;
  } else if (quality == "high") {
    intro_3d_quality_ = Intro3DQuality::kHigh;
  } else {
    intro_3d_quality_ = Intro3DQuality::kAuto;
  }

  if (intro_3d_quality_ == Intro3DQuality::kAuto) {
    const int32_t area = static_cast<int32_t>(activeDisplayWidth()) * activeDisplayHeight();
    if (area < 70000) {
      intro_3d_quality_resolved_ = Intro3DQuality::kLow;
    } else if (area < 140000) {
      intro_3d_quality_resolved_ = Intro3DQuality::kMed;
    } else {
      intro_3d_quality_resolved_ = Intro3DQuality::kHigh;
    }
  } else {
    intro_3d_quality_resolved_ = intro_3d_quality_;
  }
}

void UiManager::startIntroIfNeeded(bool force_restart) {
  ensureIntroCreated();
  if (!intro_created_ || intro_root_ == nullptr) {
    return;
  }
  if (force_restart) {
    intro_skip_latched_ = false;
  }
  if (intro_skip_latched_ && !force_restart) {
    return;
  }
  if (intro_active_ && !force_restart) {
    return;
  }
  loadSceneWinEtapeOverrides();
  startIntro();
}

void UiManager::startIntro() {
  if (!intro_created_ || intro_root_ == nullptr) {
    return;
  }

  copyStringBounded(intro_logo_ascii_,
                    sizeof(intro_logo_ascii_),
                    asciiFallbackForUiText(intro_config_.logo_text).c_str());
  copyStringBounded(intro_crack_scroll_ascii_,
                    sizeof(intro_crack_scroll_ascii_),
                    asciiFallbackForUiText(intro_config_.crack_scroll).c_str());
  copyStringBounded(intro_crack_bottom_scroll_ascii_,
                    sizeof(intro_crack_bottom_scroll_ascii_),
                    asciiFallbackForUiText(intro_config_.crack_bottom_scroll).c_str());
  copyStringBounded(intro_clean_title_ascii_,
                    sizeof(intro_clean_title_ascii_),
                    asciiFallbackForUiText(intro_config_.clean_title).c_str());
  copyStringBounded(intro_clean_scroll_ascii_,
                    sizeof(intro_clean_scroll_ascii_),
                    asciiFallbackForUiText(intro_config_.clean_scroll).c_str());

  resolveIntro3DModeAndQuality();

  intro_skip_latched_ = false;
  intro_skip_requested_ = false;
  intro_clean_loop_only_ = false;
  intro_active_ = true;
  intro_state_ = IntroState::DONE;
  intro_total_start_ms_ = lv_tick_get();
  last_tick_ms_ = intro_total_start_ms_;
  intro_wave_last_ms_ = intro_total_start_ms_;
  intro_debug_next_ms_ = intro_total_start_ms_;
  intro_phase_log_next_ms_ = intro_total_start_ms_ + 5000U;
  intro_debug_overlay_enabled_ = false;
  intro_b1_done_ = false;
  intro_next_b2_pulse_ms_ = 0U;
  intro_wave_half_height_mode_ = false;
  intro_wave_band_top_ = 0;
  intro_wave_band_bottom_ = activeDisplayHeight();
  intro_cube_morph_enabled_ = true;
  intro_cube_morph_phase_ = 0.0f;
  intro_cube_morph_speed_ = 0.9f;
  intro_c_fx_stage_ = 0U;
  intro_c_fx_stage_start_ms_ = intro_total_start_ms_;
  intro_b1_crash_ms_ = intro_config_.b1_crash_ms;
  intro_scroll_mid_a_px_per_sec_ = intro_config_.scroll_a_px_per_sec;
  intro_scroll_bot_a_px_per_sec_ = intro_config_.scroll_bot_a_px_per_sec;
  lv_obj_set_style_opa(intro_root_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_translate_x(intro_root_, 0, LV_PART_MAIN);
  lv_obj_set_style_translate_y(intro_root_, 0, LV_PART_MAIN);
  lv_obj_clear_flag(intro_root_, LV_OBJ_FLAG_HIDDEN);

  transitionIntroState(IntroState::PHASE_A_CRACKTRO);

  if (intro_timer_ == nullptr) {
    intro_timer_ = lv_timer_create(introTimerCb, kIntroTickMs, this);
  } else {
    lv_timer_set_period(intro_timer_, kIntroTickMs);
    lv_timer_resume(intro_timer_);
  }

  Serial.printf("[UI][WIN_ETAPE] start A=%lu B=%lu C=%lu B1=%u speedMidA=%u speedBotA=%u speedC=%u quality=%u mode=%u\n",
                static_cast<unsigned long>(intro_config_.a_duration_ms),
                static_cast<unsigned long>(intro_config_.b_duration_ms),
                static_cast<unsigned long>(intro_config_.c_duration_ms),
                static_cast<unsigned int>(intro_b1_crash_ms_),
                static_cast<unsigned int>(intro_scroll_mid_a_px_per_sec_),
                static_cast<unsigned int>(intro_scroll_bot_a_px_per_sec_),
                static_cast<unsigned int>(intro_config_.scroll_c_px_per_sec),
                static_cast<unsigned int>(intro_3d_quality_resolved_),
                static_cast<unsigned int>(intro_3d_mode_));
}

void UiManager::requestIntroSkip() {
  if (!intro_active_) {
    return;
  }
  if (intro_state_ == IntroState::PHASE_A_CRACKTRO || intro_state_ == IntroState::PHASE_B_TRANSITION) {
    intro_skip_latched_ = true;
    intro_skip_requested_ = true;
  }
}

void UiManager::transitionIntroState(IntroState next_state) {
  intro_state_ = next_state;
  t_state0_ms_ = lv_tick_get();

  const int16_t w = activeDisplayWidth();
  const int16_t h = activeDisplayHeight();
  const int32_t area = static_cast<int32_t>(w) * h;

  auto hide_wave_text = [this]() {
    for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
      if (intro_wave_slots_[i].glyph != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].glyph, LV_OBJ_FLAG_HIDDEN);
      }
      if (intro_wave_slots_[i].shadow != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].shadow, LV_OBJ_FLAG_HIDDEN);
      }
    }
  };

  if (next_state == IntroState::PHASE_A_CRACKTRO) {
    intro_b1_done_ = false;
    intro_next_b2_pulse_ms_ = 0U;
    intro_wave_half_height_mode_ = false;
    intro_cube_morph_phase_ = 0.0f;
    const uint8_t bar_count = static_cast<uint8_t>(clampValue<int16_t>(h / 22, 8, 18));
    int16_t stars = static_cast<int16_t>(clampValue<int32_t>(area / 1200, 60, 220));
    if (intro_config_.stars_override > 0) {
      stars = intro_config_.stars_override;
    }
    if (stars > static_cast<int16_t>(kStarfieldCount)) {
      stars = static_cast<int16_t>(kStarfieldCount);
    }

    createCopperBars(bar_count);
    createStarfield(static_cast<uint8_t>(stars), 3U);
    createLogoLabel(intro_logo_ascii_);
    intro_logo_anim_start_ms_ = t_state0_ms_;

    configureWavySineScroller(intro_crack_scroll_ascii_,
                              intro_scroll_mid_a_px_per_sec_,
                              intro_config_.sine_amp_a_px,
                              intro_config_.sine_period_px,
                              false,
                              static_cast<int16_t>(h / 2),
                              true);
    intro_wave_half_height_mode_ = true;
    intro_wave_band_top_ = static_cast<int16_t>(h / 4);
    intro_wave_band_bottom_ = static_cast<int16_t>((h * 3) / 4);
    configureBottomRollbackScroller(intro_crack_bottom_scroll_ascii_);

    if (intro_clean_title_label_ != nullptr) {
      lv_obj_add_flag(intro_clean_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (intro_clean_title_shadow_label_ != nullptr) {
      lv_obj_add_flag(intro_clean_title_shadow_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (intro_clean_scroll_label_ != nullptr) {
      lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
    }
    for (lv_obj_t* layer : intro_gradient_layers_) {
      if (layer != nullptr) {
        lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
      }
    }
    createWireCube();
    createRotoZoom();
    for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
      if (intro_roto_stripes_[i] != nullptr) {
        lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }

    Serial.printf("[UI][WIN_ETAPE] phase=A obj=%u stars=%u particles=%u quality=%u\n",
                  static_cast<unsigned int>(intro_copper_count_ + intro_star_count_ + (intro_wave_glyph_count_ * 2U) + kIntroWireEdgeCount + 8U),
                  static_cast<unsigned int>(intro_star_count_),
                  static_cast<unsigned int>(intro_firework_active_count_),
                  static_cast<unsigned int>(intro_3d_quality_resolved_));
    return;
  }

  if (next_state == IntroState::PHASE_B_TRANSITION) {
    configureBPhaseStart();
    Serial.printf("[UI][WIN_ETAPE] phase=B obj=%u stars=%u particles=%u quality=%u\n",
                  static_cast<unsigned int>(intro_copper_count_ + intro_star_count_ + intro_firework_active_count_ + (intro_wave_glyph_count_ * 2U) + kIntroWireEdgeCount + 8U),
                  static_cast<unsigned int>(intro_star_count_),
                  static_cast<unsigned int>(intro_firework_active_count_),
                  static_cast<unsigned int>(intro_3d_quality_resolved_));
    return;
  }

    if (next_state == IntroState::PHASE_C_CLEAN || next_state == IntroState::PHASE_C_LOOP) {
    startCleanReveal();
    intro_c_fx_stage_ = 0U;
    intro_c_fx_stage_start_ms_ = t_state0_ms_;
    int16_t stars = static_cast<int16_t>(clampValue<int32_t>(area / 1500, 60, 140));
    if (stars > static_cast<int16_t>(kStarfieldCount)) {
      stars = static_cast<int16_t>(kStarfieldCount);
    }
    createStarfield(static_cast<uint8_t>(stars), 3U);
    createCopperBars(0U);

    configureWavySineScroller(intro_clean_scroll_ascii_,
                              intro_config_.scroll_c_px_per_sec,
                              intro_config_.sine_amp_c_px,
                              intro_config_.sine_period_px,
                              true,
                              static_cast<int16_t>(h / 2),
                              true,
                              false);
    intro_wave_half_height_mode_ = true;
    intro_wave_band_top_ = static_cast<int16_t>(h / 4);
    intro_wave_band_bottom_ = static_cast<int16_t>((h * 3) / 4);
    if (intro_bottom_scroll_label_ != nullptr) {
      lv_obj_add_flag(intro_bottom_scroll_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (intro_logo_label_ != nullptr) {
      lv_obj_add_flag(intro_logo_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (intro_logo_shadow_label_ != nullptr) {
      lv_obj_add_flag(intro_logo_shadow_label_, LV_OBJ_FLAG_HIDDEN);
    }

    if (intro_3d_mode_ == Intro3DMode::kWireCube) {
      createWireCube();
      for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
        if (intro_roto_stripes_[i] != nullptr) {
          lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
    } else {
      for (uint8_t i = 0U; i < kIntroWireEdgeCount; ++i) {
        if (intro_wire_lines_[i] != nullptr) {
          lv_obj_add_flag(intro_wire_lines_[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
      createRotoZoom();
    }

    Serial.printf("[UI][WIN_ETAPE] phase=%s obj=%u stars=%u particles=%u quality=%u\n",
                  (next_state == IntroState::PHASE_C_CLEAN) ? "C" : "C_LOOP",
                  static_cast<unsigned int>(intro_star_count_ + (intro_wave_glyph_count_ * 2U) + 18U),
                  static_cast<unsigned int>(intro_star_count_),
                  static_cast<unsigned int>(intro_firework_active_count_),
                  static_cast<unsigned int>(intro_3d_quality_resolved_));
    return;
  }

  if (next_state == IntroState::OUTRO) {
    lv_obj_set_style_translate_x(intro_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(intro_root_, 0, LV_PART_MAIN);
    return;
  }

  if (next_state == IntroState::DONE) {
    stopIntroAndCleanup();
    hide_wave_text();
  }
}

void UiManager::configureBPhaseStart() {
  intro_b1_done_ = false;
  intro_wave_half_height_mode_ = false;
  intro_next_b2_pulse_ms_ = t_state0_ms_ + static_cast<uint32_t>(intro_b1_crash_ms_) + 2400U;
  createRotoZoom();
  startGlitch(intro_b1_crash_ms_);
  startFireworks();
}

void UiManager::updateBPhase(uint32_t dt_ms, uint32_t now_ms, uint32_t state_elapsed_ms) {
  updateCopperBars(now_ms - intro_total_start_ms_);
  updateStarfield(dt_ms);
  updateWavySineScroller(dt_ms, now_ms);
  updateBottomRollbackScroller(dt_ms);
  animateLogoOvershoot();
  updateFireworks(dt_ms);

  if (state_elapsed_ms < intro_b1_crash_ms_) {
    updateWireCube(dt_ms, true);
    if (intro_3d_mode_ != Intro3DMode::kWireCube) {
      updateRotoZoom(dt_ms);
    }
    updateGlitch(dt_ms);
    return;
  }

  if (!intro_b1_done_) {
    intro_b1_done_ = true;
    if (intro_root_ != nullptr) {
      lv_obj_set_style_translate_x(intro_root_, 0, LV_PART_MAIN);
      lv_obj_set_style_translate_y(intro_root_, 0, LV_PART_MAIN);
      lv_obj_set_style_opa(intro_root_, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  if (intro_3d_mode_ == Intro3DMode::kWireCube) {
    updateWireCube(dt_ms, false);
  }
  updateRotoZoom(dt_ms);

  if (intro_firework_active_count_ == 0U && now_ms >= intro_next_b2_pulse_ms_) {
    startFireworks();
    intro_next_b2_pulse_ms_ = now_ms + 2000U + (nextIntroRandom() % 2000U);
  }
}

void UiManager::animateLogoOvershoot() {
  if (intro_logo_label_ == nullptr || intro_logo_shadow_label_ == nullptr) {
    return;
  }
  const uint32_t now = lv_tick_get();
  const uint32_t elapsed = now - intro_logo_anim_start_ms_;
  const uint32_t drop_ms = 900U;
  int16_t translate_y = 0;
  if (elapsed < drop_ms) {
    const float t = static_cast<float>(elapsed) / static_cast<float>(drop_ms);
    const float eased = easeOutBack(t);
    translate_y = static_cast<int16_t>((1.0f - eased) * -80.0f);
  } else {
    const uint32_t bounce_elapsed = elapsed - drop_ms;
    if (bounce_elapsed < 420U) {
      const float phase = (static_cast<float>(bounce_elapsed) / 420.0f) * 3.14159f;
      translate_y = static_cast<int16_t>(std::sin(phase) * 3.0f);
    } else {
      translate_y = 0;
    }
  }
  lv_obj_set_style_translate_y(intro_logo_label_, translate_y, LV_PART_MAIN);
  lv_obj_set_style_translate_y(intro_logo_shadow_label_, translate_y, LV_PART_MAIN);
}

void UiManager::startGlitch(uint16_t duration_ms) {
  intro_glitch_duration_ms_ = duration_ms;
  intro_glitch_start_ms_ = lv_tick_get();
  intro_glitch_next_jitter_ms_ = intro_glitch_start_ms_;
}

void UiManager::updateGlitch(uint32_t dt_ms) {
  (void)dt_ms;
  if (intro_root_ == nullptr || intro_glitch_duration_ms_ == 0U) {
    return;
  }
  const uint32_t now = lv_tick_get();
  const uint32_t elapsed = now - intro_glitch_start_ms_;
  const uint16_t duration = intro_glitch_duration_ms_;
  if (elapsed >= duration) {
    lv_obj_set_style_translate_x(intro_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(intro_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(intro_root_, LV_OPA_COVER, LV_PART_MAIN);
    return;
  }

  if (now >= intro_glitch_next_jitter_ms_) {
    const int16_t jitter_x = static_cast<int16_t>(static_cast<int32_t>(nextIntroRandom() % 21U) - 10);
    const int16_t jitter_y = static_cast<int16_t>(static_cast<int32_t>(nextIntroRandom() % 17U) - 8);
    lv_obj_set_style_translate_x(intro_root_, jitter_x, LV_PART_MAIN);
    lv_obj_set_style_translate_y(intro_root_, jitter_y, LV_PART_MAIN);
    intro_glitch_next_jitter_ms_ = now + 40U + (nextIntroRandom() % 41U);
  }

  const uint16_t half = duration / 2U;
  int32_t fade = LV_OPA_COVER;
  if (elapsed < half) {
    fade = LV_OPA_COVER - static_cast<int32_t>((elapsed * 180U) / half);
  } else {
    fade = 75 + static_cast<int32_t>(((elapsed - half) * 180U) / (duration - half));
  }
  const bool blink = ((elapsed / 70U) % 2U) == 0U;
  if (blink) {
    fade = (fade * 3) / 4;
  }
  fade = clampValue<int32_t>(fade, 20, LV_OPA_COVER);
  lv_obj_set_style_opa(intro_root_, static_cast<lv_opa_t>(fade), LV_PART_MAIN);
}

void UiManager::startFireworks() {
  if (!intro_created_) {
    return;
  }
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  const int32_t area = static_cast<int32_t>(width) * height;
  uint8_t bursts = (area > 140000) ? 3U : ((area > 90000) ? 2U : 1U);
  uint8_t per_burst = static_cast<uint8_t>(clampValue<int32_t>(area / 3800, 24, 48));
  while ((static_cast<uint16_t>(bursts) * per_burst) > 72U && bursts > 1U) {
    --bursts;
  }
  while ((static_cast<uint16_t>(bursts) * per_burst) > 72U && per_burst > 24U) {
    --per_burst;
  }
  const uint16_t total = static_cast<uint16_t>(bursts * per_burst);
  intro_firework_active_count_ = total;
  static constexpr uint8_t kParticlePalette[] = {3U, 4U, 5U, 7U, 10U, 9U};

  for (uint8_t i = 0U; i < 72U; ++i) {
    IntroParticleState& state = intro_firework_states_[i];
    state = {};
    lv_obj_t* particle = intro_firework_particles_[i];
    if (particle != nullptr) {
      lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_translate_x(particle, 0, LV_PART_MAIN);
      lv_obj_set_style_translate_y(particle, 0, LV_PART_MAIN);
      lv_obj_set_style_opa(particle, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  uint16_t index = 0U;
  for (uint8_t burst = 0U; burst < bursts; ++burst) {
    const int16_t cx = static_cast<int16_t>(width / 2 + static_cast<int16_t>(nextIntroRandom() % 41U) - 20);
    const int16_t cy = static_cast<int16_t>(height / 2 + static_cast<int16_t>(nextIntroRandom() % 33U) - 16);
    for (uint8_t p = 0U; p < per_burst && index < 72U; ++p, ++index) {
      IntroParticleState& state = intro_firework_states_[index];
      lv_obj_t* particle = intro_firework_particles_[index];
      if (particle == nullptr) {
        continue;
      }
      const float angle = (6.28318530718f * static_cast<float>(p)) / static_cast<float>(per_burst);
      const float jitter = static_cast<float>(static_cast<int16_t>(nextIntroRandom() % 21U) - 10) * 0.02f;
      const float velocity = static_cast<float>(90 + (nextIntroRandom() % 90U));
      state.x_q8 = static_cast<int32_t>(cx) << 8;
      state.y_q8 = static_cast<int32_t>(cy) << 8;
      state.vx_q8 = static_cast<int32_t>(std::cos(angle + jitter) * velocity * 256.0f);
      state.vy_q8 = static_cast<int32_t>(std::sin(angle + jitter) * velocity * 256.0f) - (24 << 8);
      state.delay_ms = static_cast<uint16_t>(burst * 120U + (nextIntroRandom() % 70U));
      state.life_ms = static_cast<uint16_t>(560U + (nextIntroRandom() % 360U));
      state.age_ms = 0U;
      state.active = true;
      const uint8_t size = static_cast<uint8_t>(2U + (nextIntroRandom() % 3U));
      lv_obj_set_size(particle, size, size);
      lv_obj_set_style_bg_color(particle,
                                introPaletteColor(kParticlePalette[nextIntroRandom() % 6U]),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(particle, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_pos(particle, cx, cy);
      lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void UiManager::startCleanReveal() {
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  for (uint8_t i = 0U; i < 4U; ++i) {
    if (intro_gradient_layers_[i] == nullptr) {
      continue;
    }
    lv_obj_clear_flag(intro_gradient_layers_[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(intro_gradient_layers_[i], 0, static_cast<lv_coord_t>((height / 4) * i));
    lv_obj_set_size(intro_gradient_layers_[i], width, static_cast<lv_coord_t>((height / 4) + 2));
  }
  lv_obj_set_style_bg_color(intro_gradient_layers_[0], introPaletteColor(0), LV_PART_MAIN);
  lv_obj_set_style_bg_color(intro_gradient_layers_[1], introPaletteColor(1), LV_PART_MAIN);
  lv_obj_set_style_bg_color(intro_gradient_layers_[2], introPaletteColor(2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(intro_gradient_layers_[3], introPaletteColor(14), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(intro_gradient_layers_[0], LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(intro_gradient_layers_[1], LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(intro_gradient_layers_[2], LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(intro_gradient_layers_[3], LV_OPA_70, LV_PART_MAIN);

  intro_clean_reveal_chars_ = 0U;
  intro_clean_next_char_ms_ = lv_tick_get();

  if (intro_clean_title_label_ != nullptr) {
    lv_label_set_text(intro_clean_title_label_, "");
    lv_obj_align(intro_clean_title_label_, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(intro_clean_title_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_shadow_label_ != nullptr) {
    lv_label_set_text(intro_clean_title_shadow_label_, "");
    lv_obj_align(intro_clean_title_shadow_label_, LV_ALIGN_TOP_MID, 1, 21);
    lv_obj_clear_flag(intro_clean_title_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::stopIntroAndCleanup() {
  intro_active_ = false;
  intro_skip_requested_ = false;
  intro_state_ = IntroState::DONE;
  intro_b1_done_ = false;
  intro_glitch_duration_ms_ = 0U;
  intro_next_b2_pulse_ms_ = 0U;
  intro_firework_active_count_ = 0U;
  intro_wave_half_height_mode_ = false;
  intro_wave_band_top_ = 0;
  intro_wave_band_bottom_ = 0;
  intro_wave_use_pixel_font_ = false;
  intro_wave_font_line_height_ = 0;
  intro_cube_morph_phase_ = 0.0f;
  intro_c_fx_stage_ = 0U;
  intro_c_fx_stage_start_ms_ = 0U;

  if (intro_timer_ != nullptr) {
    lv_timer_pause(intro_timer_);
  }
  if (intro_root_ != nullptr) {
    lv_obj_set_style_translate_x(intro_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(intro_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(intro_root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(intro_root_, LV_OBJ_FLAG_HIDDEN);
  }

  for (lv_obj_t* bar : scene_cracktro_bars_) {
    if (bar == nullptr) {
      continue;
    }
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(bar, 0, LV_PART_MAIN);
  }
  for (lv_obj_t* star : scene_starfield_) {
    if (star == nullptr) {
      continue;
    }
    lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(star, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(star, 0, LV_PART_MAIN);
  }

  for (uint8_t i = 0U; i < 72U; ++i) {
    if (intro_firework_particles_[i] != nullptr) {
      lv_obj_add_flag(intro_firework_particles_[i], LV_OBJ_FLAG_HIDDEN);
    }
    intro_firework_states_[i].active = false;
  }

  for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
    if (intro_wave_slots_[i].glyph != nullptr) {
      lv_obj_add_flag(intro_wave_slots_[i].glyph, LV_OBJ_FLAG_HIDDEN);
    }
    if (intro_wave_slots_[i].shadow != nullptr) {
      lv_obj_add_flag(intro_wave_slots_[i].shadow, LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (uint8_t i = 0U; i < kIntroWireEdgeCount; ++i) {
    if (intro_wire_lines_[i] != nullptr) {
      lv_obj_add_flag(intro_wire_lines_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
    if (intro_roto_stripes_[i] != nullptr) {
      lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (intro_logo_label_ != nullptr) {
    lv_obj_add_flag(intro_logo_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_logo_shadow_label_ != nullptr) {
    lv_obj_add_flag(intro_logo_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_bottom_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_bottom_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_title_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_shadow_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_title_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_debug_label_ != nullptr) {
    lv_obj_add_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::updateFireworks(uint32_t dt_ms) {
  if (intro_firework_active_count_ == 0U || dt_ms == 0U) {
    return;
  }
  uint16_t active_count = 0U;
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  constexpr int32_t kGravityQ8 = 180 << 8;
  for (uint8_t i = 0U; i < 72U; ++i) {
    IntroParticleState& state = intro_firework_states_[i];
    lv_obj_t* particle = intro_firework_particles_[i];
    if (!state.active || particle == nullptr) {
      continue;
    }
    if (state.delay_ms > 0U) {
      if (dt_ms >= state.delay_ms) {
        state.delay_ms = 0U;
      } else {
        state.delay_ms = static_cast<uint16_t>(state.delay_ms - dt_ms);
      }
      lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
      ++active_count;
      continue;
    }
    state.age_ms = static_cast<uint16_t>(state.age_ms + dt_ms);
    if (state.age_ms >= state.life_ms) {
      state.active = false;
      lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    state.vy_q8 += static_cast<int32_t>((kGravityQ8 * static_cast<int32_t>(dt_ms)) / 1000);
    state.x_q8 += static_cast<int32_t>((state.vx_q8 * static_cast<int32_t>(dt_ms)) / 1000);
    state.y_q8 += static_cast<int32_t>((state.vy_q8 * static_cast<int32_t>(dt_ms)) / 1000);

    int16_t x = static_cast<int16_t>(state.x_q8 >> 8);
    int16_t y = static_cast<int16_t>(state.y_q8 >> 8);
    x = clampValue<int16_t>(x, -8, width + 8);
    y = clampValue<int16_t>(y, -8, height + 8);
    lv_obj_set_pos(particle, x, y);
    lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);

    const uint16_t remaining = static_cast<uint16_t>(state.life_ms - state.age_ms);
    const lv_opa_t opa =
        static_cast<lv_opa_t>(clampValue<uint16_t>((remaining * 255U) / state.life_ms, 16U, 255U));
    lv_obj_set_style_opa(particle, opa, LV_PART_MAIN);
    ++active_count;
  }
  intro_firework_active_count_ = active_count;
}

void UiManager::updateCleanReveal(uint32_t dt_ms) {
  (void)dt_ms;
  if (intro_clean_title_label_ == nullptr || intro_clean_title_shadow_label_ == nullptr) {
    return;
  }
  const uint32_t now = lv_tick_get();
  const size_t target_len = std::strlen(intro_clean_title_ascii_);
  if (intro_clean_reveal_chars_ < target_len && now >= intro_clean_next_char_ms_) {
    ++intro_clean_reveal_chars_;
    if (intro_clean_reveal_chars_ > target_len) {
      intro_clean_reveal_chars_ = static_cast<uint16_t>(target_len);
    }
    char title_buf[64] = {0};
    const size_t copy_len =
        clampValue<size_t>(intro_clean_reveal_chars_, 0U, sizeof(title_buf) - 1U);
    if (copy_len > 0U) {
      std::memcpy(title_buf, intro_clean_title_ascii_, copy_len);
    }
    lv_label_set_text(intro_clean_title_label_, title_buf);
    lv_label_set_text(intro_clean_title_shadow_label_, title_buf);
    intro_clean_next_char_ms_ = now + 55U;
  }

  // Fake near/far zoom for LVGL text: subtle vertical drift + opacity pulse.
  const float pulse_phase = static_cast<float>(now) * 0.0024f;
  const int16_t drift_y = static_cast<int16_t>(std::sin(pulse_phase) * 3.0f);
  const lv_opa_t title_opa = static_cast<lv_opa_t>(200 + static_cast<int16_t>((std::sin(pulse_phase * 0.8f) + 1.0f) * 27.0f));
  lv_obj_set_style_translate_y(intro_clean_title_label_, drift_y, LV_PART_MAIN);
  lv_obj_set_style_translate_y(intro_clean_title_shadow_label_, static_cast<int16_t>(drift_y + 1), LV_PART_MAIN);
  lv_obj_set_style_opa(intro_clean_title_label_, title_opa, LV_PART_MAIN);
  lv_obj_set_style_opa(intro_clean_title_shadow_label_,
                       static_cast<lv_opa_t>(clampValue<int16_t>(title_opa - 80, 40, LV_OPA_COVER)),
                       LV_PART_MAIN);
}

void UiManager::updateSineScroller(uint32_t t_ms) {
  const uint32_t now = t_ms;
  uint32_t dt_ms = now - intro_wave_last_ms_;
  if (dt_ms > 100U) {
    dt_ms = 100U;
  }
  intro_wave_last_ms_ = now;
  updateWavySineScroller(dt_ms, now);
}

uint8_t UiManager::estimateIntroObjectCount() const {
  uint16_t active_roto = 0U;
  for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
    if (intro_roto_stripes_[i] != nullptr && !lv_obj_has_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN)) {
      ++active_roto;
    }
  }
  const uint16_t object_count = static_cast<uint16_t>(intro_copper_count_ + intro_star_count_ +
                                                       (intro_wave_glyph_count_ * 2U) +
                                                       intro_firework_active_count_ + active_roto +
                                                       kIntroWireEdgeCount + 10U);
  return static_cast<uint8_t>(clampValue<uint16_t>(object_count, 0U, 255U));
}

void UiManager::updateC3DStage(uint32_t now_ms) {
  if (intro_state_ != IntroState::PHASE_C_CLEAN && intro_state_ != IntroState::PHASE_C_LOOP) {
    return;
  }
  const uint32_t elapsed = now_ms - t_state0_ms_;
  uint8_t next_stage = 7U;
  if (elapsed < 2500U) {
    next_stage = 0U;  // cube roto
  } else if (elapsed < 5000U) {
    next_stage = 1U;  // cube rotozoom
  } else if (elapsed < 7500U) {
    next_stage = 2U;  // ball zoom
  } else if (elapsed < 10000U) {
    next_stage = 3U;  // boing
  } else if (elapsed < 12500U) {
    next_stage = 4U;  // rnd zoom
  } else if (elapsed < 15000U) {
    next_stage = 5U;  // rnd roto
  } else if (elapsed < 17500U) {
    next_stage = 6U;  // boing
  } else {
    next_stage = 7U;  // final boing hold
  }
  if (next_stage != intro_c_fx_stage_) {
    intro_c_fx_stage_ = next_stage;
    intro_c_fx_stage_start_ms_ = now_ms;
  }

  if (intro_c_fx_stage_ <= 2U) {
    intro_3d_mode_ = Intro3DMode::kWireCube;
    intro_cube_morph_enabled_ = true;
    intro_cube_morph_speed_ = (intro_c_fx_stage_ == 1U) ? 1.8f : 1.1f;
  } else if (intro_c_fx_stage_ <= 6U) {
    intro_3d_mode_ = Intro3DMode::kRotoZoom;
  } else {
    intro_3d_mode_ = Intro3DMode::kWireCube;
    intro_cube_morph_enabled_ = true;
    intro_cube_morph_phase_ = 3.14159f;
    intro_cube_morph_speed_ = 0.18f;
  }
}

void UiManager::updateIntroDebugOverlay(uint32_t dt_ms) {
  (void)dt_ms;
  if (intro_debug_label_ == nullptr) {
    return;
  }
  if (!intro_debug_overlay_enabled_) {
    lv_obj_add_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const uint32_t now = lv_tick_get();
  if (now < intro_debug_next_ms_) {
    return;
  }
  intro_debug_next_ms_ = now + 250U;

  lv_label_set_text_fmt(intro_debug_label_,
                        "phase=%u obj=%u stars=%u p=%u q=%u",
                        static_cast<unsigned int>(intro_state_),
                        static_cast<unsigned int>(estimateIntroObjectCount()),
                        static_cast<unsigned int>(intro_star_count_),
                        static_cast<unsigned int>(intro_firework_active_count_),
                        static_cast<unsigned int>(intro_3d_quality_resolved_));
  lv_obj_clear_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
}

void UiManager::tickIntro() {
  if (!intro_active_ || intro_root_ == nullptr) {
    return;
  }
  const uint32_t now = lv_tick_get();
  fx_engine_.noteFrame(now);
  uint32_t dt_ms = now - last_tick_ms_;
  if (dt_ms > 100U) {
    dt_ms = 100U;
  }
  last_tick_ms_ = now;
  const uint32_t state_elapsed = now - t_state0_ms_;

  if (intro_skip_requested_ &&
      (intro_state_ == IntroState::PHASE_A_CRACKTRO || intro_state_ == IntroState::PHASE_B_TRANSITION)) {
    intro_skip_requested_ = false;
    transitionIntroState(IntroState::PHASE_C_CLEAN);
  }

  fx_engine_.setSceneCounts(estimateIntroObjectCount(), intro_star_count_, intro_firework_active_count_);

  if (now >= intro_phase_log_next_ms_) {
    intro_phase_log_next_ms_ = now + 5000U;
    const UiMemorySnapshot mem = memorySnapshot();
    const ui::fx::FxEngineStats fx_stats = fx_engine_.stats();
    Serial.printf("[UI][WIN_ETAPE] phase=%u t=%lu obj=%u stars=%u particles=%u fx_fps=%u q=%u heap_int=%u heap_psram=%u largest_dma=%u\n",
                  static_cast<unsigned int>(intro_state_),
                  static_cast<unsigned long>(state_elapsed),
                  static_cast<unsigned int>(estimateIntroObjectCount()),
                  static_cast<unsigned int>(intro_star_count_),
                  static_cast<unsigned int>(intro_firework_active_count_),
                  static_cast<unsigned int>(fx_stats.fps),
                  static_cast<unsigned int>(intro_3d_quality_resolved_),
                  static_cast<unsigned int>(mem.heap_internal_free),
                  static_cast<unsigned int>(mem.heap_psram_free),
                  static_cast<unsigned int>(mem.heap_largest_dma_block));
  }

  switch (intro_state_) {
    case IntroState::PHASE_A_CRACKTRO:
      if (state_elapsed < 5000U) {
        intro_cube_morph_enabled_ = false;
        intro_cube_morph_phase_ = 0.0f;
      } else if (state_elapsed < 15000U) {
        intro_cube_morph_enabled_ = true;
        intro_cube_morph_speed_ = 0.314f;
      } else if (state_elapsed >= 25000U) {
        intro_cube_morph_enabled_ = true;
        intro_cube_morph_phase_ = 3.14159f;
        intro_cube_morph_speed_ = 0.22f;
      } else {
        intro_cube_morph_enabled_ = true;
        intro_cube_morph_speed_ = 0.90f;
      }
      updateCopperBars(now - intro_total_start_ms_);
      updateStarfield(dt_ms);
      updateWavySineScroller(dt_ms, now);
      updateBottomRollbackScroller(dt_ms);
      animateLogoOvershoot();
      updateWireCube(dt_ms, false);
      updateIntroDebugOverlay(dt_ms);
      if (state_elapsed >= intro_config_.a_duration_ms) {
        transitionIntroState(IntroState::PHASE_B_TRANSITION);
      }
      break;

    case IntroState::PHASE_B_TRANSITION:
      updateBPhase(dt_ms, now, state_elapsed);
      updateIntroDebugOverlay(dt_ms);
      if (state_elapsed >= intro_config_.b_duration_ms) {
        transitionIntroState(IntroState::PHASE_C_CLEAN);
      }
      break;

    case IntroState::PHASE_C_CLEAN:
      updateC3DStage(now);
      updateStarfield(dt_ms);
      if (intro_3d_mode_ == Intro3DMode::kWireCube) {
        updateWireCube(dt_ms, false);
      } else {
        updateRotoZoom(dt_ms);
      }
      updateWavySineScroller(dt_ms, now);
      updateCleanReveal(dt_ms);
      updateFireworks(dt_ms);
      updateIntroDebugOverlay(dt_ms);
      if (state_elapsed >= intro_config_.c_duration_ms) {
        transitionIntroState(IntroState::PHASE_C_LOOP);
      }
      break;

    case IntroState::PHASE_C_LOOP:
      updateC3DStage(now);
      updateStarfield(dt_ms);
      if (intro_3d_mode_ == Intro3DMode::kWireCube) {
        updateWireCube(dt_ms, false);
      } else {
        updateRotoZoom(dt_ms);
      }
      updateWavySineScroller(dt_ms, now);
      updateCleanReveal(dt_ms);
      updateFireworks(dt_ms);
      updateIntroDebugOverlay(dt_ms);
      if (state_elapsed >= intro_config_.c_duration_ms) {
        transitionIntroState(IntroState::PHASE_C_LOOP);
      }
      break;

    case IntroState::OUTRO: {
      updateFireworks(dt_ms);
      const uint32_t elapsed = state_elapsed;
      if (elapsed >= kIntroOutroMs) {
        stopIntroAndCleanup();
      } else {
        const int32_t opa =
            LV_OPA_COVER - static_cast<int32_t>((elapsed * LV_OPA_COVER) / kIntroOutroMs);
        lv_obj_set_style_opa(intro_root_,
                             static_cast<lv_opa_t>(clampValue<int32_t>(opa, 0, LV_OPA_COVER)),
                             LV_PART_MAIN);
      }
      break;
    }

    case IntroState::DONE:
    default:
      break;
  }
}

void UiManager::introTimerCb(lv_timer_t* timer) {
  if (timer == nullptr || timer->user_data == nullptr) {
    return;
  }
  UiManager* self = static_cast<UiManager*>(timer->user_data);
  self->tickIntro();
}


void UiManager::configureWaveformOverlay(const HardwareManager::Snapshot* snapshot,
                                         bool enabled,
                                         uint8_t sample_count,
                                         uint8_t amplitude_pct,
                                         bool jitter) {
  waveform_overlay_enabled_ = enabled;
  waveform_snapshot_ref_ = snapshot;
  waveform_snapshot_valid_ = (snapshot != nullptr);
  if (snapshot != nullptr) {
    waveform_snapshot_ = *snapshot;
  }
  waveform_sample_count_ = sample_count;
  waveform_amplitude_pct_ = amplitude_pct;
  waveform_overlay_jitter_ = jitter;

  if (!waveform_overlay_enabled_ || scene_waveform_ == nullptr) {
    if (scene_waveform_outer_ != nullptr) {
      lv_obj_add_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_waveform_ != nullptr) {
      lv_obj_add_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  if (scene_waveform_outer_ != nullptr) {
    lv_obj_set_style_opa(scene_waveform_outer_, LV_OPA_60, LV_PART_MAIN);
    lv_obj_clear_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_opa(scene_waveform_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
}

void UiManager::updateLaOverlay(bool visible,
                                uint16_t freq_hz,
                                int16_t cents,
                                uint8_t confidence,
                                uint8_t level_pct,
                                uint8_t stability_pct) {
  auto hide_all = [this]() {
    if (scene_la_status_label_ != nullptr) {
      lv_obj_add_flag(scene_la_status_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_pitch_label_ != nullptr) {
      lv_obj_add_flag(scene_la_pitch_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_timer_label_ != nullptr) {
      lv_obj_add_flag(scene_la_timer_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_timeout_label_ != nullptr) {
      lv_obj_add_flag(scene_la_timeout_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_meter_bg_ != nullptr) {
      lv_obj_add_flag(scene_la_meter_bg_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_meter_fill_ != nullptr) {
      lv_obj_add_flag(scene_la_meter_fill_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_la_needle_ != nullptr) {
      lv_obj_add_flag(scene_la_needle_, LV_OBJ_FLAG_HIDDEN);
    }
    for (lv_obj_t* bar : scene_la_analyzer_bars_) {
      if (bar != nullptr) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
      }
    }
  };

  if (!visible) {
    hide_all();
    return;
  }
  if (scene_la_status_label_ == nullptr || scene_la_pitch_label_ == nullptr || scene_la_timer_label_ == nullptr ||
      scene_la_timeout_label_ == nullptr || scene_la_meter_bg_ == nullptr || scene_la_meter_fill_ == nullptr ||
      scene_la_needle_ == nullptr || scene_core_ == nullptr ||
      scene_ring_outer_ == nullptr) {
    hide_all();
    return;
  }

  const SceneState scene_state = SceneState::fromLaSample(
      la_detection_locked_, freq_hz, cents, confidence, level_pct, stability_pct);
  const int16_t info_shift_y = 36;
  const int16_t hz_line_shift_y = 8;
  const int16_t meter_shift_y = 32;
  const int16_t analyzer_shift_y = 52;
  const String status_text = asciiFallbackForUiText(scene_state.status_text);
  lv_label_set_text(scene_la_status_label_, status_text.c_str());
  lv_obj_set_style_text_color(scene_la_status_label_, lv_color_hex(scene_state.status_rgb), LV_PART_MAIN);
  lv_obj_align(scene_la_status_label_, LV_ALIGN_TOP_RIGHT, -8, static_cast<lv_coord_t>(8 + info_shift_y));
  lv_obj_clear_flag(scene_la_status_label_, LV_OBJ_FLAG_HIDDEN);

  char pitch_line[56] = {0};
  std::snprintf(pitch_line,
                sizeof(pitch_line),
                "%3u Hz  %+d c  C%u  S%u",
                static_cast<unsigned int>(freq_hz),
                static_cast<int>(cents),
                static_cast<unsigned int>(scene_state.confidence),
                static_cast<unsigned int>(scene_state.stability_pct));
  const String pitch_text = asciiFallbackForUiText(pitch_line);
  lv_label_set_text(scene_la_pitch_label_, pitch_text.c_str());
  lv_obj_align(scene_la_pitch_label_, LV_ALIGN_BOTTOM_MID, 0, static_cast<lv_coord_t>(-30 + hz_line_shift_y));
  lv_obj_clear_flag(scene_la_pitch_label_, LV_OBJ_FLAG_HIDDEN);

  const uint32_t stable_target_ms = (la_detection_stable_target_ms_ > 0U) ? la_detection_stable_target_ms_ : 3000U;
  const float stable_sec = static_cast<float>(la_detection_stable_ms_) / 1000.0f;
  const float stable_target_sec = static_cast<float>(stable_target_ms) / 1000.0f;
  char timer_line[40] = {0};
  std::snprintf(timer_line,
                sizeof(timer_line),
                "Stabilite %.1fs / %.1fs",
                static_cast<double>(stable_sec),
                static_cast<double>(stable_target_sec));
  const String timer_text = asciiFallbackForUiText(timer_line);
  lv_label_set_text(scene_la_timer_label_, timer_text.c_str());
  lv_obj_set_style_text_color(scene_la_timer_label_, lv_color_hex(la_detection_locked_ ? 0x9DFF63UL : 0x9AD6FFUL), LV_PART_MAIN);
  lv_obj_align(scene_la_timer_label_, LV_ALIGN_TOP_LEFT, 8, static_cast<lv_coord_t>(8 + info_shift_y));
  lv_obj_clear_flag(scene_la_timer_label_, LV_OBJ_FLAG_HIDDEN);

  if (la_detection_gate_timeout_ms_ > 0U) {
    const int32_t remain_ms = static_cast<int32_t>(la_detection_gate_timeout_ms_) - static_cast<int32_t>(la_detection_gate_elapsed_ms_);
    const float remain_sec = static_cast<float>((remain_ms > 0) ? remain_ms : 0) / 1000.0f;
    const float limit_sec = static_cast<float>(la_detection_gate_timeout_ms_) / 1000.0f;
    char timeout_line[42] = {0};
    std::snprintf(timeout_line,
                  sizeof(timeout_line),
                  "Timeout %.1fs / %.1fs",
                  static_cast<double>(remain_sec),
                  static_cast<double>(limit_sec));
    const String timeout_text = asciiFallbackForUiText(timeout_line);
    lv_label_set_text(scene_la_timeout_label_, timeout_text.c_str());
    lv_obj_set_style_text_color(scene_la_timeout_label_, lv_color_hex((remain_ms < 3000) ? 0xFFB06DUL : 0x84CFFFUL), LV_PART_MAIN);
    lv_obj_align(scene_la_timeout_label_, LV_ALIGN_TOP_MID, 0, static_cast<lv_coord_t>(30 + info_shift_y));
    lv_obj_clear_flag(scene_la_timeout_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(scene_la_timeout_label_, LV_OBJ_FLAG_HIDDEN);
  }

  int16_t meter_width = static_cast<int16_t>(activeDisplayWidth() - 52);
  if (meter_width < 96) {
    meter_width = 96;
  }

  lv_obj_set_size(scene_la_meter_bg_, meter_width, 10);
  lv_obj_align(scene_la_meter_bg_, LV_ALIGN_BOTTOM_MID, 0, static_cast<lv_coord_t>(-12 - meter_shift_y));
  lv_obj_clear_flag(scene_la_meter_bg_, LV_OBJ_FLAG_HIDDEN);

  const uint8_t meter_pct =
      static_cast<uint8_t>(((static_cast<uint16_t>(scene_state.confidence) * 35U) +
                            (static_cast<uint16_t>(scene_state.level_pct) * 30U) +
                            (static_cast<uint16_t>(scene_state.stability_pct) * 35U)) /
                           100U);
  int16_t fill_width = static_cast<int16_t>((static_cast<int32_t>(meter_width - 4) * meter_pct) / 100);
  if (fill_width < 6) {
    fill_width = 6;
  }
  if (fill_width > (meter_width - 4)) {
    fill_width = meter_width - 4;
  }
  lv_obj_set_size(scene_la_meter_fill_, fill_width, 6);
  lv_obj_align_to(scene_la_meter_fill_, scene_la_meter_bg_, LV_ALIGN_LEFT_MID, 2, 0);
  uint32_t meter_rgb = 0x4AD0FFUL;
  if (scene_state.locked) {
    meter_rgb = 0x8DFF63UL;
  } else if (scene_state.abs_cents <= 12 && scene_state.confidence >= 55U) {
    meter_rgb = 0xD8FF74UL;
  } else if (scene_state.abs_cents > 30) {
    meter_rgb = 0xFF8259UL;
  } else {
    meter_rgb = 0xFFC56EUL;
  }
  lv_obj_set_style_bg_color(scene_la_meter_fill_, lv_color_hex(meter_rgb), LV_PART_MAIN);
  lv_obj_clear_flag(scene_la_meter_fill_, LV_OBJ_FLAG_HIDDEN);

  const int16_t center_x = static_cast<int16_t>(lv_obj_get_x(scene_core_) + (lv_obj_get_width(scene_core_) / 2));
  const int16_t center_y = static_cast<int16_t>(lv_obj_get_y(scene_core_) + (lv_obj_get_height(scene_core_) / 2));
  int16_t ring_radius = static_cast<int16_t>(lv_obj_get_width(scene_ring_outer_) / 2);
  if (ring_radius < 40) {
    ring_radius = 40;
  }

  int16_t tuned_cents = scene_state.cents;
  if (tuned_cents < -60) {
    tuned_cents = -60;
  } else if (tuned_cents > 60) {
    tuned_cents = 60;
  }
  constexpr float kPi = 3.14159265f;
  constexpr float kHalfPi = 1.57079632f;
  const float normalized = static_cast<float>(tuned_cents) / 60.0f;
  const float jitter = (100U - scene_state.confidence) * 0.0007f;
  const float angle = (-kHalfPi) + (normalized * (kPi / 2.6f)) + jitter;
  const int16_t needle_radius = static_cast<int16_t>(ring_radius - 2);
  const int16_t x = static_cast<int16_t>(center_x + std::cos(angle) * static_cast<float>(needle_radius));
  const int16_t y = static_cast<int16_t>(center_y + std::sin(angle) * static_cast<float>(needle_radius));
  la_needle_points_[0].x = center_x;
  la_needle_points_[0].y = center_y;
  la_needle_points_[1].x = x;
  la_needle_points_[1].y = y;
  lv_line_set_points(scene_la_needle_, la_needle_points_, 2);
  lv_obj_set_pos(scene_la_needle_, 0, 0);
  lv_obj_set_style_line_width(scene_la_needle_, scene_state.locked ? 4 : 3, LV_PART_MAIN);
  lv_obj_set_style_line_color(scene_la_needle_, lv_color_hex(meter_rgb), LV_PART_MAIN);
  lv_obj_clear_flag(scene_la_needle_, LV_OBJ_FLAG_HIDDEN);

  const int16_t bar_region_width = 92;
  const int16_t bar_x_start = activeDisplayWidth() - bar_region_width - 8;
  const int16_t bar_y_bottom = static_cast<int16_t>(activeDisplayHeight() - 54 - analyzer_shift_y);
  const float freq_norm = (freq_hz <= 320U)
                              ? 0.0f
                              : ((freq_hz >= 560U) ? 1.0f : (static_cast<float>(freq_hz - 320U) / 240.0f));
  const float freq_bin_pos = freq_norm * static_cast<float>(kLaAnalyzerBarCount - 1U);
  const float signal_gain = (static_cast<float>(scene_state.level_pct) / 100.0f) *
                            (0.45f + static_cast<float>(scene_state.confidence) / 200.0f);
  for (uint8_t index = 0U; index < kLaAnalyzerBarCount; ++index) {
    lv_obj_t* bar = scene_la_analyzer_bars_[index];
    if (bar == nullptr) {
      continue;
    }
    const float distance = std::fabs(static_cast<float>(index) - freq_bin_pos);
    float profile = 1.0f - (distance / 2.8f);
    if (profile < 0.0f) {
      profile = 0.0f;
    }
    float energy = profile * signal_gain;
    if (freq_hz == 0U || scene_state.confidence < 8U) {
      const uint32_t seed = pseudoRandom32(static_cast<uint32_t>(millis()) + static_cast<uint32_t>(index * 97U));
      energy = (static_cast<float>((seed % 26U) + 8U) / 100.0f) * (static_cast<float>(scene_state.level_pct) / 100.0f);
    }
    int16_t height = static_cast<int16_t>(6 + (energy * 44.0f));
    if (height < 6) {
      height = 6;
    }
    if (height > 50) {
      height = 50;
    }
    const int16_t x = static_cast<int16_t>(bar_x_start + static_cast<int16_t>(index * 11));
    const int16_t y = static_cast<int16_t>(bar_y_bottom - height);
    lv_obj_set_size(bar, 8, height);
    lv_obj_set_pos(bar, x, y);
    uint32_t bar_color = 0x3CCBFFUL;
    if (distance <= 0.7f && scene_state.confidence >= 24U) {
      bar_color = 0xA5FF72UL;
    } else if (distance <= 1.8f) {
      bar_color = 0xFFD27AUL;
    } else if (distance >= 3.0f) {
      bar_color = 0x5F86FFUL;
    }
    lv_obj_set_style_bg_color(bar, lv_color_hex(bar_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, static_cast<lv_opa_t>(120 + (scene_state.confidence / 2U)), LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::renderMicrophoneWaveform() {
  auto hide_waveform = [this]() {
    if (scene_waveform_outer_ != nullptr) {
      lv_obj_add_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_waveform_ != nullptr) {
      lv_obj_add_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
    }
  };

  if (!ready_ || scene_waveform_ == nullptr) {
    return;
  }
  const HardwareManager::Snapshot* active_snapshot = waveform_snapshot_ref_;
  if (active_snapshot == nullptr && waveform_snapshot_valid_) {
    active_snapshot = &waveform_snapshot_;
  }
  const uint16_t freq_hz = (active_snapshot != nullptr) ? active_snapshot->mic_freq_hz : 0U;
  const int16_t cents = (active_snapshot != nullptr) ? active_snapshot->mic_pitch_cents : 0;
  const uint8_t confidence = (active_snapshot != nullptr) ? active_snapshot->mic_pitch_confidence : 0U;
  const uint8_t level_pct = (active_snapshot != nullptr) ? active_snapshot->mic_level_percent : 0U;
  const uint8_t stability_pct = la_detection_stability_pct_;

  if (!waveform_overlay_enabled_ || active_snapshot == nullptr || active_snapshot->mic_waveform_count == 0U) {
    hide_waveform();
    updateLaOverlay(la_detection_scene_, freq_hz, cents, confidence, level_pct, stability_pct);
    return;
  }
  const HardwareManager::Snapshot& snapshot = *active_snapshot;

  if (scene_core_ == nullptr || scene_ring_outer_ == nullptr) {
    hide_waveform();
    updateLaOverlay(false, 0U, 0, 0U, 0U, 0U);
    return;
  }

  uint8_t first = snapshot.mic_waveform_head;
  uint8_t count = snapshot.mic_waveform_count;
  if (count > HardwareManager::kMicWaveformCapacity) {
    count = HardwareManager::kMicWaveformCapacity;
  }
  uint16_t start = (first >= count) ? static_cast<uint16_t>(first - count) : static_cast<uint16_t>(first + HardwareManager::kMicWaveformCapacity - count);
  const uint8_t display_count = (waveform_sample_count_ == 0U) ? 1U : waveform_sample_count_;
  const uint8_t points_to_draw = (count < display_count) ? count : display_count;
  if (points_to_draw < 3U) {
    hide_waveform();
    updateLaOverlay(la_detection_scene_, freq_hz, cents, confidence, level_pct, stability_pct);
    return;
  }

  int16_t abs_cents = cents;
  if (abs_cents < 0) {
    abs_cents = static_cast<int16_t>(-abs_cents);
  }

  const bool locked_scene = (std::strcmp(last_scene_id_, "SCENE_LOCKED") == 0);
  uint32_t inner_color = 0x8FFFD2U;
  uint32_t outer_color = 0x3CD8FFU;
  if (locked_scene) {
    inner_color = (confidence >= 20U) ? 0xFFD78CU : 0xFFAA6DU;
    outer_color = (level_pct >= 22U) ? 0xFF5564U : 0xFF854EU;
  } else if (la_detection_scene_) {
    if (la_detection_locked_) {
      inner_color = 0x84FF68U;
      outer_color = 0xD8FF86U;
    } else if (stability_pct >= 70U) {
      inner_color = 0xD8FF6BU;
      outer_color = 0xFFE08AU;
    } else if (stability_pct >= 35U) {
      inner_color = 0x7EE9FFU;
      outer_color = 0x72B8FFU;
    } else {
      inner_color = 0x4ED4FFU;
      outer_color = 0x4E7DFFU;
    }
  } else if (confidence < 16U) {
    inner_color = 0x63E6FFU;
    outer_color = 0x2B90FFU;
  } else if (abs_cents <= 12) {
    inner_color = 0x7DFF7FU;
    outer_color = 0x36CF7FU;
  } else if (abs_cents <= 35) {
    inner_color = 0xFFD96AU;
    outer_color = 0xFF9F4AU;
  } else {
    inner_color = 0xFF7A62U;
    outer_color = 0xFF3F57U;
  }
  uint8_t inner_width = (confidence >= 32U) ? 3U : 2U;
  uint8_t outer_width = (confidence >= 24U) ? 2U : 1U;
  lv_opa_t inner_opa = (confidence >= 20U) ? LV_OPA_COVER : LV_OPA_70;
  lv_opa_t outer_opa = (confidence >= 20U) ? LV_OPA_70 : LV_OPA_40;
  if (la_detection_scene_) {
    inner_width = la_detection_locked_ ? 5U : ((stability_pct >= 55U) ? 4U : 3U);
    outer_width = la_detection_locked_ ? 3U : 2U;
    inner_opa = LV_OPA_COVER;
    outer_opa = la_detection_locked_ ? LV_OPA_90 : LV_OPA_70;
  }
  lv_obj_set_style_line_color(scene_waveform_, lv_color_hex(inner_color), LV_PART_MAIN);
  lv_obj_set_style_line_width(scene_waveform_, inner_width, LV_PART_MAIN);
  lv_obj_set_style_opa(scene_waveform_, inner_opa, LV_PART_MAIN);
  if (scene_waveform_outer_ != nullptr) {
    lv_obj_set_style_line_color(scene_waveform_outer_, lv_color_hex(outer_color), LV_PART_MAIN);
    lv_obj_set_style_line_width(scene_waveform_outer_, outer_width, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_waveform_outer_, outer_opa, LV_PART_MAIN);
  }

  if (locked_scene) {
    const int16_t width = activeDisplayWidth();
    const int16_t height = activeDisplayHeight();
    if (width < 40 || height < 40) {
      hide_waveform();
      updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U);
      return;
    }

    const uint32_t now_ms = millis();
    const uint16_t sweep_ms = resolveAnimMs(1600);
    float phase = static_cast<float>(now_ms % sweep_ms) / static_cast<float>(sweep_ms);
    if (phase > 0.5f) {
      phase = 1.0f - phase;
    }
    const float sweep = phase * 2.0f;

    const int16_t top_margin = 22;
    const int16_t bottom_margin = 20;
    int16_t base_y = static_cast<int16_t>(top_margin + sweep * static_cast<float>(height - top_margin - bottom_margin));
    base_y = static_cast<int16_t>(base_y + signedNoise(now_ms / 19U, reinterpret_cast<uintptr_t>(scene_waveform_) ^ 0xA5319B4DUL, 9));
    if (base_y < top_margin) {
      base_y = top_margin;
    } else if (base_y > (height - bottom_margin)) {
      base_y = static_cast<int16_t>(height - bottom_margin);
    }

    const int16_t left_margin = 12;
    const int16_t right_margin = 12;
    const int16_t usable_width = static_cast<int16_t>(width - left_margin - right_margin);
    if (usable_width < 16) {
      hide_waveform();
      updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U);
      return;
    }

    int16_t amplitude = static_cast<int16_t>(8 + (static_cast<int16_t>(waveform_amplitude_pct_) / 5) +
                                             (static_cast<int16_t>(level_pct) / 3));
    if (amplitude > 42) {
      amplitude = 42;
    }
    if (confidence < 12U) {
      amplitude = static_cast<int16_t>(amplitude * 2 / 3);
    }
    if (amplitude < 6) {
      amplitude = 6;
    }

    const int16_t scan_drift_x =
        signedNoise(now_ms / 15U, reinterpret_cast<uintptr_t>(scene_waveform_) ^ 0x7D6AB111UL, 22);
    const int16_t outer_y_bias = static_cast<int16_t>(2 + (level_pct / 24U));
    uint8_t point_index = 0U;
    for (uint8_t index = 0U; index < points_to_draw; ++index) {
      const uint16_t sample_index = static_cast<uint16_t>(start + index) % HardwareManager::kMicWaveformCapacity;
      uint8_t sample = snapshot.mic_waveform[sample_index];
      if (sample > 100U) {
        sample = 100U;
      }

      int16_t x = static_cast<int16_t>(
          left_margin + (static_cast<int32_t>(usable_width) * static_cast<int32_t>(index)) /
                            static_cast<int32_t>(points_to_draw - 1U));
      x = static_cast<int16_t>(x + scan_drift_x);
      if (waveform_overlay_jitter_) {
        x = static_cast<int16_t>(
            x + signedNoise(now_ms + static_cast<uint32_t>(index) * 31U,
                            reinterpret_cast<uintptr_t>(scene_waveform_outer_) ^ static_cast<uintptr_t>(sample_index), 3));
      }

      const int16_t centered = static_cast<int16_t>(sample) - 50;
      const int16_t spike = static_cast<int16_t>((static_cast<int32_t>(centered) * centered) / 100);
      int16_t y = static_cast<int16_t>(base_y + ((centered * amplitude) / 50) + (centered >= 0 ? spike / 5 : -spike / 7));
      if (waveform_overlay_jitter_) {
        y = static_cast<int16_t>(
            y + signedNoise((now_ms / 2U) + static_cast<uint32_t>(index) * 53U,
                            reinterpret_cast<uintptr_t>(scene_waveform_) ^ 0x5F3783A5UL,
                            static_cast<int16_t>(3 + (level_pct / 18U))));
      }

      if ((mixNoise(now_ms + static_cast<uint32_t>(index) * 67U,
                    reinterpret_cast<uintptr_t>(scene_waveform_) ^ 0xC2B2AE35UL) &
           0x0FU) == 0U) {
        y = static_cast<int16_t>(y + signedNoise(now_ms + static_cast<uint32_t>(index) * 89U,
                                                 reinterpret_cast<uintptr_t>(scene_fx_bar_) ^ 0x27D4EB2FUL,
                                                 static_cast<int16_t>(8 + (level_pct / 8U))));
      }

      if (x < 3) {
        x = 3;
      } else if (x > (width - 3)) {
        x = static_cast<int16_t>(width - 3);
      }
      if (y < 4) {
        y = 4;
      } else if (y > (height - 4)) {
        y = static_cast<int16_t>(height - 4);
      }

      int16_t y_outer = static_cast<int16_t>(
          y + outer_y_bias +
          signedNoise(now_ms + static_cast<uint32_t>(index) * 41U, reinterpret_cast<uintptr_t>(scene_waveform_outer_), 2));
      if (y_outer < 4) {
        y_outer = 4;
      } else if (y_outer > (height - 4)) {
        y_outer = static_cast<int16_t>(height - 4);
      }

      waveform_points_[point_index].x = x;
      waveform_points_[point_index].y = y;
      waveform_outer_points_[point_index].x = x;
      waveform_outer_points_[point_index].y = y_outer;
      ++point_index;
    }

    lv_line_set_points(scene_waveform_, waveform_points_, point_index);
    if (scene_waveform_outer_ != nullptr) {
      lv_line_set_points(scene_waveform_outer_, waveform_outer_points_, point_index);
      lv_obj_set_pos(scene_waveform_outer_, 0, 0);
      lv_obj_clear_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(scene_waveform_, 0, 0);
    lv_obj_clear_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
    updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U);
    return;
  }

  const int16_t center_x = static_cast<int16_t>(lv_obj_get_x(scene_core_) + (lv_obj_get_width(scene_core_) / 2));
  const int16_t center_y = static_cast<int16_t>(lv_obj_get_y(scene_core_) + (lv_obj_get_height(scene_core_) / 2));
  int16_t core_radius = static_cast<int16_t>(lv_obj_get_width(scene_core_) / 2);
  int16_t ring_radius = static_cast<int16_t>(lv_obj_get_width(scene_ring_outer_) / 2);
  if (core_radius < 12) {
    core_radius = 12;
  }
  if (ring_radius <= (core_radius + 6)) {
    ring_radius = static_cast<int16_t>(core_radius + 12);
  }

  int16_t ring_band = static_cast<int16_t>(ring_radius - core_radius);
  if (ring_band < 6) {
    ring_band = 6;
  }
  int16_t base_radius = static_cast<int16_t>(core_radius + ((ring_band * 58) / 100));
  int16_t radius_span = static_cast<int16_t>((ring_band * static_cast<int16_t>(waveform_amplitude_pct_)) / 140);
  if (radius_span < 4) {
    radius_span = 4;
  }
  const int16_t max_span = static_cast<int16_t>(ring_band - 2);
  if (radius_span > max_span) {
    radius_span = max_span;
  }
  const int16_t level_boost = static_cast<int16_t>(snapshot.mic_level_percent / 9U);
  const int16_t jitter_amp = waveform_overlay_jitter_ ? 2 : 0;
  constexpr float kTau = 6.28318530718f;
  constexpr float kHalfPi = 1.57079632679f;
  int16_t outer_offset = static_cast<int16_t>(2 + (static_cast<int16_t>(snapshot.mic_level_percent) / 28));
  if (la_detection_scene_) {
    outer_offset = static_cast<int16_t>(outer_offset + 2 + (static_cast<int16_t>(stability_pct) / 20));
  }
  const float spin_phase =
      la_detection_scene_ ? static_cast<float>((millis() / 12U) % 360U) * (kTau / 360.0f) : 0.0f;

  uint8_t point_index = 0U;
  for (uint8_t index = 0U; index < points_to_draw; ++index) {
    const uint16_t sample_index = static_cast<uint16_t>(start + index) % HardwareManager::kMicWaveformCapacity;
    uint8_t sample = snapshot.mic_waveform[sample_index];
    if (sample > 100U) {
      sample = 100U;
    }

    const uint32_t noise_seed =
        pseudoRandom32(static_cast<uint32_t>(start) + static_cast<uint32_t>((index + 1U) * 113U));
    int16_t radial_jitter = static_cast<int16_t>((noise_seed % 5U) - 2U);
    if (radial_jitter > jitter_amp) {
      radial_jitter = jitter_amp;
    }
    if (radial_jitter < -jitter_amp) {
      radial_jitter = -jitter_amp;
    }

    const int16_t centered = static_cast<int16_t>(sample) - 50;
    const int16_t punch = static_cast<int16_t>((static_cast<int32_t>(centered) * centered) / 120);
    int16_t radius =
        static_cast<int16_t>(base_radius + ((centered * radius_span) / 50) + (punch / 3) + radial_jitter + level_boost);
    const int16_t min_radius = static_cast<int16_t>(core_radius + 2);
    const int16_t max_radius = static_cast<int16_t>(ring_radius - 2);
    if (radius < min_radius) {
      radius = min_radius;
    }
    if (radius > max_radius) {
      radius = max_radius;
    }

    const float phase = static_cast<float>(index) / static_cast<float>(points_to_draw);
    float phase_warp = static_cast<float>((static_cast<int>(noise_seed >> 12U) & 0x7) - 3) * 0.0036f;
    if (la_detection_scene_) {
      phase_warp *= 1.6f;
    }
    const float angle = (-kHalfPi) + spin_phase + ((phase + phase_warp) * kTau);
    const int16_t x = static_cast<int16_t>(center_x + std::cos(angle) * static_cast<float>(radius));
    const int16_t y = static_cast<int16_t>(center_y + std::sin(angle) * static_cast<float>(radius));
    int16_t outer_radius = static_cast<int16_t>(radius + outer_offset);
    if (outer_radius > ring_radius) {
      outer_radius = ring_radius;
    }
    const int16_t x_outer = static_cast<int16_t>(center_x + std::cos(angle) * static_cast<float>(outer_radius));
    const int16_t y_outer = static_cast<int16_t>(center_y + std::sin(angle) * static_cast<float>(outer_radius));

    waveform_points_[point_index].x = x;
    waveform_points_[point_index].y = y;
    waveform_outer_points_[point_index].x = x_outer;
    waveform_outer_points_[point_index].y = y_outer;
    ++point_index;
  }

  if (point_index >= 2U && point_index < (HardwareManager::kMicWaveformCapacity + 1U)) {
    waveform_points_[point_index] = waveform_points_[0];
    waveform_outer_points_[point_index] = waveform_outer_points_[0];
    ++point_index;
  }

  lv_line_set_points(scene_waveform_, waveform_points_, point_index);
  if (scene_waveform_outer_ != nullptr) {
    lv_line_set_points(scene_waveform_outer_, waveform_outer_points_, point_index);
    lv_obj_set_pos(scene_waveform_outer_, 0, 0);
    lv_obj_clear_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_pos(scene_waveform_, 0, 0);
  lv_obj_clear_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
  updateLaOverlay(la_detection_scene_,
                  snapshot.mic_freq_hz,
                  snapshot.mic_pitch_cents,
                  snapshot.mic_pitch_confidence,
                  snapshot.mic_level_percent,
                  stability_pct);
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
    Serial.printf("[UI] unknown scene id '%s' in scenario=%s step=%s\n", raw_scene_id, scenario_id, step_id_for_log);
    return;
  }
  if (normalized_scene_id != nullptr && std::strcmp(raw_scene_id, normalized_scene_id) != 0) {
    Serial.printf("[UI] scene alias normalized: %s -> %s\n", raw_scene_id, normalized_scene_id);
  }
  const char* scene_id = normalized_scene_id;
  const bool scene_changed = (std::strcmp(last_scene_id_, scene_id) != 0);
  const bool has_previous_scene = (last_scene_id_[0] != '\0');
  const bool win_etape_intro_scene = (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0);

  if (!win_etape_intro_scene && intro_active_) {
    stopIntroAndCleanup();
  }
  if (!win_etape_intro_scene) {
    intro_skip_latched_ = false;
  }
  if (win_etape_intro_scene && intro_active_ && !scene_changed) {
    return;
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
    Serial.printf("[UI] unknown effect token '%s' in %s, fallback\n", token, source);
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
    Serial.printf("[UI] unknown transition token '%s' in %s, fallback\n", token, source);
    return fallback;
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
  bool win_etape_bravo_mode = false;
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

  if (std::strcmp(scene_id, "SCENE_LOCKED") == 0) {
    title = "Module U-SON PROTO";
    subtitle = "VERIFICATION EN COURS";
    symbol = "LOCK";
    effect = SceneEffect::kGlitch;
    waveform_enabled = true;
    waveform_sample_count = HardwareManager::kMicWaveformCapacity;
    waveform_amplitude_pct = 100U;
    waveform_jitter = true;
    bg_rgb = 0x07070FUL;
    accent_rgb = 0xFFB74EUL;
    text_rgb = 0xF6FBFFUL;
  } else if (std::strcmp(scene_id, "SCENE_BROKEN") == 0) {
    title = "PROTO U-SON";
    subtitle = "Signal brouille";
    symbol = "ALERT";
    effect = SceneEffect::kBlink;
    bg_rgb = 0x2A0508UL;
    accent_rgb = 0xFF4A45UL;
    text_rgb = 0xFFD5D1UL;
  } else if (std::strcmp(scene_id, "SCENE_LA_DETECTOR") == 0 ||
             std::strcmp(scene_id, "SCENE_SEARCH") == 0 ||
             std::strcmp(scene_id, "SCENE_CAMERA_SCAN") == 0) {
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
  } else if (std::strcmp(scene_id, "SCENE_SIGNAL_SPIKE") == 0) {
    title = "PIC DE SIGNAL";
    subtitle = "Interference detectee";
    symbol = "ALERT";
    effect = SceneEffect::kWave;
    bg_rgb = 0x24090CUL;
    accent_rgb = 0xFF6A52UL;
    text_rgb = 0xFFF2EBUL;
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 || std::strcmp(scene_id, "SCENE_REWARD") == 0) {
    title = "VICTOIRE";
    symbol = "WIN";
    effect = SceneEffect::kCelebrate;
    bg_rgb = 0x231038UL;
    accent_rgb = 0xF4CB4AUL;
    text_rgb = 0xFFF6C7UL;
    subtitle = "Etape validee";
  } else if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0) {
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
    win_etape_bravo_mode = true;
    win_etape_fireworks = false;
    subtitle_scroll_mode = SceneScrollMode::kNone;
  } else if (std::strcmp(scene_id, "SCENE_READY") == 0 || std::strcmp(scene_id, "SCENE_MEDIA_ARCHIVE") == 0) {
    title = "PRET";
    subtitle = "Scenario termine";
    symbol = "READY";
    effect = SceneEffect::kWave;
    bg_rgb = 0x0F2A12UL;
    accent_rgb = 0x6CD96BUL;
    text_rgb = 0xE8FFE7UL;
  }

  resetSceneTimeline();

  if (screen_payload_json != nullptr && screen_payload_json[0] != '\0') {
    DynamicJsonDocument document(4096);
    const DeserializationError error = deserializeJson(document, screen_payload_json);
    if (!error) {
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
      Serial.printf("[UI] invalid scene payload (%s)\n", error.c_str());
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
  configureWaveformOverlay((waveform_snapshot_ref_ != nullptr) ? waveform_snapshot_ref_
                                                                : (waveform_snapshot_valid_ ? &waveform_snapshot_
                                                                                            : nullptr),
                          waveform_enabled,
                          waveform_sample_count,
                          waveform_amplitude_pct,
                          waveform_jitter);
  if (win_etape_intro_scene) {
    effect = SceneEffect::kNone;
    transition = SceneTransition::kFade;
    transition_ms = 220U;
    subtitle_scroll_mode = SceneScrollMode::kNone;
    win_etape_fireworks = false;
    resetSceneTimeline();
  }
  if (win_etape_bravo_mode) {
    title = "BRAVO!";
    subtitle = audio_playing ? "Validation en cours..." : kWinEtapeWaitingSubtitle;
  }
  if (win_etape_bravo_mode && timeline_keyframe_count_ > 1U) {
    timeline_keyframe_count_ = 1U;
    timeline_duration_ms_ = 0U;
    timeline_loop_ = true;
    timeline_effect_index_ = -1;
  }

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
  if (win_etape_bravo_mode) {
    show_title = true;
  }
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
  applySceneFraming(frame_dx, frame_dy, frame_scale_pct, frame_split_layout);
  applySubtitleScroll(subtitle_scroll_mode, subtitle_scroll_speed_ms, subtitle_scroll_pause_ms, subtitle_scroll_loop);
  for (lv_obj_t* particle : scene_particles_) {
    lv_obj_set_style_bg_color(particle, lv_color_hex(text_rgb), LV_PART_MAIN);
  }

  lv_obj_set_style_bg_opa(scene_core_, audio_playing ? LV_OPA_COVER : LV_OPA_80, LV_PART_MAIN);
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
  std::strncpy(last_scene_id_, scene_id, sizeof(last_scene_id_) - 1U);
  last_scene_id_[sizeof(last_scene_id_) - 1U] = '\0';
  updatePageLine();
  Serial.printf("[UI] scene=%s effect=%u speed=%u title=%u symbol=%u scenario=%s audio=%u timeline=%u transition=%u:%u\n",
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
    startIntroIfNeeded(scene_changed);
  }
}

void UiManager::handleButton(uint8_t key, bool long_press) {
  if (intro_active_) {
    if (intro_state_ == IntroState::PHASE_A_CRACKTRO || intro_state_ == IntroState::PHASE_B_TRANSITION) {
      requestIntroSkip();
    } else if (long_press || key == 5U) {
      intro_debug_overlay_enabled_ = !intro_debug_overlay_enabled_;
      Serial.printf("[UI][WIN_ETAPE] debug_overlay=%u\n", intro_debug_overlay_enabled_ ? 1U : 0U);
    }
  }
  UiAction action;
  action.source = long_press ? UiActionSource::kKeyLong : UiActionSource::kKeyShort;
  action.key = key;
  player_ui_.applyAction(action);

  pending_key_code_ = toLvKey(key, long_press);
  key_press_pending_ = true;
}

void UiManager::handleTouch(int16_t x, int16_t y, bool touched) {
  const bool was_touched = touch_pressed_;
  touch_x_ = x;
  touch_y_ = y;
  touch_pressed_ = touched;
  if (touched && intro_active_) {
    if (intro_state_ == IntroState::PHASE_A_CRACKTRO || intro_state_ == IntroState::PHASE_B_TRANSITION) {
      requestIntroSkip();
    } else if (!was_touched) {
      intro_debug_overlay_enabled_ = !intro_debug_overlay_enabled_;
      Serial.printf("[UI][WIN_ETAPE] debug_overlay=%u\n", intro_debug_overlay_enabled_ ? 1U : 0U);
    }
  }
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

void UiManager::updatePageLine() {
  if (page_label_ == nullptr || lv_obj_has_flag(page_label_, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  const PlayerUiSnapshot snapshot = player_ui_.snapshot();
  lv_label_set_text_fmt(page_label_,
                        "UI %s c=%u o=%u",
                        playerUiPageLabel(snapshot.page),
                        snapshot.cursor,
                        snapshot.offset);
}

void UiManager::stopSceneAnimations() {
  if (scene_root_ == nullptr) {
    return;
  }
  win_etape_showcase_phase_ = 0xFFU;
  if (page_label_ != nullptr) {
    lv_anim_del(page_label_, animWinEtapeShowcaseTickCb);
  }
  if (scene_core_ != nullptr) {
    lv_anim_del(scene_core_, animWinEtapeShowcaseTickCb);
  }
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  int16_t min_dim = (width < height) ? width : height;
  if (min_dim < 120) {
    min_dim = 120;
  }

  lv_anim_del(scene_root_, nullptr);
  lv_obj_set_style_opa(scene_root_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_x(scene_root_, 0);
  lv_obj_set_y(scene_root_, 0);
  lv_obj_set_style_translate_x(scene_root_, 0, LV_PART_MAIN);
  lv_obj_set_style_translate_y(scene_root_, 0, LV_PART_MAIN);

  if (scene_ring_outer_ != nullptr) {
    lv_anim_del(scene_ring_outer_, nullptr);
    int16_t outer = min_dim - 44;
    if (outer < 88) {
      outer = 88;
    }
    lv_obj_set_size(scene_ring_outer_, outer, outer);
    lv_obj_center(scene_ring_outer_);
    lv_obj_set_style_opa(scene_ring_outer_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_ring_outer_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_ring_outer_, 0, LV_PART_MAIN);
  }

  if (scene_ring_inner_ != nullptr) {
    lv_anim_del(scene_ring_inner_, nullptr);
    int16_t inner = min_dim - 104;
    if (inner < 64) {
      inner = 64;
    }
    lv_obj_set_size(scene_ring_inner_, inner, inner);
    lv_obj_center(scene_ring_inner_);
    lv_obj_set_style_opa(scene_ring_inner_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_ring_inner_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_ring_inner_, 0, LV_PART_MAIN);
  }

  if (scene_core_ != nullptr) {
    lv_anim_del(scene_core_, nullptr);
    int16_t core = min_dim - 170;
    if (core < 50) {
      core = 50;
    }
    lv_obj_set_size(scene_core_, core, core);
    lv_obj_center(scene_core_);
    lv_obj_set_style_opa(scene_core_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_core_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_core_, 0, LV_PART_MAIN);
  }

  if (scene_fx_bar_ != nullptr) {
    lv_anim_del(scene_fx_bar_, nullptr);
    int16_t bar_width = width - 120;
    if (bar_width < 80) {
      bar_width = 80;
    }
    lv_obj_set_size(scene_fx_bar_, bar_width, 8);
    lv_obj_align(scene_fx_bar_, LV_ALIGN_CENTER, 0, (height / 2) - 12);
    lv_obj_set_style_opa(scene_fx_bar_, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_fx_bar_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_fx_bar_, 0, LV_PART_MAIN);
  }

  if (scene_title_label_ != nullptr) {
    lv_anim_del(scene_title_label_, nullptr);
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBodyS(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_translate_x(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_title_label_, 0, LV_PART_MAIN);
  }
  if (scene_symbol_label_ != nullptr) {
    lv_anim_del(scene_symbol_label_, nullptr);
    lv_obj_set_style_opa(scene_symbol_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(scene_symbol_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_translate_x(scene_symbol_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_symbol_label_, 0, LV_PART_MAIN);
  }
  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontBodyS(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_width(scene_subtitle_label_, width - 32);
    lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_translate_x(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_subtitle_label_, 0, LV_PART_MAIN);
  }

  for (lv_obj_t* particle : scene_particles_) {
    if (particle == nullptr) {
      continue;
    }
    lv_anim_del(particle, nullptr);
    lv_obj_center(particle);
    lv_obj_set_style_opa(particle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(particle, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(particle, 0, LV_PART_MAIN);
  }

  for (lv_obj_t* bar : scene_cracktro_bars_) {
    if (bar == nullptr) {
      continue;
    }
    lv_anim_del(bar, nullptr);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  }

  for (lv_obj_t* star : scene_starfield_) {
    if (star == nullptr) {
      continue;
    }
    lv_anim_del(star, nullptr);
    lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(star, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(star, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(star, LV_OPA_COVER, LV_PART_MAIN);
  }

  if (scene_waveform_ != nullptr) {
    lv_obj_add_flag(scene_waveform_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scene_waveform_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_waveform_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_waveform_, 0, LV_PART_MAIN);
  }
  if (scene_waveform_outer_ != nullptr) {
    lv_obj_add_flag(scene_waveform_outer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scene_waveform_outer_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_waveform_outer_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_waveform_outer_, 0, LV_PART_MAIN);
  }
  if (scene_la_needle_ != nullptr) {
    lv_obj_add_flag(scene_la_needle_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scene_la_needle_, LV_OPA_90, LV_PART_MAIN);
  }
  if (scene_la_meter_bg_ != nullptr) {
    lv_obj_add_flag(scene_la_meter_bg_, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_la_meter_fill_ != nullptr) {
    lv_obj_add_flag(scene_la_meter_fill_, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_la_status_label_ != nullptr) {
    lv_obj_add_flag(scene_la_status_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_la_pitch_label_ != nullptr) {
    lv_obj_add_flag(scene_la_pitch_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_la_timer_label_ != nullptr) {
    lv_obj_add_flag(scene_la_timer_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_la_timeout_label_ != nullptr) {
    lv_obj_add_flag(scene_la_timeout_label_, LV_OBJ_FLAG_HIDDEN);
  }
  for (lv_obj_t* bar : scene_la_analyzer_bars_) {
    if (bar != nullptr) {
      lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (page_label_ != nullptr && !lv_obj_has_flag(page_label_, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_align(page_label_, LV_ALIGN_BOTTOM_LEFT, 10, -8);
  }
}

uint16_t UiManager::resolveAnimMs(uint16_t fallback_ms) const {
  if (effect_speed_ms_ < 80U) {
    return fallback_ms;
  }
  return effect_speed_ms_;
}

void UiManager::startWinEtapeCracktroPhase() {
  win_etape_showcase_phase_ = 0U;
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  applyThemeColors(0x130A22UL, 0xD78234UL, 0xFFE8BEUL);

  if (scene_symbol_label_ != nullptr) {
    lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
  }

  constexpr uint32_t kBarColors[kCracktroBarCount] = {
      0x1A0B2CUL, 0x311446UL, 0x4E204DUL, 0x6A2B4AUL, 0x82403CUL, 0x9A5A31UL, 0xB8772CUL};
  const int16_t bar_height = static_cast<int16_t>((height / static_cast<int16_t>(kCracktroBarCount)) + 2);
  for (uint8_t index = 0U; index < kCracktroBarCount; ++index) {
    lv_obj_t* bar = scene_cracktro_bars_[index];
    if (bar == nullptr) {
      continue;
    }
    lv_anim_del(bar, nullptr);
    lv_obj_set_size(bar, static_cast<lv_coord_t>(width + 30), bar_height);
    lv_obj_set_pos(bar, -15, static_cast<lv_coord_t>(index * (bar_height - 1)));
    lv_obj_set_style_bg_color(bar, lv_color_hex(kBarColors[index]), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, static_cast<lv_opa_t>(100 + index * 14U), LV_PART_MAIN);
    lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t bar_shift;
    lv_anim_init(&bar_shift);
    lv_anim_set_var(&bar_shift, bar);
    lv_anim_set_exec_cb(&bar_shift, animSetStyleTranslateX);
    lv_anim_set_values(&bar_shift,
                       static_cast<int32_t>(-18 + static_cast<int16_t>(index) * 3),
                       static_cast<int32_t>(18 - static_cast<int16_t>(index) * 2));
    lv_anim_set_time(&bar_shift, resolveAnimMs(static_cast<uint16_t>(260U + index * 90U)));
    lv_anim_set_playback_time(&bar_shift, resolveAnimMs(static_cast<uint16_t>(260U + index * 90U)));
    lv_anim_set_repeat_count(&bar_shift, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&bar_shift);
  }

  constexpr uint16_t kStarSpeedMs[3] = {2200U, 1450U, 980U};
  constexpr uint8_t kStarSize[3] = {2U, 3U, 4U};
  constexpr lv_opa_t kStarOpa[3] = {LV_OPA_40, LV_OPA_70, LV_OPA_COVER};
  const int16_t star_track = (height > 76) ? static_cast<int16_t>(height - 76) : 40;
  for (uint8_t index = 0U; index < kStarfieldCount; ++index) {
    lv_obj_t* star = scene_starfield_[index];
    if (star == nullptr) {
      continue;
    }
    const uint8_t layer = index % 3U;
    lv_anim_del(star, nullptr);
    lv_obj_set_size(star, kStarSize[layer], kStarSize[layer]);
    lv_obj_set_style_bg_opa(star, kStarOpa[layer], LV_PART_MAIN);
    lv_obj_set_style_bg_color(star, lv_color_hex((layer == 2U) ? 0xFFFFFFUL : 0xBFE5FFUL), LV_PART_MAIN);
    const int16_t start_x = static_cast<int16_t>((index * 53 + layer * 41) % (width + 28));
    const int16_t y = static_cast<int16_t>(12 + ((index * 37 + layer * 19) % star_track));
    lv_obj_set_pos(star, start_x, y);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t star_scroll;
    lv_anim_init(&star_scroll);
    lv_anim_set_var(&star_scroll, star);
    lv_anim_set_exec_cb(&star_scroll, animSetX);
    lv_anim_set_values(&star_scroll, start_x, -14);
    lv_anim_set_time(&star_scroll, resolveAnimMs(kStarSpeedMs[layer]));
    lv_anim_set_repeat_count(&star_scroll, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_delay(&star_scroll, static_cast<uint16_t>(index * 70U));
    lv_anim_start(&star_scroll);
  }

  if (scene_title_label_ != nullptr) {
    lv_anim_del(scene_title_label_, nullptr);
    lv_label_set_text(scene_title_label_, kWinEtapeCracktroTitle);
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBodyM(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 2, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_translate_y(scene_title_label_, -66, LV_PART_MAIN);
    lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t title_drop;
    lv_anim_init(&title_drop);
    lv_anim_set_var(&title_drop, scene_title_label_);
    lv_anim_set_exec_cb(&title_drop, animSetStyleTranslateY);
    lv_anim_set_values(&title_drop, -66, 0);
    lv_anim_set_time(&title_drop, resolveAnimMs(920U));
    lv_anim_set_delay(&title_drop, 120U);
    lv_anim_set_path_cb(&title_drop, lv_anim_path_overshoot);
    lv_anim_start(&title_drop);
  }

  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_label_set_text(scene_subtitle_label_, kWinEtapeCracktroScroll);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontBodyS(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_subtitle_label_, 1, LV_PART_MAIN);
    lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    applySubtitleScroll(SceneScrollMode::kMarquee, resolveAnimMs(3400U), 120U, true);
  }
}

void UiManager::startWinEtapeCrashPhase() {
  win_etape_showcase_phase_ = 1U;
  applyThemeColors(0x1D0B20UL, 0xFF8A4DUL, 0xFFF3DDUL);

  if (scene_root_ != nullptr) {
    lv_anim_del(scene_root_, nullptr);
    lv_anim_t root_flash;
    lv_anim_init(&root_flash);
    lv_anim_set_var(&root_flash, scene_root_);
    lv_anim_set_exec_cb(&root_flash, animSetOpa);
    lv_anim_set_values(&root_flash, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&root_flash, resolveAnimMs(110U));
    lv_anim_set_playback_time(&root_flash, resolveAnimMs(110U));
    lv_anim_set_repeat_count(&root_flash, 4U);
    lv_anim_start(&root_flash);

    lv_anim_t root_jitter_x;
    lv_anim_init(&root_jitter_x);
    lv_anim_set_var(&root_jitter_x, scene_root_);
    lv_anim_set_exec_cb(&root_jitter_x, animSetRandomTranslateX);
    lv_anim_set_values(&root_jitter_x, 0, 4095);
    lv_anim_set_time(&root_jitter_x, resolveAnimMs(74U));
    lv_anim_set_repeat_count(&root_jitter_x, 10U);
    lv_anim_start(&root_jitter_x);

    lv_anim_t root_jitter_y;
    lv_anim_init(&root_jitter_y);
    lv_anim_set_var(&root_jitter_y, scene_root_);
    lv_anim_set_exec_cb(&root_jitter_y, animSetRandomTranslateY);
    lv_anim_set_values(&root_jitter_y, 0, 4095);
    lv_anim_set_time(&root_jitter_y, resolveAnimMs(66U));
    lv_anim_set_repeat_count(&root_jitter_y, 10U);
    lv_anim_start(&root_jitter_y);
  }

  for (uint8_t index = 0U; index < 4U; ++index) {
    lv_obj_t* particle = scene_particles_[index];
    if (particle == nullptr) {
      continue;
    }
    lv_anim_del(particle, nullptr);
    lv_obj_set_size(particle, 8 + static_cast<int16_t>(index * 2U), 8 + static_cast<int16_t>(index * 2U));
    lv_obj_set_style_bg_color(particle, lv_color_hex((index % 2U) == 0U ? 0xFFD66EUL : 0xFF8D55UL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(particle, LV_OPA_80, LV_PART_MAIN);
    lv_obj_align(particle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t burst_opa;
    lv_anim_init(&burst_opa);
    lv_anim_set_var(&burst_opa, particle);
    lv_anim_set_exec_cb(&burst_opa, animSetOpa);
    lv_anim_set_values(&burst_opa, 20, LV_OPA_COVER);
    lv_anim_set_time(&burst_opa, resolveAnimMs(200U));
    lv_anim_set_playback_time(&burst_opa, resolveAnimMs(260U));
    lv_anim_set_repeat_count(&burst_opa, 0U);
    lv_anim_set_delay(&burst_opa, static_cast<uint16_t>(index * 36U));
    lv_anim_start(&burst_opa);

    lv_anim_t burst_x;
    lv_anim_init(&burst_x);
    lv_anim_set_var(&burst_x, particle);
    lv_anim_set_exec_cb(&burst_x, animSetFireworkTranslateX);
    lv_anim_set_values(&burst_x, 0, 4095);
    lv_anim_set_time(&burst_x, resolveAnimMs(300U));
    lv_anim_set_playback_time(&burst_x, resolveAnimMs(240U));
    lv_anim_set_repeat_count(&burst_x, 0U);
    lv_anim_set_delay(&burst_x, static_cast<uint16_t>(index * 28U));
    lv_anim_start(&burst_x);

    lv_anim_t burst_y;
    lv_anim_init(&burst_y);
    lv_anim_set_var(&burst_y, particle);
    lv_anim_set_exec_cb(&burst_y, animSetFireworkTranslateY);
    lv_anim_set_values(&burst_y, 0, 4095);
    lv_anim_set_time(&burst_y, resolveAnimMs(320U));
    lv_anim_set_playback_time(&burst_y, resolveAnimMs(260U));
    lv_anim_set_repeat_count(&burst_y, 0U);
    lv_anim_set_delay(&burst_y, static_cast<uint16_t>(index * 24U));
    lv_anim_start(&burst_y);
  }
}

void UiManager::startWinEtapeCleanPhase() {
  win_etape_showcase_phase_ = 2U;
  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  applyThemeColors(0x091830UL, 0x5E7FBBUL, 0xF2F6FFUL);

  if (scene_root_ != nullptr) {
    lv_anim_del(scene_root_, nullptr);
    lv_obj_set_style_opa(scene_root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_translate_x(scene_root_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_root_, 0, LV_PART_MAIN);
  }

  constexpr uint32_t kCleanBars[kCracktroBarCount] = {
      0x0A162EUL, 0x10203BUL, 0x182C49UL, 0x20385AUL, 0x294369UL, 0x304C73UL, 0x36547BUL};
  const int16_t bar_height = static_cast<int16_t>((height / static_cast<int16_t>(kCracktroBarCount)) + 2);
  for (uint8_t index = 0U; index < kCracktroBarCount; ++index) {
    lv_obj_t* bar = scene_cracktro_bars_[index];
    if (bar == nullptr) {
      continue;
    }
    lv_anim_del(bar, nullptr);
    lv_obj_set_size(bar, width, bar_height);
    lv_obj_set_pos(bar, 0, static_cast<lv_coord_t>(index * (bar_height - 1)));
    lv_obj_set_style_bg_color(bar, lv_color_hex(kCleanBars[index]), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, static_cast<lv_opa_t>(48 + index * 10U), LV_PART_MAIN);
    lv_obj_set_style_translate_x(bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t index = 0U; index < kStarfieldCount; ++index) {
    lv_obj_t* star = scene_starfield_[index];
    if (star == nullptr) {
      continue;
    }
    lv_anim_del(star, nullptr);
    if (index >= 4U) {
      lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_set_size(star, 2, 2);
    lv_obj_set_style_bg_opa(star, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_bg_color(star, lv_color_hex(0xA7C8F8UL), LV_PART_MAIN);
    const int16_t start_x = static_cast<int16_t>((index * 97) % (width + 24));
    const int16_t y = static_cast<int16_t>(18 + index * 16);
    lv_obj_set_pos(star, start_x, y);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t drift;
    lv_anim_init(&drift);
    lv_anim_set_var(&drift, star);
    lv_anim_set_exec_cb(&drift, animSetX);
    lv_anim_set_values(&drift, start_x, -10);
    lv_anim_set_time(&drift, resolveAnimMs(static_cast<uint16_t>(4200U + index * 350U)));
    lv_anim_set_repeat_count(&drift, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&drift);
  }

  for (lv_obj_t* particle : scene_particles_) {
    if (particle == nullptr) {
      continue;
    }
    lv_anim_del(particle, nullptr);
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
  }
  if (scene_symbol_label_ != nullptr) {
    lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
  }

  if (scene_title_label_ != nullptr) {
    lv_anim_del(scene_title_label_, nullptr);
    lv_label_set_text(scene_title_label_, "");
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBodyM(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 1, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_translate_x(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t title_reveal;
    lv_anim_init(&title_reveal);
    lv_anim_set_var(&title_reveal, scene_title_label_);
    lv_anim_set_exec_cb(&title_reveal, animSetWinTitleReveal);
    lv_anim_set_values(&title_reveal, 0, static_cast<int32_t>(std::strlen(kWinEtapeDemoTitle)));
    lv_anim_set_time(&title_reveal, resolveAnimMs(1700U));
    lv_anim_set_delay(&title_reveal, 80U);
    lv_anim_start(&title_reveal);
  }

  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_label_set_text(scene_subtitle_label_, kWinEtapeDemoScroll);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontBodyS(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    applySubtitleScroll(SceneScrollMode::kMarquee, resolveAnimMs(7600U), 500U, true);

    lv_anim_t subtitle_sine;
    lv_anim_init(&subtitle_sine);
    lv_anim_set_var(&subtitle_sine, scene_subtitle_label_);
    lv_anim_set_exec_cb(&subtitle_sine, animSetSineTranslateY);
    lv_anim_set_values(&subtitle_sine, 0, 4095);
    lv_anim_set_time(&subtitle_sine, resolveAnimMs(2600U));
    lv_anim_set_repeat_count(&subtitle_sine, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&subtitle_sine);
  }
}

void UiManager::onWinEtapeShowcaseTick(uint16_t elapsed_ms) {
  if (!win_etape_fireworks_mode_) {
    return;
  }
  constexpr uint16_t kCracktroEndMs = 4700U;
  constexpr uint16_t kCrashEndMs = 5600U;

  if (elapsed_ms < 120U) {
    if (win_etape_showcase_phase_ != 0U) {
      startWinEtapeCracktroPhase();
    }
    return;
  }
  if (elapsed_ms < kCracktroEndMs) {
    return;
  }
  if (elapsed_ms < kCrashEndMs) {
    if (win_etape_showcase_phase_ != 1U) {
      startWinEtapeCrashPhase();
    }
    return;
  }
  if (win_etape_showcase_phase_ != 2U) {
    startWinEtapeCleanPhase();
  }
}

void UiManager::applySceneEffect(SceneEffect effect) {
  if (scene_root_ == nullptr || scene_core_ == nullptr || scene_fx_bar_ == nullptr) {
    return;
  }
  if (effect == SceneEffect::kNone) {
    return;
  }

  const int16_t width = activeDisplayWidth();
  const int16_t height = activeDisplayHeight();
  int16_t min_dim = (width < height) ? width : height;
  if (min_dim < 120) {
    min_dim = 120;
  }

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_playback_time(&anim, 0);

  if (effect == SceneEffect::kPulse) {
    const uint16_t pulse_ms = resolveAnimMs(640);
    int16_t core_small = min_dim / 4;
    if (core_small < 46) {
      core_small = 46;
    }
    int16_t core_large = core_small + (min_dim / 7);
    if (core_large < (core_small + 18)) {
      core_large = core_small + 18;
    }
    lv_anim_set_var(&anim, scene_core_);
    lv_anim_set_exec_cb(&anim, animSetSize);
    lv_anim_set_values(&anim, core_small, core_large);
    lv_anim_set_time(&anim, pulse_ms);
    lv_anim_set_playback_time(&anim, pulse_ms);
    lv_anim_start(&anim);

    if (scene_ring_inner_ != nullptr) {
      lv_anim_t ring_anim;
      lv_anim_init(&ring_anim);
      lv_anim_set_var(&ring_anim, scene_ring_inner_);
      lv_anim_set_exec_cb(&ring_anim, animSetOpa);
      lv_anim_set_values(&ring_anim, 90, LV_OPA_COVER);
      lv_anim_set_time(&ring_anim, pulse_ms);
      lv_anim_set_playback_time(&ring_anim, pulse_ms);
      lv_anim_set_repeat_count(&ring_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_anim);
    }
    if (scene_symbol_label_ != nullptr) {
      lv_anim_t symbol_anim;
      lv_anim_init(&symbol_anim);
      lv_anim_set_var(&symbol_anim, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_anim, animSetOpa);
      lv_anim_set_values(&symbol_anim, 110, LV_OPA_COVER);
      lv_anim_set_time(&symbol_anim, pulse_ms);
      lv_anim_set_playback_time(&symbol_anim, pulse_ms);
      lv_anim_set_repeat_count(&symbol_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_anim);
    }
    return;
  }

  if (effect == SceneEffect::kScan) {
    const uint16_t scan_ms = resolveAnimMs(920);
    int16_t bar_width = width - 84;
    if (bar_width < 90) {
      bar_width = 90;
    }
    lv_obj_set_size(scene_fx_bar_, bar_width, 10);
    lv_obj_align(scene_fx_bar_, LV_ALIGN_TOP_MID, 0, 20);

    lv_anim_set_var(&anim, scene_fx_bar_);
    lv_anim_set_exec_cb(&anim, animSetY);
    lv_anim_set_values(&anim, 20, height - 28);
    lv_anim_set_time(&anim, scan_ms);
    lv_anim_set_playback_time(&anim, scan_ms);
    lv_anim_start(&anim);
    if (scene_symbol_label_ != nullptr) {
      lv_obj_align(scene_symbol_label_, LV_ALIGN_CENTER, 0, -8);
      lv_anim_t symbol_scan;
      lv_anim_init(&symbol_scan);
      lv_anim_set_var(&symbol_scan, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_scan, animSetY);
      lv_anim_set_values(&symbol_scan, (height / 2) - 24, (height / 2) + 12);
      lv_anim_set_time(&symbol_scan, scan_ms);
      lv_anim_set_playback_time(&symbol_scan, scan_ms);
      lv_anim_set_repeat_count(&symbol_scan, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_scan);
    }
    return;
  }

  if (effect == SceneEffect::kRadar) {
    const uint16_t radar_ms = resolveAnimMs(780);
    if (scene_ring_outer_ != nullptr) {
      int16_t ring_small = min_dim - 96;
      if (ring_small < 78) {
        ring_small = 78;
      }
      int16_t ring_large = min_dim - 14;
      if (ring_large < ring_small + 18) {
        ring_large = ring_small + 18;
      }
      lv_anim_t ring_anim;
      lv_anim_init(&ring_anim);
      lv_anim_set_var(&ring_anim, scene_ring_outer_);
      lv_anim_set_exec_cb(&ring_anim, animSetSize);
      lv_anim_set_values(&ring_anim, ring_small, ring_large);
      lv_anim_set_time(&ring_anim, radar_ms);
      lv_anim_set_playback_time(&ring_anim, radar_ms);
      lv_anim_set_repeat_count(&ring_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_anim);
    }
    if (scene_ring_inner_ != nullptr) {
      lv_anim_t inner_opa;
      lv_anim_init(&inner_opa);
      lv_anim_set_var(&inner_opa, scene_ring_inner_);
      lv_anim_set_exec_cb(&inner_opa, animSetOpa);
      lv_anim_set_values(&inner_opa, 70, LV_OPA_COVER);
      lv_anim_set_time(&inner_opa, radar_ms);
      lv_anim_set_playback_time(&inner_opa, radar_ms);
      lv_anim_set_repeat_count(&inner_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&inner_opa);
    }
    if (scene_fx_bar_ != nullptr) {
      lv_obj_set_size(scene_fx_bar_, width - 80, 6);
      lv_obj_align(scene_fx_bar_, LV_ALIGN_CENTER, 0, 0);
      lv_anim_t sweep_anim;
      lv_anim_init(&sweep_anim);
      lv_anim_set_var(&sweep_anim, scene_fx_bar_);
      lv_anim_set_exec_cb(&sweep_anim, animSetY);
      lv_anim_set_values(&sweep_anim, -6, (height / 2) - 10);
      lv_anim_set_time(&sweep_anim, radar_ms);
      lv_anim_set_playback_time(&sweep_anim, radar_ms);
      lv_anim_set_repeat_count(&sweep_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&sweep_anim);
    }
    return;
  }

  if (effect == SceneEffect::kWave) {
    const uint16_t wave_ms = resolveAnimMs(520);
    lv_obj_set_size(scene_fx_bar_, width - 120, 8);
    lv_obj_align(scene_fx_bar_, LV_ALIGN_CENTER, 0, (height / 2) - 14);

    lv_anim_t wave_width;
    lv_anim_init(&wave_width);
    lv_anim_set_var(&wave_width, scene_fx_bar_);
    lv_anim_set_exec_cb(&wave_width, animSetWidth);
    lv_anim_set_values(&wave_width, 44, width - 44);
    lv_anim_set_time(&wave_width, wave_ms);
    lv_anim_set_playback_time(&wave_width, wave_ms);
    lv_anim_set_repeat_count(&wave_width, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&wave_width);

    lv_anim_t wave_y;
    lv_anim_init(&wave_y);
    lv_anim_set_var(&wave_y, scene_fx_bar_);
    lv_anim_set_exec_cb(&wave_y, animSetY);
    lv_anim_set_values(&wave_y, (height / 2) - 30, (height / 2) + 4);
    lv_anim_set_time(&wave_y, wave_ms);
    lv_anim_set_playback_time(&wave_y, wave_ms);
    lv_anim_set_repeat_count(&wave_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&wave_y);

    if (scene_core_ != nullptr) {
      lv_anim_t core_opa;
      lv_anim_init(&core_opa);
      lv_anim_set_var(&core_opa, scene_core_);
      lv_anim_set_exec_cb(&core_opa, animSetOpa);
      lv_anim_set_values(&core_opa, 85, LV_OPA_COVER);
      lv_anim_set_time(&core_opa, wave_ms);
      lv_anim_set_playback_time(&core_opa, wave_ms);
      lv_anim_set_repeat_count(&core_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&core_opa);
    }
    return;
  }

  if (effect == SceneEffect::kGlitch) {
    const uint16_t glitch_ms = resolveAnimMs(88);

    int32_t root_low_opa = static_cast<int32_t>(LV_OPA_COVER) - static_cast<int32_t>(demo_strobe_level_) * 4;
    if (root_low_opa < 8) {
      root_low_opa = 8;
    }
    if (root_low_opa > LV_OPA_COVER) {
      root_low_opa = LV_OPA_COVER;
    }
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetOpa);
    lv_anim_set_values(&anim, root_low_opa, LV_OPA_COVER);
    lv_anim_set_time(&anim, glitch_ms);
    lv_anim_set_playback_time(&anim, glitch_ms);
    lv_anim_start(&anim);

    lv_anim_t root_noise;
    lv_anim_init(&root_noise);
    lv_anim_set_var(&root_noise, scene_root_);
    lv_anim_set_exec_cb(&root_noise, animSetRandomOpa);
    lv_anim_set_values(&root_noise, 0, 4095);
    lv_anim_set_time(&root_noise, resolveAnimMs(56));
    lv_anim_set_repeat_count(&root_noise, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&root_noise);

    if (scene_core_ != nullptr) {
      lv_anim_t core_x;
      lv_anim_init(&core_x);
      lv_anim_set_var(&core_x, scene_core_);
      lv_anim_set_exec_cb(&core_x, animSetRandomTranslateX);
      lv_anim_set_values(&core_x, 0, 4095);
      lv_anim_set_time(&core_x, resolveAnimMs(62));
      lv_anim_set_repeat_count(&core_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&core_x);

      lv_anim_t core_y;
      lv_anim_init(&core_y);
      lv_anim_set_var(&core_y, scene_core_);
      lv_anim_set_exec_cb(&core_y, animSetRandomTranslateY);
      lv_anim_set_values(&core_y, 0, 4095);
      lv_anim_set_time(&core_y, resolveAnimMs(54));
      lv_anim_set_repeat_count(&core_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&core_y);

      lv_anim_t core_opa;
      lv_anim_init(&core_opa);
      lv_anim_set_var(&core_opa, scene_core_);
      lv_anim_set_exec_cb(&core_opa, animSetRandomOpa);
      lv_anim_set_values(&core_opa, 0, 4095);
      lv_anim_set_time(&core_opa, resolveAnimMs(60));
      lv_anim_set_repeat_count(&core_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&core_opa);
    }

    if (scene_ring_outer_ != nullptr) {
      lv_anim_t ring_outer_x;
      lv_anim_init(&ring_outer_x);
      lv_anim_set_var(&ring_outer_x, scene_ring_outer_);
      lv_anim_set_exec_cb(&ring_outer_x, animSetRandomTranslateX);
      lv_anim_set_values(&ring_outer_x, 0, 4095);
      lv_anim_set_time(&ring_outer_x, resolveAnimMs(82));
      lv_anim_set_repeat_count(&ring_outer_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_outer_x);

      lv_anim_t ring_outer_y;
      lv_anim_init(&ring_outer_y);
      lv_anim_set_var(&ring_outer_y, scene_ring_outer_);
      lv_anim_set_exec_cb(&ring_outer_y, animSetRandomTranslateY);
      lv_anim_set_values(&ring_outer_y, 0, 4095);
      lv_anim_set_time(&ring_outer_y, resolveAnimMs(74));
      lv_anim_set_repeat_count(&ring_outer_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_outer_y);
    }

    if (scene_ring_inner_ != nullptr) {
      lv_anim_t ring_inner_x;
      lv_anim_init(&ring_inner_x);
      lv_anim_set_var(&ring_inner_x, scene_ring_inner_);
      lv_anim_set_exec_cb(&ring_inner_x, animSetRandomTranslateX);
      lv_anim_set_values(&ring_inner_x, 0, 4095);
      lv_anim_set_time(&ring_inner_x, resolveAnimMs(70));
      lv_anim_set_repeat_count(&ring_inner_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_inner_x);

      lv_anim_t ring_inner_y;
      lv_anim_init(&ring_inner_y);
      lv_anim_set_var(&ring_inner_y, scene_ring_inner_);
      lv_anim_set_exec_cb(&ring_inner_y, animSetRandomTranslateY);
      lv_anim_set_values(&ring_inner_y, 0, 4095);
      lv_anim_set_time(&ring_inner_y, resolveAnimMs(66));
      lv_anim_set_repeat_count(&ring_inner_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_inner_y);
    }

    if (scene_fx_bar_ != nullptr) {
      lv_obj_set_size(scene_fx_bar_, width - 56, 14);
      lv_obj_align(scene_fx_bar_, LV_ALIGN_CENTER, 0, -22);

      lv_anim_t bar_x;
      lv_anim_init(&bar_x);
      lv_anim_set_var(&bar_x, scene_fx_bar_);
      lv_anim_set_exec_cb(&bar_x, animSetRandomTranslateX);
      lv_anim_set_values(&bar_x, 0, 4095);
      lv_anim_set_time(&bar_x, resolveAnimMs(48));
      lv_anim_set_repeat_count(&bar_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&bar_x);

      lv_anim_t bar_y;
      lv_anim_init(&bar_y);
      lv_anim_set_var(&bar_y, scene_fx_bar_);
      lv_anim_set_exec_cb(&bar_y, animSetRandomTranslateY);
      lv_anim_set_values(&bar_y, 0, 4095);
      lv_anim_set_time(&bar_y, resolveAnimMs(54));
      lv_anim_set_repeat_count(&bar_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&bar_y);

      lv_anim_t bar_opa;
      lv_anim_init(&bar_opa);
      lv_anim_set_var(&bar_opa, scene_fx_bar_);
      lv_anim_set_exec_cb(&bar_opa, animSetRandomOpa);
      lv_anim_set_values(&bar_opa, 0, 4095);
      lv_anim_set_time(&bar_opa, resolveAnimMs(46));
      lv_anim_set_repeat_count(&bar_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&bar_opa);
    }

    const int16_t dx = min_dim / 5;
    const int16_t dy = min_dim / 7;
    for (uint8_t index = 0U; index < 4U; ++index) {
      lv_obj_t* particle = scene_particles_[index];
      if (particle == nullptr) {
        continue;
      }
      const int16_t x_offset = ((index % 2U) == 0U) ? -dx : dx;
      const int16_t y_offset = (index < 2U) ? -dy : dy;
      lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(particle, 12 + static_cast<int16_t>((index % 2U) * 6U), 12 + static_cast<int16_t>((index % 2U) * 6U));
      lv_obj_align(particle, LV_ALIGN_CENTER, x_offset, y_offset);

      lv_anim_t core_x;
      lv_anim_init(&core_x);
      lv_anim_set_var(&core_x, particle);
      lv_anim_set_exec_cb(&core_x, animSetRandomTranslateX);
      lv_anim_set_values(&core_x, 0, 4095);
      lv_anim_set_time(&core_x, resolveAnimMs(static_cast<uint16_t>(48U + index * 11U)));
      lv_anim_set_repeat_count(&core_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_delay(&core_x, static_cast<uint16_t>(index * 17U));
      lv_anim_start(&core_x);

      lv_anim_t core_y;
      lv_anim_init(&core_y);
      lv_anim_set_var(&core_y, particle);
      lv_anim_set_exec_cb(&core_y, animSetRandomTranslateY);
      lv_anim_set_values(&core_y, 0, 4095);
      lv_anim_set_time(&core_y, resolveAnimMs(static_cast<uint16_t>(54U + index * 13U)));
      lv_anim_set_repeat_count(&core_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_delay(&core_y, static_cast<uint16_t>(index * 19U));
      lv_anim_start(&core_y);

      lv_anim_t particle_opa;
      lv_anim_init(&particle_opa);
      lv_anim_set_var(&particle_opa, particle);
      lv_anim_set_exec_cb(&particle_opa, animSetRandomOpa);
      lv_anim_set_values(&particle_opa, 0, 4095);
      lv_anim_set_time(&particle_opa, resolveAnimMs(static_cast<uint16_t>(44U + index * 10U)));
      lv_anim_set_repeat_count(&particle_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_delay(&particle_opa, static_cast<uint16_t>(index * 15U));
      lv_anim_start(&particle_opa);
    }

    if (scene_symbol_label_ != nullptr) {
      lv_anim_t symbol_glitch;
      lv_anim_init(&symbol_glitch);
      lv_anim_set_var(&symbol_glitch, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_glitch, animSetRandomOpa);
      lv_anim_set_values(&symbol_glitch, 0, 4095);
      lv_anim_set_time(&symbol_glitch, resolveAnimMs(50));
      lv_anim_set_repeat_count(&symbol_glitch, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_glitch);

      lv_anim_t symbol_x;
      lv_anim_init(&symbol_x);
      lv_anim_set_var(&symbol_x, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_x, animSetRandomTranslateX);
      lv_anim_set_values(&symbol_x, 0, 4095);
      lv_anim_set_time(&symbol_x, resolveAnimMs(58));
      lv_anim_set_repeat_count(&symbol_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_x);

      lv_anim_t symbol_y;
      lv_anim_init(&symbol_y);
      lv_anim_set_var(&symbol_y, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_y, animSetRandomTranslateY);
      lv_anim_set_values(&symbol_y, 0, 4095);
      lv_anim_set_time(&symbol_y, resolveAnimMs(64));
      lv_anim_set_repeat_count(&symbol_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_y);
    }

    if (scene_subtitle_label_ != nullptr) {
      lv_anim_t subtitle_jitter_x;
      lv_anim_init(&subtitle_jitter_x);
      lv_anim_set_var(&subtitle_jitter_x, scene_subtitle_label_);
      lv_anim_set_exec_cb(&subtitle_jitter_x, animSetRandomTranslateX);
      lv_anim_set_values(&subtitle_jitter_x, 0, 4095);
      lv_anim_set_time(&subtitle_jitter_x, resolveAnimMs(66));
      lv_anim_set_repeat_count(&subtitle_jitter_x, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&subtitle_jitter_x);

      lv_anim_t subtitle_opa;
      lv_anim_init(&subtitle_opa);
      lv_anim_set_var(&subtitle_opa, scene_subtitle_label_);
      lv_anim_set_exec_cb(&subtitle_opa, animSetRandomOpa);
      lv_anim_set_values(&subtitle_opa, 0, 4095);
      lv_anim_set_time(&subtitle_opa, resolveAnimMs(58));
      lv_anim_set_repeat_count(&subtitle_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&subtitle_opa);
    }
    return;
  }

  if (effect == SceneEffect::kBlink) {
    const uint16_t blink_ms = resolveAnimMs(170);
    int32_t low_opa = static_cast<int32_t>(LV_OPA_COVER) - static_cast<int32_t>(demo_strobe_level_) * 3;
    if (low_opa < 24) {
      low_opa = 24;
    }
    if (low_opa > LV_OPA_COVER) {
      low_opa = LV_OPA_COVER;
    }
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetOpa);
    lv_anim_set_values(&anim, low_opa, LV_OPA_COVER);
    lv_anim_set_time(&anim, blink_ms);
    lv_anim_set_playback_time(&anim, blink_ms);
    lv_anim_start(&anim);
    if (scene_symbol_label_ != nullptr) {
      lv_anim_t symbol_blink;
      lv_anim_init(&symbol_blink);
      lv_anim_set_var(&symbol_blink, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_blink, animSetOpa);
      lv_anim_set_values(&symbol_blink, low_opa, LV_OPA_COVER);
      lv_anim_set_time(&symbol_blink, blink_ms);
      lv_anim_set_playback_time(&symbol_blink, blink_ms);
      lv_anim_set_repeat_count(&symbol_blink, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_blink);
    }
    return;
  }

  if (effect == SceneEffect::kCelebrate) {
    const bool fireworks_mode = win_etape_fireworks_mode_;
    if (fireworks_mode) {
      lv_obj_t* controller = page_label_;
      if (controller == nullptr) {
        controller = scene_core_;
      }
      if (controller != nullptr) {
        lv_anim_del(controller, animWinEtapeShowcaseTickCb);
      }
      win_etape_showcase_phase_ = 0xFFU;
      onWinEtapeShowcaseTick(0U);

      if (controller != nullptr) {
        lv_anim_t showcase_cycle;
        lv_anim_init(&showcase_cycle);
        lv_anim_set_var(&showcase_cycle, controller);
        lv_anim_set_exec_cb(&showcase_cycle, animWinEtapeShowcaseTickCb);
        lv_anim_set_values(&showcase_cycle, 0, 12000);
        lv_anim_set_time(&showcase_cycle, 12000U);
        lv_anim_set_repeat_count(&showcase_cycle, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&showcase_cycle);
      }
      return;
    }
    const bool broken_mode = !fireworks_mode && (demo_strobe_level_ >= 85U);
    const uint16_t celebrate_ms = resolveAnimMs(fireworks_mode ? 640U : 560U);
    const uint16_t celebrate_alt_ms = resolveAnimMs(fireworks_mode ? 560U : 500U);
    const uint16_t firework_pause_ms = resolveAnimMs(190U);
    if (scene_ring_outer_ != nullptr) {
      int16_t ring_small = min_dim - 88;
      if (ring_small < 84) {
        ring_small = 84;
      }
      int16_t ring_large = min_dim - 22;
      if (ring_large < (ring_small + 22)) {
        ring_large = ring_small + 22;
      }
      lv_anim_t ring_anim;
      lv_anim_init(&ring_anim);
      lv_anim_set_var(&ring_anim, scene_ring_outer_);
      lv_anim_set_exec_cb(&ring_anim, animSetSize);
      lv_anim_set_values(&ring_anim, ring_small, ring_large);
      lv_anim_set_time(&ring_anim, celebrate_ms);
      lv_anim_set_playback_time(&ring_anim, celebrate_ms);
      lv_anim_set_repeat_count(&ring_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_anim);
    }

    lv_obj_set_size(scene_fx_bar_, width - 92, (fireworks_mode || broken_mode) ? 10 : 8);
    lv_obj_align(scene_fx_bar_, LV_ALIGN_CENTER, 0, (fireworks_mode || broken_mode) ? -18 : -10);

    lv_anim_t width_anim;
    lv_anim_init(&width_anim);
    lv_anim_set_var(&width_anim, scene_fx_bar_);
    lv_anim_set_exec_cb(&width_anim, animSetWidth);
    lv_anim_set_values(&width_anim, 36, width - 36);
    lv_anim_set_time(&width_anim, celebrate_alt_ms);
    lv_anim_set_playback_time(&width_anim, celebrate_alt_ms);
    lv_anim_set_repeat_count(&width_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&width_anim);

    if (fireworks_mode || broken_mode) {
      lv_anim_t bar_y;
      lv_anim_init(&bar_y);
      lv_anim_set_var(&bar_y, scene_fx_bar_);
      if (fireworks_mode) {
        lv_anim_set_exec_cb(&bar_y, animSetStyleTranslateY);
        lv_anim_set_values(&bar_y, -7, 7);
        lv_anim_set_time(&bar_y, resolveAnimMs(420U));
        lv_anim_set_playback_time(&bar_y, resolveAnimMs(420U));
        lv_anim_set_repeat_delay(&bar_y, firework_pause_ms);
      } else {
        lv_anim_set_exec_cb(&bar_y, animSetRandomTranslateY);
        lv_anim_set_values(&bar_y, 0, 4095);
        lv_anim_set_time(&bar_y, resolveAnimMs(140U));
      }
      lv_anim_set_repeat_count(&bar_y, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&bar_y);
    }

    const int16_t dx = min_dim / 5;
    const int16_t dy = min_dim / 7;
    constexpr uint32_t kFireworkColors[4] = {0xFFD56EUL, 0xFFE59BUL, 0xFF9B5EUL, 0xFFF3A6UL};
    const uint8_t max_particles = (demo_particle_count_ > 4U) ? 4U : demo_particle_count_;
    for (uint8_t index = 0; index < 4U; ++index) {
      lv_obj_t* particle = scene_particles_[index];
      if (particle == nullptr) {
        continue;
      }
      if (index >= max_particles) {
        lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      const int16_t x_offset = ((index % 2U) == 0U) ? -dx : dx;
      const int16_t y_offset = (index < 2U) ? -dy : dy;
      lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(particle, fireworks_mode ? 9U : (broken_mode ? 12 : 10), fireworks_mode ? 9U : (broken_mode ? 12 : 10));
      lv_obj_align(particle, LV_ALIGN_CENTER, x_offset, y_offset);
      if (fireworks_mode) {
        lv_obj_set_style_bg_color(particle, lv_color_hex(kFireworkColors[index % 4U]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(particle, LV_OPA_80, LV_PART_MAIN);
        lv_obj_align(particle, LV_ALIGN_CENTER, 0, 0);
      }

      lv_anim_t particle_opa;
      lv_anim_init(&particle_opa);
      lv_anim_set_var(&particle_opa, particle);
      if (fireworks_mode) {
        const uint16_t burst_ms = resolveAnimMs(static_cast<uint16_t>(260U + index * 34U));
        lv_anim_set_exec_cb(&particle_opa, animSetOpa);
        lv_anim_set_values(&particle_opa, 24, LV_OPA_COVER);
        lv_anim_set_time(&particle_opa, burst_ms);
        lv_anim_set_playback_time(&particle_opa, burst_ms);
        lv_anim_set_repeat_delay(&particle_opa, firework_pause_ms);
      } else if (broken_mode) {
        lv_anim_set_exec_cb(&particle_opa, animSetRandomOpa);
        lv_anim_set_values(&particle_opa, 0, 4095);
        lv_anim_set_time(&particle_opa, resolveAnimMs(96U));
      } else {
        lv_anim_set_exec_cb(&particle_opa, animSetOpa);
        lv_anim_set_values(&particle_opa, 80, LV_OPA_COVER);
        lv_anim_set_time(&particle_opa, resolveAnimMs(260U));
        lv_anim_set_playback_time(&particle_opa, resolveAnimMs(260U));
      }
      lv_anim_set_repeat_count(&particle_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_delay(&particle_opa, fireworks_mode ? static_cast<uint16_t>(60U + (index * 90U))
                                                      : static_cast<uint16_t>(80U + (index * 60U)));
      lv_anim_start(&particle_opa);

      if (fireworks_mode || broken_mode) {
        lv_anim_t particle_x;
        lv_anim_init(&particle_x);
        lv_anim_set_var(&particle_x, particle);
        lv_anim_set_exec_cb(&particle_x,
                            fireworks_mode ? animSetFireworkTranslateX : animSetRandomTranslateX);
        lv_anim_set_values(&particle_x, 0, 4095);
        lv_anim_set_time(&particle_x,
                         fireworks_mode ? resolveAnimMs(static_cast<uint16_t>(300U + (index * 28U)))
                                        : resolveAnimMs(static_cast<uint16_t>(200U + (index * 36U))));
        lv_anim_set_repeat_count(&particle_x, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&particle_x, fireworks_mode ? static_cast<uint16_t>(120U + (index * 70U))
                                                      : static_cast<uint16_t>(180U + (index * 26U)));
        if (fireworks_mode) {
          lv_anim_set_playback_time(&particle_x, resolveAnimMs(static_cast<uint16_t>(300U + (index * 28U))));
          lv_anim_set_repeat_delay(&particle_x, firework_pause_ms);
        }
        lv_anim_start(&particle_x);

        lv_anim_t particle_y;
        lv_anim_init(&particle_y);
        lv_anim_set_var(&particle_y, particle);
        lv_anim_set_exec_cb(&particle_y,
                            fireworks_mode ? animSetFireworkTranslateY : animSetRandomTranslateY);
        lv_anim_set_values(&particle_y, 0, 4095);
        lv_anim_set_time(&particle_y,
                         fireworks_mode ? resolveAnimMs(static_cast<uint16_t>(316U + (index * 30U)))
                                        : resolveAnimMs(static_cast<uint16_t>(210U + (index * 32U))));
        lv_anim_set_repeat_count(&particle_y, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&particle_y, fireworks_mode ? static_cast<uint16_t>(100U + (index * 74U))
                                                      : static_cast<uint16_t>(170U + (index * 22U)));
        if (fireworks_mode) {
          lv_anim_set_playback_time(&particle_y, resolveAnimMs(static_cast<uint16_t>(316U + (index * 30U))));
          lv_anim_set_repeat_delay(&particle_y, firework_pause_ms);
        }
        lv_anim_start(&particle_y);
      }

      if (fireworks_mode) {
        lv_anim_t particle_size;
        lv_anim_init(&particle_size);
        lv_anim_set_var(&particle_size, particle);
        lv_anim_set_exec_cb(&particle_size, animSetParticleSize);
        lv_anim_set_values(&particle_size, 4 + static_cast<int32_t>(index), 12 + static_cast<int32_t>(index * 2U));
        lv_anim_set_time(&particle_size, resolveAnimMs(static_cast<uint16_t>(260U + (index * 24U))));
        lv_anim_set_playback_time(&particle_size, resolveAnimMs(static_cast<uint16_t>(260U + (index * 24U))));
        lv_anim_set_repeat_count(&particle_size, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&particle_size, static_cast<uint16_t>(90U + (index * 72U)));
        lv_anim_set_repeat_delay(&particle_size, firework_pause_ms);
        lv_anim_start(&particle_size);
      }
    }

    if (fireworks_mode || broken_mode) {
      lv_anim_t root_flicker;
      lv_anim_init(&root_flicker);
      lv_anim_set_var(&root_flicker, scene_root_);
      if (fireworks_mode) {
        lv_anim_set_exec_cb(&root_flicker, animSetOpa);
        lv_anim_set_values(&root_flicker, LV_OPA_70, LV_OPA_COVER);
        lv_anim_set_time(&root_flicker, resolveAnimMs(340U));
        lv_anim_set_playback_time(&root_flicker, resolveAnimMs(340U));
        lv_anim_set_repeat_delay(&root_flicker, firework_pause_ms);
      } else {
        int32_t low_opa = static_cast<int32_t>(LV_OPA_COVER) - static_cast<int32_t>(demo_strobe_level_) * 3;
        if (low_opa < 12) {
          low_opa = 12;
        }
        if (low_opa > LV_OPA_COVER) {
          low_opa = LV_OPA_COVER;
        }
        lv_anim_set_exec_cb(&root_flicker, animSetOpa);
        lv_anim_set_values(&root_flicker, low_opa, LV_OPA_COVER);
        lv_anim_set_time(&root_flicker, resolveAnimMs(84U));
        lv_anim_set_playback_time(&root_flicker, resolveAnimMs(84U));
      }
      lv_anim_set_repeat_count(&root_flicker, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&root_flicker);

      if (broken_mode) {
        lv_anim_t root_noise;
        lv_anim_init(&root_noise);
        lv_anim_set_var(&root_noise, scene_root_);
        lv_anim_set_exec_cb(&root_noise, animSetRandomOpa);
        lv_anim_set_values(&root_noise, 0, 4095);
        lv_anim_set_time(&root_noise, resolveAnimMs(60U));
        lv_anim_set_repeat_count(&root_noise, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&root_noise);
      }
    }

    if (scene_symbol_label_ != nullptr) {
      lv_anim_t symbol_celebrate;
      lv_anim_init(&symbol_celebrate);
      lv_anim_set_var(&symbol_celebrate, scene_symbol_label_);
      lv_anim_set_exec_cb(&symbol_celebrate, animSetOpa);
      lv_anim_set_values(&symbol_celebrate, 120, LV_OPA_COVER);
      lv_anim_set_time(&symbol_celebrate, resolveAnimMs(360));
      lv_anim_set_playback_time(&symbol_celebrate, resolveAnimMs(360));
      lv_anim_set_repeat_count(&symbol_celebrate, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&symbol_celebrate);
    }
    if (fireworks_mode && scene_title_label_ != nullptr) {
      lv_anim_t title_celebrate;
      lv_anim_init(&title_celebrate);
      lv_anim_set_var(&title_celebrate, scene_title_label_);
      lv_anim_set_exec_cb(&title_celebrate, animSetOpa);
      lv_anim_set_values(&title_celebrate, 150, LV_OPA_COVER);
      lv_anim_set_time(&title_celebrate, resolveAnimMs(420U));
      lv_anim_set_playback_time(&title_celebrate, resolveAnimMs(420U));
      lv_anim_set_repeat_count(&title_celebrate, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_repeat_delay(&title_celebrate, firework_pause_ms);
      lv_anim_start(&title_celebrate);
    }
    if (fireworks_mode && scene_subtitle_label_ != nullptr) {
      lv_anim_t subtitle_celebrate;
      lv_anim_init(&subtitle_celebrate);
      lv_anim_set_var(&subtitle_celebrate, scene_subtitle_label_);
      lv_anim_set_exec_cb(&subtitle_celebrate, animSetOpa);
      lv_anim_set_values(&subtitle_celebrate, 130, LV_OPA_COVER);
      lv_anim_set_time(&subtitle_celebrate, resolveAnimMs(460U));
      lv_anim_set_playback_time(&subtitle_celebrate, resolveAnimMs(460U));
      lv_anim_set_repeat_count(&subtitle_celebrate, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_repeat_delay(&subtitle_celebrate, firework_pause_ms);
      lv_anim_set_delay(&subtitle_celebrate, resolveAnimMs(80U));
      lv_anim_start(&subtitle_celebrate);
    }

    if (fireworks_mode && scene_core_ != nullptr) {
      lv_anim_t core_sweep;
      lv_anim_init(&core_sweep);
      lv_anim_set_var(&core_sweep, scene_core_);
      lv_anim_set_exec_cb(&core_sweep, animSetStyleTranslateX);
      const int16_t sweep_amp = (width < 320) ? 26 : 44;
      lv_anim_set_values(&core_sweep, -sweep_amp, sweep_amp);
      lv_anim_set_time(&core_sweep, resolveAnimMs(760));
      lv_anim_set_playback_time(&core_sweep, resolveAnimMs(760));
      lv_anim_set_repeat_count(&core_sweep, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_repeat_delay(&core_sweep, resolveAnimMs(220U));
      lv_anim_set_delay(&core_sweep, resolveAnimMs(280));
      lv_anim_start(&core_sweep);
    }
  }
}

void UiManager::applySceneTransition(SceneTransition transition, uint16_t duration_ms) {
  if (scene_root_ == nullptr || transition == SceneTransition::kNone) {
    return;
  }
  if (duration_ms < 90U) {
    duration_ms = 90U;
  }
  if (duration_ms > 2200U) {
    duration_ms = 2200U;
  }

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_repeat_count(&anim, 0U);
  lv_anim_set_playback_time(&anim, 0U);

  if (transition == SceneTransition::kFade || transition == SceneTransition::kGlitch) {
    const lv_opa_t start_opa = (transition == SceneTransition::kGlitch) ? static_cast<lv_opa_t>(80) : LV_OPA_TRANSP;
    lv_obj_set_style_opa(scene_root_, start_opa, LV_PART_MAIN);
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetOpa);
    lv_anim_set_values(&anim, start_opa, LV_OPA_COVER);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_start(&anim);
    return;
  }

  if (transition == SceneTransition::kZoom && scene_core_ != nullptr) {
    const int32_t target_size = lv_obj_get_width(scene_core_);
    int32_t start_size = (target_size * 72) / 100;
    if (start_size < 24) {
      start_size = 24;
    }
    lv_obj_set_size(scene_core_, start_size, start_size);
    lv_obj_set_style_opa(scene_root_, LV_OPA_70, LV_PART_MAIN);

    lv_anim_t core_anim;
    lv_anim_init(&core_anim);
    lv_anim_set_var(&core_anim, scene_core_);
    lv_anim_set_exec_cb(&core_anim, animSetSize);
    lv_anim_set_values(&core_anim, start_size, target_size);
    lv_anim_set_time(&core_anim, duration_ms);
    lv_anim_start(&core_anim);

    lv_anim_t opa_anim;
    lv_anim_init(&opa_anim);
    lv_anim_set_var(&opa_anim, scene_root_);
    lv_anim_set_exec_cb(&opa_anim, animSetOpa);
    lv_anim_set_values(&opa_anim, LV_OPA_70, LV_OPA_COVER);
    lv_anim_set_time(&opa_anim, duration_ms);
    lv_anim_start(&opa_anim);
    return;
  }

  const int16_t dx = (activeDisplayWidth() > 240) ? 24 : 18;
  const int16_t dy = (activeDisplayHeight() > 240) ? 20 : 14;
  int16_t start_x = 0;
  int16_t start_y = 0;
  if (transition == SceneTransition::kSlideLeft) {
    start_x = dx;
  } else if (transition == SceneTransition::kSlideRight) {
    start_x = -dx;
  } else if (transition == SceneTransition::kSlideUp) {
    start_y = dy;
  } else if (transition == SceneTransition::kSlideDown) {
    start_y = -dy;
  }

  if (start_x != 0) {
    lv_obj_set_x(scene_root_, start_x);
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetX);
    lv_anim_set_values(&anim, start_x, 0);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_start(&anim);
  } else if (start_y != 0) {
    lv_obj_set_y(scene_root_, start_y);
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetY);
    lv_anim_set_values(&anim, start_y, 0);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_start(&anim);
  }

  lv_obj_set_style_opa(scene_root_, static_cast<lv_opa_t>(120), LV_PART_MAIN);
  lv_anim_t opa_anim;
  lv_anim_init(&opa_anim);
  lv_anim_set_var(&opa_anim, scene_root_);
  lv_anim_set_exec_cb(&opa_anim, animSetOpa);
  lv_anim_set_values(&opa_anim, 120, LV_OPA_COVER);
  lv_anim_set_time(&opa_anim, duration_ms);
  lv_anim_start(&opa_anim);
}

void UiManager::applySceneFraming(int16_t frame_dx, int16_t frame_dy, uint8_t frame_scale_pct, bool split_layout) {
  auto scaleSquare = [frame_scale_pct](lv_obj_t* obj, int16_t min_size) {
    if (obj == nullptr) {
      return;
    }
    int32_t width = lv_obj_get_width(obj);
    if (width < min_size) {
      width = min_size;
    }
    width = (width * frame_scale_pct) / 100;
    if (width < min_size) {
      width = min_size;
    }
    lv_obj_set_size(obj, width, width);
  };
  auto scaleWidth = [frame_scale_pct](lv_obj_t* obj, int16_t min_width) {
    if (obj == nullptr) {
      return;
    }
    int32_t width = lv_obj_get_width(obj);
    if (width < min_width) {
      width = min_width;
    }
    width = (width * frame_scale_pct) / 100;
    if (width < min_width) {
      width = min_width;
    }
    lv_obj_set_width(obj, width);
  };
  auto offset = [frame_dx, frame_dy](lv_obj_t* obj) {
    if (obj == nullptr) {
      return;
    }
    lv_obj_set_pos(obj, lv_obj_get_x(obj) + frame_dx, lv_obj_get_y(obj) + frame_dy);
  };

  if (frame_scale_pct != 100U) {
    scaleSquare(scene_ring_outer_, 80);
    scaleSquare(scene_ring_inner_, 58);
    scaleSquare(scene_core_, 44);
    scaleWidth(scene_fx_bar_, 72);
  }

  if (split_layout) {
    if (scene_core_ != nullptr) {
      lv_obj_set_x(scene_core_, lv_obj_get_x(scene_core_) - 28);
    }
    if (scene_ring_inner_ != nullptr) {
      lv_obj_set_x(scene_ring_inner_, lv_obj_get_x(scene_ring_inner_) - 16);
    }
    if (scene_ring_outer_ != nullptr) {
      lv_obj_set_x(scene_ring_outer_, lv_obj_get_x(scene_ring_outer_) - 10);
    }
    if (scene_symbol_label_ != nullptr) {
      lv_obj_set_x(scene_symbol_label_, lv_obj_get_x(scene_symbol_label_) + 52);
    }
    if (scene_title_label_ != nullptr) {
      lv_obj_set_x(scene_title_label_, lv_obj_get_x(scene_title_label_) - 18);
    }
    if (scene_subtitle_label_ != nullptr) {
      lv_obj_set_x(scene_subtitle_label_, lv_obj_get_x(scene_subtitle_label_) - 18);
    }
  }

  if (frame_dx != 0 || frame_dy != 0) {
    offset(scene_ring_outer_);
    offset(scene_ring_inner_);
    offset(scene_core_);
    offset(scene_fx_bar_);
    offset(scene_title_label_);
    offset(scene_subtitle_label_);
    offset(scene_symbol_label_);
    for (lv_obj_t* particle : scene_particles_) {
      offset(particle);
    }
  }
}

void UiManager::applyTextLayout(SceneTextAlign title_align, SceneTextAlign subtitle_align) {
  if (scene_title_label_ != nullptr) {
    if (title_align == SceneTextAlign::kCenter) {
      lv_obj_align(scene_title_label_, LV_ALIGN_CENTER, 0, -56);
    } else if (title_align == SceneTextAlign::kBottom) {
      lv_obj_align(scene_title_label_, LV_ALIGN_BOTTOM_MID, 0, -76);
    } else {
      lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 10);
    }
  }

  if (scene_subtitle_label_ != nullptr) {
    if (subtitle_align == SceneTextAlign::kTop) {
      lv_obj_align(scene_subtitle_label_, LV_ALIGN_TOP_MID, 0, 34);
    } else if (subtitle_align == SceneTextAlign::kCenter) {
      lv_obj_align(scene_subtitle_label_, LV_ALIGN_CENTER, 0, 58);
    } else {
      lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
    }
  }
}

void UiManager::applySubtitleScroll(SceneScrollMode mode, uint16_t speed_ms, uint16_t pause_ms, bool loop) {
  if (scene_subtitle_label_ == nullptr) {
    return;
  }
  lv_anim_del(scene_subtitle_label_, nullptr);

  int16_t label_width = activeDisplayWidth() - 32;
  if (label_width < 80) {
    label_width = 80;
  }
  lv_obj_set_width(scene_subtitle_label_, label_width);

  if (lv_obj_has_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }

  if (mode == SceneScrollMode::kNone) {
    lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return;
  }

  const char* subtitle_text = lv_label_get_text(scene_subtitle_label_);
  if (subtitle_text == nullptr || subtitle_text[0] == '\0') {
    return;
  }

  const lv_font_t* font = lv_obj_get_style_text_font(scene_subtitle_label_, LV_PART_MAIN);
  if (font == nullptr) {
    return;
  }

  lv_point_t text_size = {0, 0};
  lv_txt_get_size(&text_size,
                  subtitle_text,
                  font,
                  lv_obj_get_style_text_letter_space(scene_subtitle_label_, LV_PART_MAIN),
                  lv_obj_get_style_text_line_space(scene_subtitle_label_, LV_PART_MAIN),
                  LV_COORD_MAX,
                  LV_TEXT_FLAG_NONE);

  const int16_t overflow = static_cast<int16_t>(text_size.x - label_width);
  if (overflow <= 4) {
    lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return;
  }

  if (speed_ms < 600U) {
    speed_ms = 600U;
  }
  if (pause_ms > 8000U) {
    pause_ms = 8000U;
  }

  lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  const int32_t start_x = lv_obj_get_x(scene_subtitle_label_);
  const int32_t end_x = start_x - overflow - 14;

  lv_anim_t scroll_anim;
  lv_anim_init(&scroll_anim);
  lv_anim_set_var(&scroll_anim, scene_subtitle_label_);
  lv_anim_set_exec_cb(&scroll_anim, animSetX);
  lv_anim_set_values(&scroll_anim, start_x, end_x);
  lv_anim_set_time(&scroll_anim, speed_ms);
  lv_anim_set_delay(&scroll_anim, pause_ms);
  lv_anim_set_repeat_delay(&scroll_anim, pause_ms);
  lv_anim_set_repeat_count(&scroll_anim, loop ? LV_ANIM_REPEAT_INFINITE : 0U);
  lv_anim_set_playback_time(&scroll_anim, loop ? speed_ms : 0U);
  lv_anim_start(&scroll_anim);
}

void UiManager::applyThemeColors(uint32_t bg_rgb, uint32_t accent_rgb, uint32_t text_rgb) {
  const lv_color_t bg = quantize565ToTheme256(lv_color_hex(bg_rgb));
  const lv_color_t accent = quantize565ToTheme256(lv_color_hex(accent_rgb));
  const lv_color_t text = quantize565ToTheme256(lv_color_hex(text_rgb));

  lv_obj_set_style_bg_color(scene_root_, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_core_, accent, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_core_, text, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_outer_, accent, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_inner_, text, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_fx_bar_, accent, LV_PART_MAIN);
  if (scene_waveform_outer_ != nullptr) {
    lv_obj_set_style_line_color(scene_waveform_outer_, accent, LV_PART_MAIN);
  }
  if (scene_waveform_ != nullptr) {
    lv_obj_set_style_line_color(scene_waveform_, text, LV_PART_MAIN);
  }
  lv_obj_set_style_text_color(scene_title_label_, text, LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_subtitle_label_, text, LV_PART_MAIN);
  lv_obj_set_style_text_color(scene_symbol_label_, text, LV_PART_MAIN);
  if (scene_la_pitch_label_ != nullptr) {
    lv_obj_set_style_text_color(scene_la_pitch_label_, text, LV_PART_MAIN);
  }
  if (scene_la_meter_bg_ != nullptr) {
    lv_obj_set_style_border_color(scene_la_meter_bg_, accent, LV_PART_MAIN);
  }
  for (lv_obj_t* particle : scene_particles_) {
    if (particle == nullptr) {
      continue;
    }
    lv_obj_set_style_bg_color(particle, text, LV_PART_MAIN);
  }
}

uint8_t UiManager::particleIndexForObj(const lv_obj_t* target) const {
  if (target == nullptr) {
    return 4U;
  }
  for (uint8_t index = 0U; index < 4U; ++index) {
    if (target == scene_particles_[index]) {
      return index;
    }
  }
  return 4U;
}

void UiManager::resetSceneTimeline() {
  timeline_keyframe_count_ = 0U;
  timeline_duration_ms_ = 0U;
  timeline_loop_ = true;
  timeline_effect_index_ = -1;
}

void UiManager::onTimelineTick(uint16_t elapsed_ms) {
  if (timeline_keyframe_count_ == 0U) {
    return;
  }
  if (timeline_keyframe_count_ == 1U || timeline_duration_ms_ == 0U) {
    const SceneTimelineKeyframe& only = timeline_keyframes_[0];
    applyThemeColors(only.bg_rgb, only.accent_rgb, only.text_rgb);
    if (timeline_effect_index_ != 0) {
      stopSceneAnimations();
      effect_speed_ms_ = only.speed_ms;
      applySceneEffect(only.effect);
      timeline_effect_index_ = 0;
    }
    return;
  }

  if (timeline_loop_ && elapsed_ms >= timeline_duration_ms_) {
    elapsed_ms = static_cast<uint16_t>(elapsed_ms % timeline_duration_ms_);
  } else if (!timeline_loop_ && elapsed_ms > timeline_duration_ms_) {
    elapsed_ms = timeline_duration_ms_;
  }

  uint8_t segment_index = 0U;
  for (uint8_t index = 0U; (index + 1U) < timeline_keyframe_count_; ++index) {
    if (elapsed_ms < timeline_keyframes_[index + 1U].at_ms) {
      segment_index = index;
      break;
    }
    segment_index = index + 1U;
  }
  if (segment_index >= timeline_keyframe_count_) {
    segment_index = timeline_keyframe_count_ - 1U;
  }

  const SceneTimelineKeyframe& from = timeline_keyframes_[segment_index];
  const SceneTimelineKeyframe& to =
      (segment_index + 1U < timeline_keyframe_count_) ? timeline_keyframes_[segment_index + 1U] : from;

  if (timeline_effect_index_ != static_cast<int8_t>(segment_index)) {
    stopSceneAnimations();
    effect_speed_ms_ = from.speed_ms;
    applySceneEffect(from.effect);
    timeline_effect_index_ = static_cast<int8_t>(segment_index);
  }

  uint16_t progress = 1000U;
  if (to.at_ms > from.at_ms) {
    const uint16_t span = static_cast<uint16_t>(to.at_ms - from.at_ms);
    const uint16_t offset = (elapsed_ms > from.at_ms) ? static_cast<uint16_t>(elapsed_ms - from.at_ms) : 0U;
    progress = static_cast<uint16_t>((static_cast<uint32_t>(offset) * 1000U) / span);
    if (progress > 1000U) {
      progress = 1000U;
    }
  }

  const uint32_t bg_rgb = lerpRgb(from.bg_rgb, to.bg_rgb, progress);
  const uint32_t accent_rgb = lerpRgb(from.accent_rgb, to.accent_rgb, progress);
  const uint32_t text_rgb = lerpRgb(from.text_rgb, to.text_rgb, progress);
  applyThemeColors(bg_rgb, accent_rgb, text_rgb);
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

void UiManager::displayFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  if (disp == nullptr || area == nullptr || color_p == nullptr) {
    if (disp != nullptr) {
      lv_disp_flush_ready(disp);
    }
    return;
  }
  if (g_instance == nullptr) {
    lv_disp_flush_ready(disp);
    return;
  }

  UiManager* self = g_instance;
  drivers::display::DisplayHal& display = drivers::display::displayHal();
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  const uint32_t pixel_count = width * height;
  const uint32_t started_us = micros();
  const bool needs_convert = kUseColor256Runtime;
  const bool needs_copy_to_trans = self->buffer_cfg_.draw_in_psram || self->buffer_cfg_.full_frame;
  bool async_dma = self->async_flush_enabled_ && self->dma_available_ && !self->flush_ctx_.pending;
  bool tx_pixels_prepared = false;

  uint16_t* tx_pixels = reinterpret_cast<uint16_t*>(&color_p->full);
  if (needs_convert || needs_copy_to_trans) {
    if (self->dma_trans_buf_ != nullptr && pixel_count <= self->dma_trans_buf_pixels_) {
      tx_pixels = self->dma_trans_buf_;
      if (needs_convert) {
        self->convertLineRgb332ToRgb565(color_p, tx_pixels, pixel_count);
      } else {
        std::memcpy(tx_pixels, reinterpret_cast<uint16_t*>(&color_p->full), pixel_count * sizeof(uint16_t));
      }
      tx_pixels_prepared = true;
    } else {
      async_dma = false;
    }
  }

  if (async_dma) {
    display.startWrite();
    display.pushImageDma(area->x1,
                         area->y1,
                         static_cast<int16_t>(width),
                         static_cast<int16_t>(height),
                         tx_pixels);
    self->flush_ctx_.pending = true;
    self->flush_ctx_.using_dma = true;
    self->flush_ctx_.converted = (needs_convert || needs_copy_to_trans);
    self->flush_ctx_.disp = disp;
    self->flush_ctx_.area = *area;
    self->flush_ctx_.row_count = height;
    self->flush_ctx_.started_ms = started_us;
    return;
  }

  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, static_cast<int16_t>(width), static_cast<int16_t>(height));

  if (needs_convert && !tx_pixels_prepared) {
    static uint16_t row_buffer[(FREENOVE_LCD_WIDTH > FREENOVE_LCD_HEIGHT) ? FREENOVE_LCD_WIDTH
                                                                           : FREENOVE_LCD_HEIGHT];
    const uint32_t max_row = sizeof(row_buffer) / sizeof(row_buffer[0]);
    if (self->dma_trans_buf_ != nullptr && self->dma_trans_buf_pixels_ >= width) {
      for (uint32_t row = 0U; row < height; ++row) {
        const lv_color_t* src_row = color_p + (row * width);
        self->convertLineRgb332ToRgb565(src_row, self->dma_trans_buf_, width);
        display.pushColors(self->dma_trans_buf_, width, true);
      }
    } else if (width <= max_row) {
      for (uint32_t row = 0U; row < height; ++row) {
        const lv_color_t* src_row = color_p + (row * width);
        self->convertLineRgb332ToRgb565(src_row, row_buffer, width);
        display.pushColors(row_buffer, width, true);
      }
    } else {
      for (uint32_t pixel = 0U; pixel < pixel_count; ++pixel) {
#if LV_COLOR_DEPTH == 8
        const uint16_t c565 = self->rgb332_to_565_lut_[color_p[pixel].full];
        display.pushColor(c565);
#else
        display.pushColor(static_cast<uint16_t>(color_p[pixel].full));
#endif
      }
    }
  } else if (needs_copy_to_trans && tx_pixels_prepared) {
    display.pushColors(tx_pixels, pixel_count, true);
  } else {
    display.pushColors(tx_pixels, pixel_count, true);
  }
  display.endWrite();

  const uint32_t elapsed_us = micros() - started_us;
  self->graphics_stats_.flush_count += 1U;
  self->graphics_stats_.sync_flush_count += 1U;
  self->graphics_stats_.flush_time_total_us += elapsed_us;
  if (elapsed_us > self->graphics_stats_.flush_time_max_us) {
    self->graphics_stats_.flush_time_max_us = elapsed_us;
  }
  perfMonitor().noteUiFlush(false, elapsed_us);
  lv_disp_flush_ready(disp);
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
