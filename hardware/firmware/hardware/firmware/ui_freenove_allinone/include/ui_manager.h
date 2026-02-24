// ui_manager.h - LVGL/TFT scene renderer for Freenove all-in-one.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "core/scenario_def.h"
#include "hardware_manager.h"
#include "ui/player_ui_model.h"

class UiManager {
 public:
  bool begin();
  void update();
  void setHardwareSnapshot(const HardwareManager::Snapshot& snapshot);
  void setHardwareSnapshotRef(const HardwareManager::Snapshot* snapshot);
  void setLaDetectionState(bool locked,
                           uint8_t stability_pct,
                           uint32_t stable_ms,
                           uint32_t stable_target_ms,
                           uint32_t gate_elapsed_ms,
                           uint32_t gate_timeout_ms);

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
    kRadar,
    kWave,
    kBlink,
    kGlitch,
    kCelebrate,
  };

  enum class SceneTransition : uint8_t {
    kNone = 0,
    kFade,
    kSlideLeft,
    kSlideRight,
    kSlideUp,
    kSlideDown,
    kZoom,
    kGlitch,
  };

  enum class SceneTextAlign : uint8_t {
    kTop = 0,
    kCenter,
    kBottom,
  };

  enum class SceneScrollMode : uint8_t {
    kNone = 0,
    kMarquee,
  };

  struct SceneTimelineKeyframe {
    uint16_t at_ms = 0U;
    SceneEffect effect = SceneEffect::kNone;
    uint16_t speed_ms = 0U;
    uint32_t bg_rgb = 0x000000UL;
    uint32_t accent_rgb = 0x000000UL;
    uint32_t text_rgb = 0x000000UL;
  };

  void createWidgets();
  void updatePageLine();
  void stopSceneAnimations();
  void applySceneEffect(SceneEffect effect);
  void applySceneTransition(SceneTransition transition, uint16_t duration_ms);
  void applySceneFraming(int16_t frame_dx, int16_t frame_dy, uint8_t frame_scale_pct, bool split_layout);
  void applyTextLayout(SceneTextAlign title_align, SceneTextAlign subtitle_align);
  void applySubtitleScroll(SceneScrollMode mode, uint16_t speed_ms, uint16_t pause_ms, bool loop);
  void configureWaveformOverlay(const HardwareManager::Snapshot* snapshot, bool enabled, uint8_t sample_count, uint8_t amplitude_pct, bool jitter);
  void updateLaOverlay(bool visible,
                       uint16_t freq_hz,
                       int16_t cents,
                       uint8_t confidence,
                       uint8_t level_pct,
                       uint8_t stability_pct);
  void renderMicrophoneWaveform();
  uint16_t resolveAnimMs(uint16_t fallback_ms) const;
  void applyThemeColors(uint32_t bg_rgb, uint32_t accent_rgb, uint32_t text_rgb);
  void resetSceneTimeline();
  void onTimelineTick(uint16_t elapsed_ms);

  static void displayFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
  static void keypadReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void animSetY(void* obj, int32_t value);
  static void animSetX(void* obj, int32_t value);
  static void animSetStyleTranslateX(void* obj, int32_t value);
  static void animSetStyleTranslateY(void* obj, int32_t value);
  static void animSetFireworkTranslateX(void* obj, int32_t value);
  static void animSetFireworkTranslateY(void* obj, int32_t value);
  static void animSetOpa(void* obj, int32_t value);
  static void animSetSize(void* obj, int32_t value);
  static void animSetParticleSize(void* obj, int32_t value);
  static void animSetWidth(void* obj, int32_t value);
  static void animSetRandomTranslateX(void* obj, int32_t value);
  static void animSetRandomTranslateY(void* obj, int32_t value);
  static void animSetRandomOpa(void* obj, int32_t value);
  static void animTimelineTickCb(void* obj, int32_t value);

  bool ready_ = false;
  PlayerUiModel player_ui_;

  lv_obj_t* scene_root_ = nullptr;
  lv_obj_t* scene_core_ = nullptr;
  lv_obj_t* scene_ring_outer_ = nullptr;
  lv_obj_t* scene_ring_inner_ = nullptr;
  lv_obj_t* scene_fx_bar_ = nullptr;
  lv_obj_t* page_label_ = nullptr;
  lv_obj_t* scene_title_label_ = nullptr;
  lv_obj_t* scene_subtitle_label_ = nullptr;
  lv_obj_t* scene_symbol_label_ = nullptr;
  lv_obj_t* scene_particles_[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* scene_waveform_outer_ = nullptr;
  lv_obj_t* scene_waveform_ = nullptr;
  lv_obj_t* scene_la_status_label_ = nullptr;
  lv_obj_t* scene_la_pitch_label_ = nullptr;
  lv_obj_t* scene_la_timer_label_ = nullptr;
  lv_obj_t* scene_la_timeout_label_ = nullptr;
  lv_obj_t* scene_la_meter_bg_ = nullptr;
  lv_obj_t* scene_la_meter_fill_ = nullptr;
  lv_obj_t* scene_la_needle_ = nullptr;
  static constexpr uint8_t kLaAnalyzerBarCount = 8U;
  lv_obj_t* scene_la_analyzer_bars_[kLaAnalyzerBarCount] = {nullptr};
  lv_point_t waveform_points_[HardwareManager::kMicWaveformCapacity + 1U] = {};
  lv_point_t waveform_outer_points_[HardwareManager::kMicWaveformCapacity + 1U] = {};
  lv_point_t la_needle_points_[2] = {};
  SceneEffect current_effect_ = SceneEffect::kNone;
  uint16_t effect_speed_ms_ = 0U;
  static constexpr uint8_t kMaxTimelineKeyframes = 8U;
  SceneTimelineKeyframe timeline_keyframes_[kMaxTimelineKeyframes];
  uint8_t timeline_keyframe_count_ = 0U;
  uint16_t timeline_duration_ms_ = 0U;
  bool timeline_loop_ = true;
  int8_t timeline_effect_index_ = -1;
  uint8_t particleIndexForObj(const lv_obj_t* target) const;
  char last_scene_id_[40] = {0};
  uint8_t demo_particle_count_ = 4U;
  uint8_t demo_strobe_level_ = 65U;
  bool win_etape_fireworks_mode_ = false;
  uint32_t last_lvgl_tick_ms_ = 0U;

  uint32_t pending_key_code_ = LV_KEY_ENTER;
  bool key_press_pending_ = false;
  bool key_release_pending_ = false;
  const HardwareManager::Snapshot* waveform_snapshot_ref_ = nullptr;
  HardwareManager::Snapshot waveform_snapshot_ = {};
  bool waveform_snapshot_valid_ = false;
  bool waveform_overlay_enabled_ = false;
  bool waveform_overlay_jitter_ = true;
  uint8_t waveform_sample_count_ = HardwareManager::kMicWaveformCapacity;
  uint8_t waveform_amplitude_pct_ = 95U;
  bool la_detection_scene_ = false;
  bool la_detection_locked_ = false;
  uint8_t la_detection_stability_pct_ = 0U;
  uint32_t la_detection_stable_ms_ = 0U;
  uint32_t la_detection_stable_target_ms_ = 0U;
  uint32_t la_detection_gate_elapsed_ms_ = 0U;
  uint32_t la_detection_gate_timeout_ms_ = 0U;

  int16_t touch_x_ = 0;
  int16_t touch_y_ = 0;
  bool touch_pressed_ = false;
};
