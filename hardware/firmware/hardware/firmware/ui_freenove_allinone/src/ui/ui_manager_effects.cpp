#if defined(UI_MANAGER_SPLIT_IMPL)

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
                                uint8_t stability_pct,
                                const HardwareManager::Snapshot* snapshot) {
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
  const bool have_spectrum = (snapshot != nullptr && snapshot->mic_spectrum_peak_hz >= 380U);
  const float signal_gain = (static_cast<float>(scene_state.level_pct) / 100.0f) *
                            (0.45f + static_cast<float>(scene_state.confidence) / 200.0f);
  auto spectrumValueAt = [&](uint8_t slot) -> float {
    if (!have_spectrum) {
      return 0.0f;
    }
    constexpr float kStartHz = 400.0f;
    constexpr float kSpanHz = 80.0f;
    const float hz = kStartHz + (kSpanHz * static_cast<float>(slot) / static_cast<float>(kLaAnalyzerBarCount - 1U));
    if (hz <= 400.0f) {
      return static_cast<float>(snapshot->mic_spectrum[0]) / 100.0f;
    }
    if (hz >= 480.0f) {
      return static_cast<float>(snapshot->mic_spectrum[HardwareManager::kMicSpectrumBinCount - 1U]) / 100.0f;
    }
    const float pos = (hz - 400.0f) / 20.0f;
    uint8_t low = static_cast<uint8_t>(pos);
    if (low >= (HardwareManager::kMicSpectrumBinCount - 1U)) {
      low = HardwareManager::kMicSpectrumBinCount - 2U;
    }
    const uint8_t high = static_cast<uint8_t>(low + 1U);
    const float frac = pos - static_cast<float>(low);
    const float lo_val = static_cast<float>(snapshot->mic_spectrum[low]) / 100.0f;
    const float hi_val = static_cast<float>(snapshot->mic_spectrum[high]) / 100.0f;
    return lo_val + (hi_val - lo_val) * frac;
  };
  for (uint8_t index = 0U; index < kLaAnalyzerBarCount; ++index) {
    lv_obj_t* bar = scene_la_analyzer_bars_[index];
    if (bar == nullptr) {
      continue;
    }
    float energy = 0.0f;
    if (have_spectrum) {
      energy = spectrumValueAt(index) * signal_gain;
    } else {
      const float freq_norm = (freq_hz <= 320U)
                                  ? 0.0f
                                  : ((freq_hz >= 560U) ? 1.0f : (static_cast<float>(freq_hz - 320U) / 240.0f));
      const float freq_bin_pos = freq_norm * static_cast<float>(kLaAnalyzerBarCount - 1U);
      const float distance = std::fabs(static_cast<float>(index) - freq_bin_pos);
      float profile = 1.0f - (distance / 2.8f);
      if (profile < 0.0f) {
        profile = 0.0f;
      }
      energy = profile * signal_gain;
    }
    if ((freq_hz == 0U || scene_state.confidence < 8U) && !have_spectrum) {
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
    if (index <= 2U) {
      bar_color = 0xFF6E66UL;  // low-band side
    } else if (index >= (kLaAnalyzerBarCount - 3U)) {
      bar_color = 0x5F86FFUL;  // high-band side
    } else {
      bar_color = 0xA5FF72UL;  // center around A4
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
  if (kUseWinEtapeSimplifiedEffects && intro_active_) {
    hide_waveform();
    updateLaOverlay(false, 0U, 0, 0U, 0U, 0U, nullptr);
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

  if (la_detection_scene_ && scene_use_lgfx_text_overlay_) {
    hide_waveform();
    setBaseSceneFxVisible(false);
    updateLaOverlay(false, freq_hz, cents, confidence, level_pct, stability_pct, active_snapshot);
    return;
  }

  if (!waveform_overlay_enabled_ || active_snapshot == nullptr || active_snapshot->mic_waveform_count == 0U) {
    hide_waveform();
    updateLaOverlay(la_detection_scene_, freq_hz, cents, confidence, level_pct, stability_pct, active_snapshot);
    return;
  }
  const HardwareManager::Snapshot& snapshot = *active_snapshot;

  if (scene_core_ == nullptr || scene_ring_outer_ == nullptr) {
    hide_waveform();
    updateLaOverlay(false, 0U, 0, 0U, 0U, 0U, nullptr);
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
    updateLaOverlay(la_detection_scene_, freq_hz, cents, confidence, level_pct, stability_pct, active_snapshot);
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
    // LA_DETECTOR oscilloscope stays green for readability while meter/FFT provide tuner colors.
    if (la_detection_locked_) {
      inner_color = 0x7DFF7FU;
      outer_color = 0xC8FFD0U;
    } else if (stability_pct >= 70U) {
      inner_color = 0x66FF74U;
      outer_color = 0x8DFF9DU;
    } else if (stability_pct >= 35U) {
      inner_color = 0x52F76AU;
      outer_color = 0x6FEA88U;
    } else {
      inner_color = 0x3BE35AU;
      outer_color = 0x53C76EU;
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
      updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U, active_snapshot);
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
      updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U, active_snapshot);
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
    updateLaOverlay(false, freq_hz, cents, confidence, level_pct, 0U, active_snapshot);
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
                  stability_pct,
                  &snapshot);
}

uint32_t UiManager::hashScenePayload(const char* payload) {
  // FNV-1a 32-bit keeps payload-delta checks deterministic and cheap on MCU.
  uint32_t hash = 2166136261UL;
  if (payload == nullptr) {
    return hash;
  }
  for (size_t index = 0U; payload[index] != '\0'; ++index) {
    hash ^= static_cast<uint8_t>(payload[index]);
    hash *= 16777619UL;
  }
  return hash;
}

bool UiManager::shouldApplySceneStaticState(const char* scene_id,
                                            const char* payload_json,
                                            bool scene_changed) const {
  const uint32_t payload_hash = hashScenePayload(payload_json);
  if (scene_changed) {
    return true;
  }
  if (std::strcmp(last_scene_id_, scene_id) != 0) {
    return true;
  }
  return payload_hash != last_payload_crc_;
}

void UiManager::applySceneDynamicState(const String& subtitle,
                                       bool show_subtitle,
                                       bool audio_playing,
                                       uint32_t text_rgb) {
  if (scene_disable_lvgl_text_) {
    if (scene_title_label_ != nullptr) {
      lv_obj_add_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_subtitle_label_ != nullptr) {
      lv_obj_add_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (scene_symbol_label_ != nullptr) {
      lv_obj_add_flag(scene_symbol_label_, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (scene_subtitle_label_ != nullptr) {
    const String subtitle_ui = asciiFallbackForUiText(subtitle.c_str());
    lv_label_set_text(scene_subtitle_label_, subtitle_ui.c_str());
    if (show_subtitle && subtitle.length() > 0U) {
      lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(scene_subtitle_label_);
      lv_obj_set_style_text_color(scene_subtitle_label_, lv_color_hex(text_rgb), LV_PART_MAIN);
    } else {
      lv_obj_add_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (scene_core_ != nullptr) {
    lv_obj_set_style_bg_opa(scene_core_, audio_playing ? LV_OPA_COVER : LV_OPA_80, LV_PART_MAIN);
  }
  last_audio_playing_ = audio_playing;
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

bool UiManager::isWinEtapeSceneId(const char* scene_id) const {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 ||
         std::strcmp(scene_id, "SCENE_WIN_ETAPE1") == 0 ||
         std::strcmp(scene_id, "SCENE_WIN_ETAPE2") == 0 ||
         std::strcmp(scene_id, "SCENE_CREDITS") == 0 ||
         std::strcmp(scene_id, "SCENE_CREDIT") == 0;
}

bool UiManager::isDirectFxSceneId(const char* scene_id) const {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strncmp(scene_id, "SCENE_", 6U) == 0;
}

void UiManager::armDirectFxScene(const char* scene_id,
                                 bool test_lab_lgfx_scroller,
                                 const char* title_text,
                                 const char* subtitle_text) {
  if (scene_id == nullptr || !fx_engine_.config().lgfx_backend) {
    return;
  }
  if (!isDirectFxSceneId(scene_id)) {
    return;
  }

  if (!fx_engine_.ready()) {
    ui::fx::FxEngineConfig retry_cfg = fx_engine_.config();
    bool retry_ok = fx_engine_.begin(retry_cfg);
    if (!retry_ok) {
      // Last-resort geometry to keep FX alive when memory pressure is high.
      retry_cfg.sprite_width = 128U;
      retry_cfg.sprite_height = 96U;
      if (retry_cfg.target_fps > 15U) {
        retry_cfg.target_fps = 15U;
      }
      retry_ok = fx_engine_.begin(retry_cfg);
    }
    if (!retry_ok) {
      fx_engine_.setEnabled(false);
      direct_fx_scene_active_ = false;
      fx_rearm_retry_after_ms_ = millis() + 2000U;
      UI_LOGI("FX rearm skipped scene=%s reason=engine_not_ready", scene_id);
      return;
    }
    fx_rearm_retry_after_ms_ = 0U;
    UI_LOGI("FX engine recovered scene=%s sprite=%ux%u fps=%u",
            scene_id,
            static_cast<unsigned int>(retry_cfg.sprite_width),
            static_cast<unsigned int>(retry_cfg.sprite_height),
            static_cast<unsigned int>(retry_cfg.target_fps));
  }

  if (test_lab_lgfx_scroller) {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kDemo;
  } else if (std::strcmp(scene_id, "SCENE_U_SON_PROTO") == 0) {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kUsonProto;
  } else if (std::strcmp(scene_id, "SCENE_WIN_ETAPE1") == 0) {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kWinEtape1;
  } else if (std::strcmp(scene_id, "SCENE_CREDITS") == 0 ||
             std::strcmp(scene_id, "SCENE_CREDIT") == 0) {
    // Credits StarWars crawl is drawn by overlay text, keep the FX scroller empty.
    direct_fx_scene_preset_ = ui::fx::FxPreset::kUsonProto;
  } else if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 || std::strcmp(scene_id, "SCENE_FIREWORKS") == 0) {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kFireworks;
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 || std::strcmp(scene_id, "SCENE_REWARD") == 0 ||
             std::strcmp(scene_id, "SCENE_WINNER") == 0 || std::strcmp(scene_id, "SCENE_WIN_ETAPE2") == 0 ||
             std::strcmp(scene_id, "SCENE_FINAL_WIN") == 0) {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kWinner;
  } else {
    direct_fx_scene_preset_ = ui::fx::FxPreset::kDemo;
  }

  fx_engine_.setEnabled(true);
  fx_rearm_retry_after_ms_ = 0U;
  fx_engine_.setPreset(direct_fx_scene_preset_);
  fx_engine_.setMode(ui::fx::FxMode::kClassic);
  fx_engine_.setBpm(125U);
  fx_engine_.setScrollFont(ui::fx::FxScrollFont::kItalic);
  static constexpr const char* kWinEtapeScrollA = " -- en attente de validation distante ---";
  static constexpr const char* kWinEtapeScrollB = " -- validation non recue, merci de patienter ---";
  if (test_lab_lgfx_scroller) {
    fx_engine_.setAlternatingScrollText(nullptr, nullptr, false);
    fx_engine_.setScrollerCentered(true);
    fx_engine_.setScrollText("RVBCMJ -- DEMOMAKING RULEZ ---");
  } else if (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0) {
    fx_engine_.setScrollerCentered(false);
    fx_engine_.setAlternatingScrollText(kWinEtapeScrollA, kWinEtapeScrollB, true);
  } else if (std::strcmp(scene_id, "SCENE_CREDITS") == 0 || std::strcmp(scene_id, "SCENE_CREDIT") == 0) {
    fx_engine_.setAlternatingScrollText(nullptr, nullptr, false);
    fx_engine_.setScrollerCentered(false);
    fx_engine_.setScrollText(nullptr);
  } else {
    fx_engine_.setAlternatingScrollText(nullptr, nullptr, false);
    fx_engine_.setScrollerCentered(false);
    const char* source_text =
        (subtitle_text != nullptr && subtitle_text[0] != '\0') ? subtitle_text : title_text;
    const String fx_scroll_text = asciiFallbackForUiText(source_text != nullptr ? source_text : "");
    if (fx_scroll_text.length() > 0U) {
      fx_engine_.setScrollText(fx_scroll_text.c_str());
    } else {
      fx_engine_.setScrollText(" -- mode demo fx ---");
    }
  }
}

void UiManager::cleanupSceneTransitionAssets(const char* from_scene_id, const char* to_scene_id) {
  UI_LOGI("cleanup scene assets transition %s -> %s", from_scene_id, to_scene_id);
  direct_fx_scene_active_ = false;
  if (intro_active_) {
    stopIntroAndCleanup();
  }

  // Always hard-reset scene-level graphics state to avoid cross-scene artifacts.
  fx_engine_.setEnabled(false);
  fx_engine_.setScrollText(nullptr);
  fx_engine_.reset();
  resetSceneTimeline();
  waveform_overlay_enabled_ = false;
  waveform_overlay_jitter_ = false;
  la_detection_scene_ = false;
  la_detection_locked_ = false;
  la_detection_stability_pct_ = 0U;
  la_detection_stable_ms_ = 0U;
  la_detection_stable_target_ms_ = 0U;
  la_detection_gate_elapsed_ms_ = 0U;
  la_detection_gate_timeout_ms_ = 0U;
  warning_gyrophare_.destroy();
  warning_gyrophare_enabled_ = false;
  warning_gyrophare_disable_direct_fx_ = false;
  warning_lgfx_only_ = false;
  warning_siren_enabled_ = false;
  warning_lgfx_started_ms_ = 0U;
  scene_use_lgfx_text_overlay_ = false;
  scene_lgfx_hard_mode_ = false;
  setBaseSceneFxVisible(false);
  stopSceneAnimations();
}

void UiManager::setBaseSceneFxVisible(bool visible) {
  auto set_hidden = [visible](lv_obj_t* target) {
    if (target == nullptr) {
      return;
    }
    if (visible) {
      lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
    }
  };
  set_hidden(scene_ring_outer_);
  set_hidden(scene_ring_inner_);
  set_hidden(scene_core_);
  set_hidden(scene_fx_bar_);
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
  setBaseSceneFxVisible(false);

  if (scene_title_label_ != nullptr) {
    lv_anim_del(scene_title_label_, nullptr);
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBold24(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_translate_x(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_angle(scene_title_label_, 0, LV_PART_MAIN);
  }
  if (scene_symbol_label_ != nullptr) {
    lv_anim_del(scene_symbol_label_, nullptr);
    lv_obj_set_style_text_opa(scene_symbol_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_symbol_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(scene_symbol_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_translate_x(scene_symbol_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_symbol_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_angle(scene_symbol_label_, 0, LV_PART_MAIN);
  }
  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontItalic12(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_width(scene_subtitle_label_, width - 32);
    lv_label_set_long_mode(scene_subtitle_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(scene_subtitle_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_translate_x(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_angle(scene_subtitle_label_, 0, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBold24(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 2, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_translate_y(scene_title_label_, kUseWinEtapeSimplifiedEffects ? 0 : -66, LV_PART_MAIN);
    lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);

    if (!kUseWinEtapeSimplifiedEffects) {
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
  }

  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_label_set_text(scene_subtitle_label_, kWinEtapeCracktroScroll);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontItalic12(), LV_PART_MAIN);
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
    lv_label_set_text(scene_title_label_, kUseWinEtapeSimplifiedEffects ? kWinEtapeDemoTitle : "");
    lv_obj_set_style_text_font(scene_title_label_, UiFonts::fontBold24(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_title_label_, 1, LV_PART_MAIN);
    lv_obj_align(scene_title_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_translate_x(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(scene_title_label_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scene_title_label_, LV_OBJ_FLAG_HIDDEN);

    if (!kUseWinEtapeSimplifiedEffects) {
      lv_anim_t title_reveal;
      lv_anim_init(&title_reveal);
      lv_anim_set_var(&title_reveal, scene_title_label_);
      lv_anim_set_exec_cb(&title_reveal, animSetWinTitleReveal);
      lv_anim_set_values(&title_reveal, 0, static_cast<int32_t>(std::strlen(kWinEtapeDemoTitle)));
      lv_anim_set_time(&title_reveal, resolveAnimMs(1700U));
      lv_anim_set_delay(&title_reveal, 80U);
      lv_anim_start(&title_reveal);
    }
  }

  if (scene_subtitle_label_ != nullptr) {
    lv_anim_del(scene_subtitle_label_, nullptr);
    lv_label_set_text(scene_subtitle_label_, kWinEtapeDemoScroll);
    lv_obj_set_style_text_font(scene_subtitle_label_, UiFonts::fontItalic12(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_align(scene_subtitle_label_, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_translate_y(scene_subtitle_label_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scene_subtitle_label_, LV_OBJ_FLAG_HIDDEN);
    applySubtitleScroll(SceneScrollMode::kMarquee, resolveAnimMs(7600U), 500U, true);

    if (!kUseWinEtapeSimplifiedEffects) {
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
  if (scene_root_ == nullptr) {
    return;
  }
  if (effect == SceneEffect::kNone) {
    setBaseSceneFxVisible(false);
    return;
  }
  setBaseSceneFxVisible(true);
  if (scene_core_ == nullptr || scene_fx_bar_ == nullptr) {
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
    const bool rotate_direction_forward =
        ((mixNoise(static_cast<uint32_t>(lv_tick_get()), reinterpret_cast<uintptr_t>(scene_root_) ^ 0xA5B4C3D2UL) & 1U) != 0U);

    lv_obj_set_style_opa(scene_root_, LV_OPA_COVER, LV_PART_MAIN);

    if (scene_core_ != nullptr) {
      lv_anim_t core_rot;
      lv_anim_init(&core_rot);
      lv_anim_set_var(&core_rot, scene_core_);
      lv_anim_set_exec_cb(&core_rot, animSetStyleRotate);
      lv_anim_set_values(&core_rot, rotate_direction_forward ? -3600 : 3600, rotate_direction_forward ? 3600 : -3600);
      lv_anim_set_time(&core_rot, glitch_ms);
      lv_anim_set_playback_time(&core_rot, glitch_ms);
      lv_anim_set_repeat_count(&core_rot, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&core_rot);
    }
    if (scene_fx_bar_ != nullptr) {
      lv_anim_t bar_rot;
      lv_anim_init(&bar_rot);
      lv_anim_set_var(&bar_rot, scene_fx_bar_);
      lv_anim_set_exec_cb(&bar_rot, animSetStyleRotate);
      lv_anim_set_values(&bar_rot, rotate_direction_forward ? -900 : 900, rotate_direction_forward ? 900 : -900);
      lv_anim_set_time(&bar_rot, static_cast<uint16_t>(glitch_ms + 120U));
      lv_anim_set_playback_time(&bar_rot, static_cast<uint16_t>(glitch_ms + 120U));
      lv_anim_set_repeat_count(&bar_rot, LV_ANIM_REPEAT_INFINITE);
      lv_anim_start(&bar_rot);
    }

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

    const bool text_glitch_enabled = (text_glitch_pct_ > 0U);
    const uint16_t text_glitch_base_ms =
        resolveAnimMs(static_cast<uint16_t>(48U + (static_cast<uint16_t>(100U - text_glitch_pct_) * 2U)));
    if (scene_title_label_ != nullptr) {
      if (text_glitch_enabled) {
        lv_anim_t title_jitter_x;
        lv_anim_init(&title_jitter_x);
        lv_anim_set_var(&title_jitter_x, scene_title_label_);
        lv_anim_set_exec_cb(&title_jitter_x, animSetRandomTranslateX);
        lv_anim_set_values(&title_jitter_x, 0, 4095);
        lv_anim_set_time(&title_jitter_x, text_glitch_base_ms);
        lv_anim_set_repeat_count(&title_jitter_x, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&title_jitter_x);

        lv_anim_t title_jitter_y;
        lv_anim_init(&title_jitter_y);
        lv_anim_set_var(&title_jitter_y, scene_title_label_);
        lv_anim_set_exec_cb(&title_jitter_y, animSetRandomTranslateY);
        lv_anim_set_values(&title_jitter_y, 0, 4095);
        lv_anim_set_time(&title_jitter_y, static_cast<uint16_t>(text_glitch_base_ms + 12U));
        lv_anim_set_repeat_count(&title_jitter_y, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&title_jitter_y);

        lv_anim_t title_opa;
        lv_anim_init(&title_opa);
        lv_anim_set_var(&title_opa, scene_title_label_);
        lv_anim_set_exec_cb(&title_opa, animSetRandomTextOpa);
        lv_anim_set_values(&title_opa, 0, 4095);
        lv_anim_set_time(&title_opa, static_cast<uint16_t>(text_glitch_base_ms + 8U));
        lv_anim_set_repeat_count(&title_opa, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&title_opa);
      } else {
        lv_obj_set_style_translate_x(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_translate_y(scene_title_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_text_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(scene_title_label_, LV_OPA_COVER, LV_PART_MAIN);
      }
    }

    if (!kUseWinEtapeSimplifiedEffects && scene_subtitle_label_ != nullptr) {
      if (text_glitch_enabled) {
        lv_anim_t subtitle_jitter_x;
        lv_anim_init(&subtitle_jitter_x);
        lv_anim_set_var(&subtitle_jitter_x, scene_subtitle_label_);
        lv_anim_set_exec_cb(&subtitle_jitter_x, animSetRandomTranslateX);
        lv_anim_set_values(&subtitle_jitter_x, 0, 4095);
        lv_anim_set_time(&subtitle_jitter_x, static_cast<uint16_t>(text_glitch_base_ms + 14U));
        lv_anim_set_repeat_count(&subtitle_jitter_x, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&subtitle_jitter_x);

        lv_anim_t subtitle_jitter_y;
        lv_anim_init(&subtitle_jitter_y);
        lv_anim_set_var(&subtitle_jitter_y, scene_subtitle_label_);
        lv_anim_set_exec_cb(&subtitle_jitter_y, animSetRandomTranslateY);
        lv_anim_set_values(&subtitle_jitter_y, 0, 4095);
        lv_anim_set_time(&subtitle_jitter_y, static_cast<uint16_t>(text_glitch_base_ms + 18U));
        lv_anim_set_repeat_count(&subtitle_jitter_y, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&subtitle_jitter_y);

        lv_anim_t subtitle_opa;
        lv_anim_init(&subtitle_opa);
        lv_anim_set_var(&subtitle_opa, scene_subtitle_label_);
        lv_anim_set_exec_cb(&subtitle_opa, animSetRandomTextOpa);
        lv_anim_set_values(&subtitle_opa, 0, 4095);
        lv_anim_set_time(&subtitle_opa, static_cast<uint16_t>(text_glitch_base_ms + 10U));
        lv_anim_set_repeat_count(&subtitle_opa, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&subtitle_opa);
      } else {
        lv_obj_set_style_translate_x(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_translate_y(scene_subtitle_label_, 0, LV_PART_MAIN);
        lv_obj_set_style_text_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(scene_subtitle_label_, LV_OPA_COVER, LV_PART_MAIN);
      }
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
    if (!kUseWinEtapeSimplifiedEffects && fireworks_mode && scene_title_label_ != nullptr) {
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
    if (!kUseWinEtapeSimplifiedEffects && fireworks_mode && scene_subtitle_label_ != nullptr) {
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

void UiManager::applyTextLayout(SceneTextAlign title_align, SceneTextAlign subtitle_align, SceneTextAlign symbol_align) {
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

  if (scene_symbol_label_ != nullptr) {
    if (symbol_align == SceneTextAlign::kTop) {
      lv_obj_align(scene_symbol_label_, LV_ALIGN_TOP_MID, 0, 8);
    } else if (symbol_align == SceneTextAlign::kBottom) {
      lv_obj_align(scene_symbol_label_, LV_ALIGN_BOTTOM_MID, 0, -48);
    } else {
      lv_obj_align(scene_symbol_label_, LV_ALIGN_CENTER, 0, 0);
    }
  }
}

void UiManager::applySubtitleScroll(SceneScrollMode mode, uint16_t speed_ms, uint16_t pause_ms, bool loop) {
  if (scene_subtitle_label_ == nullptr) {
    return;
  }
  if (kUseWinEtapeSimplifiedEffects) {
    mode = SceneScrollMode::kNone;
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
  const uint32_t bg_key = static_cast<uint32_t>(bg.full);
  const uint32_t accent_key = static_cast<uint32_t>(accent.full);
  const uint32_t text_key = static_cast<uint32_t>(text.full);

  if (theme_cache_valid_ && theme_cache_bg_ == bg_key && theme_cache_accent_ == accent_key && theme_cache_text_ == text_key) {
    return;
  }
  theme_cache_valid_ = true;
  theme_cache_bg_ = bg_key;
  theme_cache_accent_ = accent_key;
  theme_cache_text_ = text_key;

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
  timeline_segment_cache_index_ = -1;
  timeline_segment_cache_elapsed_ms_ = 0U;
  theme_cache_valid_ = false;
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
  if (timeline_segment_cache_index_ >= 0 &&
      static_cast<uint8_t>(timeline_segment_cache_index_) < timeline_keyframe_count_) {
    segment_index = static_cast<uint8_t>(timeline_segment_cache_index_);
    if (elapsed_ms < timeline_segment_cache_elapsed_ms_) {
      segment_index = 0U;
    }
    while ((segment_index + 1U) < timeline_keyframe_count_ &&
           elapsed_ms >= timeline_keyframes_[segment_index + 1U].at_ms) {
      ++segment_index;
    }
    while (segment_index > 0U && elapsed_ms < timeline_keyframes_[segment_index].at_ms) {
      --segment_index;
    }
  } else {
    for (uint8_t index = 0U; (index + 1U) < timeline_keyframe_count_; ++index) {
      if (elapsed_ms < timeline_keyframes_[index + 1U].at_ms) {
        segment_index = index;
        break;
      }
      segment_index = index + 1U;
    }
  }
  if (segment_index >= timeline_keyframe_count_) {
    segment_index = timeline_keyframe_count_ - 1U;
  }
  timeline_segment_cache_index_ = static_cast<int8_t>(segment_index);
  timeline_segment_cache_elapsed_ms_ = elapsed_ms;

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

#endif  // UI_MANAGER_SPLIT_IMPL
