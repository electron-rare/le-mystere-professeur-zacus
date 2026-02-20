// ui_manager.h - LVGL/TFT scene renderer for Freenove all-in-one.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "core/scenario_def.h"
#include "ui/player_ui_model.h"

class UiManager {
 public:
  bool begin();
  void update();

  void renderScene(const ScenarioDef* scenario,
                   const char* screen_scene_id,
                   const char* step_id,
                   const char* audio_pack_id,
                   bool audio_playing,
                   const char* screen_payload_json);
  void handleButton(uint8_t key, bool long_press);
  void handleTouch(int16_t x, int16_t y, bool touched);

 private:
  enum class SceneEffect : uint8_t {
    kNone = 0,
    kPulse,
    kScan,
    kBlink,
    kCelebrate,
  };

  void createWidgets();
  void updatePageLine();
  void stopSceneAnimations();
  void applySceneEffect(SceneEffect effect);

  static void displayFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
  static void keypadReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void animSetY(void* obj, int32_t value);
  static void animSetOpa(void* obj, int32_t value);
  static void animSetSize(void* obj, int32_t value);
  static void animSetWidth(void* obj, int32_t value);

  bool ready_ = false;
  PlayerUiModel player_ui_;

  lv_obj_t* scene_root_ = nullptr;
  lv_obj_t* scene_core_ = nullptr;
  lv_obj_t* scene_ring_outer_ = nullptr;
  lv_obj_t* scene_ring_inner_ = nullptr;
  lv_obj_t* scene_fx_bar_ = nullptr;
  lv_obj_t* page_label_ = nullptr;
  lv_obj_t* scene_particles_[4] = {nullptr, nullptr, nullptr, nullptr};
  SceneEffect current_effect_ = SceneEffect::kNone;

  uint32_t pending_key_code_ = LV_KEY_ENTER;
  bool key_press_pending_ = false;
  bool key_release_pending_ = false;

  int16_t touch_x_ = 0;
  int16_t touch_y_ = 0;
  bool touch_pressed_ = false;
};
