// ui_manager.h - LVGL/TFT scene renderer for Freenove all-in-one.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "core/scenario_def.h"
#include "hardware_manager.h"
#include "ui/fx/fx_engine.h"
#include "ui/player_ui_model.h"
#include "ui/qr/qr_scene_controller.h"
#include "ui/qr/qr_scan_controller.h"
#include "ui/qr/qr_validation_rules.h"

struct UiSceneFrame {
  const ScenarioDef* scenario = nullptr;
  const char* screen_scene_id = nullptr;
  const char* step_id = nullptr;
  const char* audio_pack_id = nullptr;
  bool audio_playing = false;
  const char* screen_payload_json = nullptr;
};

enum class UiInputEventType : uint8_t {
  kButton = 0,
  kTouch,
};

struct UiInputEvent {
  UiInputEventType type = UiInputEventType::kButton;
  uint8_t key = 0U;
  bool long_press = false;
  int16_t touch_x = 0;
  int16_t touch_y = 0;
  bool touch_pressed = false;
};

struct UiLaMetrics {
  bool locked = false;
  uint8_t stability_pct = 0U;
  uint32_t stable_ms = 0U;
  uint32_t stable_target_ms = 0U;
  uint32_t gate_elapsed_ms = 0U;
  uint32_t gate_timeout_ms = 0U;
};

struct UiMemorySnapshot {
  uint32_t heap_internal_free = 0U;
  uint32_t heap_dma_free = 0U;
  uint32_t heap_psram_free = 0U;
  uint32_t heap_largest_dma_block = 0U;
  uint32_t lv_mem_used = 0U;
  uint32_t lv_mem_free = 0U;
  uint32_t lv_mem_max_used = 0U;
  uint8_t lv_mem_frag_pct = 0U;
  uint32_t alloc_failures = 0U;
  uint16_t draw_lines = 0U;
  bool draw_in_psram = false;
  bool full_frame = false;
  bool dma_async_enabled = false;
  uint32_t draw_buffer_bytes = 0U;
  uint32_t trans_buffer_bytes = 0U;
  uint16_t selected_trans_lines = 0U;
  uint32_t async_fallback_count = 0U;
  uint16_t fx_fps = 0U;
  uint32_t fx_frame_count = 0U;
  uint32_t fx_blit_cpu_us = 0U;
  uint32_t fx_blit_submit_us = 0U;
  uint32_t fx_blit_wait_us = 0U;
  uint32_t fx_blit_tail_wait_us = 0U;
  uint32_t fx_dma_timeout_count = 0U;
  uint32_t fx_blit_fail_busy = 0U;
  uint32_t fx_skip_flush_busy = 0U;
  uint32_t flush_blocked = 0U;
  uint32_t flush_overflow = 0U;
  uint32_t flush_time_avg_us = 0U;
  uint32_t flush_time_max_us = 0U;
  uint32_t flush_stall = 0U;
  uint32_t flush_recover = 0U;
  uint32_t draw_time_avg_us = 0U;
  uint32_t draw_time_max_us = 0U;
  uint32_t draw_lvgl_us = 0U;
  uint32_t flush_spi_us = 0U;
  uint32_t draw_flush_stall = 0U;
  uint16_t conv_pixels_per_ms = 0U;
};

struct UiSceneStatusSnapshot {
  bool valid = false;
  bool audio_playing = false;
  bool show_title = false;
  bool show_subtitle = false;
  bool show_symbol = false;
  uint32_t payload_crc = 0U;
  uint16_t effect_speed_ms = 0U;
  uint16_t transition_ms = 0U;
  uint32_t bg_rgb = 0U;
  uint32_t accent_rgb = 0U;
  uint32_t text_rgb = 0U;
  char scenario_id[48] = {0};
  char step_id[64] = {0};
  char scene_id[64] = {0};
  char audio_pack_id[64] = {0};
  char title[96] = {0};
  char subtitle[160] = {0};
  char symbol[48] = {0};
  char effect[24] = {0};
  char transition[24] = {0};
};

enum class UiStatusTopic : uint8_t {
  kGraphics = 0,
  kMemory,
};

class UiManager {
 public:
  UiManager() = default;
  ~UiManager() = default;
  UiManager(const UiManager&) = delete;
  UiManager& operator=(const UiManager&) = delete;
  UiManager(UiManager&&) = delete;
  UiManager& operator=(UiManager&&) = delete;

  bool begin();
  void tick(uint32_t now_ms);
  void setHardwareController(HardwareManager* hardware);
  void setHardwareSnapshot(const HardwareManager::Snapshot& snapshot);
  void setHardwareSnapshotRef(const HardwareManager::Snapshot* snapshot);
  void setLaMetrics(const UiLaMetrics& metrics);
  void submitSceneFrame(const UiSceneFrame& frame);
  void submitInputEvent(const UiInputEvent& event);
  bool consumeRuntimeEvent(char* out_event, size_t capacity);
  bool simulateQrPayload(const char* payload);
  void dumpStatus(UiStatusTopic topic) const;
  UiMemorySnapshot memorySnapshot() const;
  UiSceneStatusSnapshot sceneStatusSnapshot() const;

 private:
  void update();
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
  void dumpGraphicsStatus() const;
  void dumpMemoryStatus() const;

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

  enum class IntroRenderMode : uint8_t {
    kLegacy = 0,
    kFxOnlyV8,
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
    uint16_t b1_crash_ms = 4000U;
    uint16_t scroll_a_px_per_sec = 216U;
    uint16_t scroll_bot_a_px_per_sec = 108U;
    uint16_t scroll_c_px_per_sec = 72U;
    uint8_t sine_amp_a_px = 96U;
    uint8_t sine_amp_c_px = 96U;
    uint16_t sine_period_px = 104U;
    float sine_phase_speed = 1.9f;
    int16_t stars_override = -1;
    char fx_backend[16] = {0};
    char fx_quality[16] = {0};
    char fx_3d[20] = {0};
    char fx_3d_quality[16] = {0};
    char font_mode[16] = {0};
    ui::fx::FxPreset fx_preset_a = ui::fx::FxPreset::kDemo;
    ui::fx::FxPreset fx_preset_b = ui::fx::FxPreset::kWinner;
    ui::fx::FxPreset fx_preset_c = ui::fx::FxPreset::kBoingball;
    ui::fx::FxMode fx_mode_a = ui::fx::FxMode::kClassic;
    ui::fx::FxMode fx_mode_b = ui::fx::FxMode::kClassic;
    ui::fx::FxMode fx_mode_c = ui::fx::FxMode::kClassic;
    char fx_scroll_text_a[240] = {0};
    char fx_scroll_text_b[240] = {0};
    char fx_scroll_text_c[240] = {0};
    ui::fx::FxScrollFont fx_scroll_font = ui::fx::FxScrollFont::kItalic;
    uint16_t fx_bpm = 125U;
  };

  struct IntroGlyphSlot {
    lv_obj_t* glyph = nullptr;
    lv_obj_t* shadow = nullptr;
  };

  struct FlushContext {
    bool pending = false;
    bool using_dma = false;
    bool converted = false;
    bool dma_in_flight = false;
    bool prepared = false;
    lv_disp_drv_t* disp = nullptr;
    lv_area_t area = {0, 0, 0, 0};
    const lv_color_t* src = nullptr;
    const uint16_t* prepared_tx = nullptr;
    uint16_t col_count = 0U;
    uint32_t started_ms = 0U;
    uint32_t row_count = 0U;
  };

  struct BufferConfig {
    uint16_t lines = 0U;
    uint16_t selected_trans_lines = 0U;
    uint8_t bpp = 16U;
    bool draw_in_psram = false;
    bool full_frame = false;
    bool double_buffer = false;
    bool dma_enabled = false;
  };

  struct GraphicsStats {
    uint32_t flush_count = 0U;
    uint32_t dma_flush_count = 0U;
    uint32_t sync_flush_count = 0U;
    uint32_t flush_time_total_us = 0U;
    uint32_t flush_time_max_us = 0U;
    uint32_t draw_count = 0U;
    uint32_t draw_time_total_us = 0U;
    uint32_t draw_time_max_us = 0U;
    uint32_t flush_busy_poll_count = 0U;
    uint32_t flush_overflow_count = 0U;
    uint32_t flush_blocked_count = 0U;
    uint32_t flush_stall_count = 0U;
    uint32_t flush_recover_count = 0U;
    uint32_t fx_skip_flush_busy = 0U;
    uint32_t async_fallback_count = 0U;
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
  void transitionIntroState(IntroState next_state);
  void hideLegacyIntroObjectsForFxOnly();
  void applyIntroFxOnlyPhasePreset(IntroState state);
  void tickIntro();
  void configureBPhaseStart();
  void updateBPhase(uint32_t dt_ms, uint32_t now_ms, uint32_t state_elapsed_ms);
  void updateC3DStage(uint32_t now_ms);
  uint8_t estimateIntroObjectCount() const;
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
  uint8_t resolveCenterWaveAmplitudePx(const lv_font_t* wave_font) const;
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
  void initGraphicsPipeline();
  bool allocateDrawBuffers();
  bool initDmaEngine();
  bool isDisplayOutputBusy() const;
  void pollAsyncFlush();
  void completePendingFlush();
  uint16_t convertLineRgb332ToRgb565(const lv_color_t* src, uint16_t* dst, uint32_t px_count) const;
  lv_color_t quantize565ToTheme256(lv_color_t color) const;
  void invalidateFxOverlayObjects();
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
  static uint32_t hashScenePayload(const char* payload);
  bool shouldApplySceneStaticState(const char* scene_id, const char* payload_json, bool scene_changed) const;
  void applySceneDynamicState(const String& subtitle, bool show_subtitle, bool audio_playing, uint32_t text_rgb);
  void resetSceneTimeline();
  void onTimelineTick(uint16_t elapsed_ms);
  bool isWinEtapeSceneId(const char* scene_id) const;
  bool isDirectFxSceneId(const char* scene_id) const;
  void cleanupSceneTransitionAssets(const char* from_scene_id, const char* to_scene_id);

  static void displayFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
  static void keypadReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  static void animSetY(void* obj, int32_t value);
  static void animSetX(void* obj, int32_t value);
  static void animSetStyleTranslateX(void* obj, int32_t value);
  static void animSetStyleTranslateY(void* obj, int32_t value);
  static void animSetStyleRotate(void* obj, int32_t value);
  static void animSetFireworkTranslateX(void* obj, int32_t value);
  static void animSetFireworkTranslateY(void* obj, int32_t value);
  static void animSetOpa(void* obj, int32_t value);
  static void animSetSize(void* obj, int32_t value);
  static void animSetParticleSize(void* obj, int32_t value);
  static void animSetWidth(void* obj, int32_t value);
  static void animSetRandomTranslateX(void* obj, int32_t value);
  static void animSetRandomTranslateY(void* obj, int32_t value);
  static void animSetRandomOpa(void* obj, int32_t value);
  static void animSetRandomTextOpa(void* obj, int32_t value);
  static void animTimelineTickCb(void* obj, int32_t value);
  static void animWinEtapeShowcaseTickCb(void* obj, int32_t value);
  static void animSetWinTitleReveal(void* obj, int32_t value);
  static void animSetSineTranslateY(void* obj, int32_t value);
  static void introTimerCb(lv_timer_t* timer);

  bool ready_ = false;
  PlayerUiModel player_ui_;
  lv_disp_draw_buf_t draw_buf_;
  lv_color_t* draw_buf1_ = nullptr;
  lv_color_t* draw_buf2_ = nullptr;
  bool draw_buf1_owned_ = false;
  bool draw_buf2_owned_ = false;
  HardwareManager* hardware_ = nullptr;
  uint16_t* dma_trans_buf_ = nullptr;
  size_t dma_trans_buf_pixels_ = 0U;
  bool dma_trans_buf_owned_ = false;
  lv_color_t* full_frame_buf_ = nullptr;
  bool full_frame_buf_owned_ = false;
  FlushContext flush_ctx_;
  BufferConfig buffer_cfg_;
  GraphicsStats graphics_stats_;
  UiSceneStatusSnapshot scene_status_;
  uint16_t rgb332_to_565_lut_[256] = {};
  bool color_lut_ready_ = false;
  bool dma_requested_ = false;
  bool dma_available_ = false;
  bool async_flush_enabled_ = false;
  bool pending_lvgl_flush_request_ = false;
  bool pending_full_repaint_request_ = false;
  uint32_t flush_pending_since_ms_ = 0U;
  uint32_t flush_last_progress_ms_ = 0U;
  uint32_t async_fallback_until_ms_ = 0U;
  uint32_t graphics_stats_last_report_ms_ = 0U;

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
  int8_t timeline_segment_cache_index_ = -1;
  uint16_t timeline_segment_cache_elapsed_ms_ = 0U;
  uint8_t particleIndexForObj(const lv_obj_t* target) const;
  char last_scene_id_[40] = {0};
  uint32_t last_payload_crc_ = 0U;
  bool last_audio_playing_ = false;
  bool theme_cache_valid_ = false;
  uint32_t theme_cache_bg_ = 0U;
  uint32_t theme_cache_accent_ = 0U;
  uint32_t theme_cache_text_ = 0U;
  uint8_t demo_particle_count_ = 4U;
  uint8_t demo_strobe_level_ = 65U;
  bool win_etape_fireworks_mode_ = false;
  uint8_t win_etape_showcase_phase_ = 0xFFU;
  bool direct_fx_scene_active_ = false;
  ui::fx::FxPreset direct_fx_scene_preset_ = ui::fx::FxPreset::kDemo;
  uint32_t last_lvgl_tick_ms_ = 0U;
  bool intro_created_ = false;
  bool intro_active_ = false;
  bool intro_clean_loop_only_ = false;
  IntroRenderMode intro_render_mode_ = IntroRenderMode::kLegacy;
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
  uint16_t intro_b1_crash_ms_ = 4000U;
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
  int16_t intro_wave_font_line_height_ = 40;
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
  uint8_t intro_wave_amp_px_ = 96U;
  uint16_t intro_wave_period_px_ = 104U;
  float intro_wave_phase_ = 0.0f;
  float intro_wave_phase_speed_ = 1.9f;
  bool intro_wave_use_pixel_font_ = false;
  bool intro_b1_done_ = false;
  bool intro_cube_morph_enabled_ = true;
  float intro_cube_morph_phase_ = 0.0f;
  float intro_cube_morph_speed_ = 1.2f;
  uint16_t intro_cube_yaw_ = 0U;
  uint16_t intro_cube_pitch_ = 0U;
  uint16_t intro_cube_roll_ = 0U;
  float intro_roto_phase_ = 0.0f;
  bool intro_debug_overlay_enabled_ = false;
  uint32_t intro_phase_log_next_ms_ = 0U;
  uint32_t intro_overlay_invalidate_ms_ = 0U;
  uint8_t intro_c_fx_stage_ = 0U;
  uint32_t intro_c_fx_stage_start_ms_ = 0U;
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
  ui::fx::FxEngine fx_engine_;
  ui::QrScanController qr_scan_;
  ui::QrValidationRules qr_rules_;
  ui::QrSceneController qr_scene_controller_;
};
