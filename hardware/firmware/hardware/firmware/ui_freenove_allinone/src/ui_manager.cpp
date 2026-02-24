// ui_manager.cpp - LVGL binding for TFT + keypad events.
#include "ui_manager.h"

#include <ArduinoJson.h>
#include <TFT_eSPI.h>
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
#include "resources/screen_scene_registry.h"
#include "ui/scene_element.h"
#include "ui/scene_state.h"

namespace {

constexpr uint16_t kDrawBufferLines = 24;
constexpr uint16_t kPsramDrawBufferLines = 48;
constexpr uint16_t kPsramDrawBufferLinesFallback = 32;
constexpr size_t kPsramDrawBufferReserveBytes = 96U * 1024U;

TFT_eSPI g_tft = TFT_eSPI(FREENOVE_LCD_WIDTH, FREENOVE_LCD_HEIGHT);
lv_disp_draw_buf_t g_draw_buf;
lv_color_t g_draw_pixels_local[FREENOVE_LCD_WIDTH * kDrawBufferLines];
lv_color_t* g_draw_pixels = g_draw_pixels_local;
size_t g_draw_pixels_count = FREENOVE_LCD_WIDTH * static_cast<size_t>(kDrawBufferLines);
bool g_draw_buffer_in_psram = false;
UiManager* g_instance = nullptr;

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

bool tryAllocatePsramDrawBuffer(uint16_t draw_lines) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FREENOVE_PSRAM_UI_DRAW_BUFFER)
  const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  if (free_psram == 0U) {
    return false;
  }
  const size_t pixels = static_cast<size_t>(draw_lines) * FREENOVE_LCD_WIDTH;
  const size_t bytes = pixels * sizeof(lv_color_t);
  if (free_psram <= (bytes + kPsramDrawBufferReserveBytes)) {
    return false;
  }
  lv_color_t* const buffer = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr) {
    return false;
  }
  g_draw_pixels = buffer;
  g_draw_pixels_count = pixels;
  g_draw_buffer_in_psram = true;
  Serial.printf("[UI] draw buffer in PSRAM: lines=%u bytes=%u free_psram=%u\n",
                static_cast<unsigned int>(draw_lines),
                static_cast<unsigned int>(bytes),
                static_cast<unsigned int>(free_psram));
  return true;
#else
  (void)draw_lines;
  return false;
#endif
}

void initDrawBufferFromPsram() {
  g_draw_pixels = g_draw_pixels_local;
  g_draw_pixels_count = FREENOVE_LCD_WIDTH * static_cast<size_t>(kDrawBufferLines);
  g_draw_buffer_in_psram = false;

#if defined(ARDUINO_ARCH_ESP32) && defined(FREENOVE_PSRAM_UI_DRAW_BUFFER)
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0U) {
    Serial.printf("[UI] PSRAM unavailable for draw buffer, using internal RAM (%u lines)\n",
                  static_cast<unsigned int>(kDrawBufferLines));
  } else if (tryAllocatePsramDrawBuffer(kPsramDrawBufferLines) ||
             tryAllocatePsramDrawBuffer(kPsramDrawBufferLinesFallback)) {
  } else {
    Serial.printf("[UI] PSRAM insufficient, fallback draw buffer lines=%u in internal RAM\n",
                  static_cast<unsigned int>(kDrawBufferLines));
  }
#else
  Serial.printf("[UI] PSRAM draw buffer disabled, using internal RAM lines=%u\n",
                static_cast<unsigned int>(kDrawBufferLines));
#endif
  const uint16_t effective_lines =
      static_cast<uint16_t>(g_draw_pixels_count / static_cast<size_t>(FREENOVE_LCD_WIDTH));
  Serial.printf("[UI] LVGL draw buffer ready: source=%s lines=%u bytes=%u\n",
                g_draw_buffer_in_psram ? "PSRAM" : "DRAM",
                static_cast<unsigned int>(effective_lines),
                static_cast<unsigned int>(g_draw_pixels_count * sizeof(lv_color_t)));
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

}  // namespace

bool UiManager::begin() {
  if (ready_) {
    return true;
  }

  g_instance = this;
  lv_init();

  g_tft.begin();
  g_tft.setRotation(FREENOVE_LCD_ROTATION);
  g_tft.fillScreen(TFT_BLACK);
  initDrawBufferFromPsram();

  lv_disp_draw_buf_init(&g_draw_buf, g_draw_pixels, nullptr, g_draw_pixels_count);

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
  disp_drv.draw_buf = &g_draw_buf;
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
  createWidgets();
  last_lvgl_tick_ms_ = millis();
  ready_ = true;
  Serial.println("[UI] LVGL + TFT ready");
  return true;
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
  lv_timer_handler();
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
  const char* raw_scene_id = (screen_scene_id != nullptr && screen_scene_id[0] != '\0') ? screen_scene_id : "SCENE_READY";
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
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 || std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 ||
             std::strcmp(scene_id, "SCENE_REWARD") == 0) {
    title = "VICTOIRE";
    symbol = "WIN";
    effect = SceneEffect::kCelebrate;
    bg_rgb = 0x231038UL;
    accent_rgb = 0xF4CB4AUL;
    text_rgb = 0xFFF6C7UL;

    if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 &&
        std::strcmp(audio_pack_id_for_ui, "PACK_WIN") == 0 &&
        std::strcmp(step_id_for_ui, "STEP_ETAPE2") == 0) {
      title = "BRAVO!";
      subtitle = audio_playing ? "Validation en cours..." : "BRAVO! vous avez eu juste";
      win_etape_bravo_mode = true;
      show_title = true;
      demo_mode = "fireworks";
      demo_particle_count = 4U;
      demo_strobe_level = 92U;
      win_etape_fireworks = true;
    } else {
      subtitle = "Etape validee";
    }
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
  if (win_etape_bravo_mode) {
    title = "BRAVO!";
    subtitle = audio_playing ? "Validation en cours..." : "BRAVO! vous avez eu juste";
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
  lv_obj_set_style_text_font(scene_title_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_subtitle_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_symbol_label_, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_status_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_pitch_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_timer_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_font(scene_la_timeout_label_, &lv_font_montserrat_14, LV_PART_MAIN);
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
  const lv_color_t bg = lv_color_hex(bg_rgb);
  const lv_color_t accent = lv_color_hex(accent_rgb);
  const lv_color_t text = lv_color_hex(text_rgb);

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

void UiManager::displayFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  (void)disp;
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  g_tft.startWrite();
  g_tft.setAddrWindow(area->x1, area->y1, width, height);
  g_tft.pushColors(reinterpret_cast<uint16_t*>(&color_p->full), width * height, true);
  g_tft.endWrite();
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
