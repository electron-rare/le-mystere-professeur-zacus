#if defined(UI_MANAGER_SPLIT_IMPL)

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
  copyStringBounded(intro_config_.fx_backend, sizeof(intro_config_.fx_backend), "auto");
  copyStringBounded(intro_config_.fx_quality, sizeof(intro_config_.fx_quality), "auto");
  copyStringBounded(intro_config_.fx_3d, sizeof(intro_config_.fx_3d), "rotozoom");
  copyStringBounded(intro_config_.fx_3d_quality, sizeof(intro_config_.fx_3d_quality), "auto");
  copyStringBounded(intro_config_.font_mode, sizeof(intro_config_.font_mode), "orbitron");
  intro_config_.fx_preset_a = ui::fx::FxPreset::kDemo;
  intro_config_.fx_preset_b = ui::fx::FxPreset::kWinner;
  intro_config_.fx_preset_c = ui::fx::FxPreset::kBoingball;
  intro_config_.fx_mode_a = ui::fx::FxMode::kStarfield3D;
  intro_config_.fx_mode_b = ui::fx::FxMode::kDotSphere3D;
  intro_config_.fx_mode_c = ui::fx::FxMode::kRayCorridor;
  copyStringBounded(intro_config_.fx_scroll_text_a,
                    sizeof(intro_config_.fx_scroll_text_a),
                    kWinEtapeFxScrollTextA);
  copyStringBounded(intro_config_.fx_scroll_text_b,
                    sizeof(intro_config_.fx_scroll_text_b),
                    kWinEtapeFxScrollTextB);
  copyStringBounded(intro_config_.fx_scroll_text_c,
                    sizeof(intro_config_.fx_scroll_text_c),
                    kWinEtapeFxScrollTextC);
  intro_config_.fx_scroll_font = ui::fx::FxScrollFont::kItalic;
  intro_config_.fx_bpm = kIntroFxBpmDefault;
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
    if (key == "FX_BPM" && parseUint32Text(value, &parsed_u32)) {
      intro_config_.fx_bpm = static_cast<uint16_t>(parsed_u32);
      continue;
    }

    if (key == "FX_PRESET_A") {
      ui::fx::FxPreset preset = intro_config_.fx_preset_a;
      if (parseFxPresetToken(value, &preset)) {
        intro_config_.fx_preset_a = preset;
      }
      continue;
    }
    if (key == "FX_PRESET_B") {
      ui::fx::FxPreset preset = intro_config_.fx_preset_b;
      if (parseFxPresetToken(value, &preset)) {
        intro_config_.fx_preset_b = preset;
      }
      continue;
    }
    if (key == "FX_PRESET_C") {
      ui::fx::FxPreset preset = intro_config_.fx_preset_c;
      if (parseFxPresetToken(value, &preset)) {
        intro_config_.fx_preset_c = preset;
      }
      continue;
    }
    if (key == "FX_MODE_A") {
      ui::fx::FxMode mode = intro_config_.fx_mode_a;
      if (parseFxModeToken(value, &mode)) {
        intro_config_.fx_mode_a = mode;
      }
      continue;
    }
    if (key == "FX_MODE_B") {
      ui::fx::FxMode mode = intro_config_.fx_mode_b;
      if (parseFxModeToken(value, &mode)) {
        intro_config_.fx_mode_b = mode;
      }
      continue;
    }
    if (key == "FX_MODE_C") {
      ui::fx::FxMode mode = intro_config_.fx_mode_c;
      if (parseFxModeToken(value, &mode)) {
        intro_config_.fx_mode_c = mode;
      }
      continue;
    }
    if (key == "FX_SCROLL_TEXT_A") {
      copyStringBounded(intro_config_.fx_scroll_text_a, sizeof(intro_config_.fx_scroll_text_a), value.c_str());
      continue;
    }
    if (key == "FX_SCROLL_TEXT_B") {
      copyStringBounded(intro_config_.fx_scroll_text_b, sizeof(intro_config_.fx_scroll_text_b), value.c_str());
      continue;
    }
    if (key == "FX_SCROLL_TEXT_C") {
      copyStringBounded(intro_config_.fx_scroll_text_c, sizeof(intro_config_.fx_scroll_text_c), value.c_str());
      continue;
    }
    if (key == "FX_SCROLL_FONT") {
      ui::fx::FxScrollFont font = intro_config_.fx_scroll_font;
      if (parseFxScrollFontToken(value, &font)) {
        intro_config_.fx_scroll_font = font;
      }
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
    UI_LOGI("intro overrides parse error path=%s err=%s defaults",
            (path_for_log != nullptr) ? path_for_log : "n/a",
            err.c_str());
    return;
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
  if (doc["FX_BPM"].is<unsigned int>()) {
    intro_config_.fx_bpm = static_cast<uint16_t>(doc["FX_BPM"].as<unsigned int>());
  } else if (doc["fx_bpm"].is<unsigned int>()) {
    intro_config_.fx_bpm = static_cast<uint16_t>(doc["fx_bpm"].as<unsigned int>());
  }

  auto parse_preset = [&](const char* key_upper, const char* key_lower, ui::fx::FxPreset* target) {
    if (target == nullptr) {
      return;
    }
    const char* token = "";
    if (key_upper != nullptr && key_upper[0] != '\0') {
      token = doc[key_upper] | "";
    }
    if ((token == nullptr || token[0] == '\0') && key_lower != nullptr && key_lower[0] != '\0') {
      token = doc[key_lower] | "";
    }
    if (token != nullptr && token[0] != '\0') {
      ui::fx::FxPreset parsed = *target;
      if (parseFxPresetToken(String(token), &parsed)) {
        *target = parsed;
      }
    }
  };
  parse_preset("FX_PRESET_A", "fx_preset_a", &intro_config_.fx_preset_a);
  parse_preset("FX_PRESET_B", "fx_preset_b", &intro_config_.fx_preset_b);
  parse_preset("FX_PRESET_C", "fx_preset_c", &intro_config_.fx_preset_c);

  auto parse_mode = [&](const char* key_upper, const char* key_lower, ui::fx::FxMode* target) {
    if (target == nullptr) {
      return;
    }
    const char* token = "";
    if (key_upper != nullptr && key_upper[0] != '\0') {
      token = doc[key_upper] | "";
    }
    if ((token == nullptr || token[0] == '\0') && key_lower != nullptr && key_lower[0] != '\0') {
      token = doc[key_lower] | "";
    }
    if (token != nullptr && token[0] != '\0') {
      ui::fx::FxMode parsed = *target;
      if (parseFxModeToken(String(token), &parsed)) {
        *target = parsed;
      }
    }
  };
  parse_mode("FX_MODE_A", "fx_mode_a", &intro_config_.fx_mode_a);
  parse_mode("FX_MODE_B", "fx_mode_b", &intro_config_.fx_mode_b);
  parse_mode("FX_MODE_C", "fx_mode_c", &intro_config_.fx_mode_c);

  const char* scroll_a = doc["FX_SCROLL_TEXT_A"] | doc["fx_scroll_text_a"] | "";
  if (scroll_a != nullptr && scroll_a[0] != '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_a, sizeof(intro_config_.fx_scroll_text_a), scroll_a);
  }
  const char* scroll_b = doc["FX_SCROLL_TEXT_B"] | doc["fx_scroll_text_b"] | "";
  if (scroll_b != nullptr && scroll_b[0] != '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_b, sizeof(intro_config_.fx_scroll_text_b), scroll_b);
  }
  const char* scroll_c = doc["FX_SCROLL_TEXT_C"] | doc["fx_scroll_text_c"] | "";
  if (scroll_c != nullptr && scroll_c[0] != '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_c, sizeof(intro_config_.fx_scroll_text_c), scroll_c);
  }

  const char* font = doc["FX_SCROLL_FONT"] | doc["fx_scroll_font"] | "";
  if (font != nullptr && font[0] != '\0') {
    ui::fx::FxScrollFont parsed = intro_config_.fx_scroll_font;
    if (parseFxScrollFontToken(String(font), &parsed)) {
      intro_config_.fx_scroll_font = parsed;
    }
  }

  UI_LOGI("intro overrides loaded from %s", (path_for_log != nullptr) ? path_for_log : "json");
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
      UI_LOGI("intro overrides loaded from %s", loaded_path.c_str());
    } else {
      parseSceneWinEtapeJsonOverrides(payload.c_str(), loaded_path.c_str());
    }
  } else {
    UI_LOGI("intro overrides: no file, defaults");
  }

  intro_config_.a_duration_ms =
      clampValue<uint32_t>(intro_config_.a_duration_ms, kIntroCracktroMsMin, kIntroCracktroMsMax);
  intro_config_.b_duration_ms =
      clampValue<uint32_t>(intro_config_.b_duration_ms, kIntroTransitionMsMin, kIntroTransitionMsMax);
  intro_config_.c_duration_ms =
      clampValue<uint32_t>(intro_config_.c_duration_ms, kIntroCleanMsMin, kIntroCleanMsMax);
  intro_config_.fx_bpm = clampValue<uint16_t>(intro_config_.fx_bpm, 60U, 220U);
  if (intro_config_.fx_scroll_text_a[0] == '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_a,
                      sizeof(intro_config_.fx_scroll_text_a),
                      kWinEtapeFxScrollTextA);
  }
  if (intro_config_.fx_scroll_text_b[0] == '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_b,
                      sizeof(intro_config_.fx_scroll_text_b),
                      kWinEtapeFxScrollTextB);
  }
  if (intro_config_.fx_scroll_text_c[0] == '\0') {
    copyStringBounded(intro_config_.fx_scroll_text_c,
                      sizeof(intro_config_.fx_scroll_text_c),
                      kWinEtapeFxScrollTextC);
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
  lv_obj_set_style_bg_opa(intro_root_, LV_OPA_TRANSP, LV_PART_MAIN);
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
  const int16_t start_y = kUseWinEtapeSimplifiedEffects ? 0 : -80;
  lv_obj_set_style_translate_y(intro_logo_shadow_label_, start_y, LV_PART_MAIN);
  lv_obj_set_style_translate_y(intro_logo_label_, start_y, LV_PART_MAIN);
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
  String wave_text = asciiFallbackForUiText(text);
  String padded = wave_text;
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

  if (kUseWinEtapeSimplifiedEffects) {
    intro_wave_glyph_count_ = 0U;
    for (uint8_t i = 0U; i < kIntroWaveGlyphMax; ++i) {
      if (intro_wave_slots_[i].glyph != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].glyph, LV_OBJ_FLAG_HIDDEN);
      }
      if (intro_wave_slots_[i].shadow != nullptr) {
        lv_obj_add_flag(intro_wave_slots_[i].shadow, LV_OBJ_FLAG_HIDDEN);
      }
    }

    lv_obj_t* active_scroll = large_text ? intro_clean_scroll_label_ : intro_crack_scroll_label_;
    lv_obj_t* inactive_scroll = large_text ? intro_crack_scroll_label_ : intro_clean_scroll_label_;
    if (inactive_scroll != nullptr) {
      lv_obj_add_flag(inactive_scroll, LV_OBJ_FLAG_HIDDEN);
    }
    if (active_scroll != nullptr) {
      lv_label_set_text(active_scroll, wave_text.c_str());
      lv_obj_set_style_text_font(active_scroll, wave_font, LV_PART_MAIN);
      lv_obj_set_style_text_color(active_scroll, introPaletteColor(7), LV_PART_MAIN);
      lv_obj_set_style_text_opa(active_scroll, LV_OPA_90, LV_PART_MAIN);
      lv_obj_set_style_text_align(active_scroll, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      lv_label_set_long_mode(active_scroll, LV_LABEL_LONG_CLIP);
      lv_obj_set_width(active_scroll, static_cast<lv_coord_t>((width > 24) ? (width - 24) : width));
      lv_obj_align(active_scroll, LV_ALIGN_TOP_MID, 0, base_y);
      lv_obj_clear_flag(active_scroll, LV_OBJ_FLAG_HIDDEN);
    }
    return;
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
      if (char_width_q8 <= 0) {
        return;
      }
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
  String fx_backend_mode = intro_config_.fx_backend;
  fx_backend_mode.toLowerCase();
  const bool fx_lgfx_available = fx_engine_.config().lgfx_backend;
  intro_render_mode_ = fx_lgfx_available ? IntroRenderMode::kFxOnlyV8 : IntroRenderMode::kLegacy;

  bool fx_enabled = fx_lgfx_available;
  if (fx_backend_mode == "lvgl_canvas" || fx_backend_mode == "lvgl") {
    fx_enabled = false;
  } else if (fx_backend_mode == "lgfx") {
    fx_enabled = fx_lgfx_available;
  } else {
    fx_enabled = fx_lgfx_available;
  }
  fx_engine_.setEnabled(fx_enabled);

  String fx_quality_mode = intro_config_.fx_quality;
  fx_quality_mode.toLowerCase();
  uint8_t fx_quality_level = 0U;
  if (fx_quality_mode == "low") {
    fx_quality_level = 1U;
  } else if (fx_quality_mode == "med" || fx_quality_mode == "medium") {
    fx_quality_level = 2U;
  } else if (fx_quality_mode == "high") {
    fx_quality_level = 3U;
  }
  if (kUseWinEtapeSimplifiedEffects && fx_quality_mode == "auto") {
    fx_quality_level = 1U;
  }
  fx_engine_.setQualityLevel(fx_quality_level);
  if (kUseWinEtapeSimplifiedEffects) {
    const ui::fx::FxEngineConfig current_fx_cfg = fx_engine_.config();
    ui::fx::FxEngineConfig simplified_fx_cfg = current_fx_cfg;
    const uint16_t display_w = static_cast<uint16_t>(activeDisplayWidth() > 0 ? activeDisplayWidth()
                                                                               : current_fx_cfg.sprite_width);
    const uint16_t display_h = static_cast<uint16_t>(activeDisplayHeight() > 0 ? activeDisplayHeight()
                                                                                 : current_fx_cfg.sprite_height);
    simplified_fx_cfg.sprite_width = clampValue<uint16_t>(static_cast<uint16_t>(display_w / 2U), 96U, 240U);
    simplified_fx_cfg.sprite_height = clampValue<uint16_t>(static_cast<uint16_t>(display_h / 2U), 72U, 240U);
    simplified_fx_cfg.target_fps = 10U;
    if (current_fx_cfg.sprite_width != simplified_fx_cfg.sprite_width ||
        current_fx_cfg.sprite_height != simplified_fx_cfg.sprite_height ||
        current_fx_cfg.target_fps != simplified_fx_cfg.target_fps) {
      fx_engine_.begin(simplified_fx_cfg);
      fx_engine_.setEnabled(fx_enabled);
      fx_engine_.setQualityLevel(fx_quality_level);
    }
  }
  if (kUseWinEtapeSimplifiedEffects &&
      static_cast<uint8_t>(intro_3d_quality_resolved_) >
          static_cast<uint8_t>(Intro3DQuality::kLow)) {
    intro_3d_quality_resolved_ = Intro3DQuality::kLow;
  }
  if (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) {
    fx_enabled = fx_lgfx_available;
    fx_engine_.setEnabled(fx_enabled);
    fx_engine_.setBpm(intro_config_.fx_bpm);
    fx_engine_.setScrollFont(intro_config_.fx_scroll_font);
  }

  intro_clean_loop_only_ = false;
  intro_active_ = true;
  intro_state_ = IntroState::DONE;
  intro_total_start_ms_ = lv_tick_get();
  last_tick_ms_ = intro_total_start_ms_;
  intro_wave_last_ms_ = intro_total_start_ms_;
  intro_debug_next_ms_ = intro_total_start_ms_;
  intro_phase_log_next_ms_ = intro_total_start_ms_ + 5000U;
  intro_overlay_invalidate_ms_ = 0U;
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
  if (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) {
    hideLegacyIntroObjectsForFxOnly();
  }

  transitionIntroState(IntroState::PHASE_A_CRACKTRO);

  if (intro_timer_ == nullptr) {
    intro_timer_ = lv_timer_create(introTimerCb, kIntroTickMs, this);
  } else {
    lv_timer_set_period(intro_timer_, kIntroTickMs);
    lv_timer_resume(intro_timer_);
  }

  UI_LOGI("[WIN_ETAPE] start mode=%s A=%lu B=%lu C=%lu quality=%u 3d=%u",
          (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) ? "fx_only_v8" : "legacy",
          static_cast<unsigned long>(intro_config_.a_duration_ms),
          static_cast<unsigned long>(intro_config_.b_duration_ms),
          static_cast<unsigned long>(intro_config_.c_duration_ms),
          static_cast<unsigned int>(intro_3d_quality_resolved_),
          static_cast<unsigned int>(intro_3d_mode_));
  UI_LOGI("[WIN_ETAPE] fx backend=%s enabled=%u quality=%s target_fps=%u sprite=%ux%u bpm=%u font=%s",
          fx_backend_mode.c_str(),
          fx_enabled ? 1U : 0U,
          fx_quality_mode.c_str(),
          static_cast<unsigned int>(fx_engine_.config().target_fps),
          static_cast<unsigned int>(fx_engine_.config().sprite_width),
          static_cast<unsigned int>(fx_engine_.config().sprite_height),
          static_cast<unsigned int>(intro_config_.fx_bpm),
          fxScrollFontToken(intro_config_.fx_scroll_font));
  UI_LOGI("[WIN_ETAPE] presets A=%s B=%s C=%s",
          fxPresetToken(intro_config_.fx_preset_a),
          fxPresetToken(intro_config_.fx_preset_b),
          fxPresetToken(intro_config_.fx_preset_c));
  UI_LOGI("[WIN_ETAPE] fx modes A=%s B=%s C=%s",
          fxModeToken(intro_config_.fx_mode_a),
          fxModeToken(intro_config_.fx_mode_b),
          fxModeToken(intro_config_.fx_mode_c));
}

void UiManager::hideLegacyIntroObjectsForFxOnly() {
  intro_copper_count_ = 0U;
  intro_star_count_ = 0U;
  intro_firework_active_count_ = 0U;
  intro_wave_glyph_count_ = 0U;

  for (lv_obj_t* bar : scene_cracktro_bars_) {
    if (bar != nullptr) {
      lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (lv_obj_t* star : scene_starfield_) {
    if (star != nullptr) {
      lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (lv_obj_t* layer : intro_gradient_layers_) {
    if (layer != nullptr) {
      lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
    }
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
  for (uint8_t i = 0U; i < 72U; ++i) {
    if (intro_firework_particles_[i] != nullptr) {
      lv_obj_add_flag(intro_firework_particles_[i], LV_OBJ_FLAG_HIDDEN);
    }
    intro_firework_states_[i].active = false;
  }

  if (intro_logo_label_ != nullptr) {
    lv_obj_add_flag(intro_logo_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_logo_shadow_label_ != nullptr) {
    lv_obj_add_flag(intro_logo_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_crack_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_crack_scroll_label_, LV_OBJ_FLAG_HIDDEN);
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
  if (intro_clean_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_debug_label_ != nullptr) {
    lv_obj_add_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::applyIntroFxOnlyPhasePreset(IntroState state) {
  ui::fx::FxPreset preset = intro_config_.fx_preset_a;
  ui::fx::FxMode mode = intro_config_.fx_mode_a;
  const char* text = intro_config_.fx_scroll_text_a;
  if (state == IntroState::PHASE_B_TRANSITION) {
    preset = intro_config_.fx_preset_b;
    mode = intro_config_.fx_mode_b;
    text = intro_config_.fx_scroll_text_b;
  } else if (state == IntroState::PHASE_C_CLEAN || state == IntroState::PHASE_C_LOOP) {
    preset = intro_config_.fx_preset_c;
    mode = intro_config_.fx_mode_c;
    text = intro_config_.fx_scroll_text_c;
  }

  fx_engine_.setPreset(preset);
  fx_engine_.setMode(mode);
  fx_engine_.setBpm(intro_config_.fx_bpm);
  fx_engine_.setScrollFont(intro_config_.fx_scroll_font);
  if (text == nullptr || text[0] == '\0') {
    fx_engine_.setScrollText(nullptr);
    return;
  }
  const String ascii_text = asciiFallbackForUiText(text);
  if (ascii_text.length() == 0U) {
    fx_engine_.setScrollText(nullptr);
    return;
  }
  fx_engine_.setScrollText(ascii_text.c_str());
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

  if (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) {
    hideLegacyIntroObjectsForFxOnly();
    if (next_state == IntroState::PHASE_A_CRACKTRO ||
        next_state == IntroState::PHASE_B_TRANSITION ||
        next_state == IntroState::PHASE_C_CLEAN ||
        next_state == IntroState::PHASE_C_LOOP) {
      applyIntroFxOnlyPhasePreset(next_state);
      UI_LOGI("[WIN_ETAPE] phase=%s preset=%s bpm=%u font=%s",
              (next_state == IntroState::PHASE_A_CRACKTRO)
                  ? "A"
                  : ((next_state == IntroState::PHASE_B_TRANSITION) ? "B"
                                                                     : ((next_state == IntroState::PHASE_C_CLEAN)
                                                                            ? "C"
                                                                            : "C_LOOP")),
              (next_state == IntroState::PHASE_A_CRACKTRO)
                  ? fxPresetToken(intro_config_.fx_preset_a)
                  : ((next_state == IntroState::PHASE_B_TRANSITION)
                         ? fxPresetToken(intro_config_.fx_preset_b)
                         : fxPresetToken(intro_config_.fx_preset_c)),
              static_cast<unsigned int>(intro_config_.fx_bpm),
              fxScrollFontToken(intro_config_.fx_scroll_font));
      UI_LOGI("[WIN_ETAPE] phase=%s mode=%s",
              (next_state == IntroState::PHASE_A_CRACKTRO)
                  ? "A"
                  : ((next_state == IntroState::PHASE_B_TRANSITION) ? "B"
                                                                     : ((next_state == IntroState::PHASE_C_CLEAN)
                                                                            ? "C"
                                                                            : "C_LOOP")),
              (next_state == IntroState::PHASE_A_CRACKTRO)
                  ? fxModeToken(intro_config_.fx_mode_a)
                  : ((next_state == IntroState::PHASE_B_TRANSITION)
                         ? fxModeToken(intro_config_.fx_mode_b)
                         : fxModeToken(intro_config_.fx_mode_c)));
      return;
    }
    if (next_state == IntroState::OUTRO) {
      return;
    }
    if (next_state == IntroState::DONE) {
      stopIntroAndCleanup();
      hide_wave_text();
      return;
    }
  }

  if (next_state == IntroState::PHASE_A_CRACKTRO) {
    intro_b1_done_ = false;
    intro_next_b2_pulse_ms_ = 0U;
    intro_wave_half_height_mode_ = false;
    intro_cube_morph_phase_ = 0.0f;
    const uint8_t bar_count = kUseWinEtapeSimplifiedEffects
                                  ? 0U
                                  : static_cast<uint8_t>(clampValue<int16_t>(h / 22, 8, 18));
    int16_t stars = kUseWinEtapeSimplifiedEffects
                        ? 0
                        : static_cast<int16_t>(clampValue<int32_t>(area / 1200, 60, 220));
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
    if (kUseWinEtapeSimplifiedEffects) {
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
    } else {
      createWireCube();
      createRotoZoom();
      for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
        if (intro_roto_stripes_[i] != nullptr) {
          lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
    }

    UI_LOGI("[WIN_ETAPE] phase=A obj=%u stars=%u particles=%u quality=%u",
            static_cast<unsigned int>(intro_copper_count_ + intro_star_count_ + (intro_wave_glyph_count_ * 2U) + kIntroWireEdgeCount + 8U),
            static_cast<unsigned int>(intro_star_count_),
            static_cast<unsigned int>(intro_firework_active_count_),
            static_cast<unsigned int>(intro_3d_quality_resolved_));
    return;
  }

  if (next_state == IntroState::PHASE_B_TRANSITION) {
    configureBPhaseStart();
    UI_LOGI("[WIN_ETAPE] phase=B obj=%u stars=%u particles=%u quality=%u",
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
    int16_t stars = kUseWinEtapeSimplifiedEffects
                        ? 0
                        : static_cast<int16_t>(clampValue<int32_t>(area / 1500, 60, 140));
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

    if (!kUseWinEtapeSimplifiedEffects && intro_3d_mode_ == Intro3DMode::kWireCube) {
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
      if (kUseWinEtapeSimplifiedEffects) {
        for (uint8_t i = 0U; i < kIntroRotoStripeMax; ++i) {
          if (intro_roto_stripes_[i] != nullptr) {
            lv_obj_add_flag(intro_roto_stripes_[i], LV_OBJ_FLAG_HIDDEN);
          }
        }
      } else {
        createRotoZoom();
      }
    }

    UI_LOGI("[WIN_ETAPE] phase=%s obj=%u stars=%u particles=%u quality=%u",
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
  if (kUseWinEtapeSimplifiedEffects) {
    intro_b1_done_ = true;
    intro_firework_active_count_ = 0U;
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
    return;
  }
  createRotoZoom();
  startGlitch(intro_b1_crash_ms_);
  startFireworks();
}

void UiManager::updateBPhase(uint32_t dt_ms, uint32_t now_ms, uint32_t state_elapsed_ms) {
  updateCopperBars(now_ms - intro_total_start_ms_);
  updateStarfield(dt_ms);
  updateWavySineScroller(dt_ms, now_ms);
  if (!kUseWinEtapeSimplifiedEffects) {
    updateBottomRollbackScroller(dt_ms);
    animateLogoOvershoot();
  }
  if (kUseWinEtapeSimplifiedEffects) {
    return;
  }
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
    if (kUseWinEtapeSimplifiedEffects) {
      lv_label_set_text(intro_clean_title_label_, intro_clean_title_ascii_);
    } else {
      lv_label_set_text(intro_clean_title_label_, "");
    }
    lv_obj_align(intro_clean_title_label_, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_translate_y(intro_clean_title_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(intro_clean_title_label_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(intro_clean_title_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_title_shadow_label_ != nullptr) {
    if (kUseWinEtapeSimplifiedEffects) {
      lv_label_set_text(intro_clean_title_shadow_label_, intro_clean_title_ascii_);
    } else {
      lv_label_set_text(intro_clean_title_shadow_label_, "");
    }
    lv_obj_align(intro_clean_title_shadow_label_, LV_ALIGN_TOP_MID, 1, 21);
    lv_obj_set_style_translate_y(intro_clean_title_shadow_label_, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(intro_clean_title_shadow_label_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_clear_flag(intro_clean_title_shadow_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_clean_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

void UiManager::stopIntroAndCleanup() {
  intro_active_ = false;
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
  intro_overlay_invalidate_ms_ = 0U;

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
  if (intro_crack_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_crack_scroll_label_, LV_OBJ_FLAG_HIDDEN);
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
  if (intro_clean_scroll_label_ != nullptr) {
    lv_obj_add_flag(intro_clean_scroll_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (intro_debug_label_ != nullptr) {
    lv_obj_add_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
  }
  fx_engine_.setEnabled(false);
  fx_engine_.reset();
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
  if (kUseWinEtapeSimplifiedEffects) {
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
  if (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) {
    return 0U;
  }
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

  const ui::fx::FxEngineStats fx_stats = fx_engine_.stats();
  lv_label_set_text_fmt(intro_debug_label_,
                        "phase=%u obj=%u stars=%u p=%u q=%u fx=%u",
                        static_cast<unsigned int>(intro_state_),
                        static_cast<unsigned int>(estimateIntroObjectCount()),
                        static_cast<unsigned int>(intro_star_count_),
                        static_cast<unsigned int>(intro_firework_active_count_),
                        static_cast<unsigned int>(intro_3d_quality_resolved_),
                        static_cast<unsigned int>(fx_stats.fps));
  lv_obj_clear_flag(intro_debug_label_, LV_OBJ_FLAG_HIDDEN);
}

void UiManager::tickIntro() {
  if (!intro_active_ || intro_root_ == nullptr) {
    return;
  }
  const uint32_t now = lv_tick_get();
  if (kUseDemoAutorunWinEtapeRuntime && (now - intro_total_start_ms_ >= kWinEtapeAutorunLoopMs)) {
    UI_LOGI("[WIN_ETAPE] autorun loop timeout: restarting A->B->C");
    startIntro();
    return;
  }
  uint32_t dt_ms = now - last_tick_ms_;
  if (dt_ms > 100U) {
    dt_ms = 100U;
  }
  last_tick_ms_ = now;
  const uint32_t state_elapsed = now - t_state0_ms_;

  fx_engine_.setSceneCounts(estimateIntroObjectCount(), intro_star_count_, intro_firework_active_count_);

  if (now >= intro_phase_log_next_ms_) {
    intro_phase_log_next_ms_ = now + 5000U;
    const UiMemorySnapshot mem = memorySnapshot();
    const ui::fx::FxEngineStats fx_stats = fx_engine_.stats();
    UI_LOGI("[WIN_ETAPE] phase=%u t=%lu obj=%u stars=%u particles=%u fx_fps=%u q=%u heap_int=%u heap_psram=%u largest_dma=%u",
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

  if (intro_render_mode_ == IntroRenderMode::kFxOnlyV8) {
    updateIntroDebugOverlay(dt_ms);
    switch (intro_state_) {
      case IntroState::PHASE_A_CRACKTRO:
        if (state_elapsed >= intro_config_.a_duration_ms) {
          transitionIntroState(IntroState::PHASE_B_TRANSITION);
        }
        break;
      case IntroState::PHASE_B_TRANSITION:
        if (state_elapsed >= intro_config_.b_duration_ms) {
          transitionIntroState(IntroState::PHASE_C_CLEAN);
        }
        break;
      case IntroState::PHASE_C_CLEAN:
        if (state_elapsed >= intro_config_.c_duration_ms) {
          transitionIntroState(IntroState::PHASE_C_LOOP);
        }
        break;
      case IntroState::PHASE_C_LOOP:
        if (state_elapsed >= intro_config_.c_duration_ms) {
          transitionIntroState(IntroState::PHASE_C_LOOP);
        }
        break;
      case IntroState::OUTRO:
      case IntroState::DONE:
      default:
        break;
    }
    return;
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
      if (!kUseWinEtapeSimplifiedEffects) {
        updateBottomRollbackScroller(dt_ms);
        animateLogoOvershoot();
      }
      if (!kUseWinEtapeSimplifiedEffects) {
        updateWireCube(dt_ms, false);
      }
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
      if (!kUseWinEtapeSimplifiedEffects) {
        updateC3DStage(now);
      }
      updateStarfield(dt_ms);
      if (!kUseWinEtapeSimplifiedEffects) {
        if (intro_3d_mode_ == Intro3DMode::kWireCube) {
          updateWireCube(dt_ms, false);
        } else {
          updateRotoZoom(dt_ms);
        }
      }
      updateWavySineScroller(dt_ms, now);
      updateCleanReveal(dt_ms);
      if (!kUseWinEtapeSimplifiedEffects) {
        updateFireworks(dt_ms);
      }
      updateIntroDebugOverlay(dt_ms);
      if (state_elapsed >= intro_config_.c_duration_ms) {
        transitionIntroState(IntroState::PHASE_C_LOOP);
      }
      break;

    case IntroState::PHASE_C_LOOP:
      if (!kUseWinEtapeSimplifiedEffects) {
        updateC3DStage(now);
      }
      updateStarfield(dt_ms);
      if (!kUseWinEtapeSimplifiedEffects) {
        if (intro_3d_mode_ == Intro3DMode::kWireCube) {
          updateWireCube(dt_ms, false);
        } else {
          updateRotoZoom(dt_ms);
        }
      }
      updateWavySineScroller(dt_ms, now);
      updateCleanReveal(dt_ms);
      if (!kUseWinEtapeSimplifiedEffects) {
        updateFireworks(dt_ms);
      }
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

#endif  // UI_MANAGER_SPLIT_IMPL
