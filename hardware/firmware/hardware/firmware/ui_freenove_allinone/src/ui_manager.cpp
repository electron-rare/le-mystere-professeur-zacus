// ui_manager.cpp - LVGL binding for TFT + keypad events.
#include "ui_manager.h"

#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <cstdlib>
#include <cstring>

#include "ui_freenove_config.h"

namespace {

constexpr uint16_t kDrawBufferLines = 24;

TFT_eSPI g_tft = TFT_eSPI(FREENOVE_LCD_WIDTH, FREENOVE_LCD_HEIGHT);
lv_disp_draw_buf_t g_draw_buf;
lv_color_t g_draw_pixels[FREENOVE_LCD_WIDTH * kDrawBufferLines];
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

bool parseHexColor(const char* text, lv_color_t* out_color) {
  if (text == nullptr || text[0] == '\0' || out_color == nullptr) {
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
  *out_color = lv_color_hex(static_cast<uint32_t>(value));
  return true;
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

  lv_disp_draw_buf_init(&g_draw_buf, g_draw_pixels, nullptr, FREENOVE_LCD_WIDTH * kDrawBufferLines);

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
  ready_ = true;
  Serial.println("[UI] LVGL + TFT ready");
  return true;
}

void UiManager::update() {
  if (!ready_) {
    return;
  }
  if (player_ui_.consumeDirty()) {
    updatePageLine();
  }
  lv_timer_handler();
}

void UiManager::renderScene(const ScenarioDef* scenario,
                            const char* screen_scene_id,
                            const char* step_id,
                            const char* audio_pack_id,
                            bool audio_playing,
                            const char* screen_payload_json) {
  (void)step_id;
  (void)audio_pack_id;
  if (!ready_) {
    return;
  }

  const char* scenario_id = (scenario != nullptr && scenario->id != nullptr) ? scenario->id : "N/A";
  const char* scene_id = (screen_scene_id != nullptr && screen_scene_id[0] != '\0') ? screen_scene_id : "SCENE_READY";

  SceneEffect effect = SceneEffect::kPulse;
  lv_color_t bg = lv_color_hex(0x07132A);
  lv_color_t accent = lv_color_hex(0x2A76FF);
  lv_color_t secondary = lv_color_hex(0xE8F1FF);

  if (std::strcmp(scene_id, "SCENE_LOCKED") == 0) {
    effect = SceneEffect::kPulse;
    bg = lv_color_hex(0x08152D);
    accent = lv_color_hex(0x3E8DFF);
    secondary = lv_color_hex(0xDBE8FF);
  } else if (std::strcmp(scene_id, "SCENE_BROKEN") == 0) {
    effect = SceneEffect::kBlink;
    bg = lv_color_hex(0x2A0508);
    accent = lv_color_hex(0xFF4A45);
    secondary = lv_color_hex(0xFFD5D1);
  } else if (std::strcmp(scene_id, "SCENE_LA_DETECT") == 0 || std::strcmp(scene_id, "SCENE_SEARCH") == 0) {
    effect = SceneEffect::kScan;
    bg = lv_color_hex(0x041F1B);
    accent = lv_color_hex(0x2CE5A6);
    secondary = lv_color_hex(0xD9FFF0);
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 || std::strcmp(scene_id, "SCENE_REWARD") == 0) {
    effect = SceneEffect::kCelebrate;
    bg = lv_color_hex(0x231038);
    accent = lv_color_hex(0xF4CB4A);
    secondary = lv_color_hex(0xFFF6C7);
  } else if (std::strcmp(scene_id, "SCENE_READY") == 0) {
    effect = SceneEffect::kPulse;
    bg = lv_color_hex(0x0F2A12);
    accent = lv_color_hex(0x6CD96B);
    secondary = lv_color_hex(0xE8FFE7);
  }

  if (screen_payload_json != nullptr && screen_payload_json[0] != '\0') {
    StaticJsonDocument<1024> document;
    const DeserializationError error = deserializeJson(document, screen_payload_json);
    if (!error) {
      const char* payload_effect = document["effect"] | document["visual"]["effect"] | document["content"]["effect"] | "";
      if (std::strcmp(payload_effect, "none") == 0 || std::strcmp(payload_effect, "steady") == 0) {
        effect = SceneEffect::kNone;
      } else if (std::strcmp(payload_effect, "pulse") == 0) {
        effect = SceneEffect::kPulse;
      } else if (std::strcmp(payload_effect, "scan") == 0) {
        effect = SceneEffect::kScan;
      } else if (std::strcmp(payload_effect, "blink") == 0 || std::strcmp(payload_effect, "glitch") == 0) {
        effect = SceneEffect::kBlink;
      } else if (std::strcmp(payload_effect, "celebrate") == 0) {
        effect = SceneEffect::kCelebrate;
      }

      const char* payload_bg = document["theme"]["bg"] | document["visual"]["theme"]["bg"] | document["bg"] | "";
      const char* payload_accent =
          document["theme"]["accent"] | document["visual"]["theme"]["accent"] | document["accent"] | "";
      const char* payload_secondary =
          document["theme"]["text"] | document["visual"]["theme"]["text"] | document["text"] | "";
      parseHexColor(payload_bg, &bg);
      parseHexColor(payload_accent, &accent);
      parseHexColor(payload_secondary, &secondary);
    } else {
      Serial.printf("[UI] invalid scene payload (%s)\n", error.c_str());
    }
  }

  stopSceneAnimations();
  current_effect_ = effect;

  lv_obj_set_style_bg_color(scene_root_, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_core_, accent, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_core_, secondary, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_outer_, accent, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_inner_, secondary, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_fx_bar_, accent, LV_PART_MAIN);
  for (lv_obj_t* particle : scene_particles_) {
    lv_obj_set_style_bg_color(particle, secondary, LV_PART_MAIN);
  }

  lv_obj_set_style_bg_opa(scene_core_, audio_playing ? LV_OPA_COVER : LV_OPA_80, LV_PART_MAIN);
  applySceneEffect(effect);
  updatePageLine();
  Serial.printf("[UI] scene=%s effect=%u scenario=%s audio=%u\n",
                scene_id,
                static_cast<unsigned int>(effect),
                scenario_id,
                audio_playing ? 1U : 0U);
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
  lv_obj_remove_style_all(scene_ring_outer_);
  lv_obj_set_style_radius(scene_ring_outer_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_ring_outer_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(scene_ring_outer_, 3, LV_PART_MAIN);
  lv_obj_set_style_border_opa(scene_ring_outer_, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_outer_, lv_color_hex(0x2A76FF), LV_PART_MAIN);

  scene_ring_inner_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(scene_ring_inner_);
  lv_obj_set_style_radius(scene_ring_inner_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_ring_inner_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(scene_ring_inner_, 2, LV_PART_MAIN);
  lv_obj_set_style_border_opa(scene_ring_inner_, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_ring_inner_, lv_color_hex(0xC8DCFF), LV_PART_MAIN);

  scene_core_ = lv_obj_create(scene_root_);
  lv_obj_remove_style_all(scene_core_);
  lv_obj_set_style_radius(scene_core_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scene_core_, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scene_core_, lv_color_hex(0x2A76FF), LV_PART_MAIN);
  lv_obj_set_style_border_width(scene_core_, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_core_, lv_color_hex(0xE8F1FF), LV_PART_MAIN);

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

  page_label_ = lv_label_create(scene_root_);
  lv_obj_add_flag(page_label_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_opa(page_label_, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_text_color(page_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

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

  if (scene_ring_outer_ != nullptr) {
    lv_anim_del(scene_ring_outer_, nullptr);
    int16_t outer = min_dim - 44;
    if (outer < 88) {
      outer = 88;
    }
    lv_obj_set_size(scene_ring_outer_, outer, outer);
    lv_obj_center(scene_ring_outer_);
    lv_obj_set_style_opa(scene_ring_outer_, LV_OPA_80, LV_PART_MAIN);
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
  }

  for (lv_obj_t* particle : scene_particles_) {
    if (particle == nullptr) {
      continue;
    }
    lv_anim_del(particle, nullptr);
    lv_obj_center(particle);
    lv_obj_set_style_opa(particle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
  }

  if (page_label_ != nullptr && !lv_obj_has_flag(page_label_, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_align(page_label_, LV_ALIGN_BOTTOM_LEFT, 10, -8);
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
    lv_anim_set_time(&anim, 640);
    lv_anim_set_playback_time(&anim, 640);
    lv_anim_start(&anim);

    if (scene_ring_inner_ != nullptr) {
      lv_anim_t ring_anim;
      lv_anim_init(&ring_anim);
      lv_anim_set_var(&ring_anim, scene_ring_inner_);
      lv_anim_set_exec_cb(&ring_anim, animSetOpa);
      lv_anim_set_values(&ring_anim, 90, LV_OPA_COVER);
      lv_anim_set_time(&ring_anim, 640);
      lv_anim_set_playback_time(&ring_anim, 640);
      lv_anim_set_repeat_count(&ring_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_anim);
    }
    return;
  }

  if (effect == SceneEffect::kScan) {
    int16_t bar_width = width - 84;
    if (bar_width < 90) {
      bar_width = 90;
    }
    lv_obj_set_size(scene_fx_bar_, bar_width, 10);
    lv_obj_align(scene_fx_bar_, LV_ALIGN_TOP_MID, 0, 20);

    lv_anim_set_var(&anim, scene_fx_bar_);
    lv_anim_set_exec_cb(&anim, animSetY);
    lv_anim_set_values(&anim, 20, height - 28);
    lv_anim_set_time(&anim, 920);
    lv_anim_set_playback_time(&anim, 920);
    lv_anim_start(&anim);
    return;
  }

  if (effect == SceneEffect::kBlink) {
    lv_anim_set_var(&anim, scene_root_);
    lv_anim_set_exec_cb(&anim, animSetOpa);
    lv_anim_set_values(&anim, 120, LV_OPA_COVER);
    lv_anim_set_time(&anim, 170);
    lv_anim_set_playback_time(&anim, 170);
    lv_anim_start(&anim);
    return;
  }

  if (effect == SceneEffect::kCelebrate) {
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
      lv_anim_set_time(&ring_anim, 460);
      lv_anim_set_playback_time(&ring_anim, 460);
      lv_anim_set_repeat_count(&ring_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&ring_anim);
    }

    lv_anim_t width_anim;
    lv_anim_init(&width_anim);
    lv_anim_set_var(&width_anim, scene_fx_bar_);
    lv_anim_set_exec_cb(&width_anim, animSetWidth);
    lv_anim_set_values(&width_anim, 36, width - 36);
    lv_anim_set_time(&width_anim, 420);
    lv_anim_set_playback_time(&width_anim, 420);
    lv_anim_set_repeat_count(&width_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&width_anim);

    const int16_t dx = min_dim / 5;
    const int16_t dy = min_dim / 7;
    for (uint8_t index = 0; index < 4U; ++index) {
      lv_obj_t* particle = scene_particles_[index];
      if (particle == nullptr) {
        continue;
      }
      const int16_t x_offset = ((index % 2U) == 0U) ? -dx : dx;
      const int16_t y_offset = (index < 2U) ? -dy : dy;
      lv_obj_clear_flag(particle, LV_OBJ_FLAG_HIDDEN);
      lv_obj_align(particle, LV_ALIGN_CENTER, x_offset, y_offset);

      lv_anim_t particle_opa;
      lv_anim_init(&particle_opa);
      lv_anim_set_var(&particle_opa, particle);
      lv_anim_set_exec_cb(&particle_opa, animSetOpa);
      lv_anim_set_values(&particle_opa, 80, LV_OPA_COVER);
      lv_anim_set_time(&particle_opa, 260);
      lv_anim_set_playback_time(&particle_opa, 260);
      lv_anim_set_repeat_count(&particle_opa, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_delay(&particle_opa, static_cast<uint16_t>(index * 40U));
      lv_anim_start(&particle_opa);
    }
  }
}

void UiManager::animSetY(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_y(static_cast<lv_obj_t*>(obj), value);
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

void UiManager::animSetWidth(void* obj, int32_t value) {
  if (obj == nullptr) {
    return;
  }
  if (value < 16) {
    value = 16;
  }
  lv_obj_set_width(static_cast<lv_obj_t*>(obj), value);
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
