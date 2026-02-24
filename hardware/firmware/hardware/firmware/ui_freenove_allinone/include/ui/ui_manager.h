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

  enum class IntroState : uint8_t {
    PHASE_A_CRACKTRO = 0,
    PHASE_B_TRANSITION,
    PHASE_C_CLEAN,
    PHASE_C_LOOP,
    OUTRO,
    DONE,
  };

  enum class Intro3DMode : uint8_t {
    kWireCube = 0,
    kRotoZoom,
    kTunnel,
    kPerspectiveStarfield,
  };

  enum class Intro3DQuality : uint8_t {
    kAuto = 0,
    kLow,
    kMed,
    kHigh,
  };

  struct SceneTimelineKeyframe {
    uint16_t at_ms = 0U;
    SceneEffect effect = SceneEffect::kNone;
    uint16_t speed_ms = 0U;
    uint32_t bg_rgb = 0x000000UL;
    uint32_t accent_rgb = 0x000000UL;
    uint32_t text_rgb = 0x000000UL;
  };

  struct IntroStarState {
    int32_t x_q8 = 0;
    int32_t y_q8 = 0;
    int16_t speed_px_per_s = 0;
    uint8_t size_px = 1U;
    uint8_t layer = 0U;
  };

  struct IntroParticleState {
    int32_t x_q8 = 0;
    int32_t y_q8 = 0;
    int32_t vx_q8 = 0;
    int32_t vy_q8 = 0;
    uint16_t life_ms = 0U;
    uint16_t age_ms = 0U;
    uint16_t delay_ms = 0U;
    bool active = false;
  };

  struct IntroConfig {
    char logo_text[64] = {0};
    char crack_scroll[240] = {0};
    char crack_bottom_scroll[128] = {0};
    char clean_title[64] = {0};
    char clean_scroll[240] = {0};
    uint32_t a_duration_ms = 30000U;
    uint32_t b_duration_ms = 15000U;
    uint32_t c_duration_ms = 20000U;
    uint16_t b1_crash_ms = 900U;
    uint16_t scroll_a_px_per_sec = 216U;
    uint16_t scroll_bot_a_px_per_sec = 108U;
    uint16_t scroll_c_px_per_sec = 72U;
    uint8_t sine_amp_a_px = 18U;
    uint8_t sine_amp_c_px = 12U;
    uint16_t sine_period_px = 104U;
    float sine_phase_speed = 1.9f;
    int16_t stars_override = -1;
    char fx_3d[20] = {0};
    char fx_3d_quality[16] = {0};
  };

  struct IntroGlyphSlot {
    lv_obj_t* glyph = nullptr;
    lv_obj_t* shadow = nullptr;
  };

  void createWidgets();
  void ensureIntroCreated();
  void resetIntroConfigDefaults();
  void loadSceneWinEtapeOverrides();
  void parseSceneWinEtapeTxtOverrides(const char* payload);
  void parseSceneWinEtapeJsonOverrides(const char* payload, const char* path_for_log);
  void startIntroIfNeeded(bool force_restart);
  void startIntro();
  void stopIntroAndCleanup();
  void requestIntroSkip();
  void transitionIntroState(IntroState next_state);
  void tickIntro();
  void configureBPhaseStart();
  void updateBPhase(uint32_t dt_ms, uint32_t now_ms, uint32_t state_elapsed_ms);
  void createCopperBars(uint8_t count);
  void updateCopperBars(uint32_t t_ms);
  void createCopperWavyRings(uint8_t count);
  void updateCopperWavyRings(uint32_t t_ms);
  void createStarfield(uint8_t count, uint8_t layers);
  void updateStarfield(uint32_t dt_ms);
  void createLogoLabel(const char* text);
  void animateLogoOvershoot();
  void configureWavySineScroller(const char* text,
                                 uint16_t speed_px_per_sec,
                                 uint8_t amp_px,
                                 uint16_t period_px,
                                 bool ping_pong,
                                 int16_t base_y,
                                 bool large_text = false,
                                 bool limit_to_half_width = false);
  void updateWavySineScroller(uint32_t dt_ms, uint32_t now_ms);
  void updateBottomRollbackScroller(uint32_t dt_ms);
  void configureBottomRollbackScroller(const char* text);
  void clampWaveYToBand(int16_t* y) const;
  void createWireCube();
  void updateWireCube(uint32_t dt_ms, bool crash_boost);
  void createRotoZoom();
  void updateRotoZoom(uint32_t dt_ms);
  void resolveIntro3DModeAndQuality();
  void updateIntroDebugOverlay(uint32_t dt_ms);
  void startGlitch(uint16_t duration_ms);
  void updateGlitch(uint32_t dt_ms);
  void startFireworks();
  void updateFireworks(uint32_t dt_ms);
  void startCleanReveal();
  void updateCleanReveal(uint32_t dt_ms);
  void updateSineScroller(uint32_t t_ms);
  uint32_t nextIntroRandom();
  void updatePageLine();
  void stopSceneAnimations();
  void applySceneEffect(SceneEffect effect);
  void applySceneTransition(SceneTransition transition, uint16_t duration_ms);
  void applySceneFraming(int16_t frame_dx, int16_t frame_dy, uint8_t frame_scale_pct, bool split_layout);
  void applyTextLayout(SceneTextAlign title_align, SceneTextAlign subtitle_align);
  void applySubtitleScroll(SceneScrollMode mode, uint16_t speed_ms, uint16_t pause_ms, bool loop);
  void onWinEtapeShowcaseTick(uint16_t elapsed_ms);
  void startWinEtapeCracktroPhase();
  void startWinEtapeCrashPhase();
  void startWinEtapeCleanPhase();
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
  static void animWinEtapeShowcaseTickCb(void* obj, int32_t value);
  static void animSetWinTitleReveal(void* obj, int32_t value);
  static void animSetSineTranslateY(void* obj, int32_t value);
  static void introTimerCb(lv_timer_t* timer);

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
  static constexpr uint8_t kCracktroBarCount = 12U;
  lv_obj_t* scene_cracktro_bars_[kCracktroBarCount] = {nullptr};
  static constexpr uint8_t kStarfieldCount = 48U;
  lv_obj_t* scene_starfield_[kStarfieldCount] = {nullptr};
  lv_obj_t* scene_particles_[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* intro_root_ = nullptr;
  lv_obj_t* intro_gradient_layers_[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* intro_logo_label_ = nullptr;
  lv_obj_t* intro_logo_shadow_label_ = nullptr;
  lv_obj_t* intro_crack_scroll_label_ = nullptr;
  lv_obj_t* intro_bottom_scroll_label_ = nullptr;
  lv_obj_t* intro_clean_title_label_ = nullptr;
  lv_obj_t* intro_clean_title_shadow_label_ = nullptr;
  lv_obj_t* intro_clean_scroll_label_ = nullptr;
  lv_obj_t* intro_debug_label_ = nullptr;
  static constexpr uint8_t kIntroWaveGlyphMax = 64U;
  IntroGlyphSlot intro_wave_slots_[kIntroWaveGlyphMax] = {};
  static constexpr uint8_t kIntroWireEdgeCount = 12U;
  lv_obj_t* intro_wire_lines_[kIntroWireEdgeCount] = {nullptr};
  lv_point_t intro_wire_points_[kIntroWireEdgeCount][2] = {};
  static constexpr uint8_t kIntroRotoStripeMax = 18U;
  lv_obj_t* intro_roto_stripes_[kIntroRotoStripeMax] = {nullptr};
  lv_obj_t* intro_firework_particles_[72] = {nullptr};
  IntroParticleState intro_firework_states_[72];
  IntroStarState intro_star_states_[kStarfieldCount];
  IntroConfig intro_config_;
  char intro_logo_ascii_[64] = {0};
  char intro_crack_scroll_ascii_[240] = {0};
  char intro_crack_bottom_scroll_ascii_[128] = {0};
  char intro_clean_title_ascii_[64] = {0};
  char intro_clean_scroll_ascii_[240] = {0};
  char intro_wave_text_ascii_[240] = {0};
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
  uint8_t win_etape_showcase_phase_ = 0xFFU;
  uint32_t last_lvgl_tick_ms_ = 0U;
  bool intro_created_ = false;
  bool intro_active_ = false;
  bool intro_skip_requested_ = false;
  bool intro_skip_latched_ = false;
  bool intro_clean_loop_only_ = false;
  IntroState intro_state_ = IntroState::DONE;
  Intro3DMode intro_3d_mode_ = Intro3DMode::kWireCube;
  Intro3DQuality intro_3d_quality_ = Intro3DQuality::kAuto;
  Intro3DQuality intro_3d_quality_resolved_ = Intro3DQuality::kMed;
  uint32_t t_state0_ms_ = 0U;
  uint32_t last_tick_ms_ = 0U;
  uint32_t intro_total_start_ms_ = 0U;
  uint32_t intro_logo_anim_start_ms_ = 0U;
  uint32_t intro_glitch_start_ms_ = 0U;
  uint32_t intro_glitch_next_jitter_ms_ = 0U;
  uint32_t intro_next_b2_pulse_ms_ = 0U;
  uint32_t intro_wave_last_ms_ = 0U;
  uint32_t intro_debug_next_ms_ = 0U;
  uint16_t intro_glitch_duration_ms_ = 0U;
  uint16_t intro_b1_crash_ms_ = 900U;
  uint16_t intro_scroll_mid_a_px_per_sec_ = 216U;
  uint16_t intro_scroll_bot_a_px_per_sec_ = 108U;
  uint16_t intro_copper_count_ = 0U;
  uint16_t intro_star_count_ = 0U;
  uint16_t intro_firework_active_count_ = 0U;
  uint16_t intro_clean_reveal_chars_ = 0U;
  uint32_t intro_clean_next_char_ms_ = 0U;
  int16_t intro_clean_scroll_base_y_ = -14;
  int16_t intro_bottom_scroll_base_y_ = -8;
  int32_t intro_bottom_scroll_x_q8_ = 0;
  int32_t intro_bottom_scroll_min_x_q8_ = 0;
  int32_t intro_bottom_scroll_max_x_q8_ = 0;
  int8_t intro_bottom_scroll_dir_ = -1;
  uint16_t intro_bottom_scroll_speed_px_per_sec_ = 128U;
  uint8_t intro_wave_glyph_count_ = 0U;
  uint16_t intro_wave_text_len_ = 0U;
  uint16_t intro_wave_head_index_ = 0U;
  int16_t intro_wave_char_width_ = 9;
  int16_t intro_wave_base_y_ = 128;
  int32_t intro_wave_pingpong_x_q8_ = 0;
  int32_t intro_wave_pingpong_min_x_q8_ = 0;
  int32_t intro_wave_pingpong_max_x_q8_ = 0;
  int8_t intro_wave_dir_ = -1;
  bool intro_wave_half_height_mode_ = false;
  int16_t intro_wave_band_top_ = 0;
  int16_t intro_wave_band_bottom_ = 0;
  bool intro_wave_pingpong_mode_ = false;
  uint16_t intro_wave_speed_px_per_sec_ = 120U;
  uint8_t intro_wave_amp_px_ = 12U;
  uint16_t intro_wave_period_px_ = 104U;
  float intro_wave_phase_ = 0.0f;
  float intro_wave_phase_speed_ = 1.9f;
  bool intro_b1_done_ = false;
  bool intro_cube_morph_enabled_ = true;
  float intro_cube_morph_phase_ = 0.0f;
  float intro_cube_morph_speed_ = 1.2f;
  uint16_t intro_cube_yaw_ = 0U;
  uint16_t intro_cube_pitch_ = 0U;
  uint16_t intro_cube_roll_ = 0U;
  float intro_roto_phase_ = 0.0f;
  bool intro_debug_overlay_enabled_ = false;
  uint32_t intro_rng_state_ = 0x1234ABCDUL;
  lv_timer_t* intro_timer_ = nullptr;

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
