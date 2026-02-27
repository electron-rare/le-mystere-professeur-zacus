#if defined(UI_MANAGER_SPLIT_IMPL)

void UiManager::initGraphicsPipeline() {
  flush_ctx_ = {};
  buffer_cfg_ = {};
  graphics_stats_ = {};
  pending_lvgl_flush_request_ = false;
  pending_full_repaint_request_ = false;
  flush_pending_since_ms_ = 0U;
  flush_last_progress_ms_ = 0U;
  async_fallback_until_ms_ = 0U;

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
  buffer_cfg_.selected_trans_lines = 0U;

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
    UI_LOGI("draw buffer allocation failed");
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
      UI_LOGI("full-frame size overflow, fallback to line buffers");
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
      UI_LOGI("draw buffer full-frame bench enabled bytes=%u", static_cast<unsigned int>(full_bytes));
      return true;
    }
    UI_LOGI("full-frame bench requested but unavailable, fallback to line buffers");
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
        UI_LOGD("draw buffer size overflow lines=%u", static_cast<unsigned int>(lines));
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
        UI_LOGI("draw buffers ready lines=%u bytes=%u source=%s double=1",
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
      UI_LOGI("draw buffer fallback mono lines=%u bytes=%u source=%s",
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
      UI_LOGI("draw buffer source fallback=%s", (!preferred_psram) ? "PSRAM" : "SRAM_DMA");
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
        UI_LOGD("trans buffer size overflow lines=%u", static_cast<unsigned int>(trans_lines));
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
        UI_LOGD("trans buffer size overflow lines=%u", static_cast<unsigned int>(trans_lines));
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
      buffer_cfg_.selected_trans_lines = selected_trans_lines;
      UI_LOGI("trans buffer ready lines=%u pixels=%u source=%s",
              static_cast<unsigned int>(selected_trans_lines),
              static_cast<unsigned int>(dma_trans_buf_pixels_),
              kUseDmaTxInDramRuntime ? "INTERNAL_DMA" : "DEFAULT");
      if (selected_trans_lines < buffer_cfg_.lines &&
          (kUseAsyncDmaRuntime || kUseColor256Runtime || buffer_cfg_.draw_in_psram)) {
        UI_LOGI("draw lines reduced for trans buffer: %u -> %u",
                static_cast<unsigned int>(buffer_cfg_.lines),
                static_cast<unsigned int>(selected_trans_lines));
        buffer_cfg_.lines = selected_trans_lines;
      }
    } else {
      buffer_cfg_.selected_trans_lines = 0U;
      dma_trans_buf_owned_ = false;
      dma_trans_buf_pixels_ = 0U;
      UI_LOGI("trans buffer unavailable; async DMA may be disabled");
    }
  } else {
    buffer_cfg_.selected_trans_lines = 0U;
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
    UI_LOGI("DMA engine unavailable, keeping sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  const bool needs_trans_buffer = kUseColor256Runtime || buffer_cfg_.draw_in_psram;
  if (needs_trans_buffer && dma_trans_buf_ == nullptr) {
    UI_LOGI("DMA enabled but trans buffer missing, keeping sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  if (kUseColor256Runtime && !kUseRgb332AsyncExperimental) {
    UI_LOGI("RGB332 async DMA disabled (UI_DMA_RGB332_ASYNC_EXPERIMENTAL=0), keeping sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  if (buffer_cfg_.full_frame) {
    UI_LOGI("full-frame bench forces sync flush");
    buffer_cfg_.dma_enabled = false;
    return false;
  }

  async_flush_enabled_ = true;
  buffer_cfg_.dma_enabled = true;
  async_fallback_until_ms_ = 0U;
  if (kUseColor256Runtime) {
    UI_LOGI("DMA async enabled (RGB332 -> RGB565 via trans buffer)");
  } else {
    UI_LOGI("DMA async flush enabled");
  }
  return true;
}

bool UiManager::isDisplayOutputBusy() const {
  if (flush_ctx_.pending) {
    return true;
  }
  return drivers::display::displayHal().dmaBusy();
}

void UiManager::pollAsyncFlush() {
  if (!flush_ctx_.pending) {
    flush_pending_since_ms_ = 0U;
    return;
  }

  const uint32_t now_ms = millis();
  if (flush_pending_since_ms_ == 0U) {
    flush_pending_since_ms_ = now_ms;
    flush_last_progress_ms_ = now_ms;
  }

  auto recover_stalled_flush = [this]() {
    const bool used_dma = flush_ctx_.using_dma;
    if (flush_ctx_.disp != nullptr) {
      lv_disp_flush_ready(flush_ctx_.disp);
    }
    flush_ctx_ = {};
    flush_pending_since_ms_ = 0U;
    flush_last_progress_ms_ = millis();
    pending_lvgl_flush_request_ = true;
    pending_full_repaint_request_ = true;
    graphics_stats_.flush_stall_count += 1U;
    graphics_stats_.flush_recover_count += 1U;
    if (used_dma && async_flush_enabled_) {
      async_flush_enabled_ = false;
      buffer_cfg_.dma_enabled = false;
      graphics_stats_.async_fallback_count += 1U;
      async_fallback_until_ms_ = millis() + kAsyncFallbackRecoverMs;
    }
  };

  if (flush_ctx_.using_dma && dma_available_ && drivers::display::displayHal().dmaBusy()) {
    graphics_stats_.flush_busy_poll_count += 1U;
    if ((now_ms - flush_pending_since_ms_) >= kFlushStallTimeoutMs) {
      recover_stalled_flush();
    }
    return;
  }

  completePendingFlush();
  if (!flush_ctx_.pending) {
    flush_pending_since_ms_ = 0U;
    flush_last_progress_ms_ = now_ms;
    return;
  }
  if ((now_ms - flush_pending_since_ms_) >= kFlushStallTimeoutMs) {
    recover_stalled_flush();
  }
}

void UiManager::completePendingFlush() {
  if (!flush_ctx_.pending) {
    return;
  }

  drivers::display::DisplayHal& display = drivers::display::displayHal();
  const uint32_t width = static_cast<uint32_t>(flush_ctx_.col_count);
  const uint32_t height = static_cast<uint32_t>(flush_ctx_.row_count);
  if (width == 0U || height == 0U || flush_ctx_.src == nullptr || flush_ctx_.disp == nullptr) {
    if (flush_ctx_.disp != nullptr) {
      lv_disp_flush_ready(flush_ctx_.disp);
    }
    flush_ctx_ = {};
    flush_pending_since_ms_ = 0U;
    flush_last_progress_ms_ = millis();
    return;
  }

  const uint32_t pixel_count = width * height;
  const bool has_valid_dma_tx = flush_ctx_.prepared && (flush_ctx_.prepared_tx != nullptr);
  bool use_dma = flush_ctx_.using_dma;
  if (use_dma && !has_valid_dma_tx) {
    use_dma = false;
  }

  if (flush_ctx_.using_dma && flush_ctx_.dma_in_flight) {
    if (display.dmaBusy()) {
      graphics_stats_.flush_busy_poll_count += 1U;
      return;
    }
    if (!display.startWrite()) {
      graphics_stats_.flush_busy_poll_count += 1U;
      return;
    }
    display.endWrite();
    flush_ctx_.dma_in_flight = false;
    if (flush_ctx_.disp != nullptr) {
      lv_disp_flush_ready(flush_ctx_.disp);
    }
    const uint32_t elapsed_us = micros() - flush_ctx_.started_ms;
    graphics_stats_.flush_count += 1U;
    graphics_stats_.dma_flush_count += 1U;
    graphics_stats_.flush_time_total_us += elapsed_us;
    if (elapsed_us > graphics_stats_.flush_time_max_us) {
      graphics_stats_.flush_time_max_us = elapsed_us;
    }
    perfMonitor().noteUiFlush(true, elapsed_us);
    flush_ctx_ = {};
    flush_pending_since_ms_ = 0U;
    flush_last_progress_ms_ = millis();
    return;
  } else {
    if (flush_ctx_.using_dma && use_dma) {
      if (!display.startWrite()) {
        graphics_stats_.flush_busy_poll_count += 1U;
        return;
      }
      display.pushImageDma(flush_ctx_.area.x1,
                           flush_ctx_.area.y1,
                           static_cast<int16_t>(width),
                           static_cast<int16_t>(height),
                           flush_ctx_.prepared_tx);
      display.endWrite();
      flush_ctx_.dma_in_flight = true;
      flush_last_progress_ms_ = millis();
      return;
    }

    if (!display.startWrite()) {
      return;
    }
    if (flush_ctx_.converted) {
      display.setAddrWindow(flush_ctx_.area.x1,
                           flush_ctx_.area.y1,
                           static_cast<int16_t>(width),
                           static_cast<int16_t>(height));
      if (flush_ctx_.prepared && flush_ctx_.prepared_tx != nullptr) {
        display.pushColors(flush_ctx_.prepared_tx, pixel_count, true);
      } else if (dma_trans_buf_ != nullptr && dma_trans_buf_pixels_ >= width) {
        for (uint32_t row = 0U; row < height; ++row) {
          const lv_color_t* src_row = flush_ctx_.src + (row * width);
          convertLineRgb332ToRgb565(src_row, dma_trans_buf_, width);
          display.pushColors(dma_trans_buf_, width, true);
        }
      } else {
        static uint16_t row_buffer[(FREENOVE_LCD_WIDTH > FREENOVE_LCD_HEIGHT) ? FREENOVE_LCD_WIDTH
                                                                           : FREENOVE_LCD_HEIGHT];
        const uint32_t max_row = sizeof(row_buffer) / sizeof(row_buffer[0]);
        const lv_color_t* src = flush_ctx_.src;
        if (src != nullptr && width <= max_row) {
          for (uint32_t row = 0U; row < height; ++row) {
            const lv_color_t* src_row = src + (row * width);
            convertLineRgb332ToRgb565(src_row, row_buffer, width);
            display.pushColors(row_buffer, width, true);
          }
        } else {
          for (uint32_t pixel = 0U; pixel < pixel_count; ++pixel) {
#if LV_COLOR_DEPTH == 8
            const uint16_t c565 = rgb332_to_565_lut_[src[pixel].full];
            display.pushColor(c565);
#else
            display.pushColor(static_cast<uint16_t>(src[pixel].full));
#endif
          }
        }
      }
      display.endWrite();
    } else {
      display.setAddrWindow(flush_ctx_.area.x1,
                           flush_ctx_.area.y1,
                           static_cast<int16_t>(width),
                           static_cast<int16_t>(height));
      display.pushColors(reinterpret_cast<const uint16_t*>(flush_ctx_.src), pixel_count, true);
      display.endWrite();
    }
  }

  if (flush_ctx_.disp != nullptr) {
    lv_disp_flush_ready(flush_ctx_.disp);
  }

  const uint32_t elapsed_us = micros() - flush_ctx_.started_ms;
  graphics_stats_.flush_count += 1U;
  if (use_dma) {
    graphics_stats_.dma_flush_count += 1U;
  } else {
    graphics_stats_.sync_flush_count += 1U;
  }
  graphics_stats_.flush_time_total_us += elapsed_us;
  if (elapsed_us > graphics_stats_.flush_time_max_us) {
    graphics_stats_.flush_time_max_us = elapsed_us;
  }
  perfMonitor().noteUiFlush(use_dma, elapsed_us);
  flush_ctx_ = {};
  flush_pending_since_ms_ = 0U;
  flush_last_progress_ms_ = millis();
}

uint16_t UiManager::convertLineRgb332ToRgb565(const lv_color_t* src,
                                              uint16_t* dst,
                                              uint32_t px_count) const {
  if (src == nullptr || dst == nullptr || px_count == 0U || !color_lut_ready_) {
    return 0U;
  }
#if LV_COLOR_DEPTH == 8
  if (sizeof(lv_color_t) == sizeof(uint8_t)) {
    runtime::simd::simd_index8_to_rgb565(dst,
                                         reinterpret_cast<const uint8_t*>(src),
                                         rgb332_to_565_lut_,
                                         static_cast<size_t>(px_count));
    return static_cast<uint16_t>((px_count > 0xFFFFU) ? 0xFFFFU : px_count);
  }
  for (uint32_t index = 0U; index < px_count; ++index) {
    dst[index] = rgb332_to_565_lut_[src[index].full];
  }
#else
  for (uint32_t index = 0U; index < px_count; ++index) {
    dst[index] = src[index].full;
  }
#endif
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

void UiManager::invalidateFxOverlayObjects() {
  if (intro_active_ && intro_root_ != nullptr) {
    // Keep the LVGL overlay above FX by forcing an overlay redraw each rendered FX frame.
    lv_obj_invalidate(intro_root_);
    return;
  }

  bool invalidated = false;
  auto invalidate_if_visible = [&](lv_obj_t* obj) {
    if (obj == nullptr || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
      return;
    }
    lv_obj_invalidate(obj);
    invalidated = true;
  };

  if (intro_active_) {
    invalidate_if_visible(intro_logo_shadow_label_);
    invalidate_if_visible(intro_logo_label_);
    invalidate_if_visible(intro_crack_scroll_label_);
    invalidate_if_visible(intro_bottom_scroll_label_);
    invalidate_if_visible(intro_clean_title_shadow_label_);
    invalidate_if_visible(intro_clean_title_label_);
    invalidate_if_visible(intro_clean_scroll_label_);
    invalidate_if_visible(intro_debug_label_);
    if (!kUseWinEtapeSimplifiedEffects) {
      const uint8_t glyph_count =
          (intro_wave_glyph_count_ > kIntroWaveGlyphMax) ? kIntroWaveGlyphMax : intro_wave_glyph_count_;
      for (uint8_t index = 0U; index < glyph_count; ++index) {
        invalidate_if_visible(intro_wave_slots_[index].shadow);
        invalidate_if_visible(intro_wave_slots_[index].glyph);
      }
    }
  } else {
    if (!scene_disable_lvgl_text_) {
      invalidate_if_visible(scene_title_label_);
      invalidate_if_visible(scene_subtitle_label_);
      invalidate_if_visible(scene_symbol_label_);
    }
    invalidate_if_visible(page_label_);
  }

  if (!invalidated) {
    if (scene_disable_lvgl_text_ && !intro_active_) {
      drivers::display::displayHalInvalidateOverlay();
      return;
    }
    if (intro_root_ != nullptr) {
      lv_obj_invalidate(intro_root_);
      return;
    }
    if (scene_root_ != nullptr) {
      lv_obj_invalidate(scene_root_);
      return;
    }
    drivers::display::displayHalInvalidateOverlay();
  }
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
  if (self->isDisplayOutputBusy()) {
    self->pollAsyncFlush();
    if (self->isDisplayOutputBusy()) {
      self->graphics_stats_.flush_overflow_count += 1U;
      self->graphics_stats_.flush_blocked_count += 1U;
      self->pending_lvgl_flush_request_ = true;
      if (!self->pending_full_repaint_request_) {
        self->pending_full_repaint_request_ = true;
      }
      lv_disp_flush_ready(disp);
      return;
    }
  }
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
    if (!display.startWrite()) {
      self->graphics_stats_.flush_overflow_count += 1U;
      self->graphics_stats_.flush_blocked_count += 1U;
      self->pending_lvgl_flush_request_ = true;
      self->pending_full_repaint_request_ = true;
      lv_disp_flush_ready(disp);
      return;
    }
    display.pushImageDma(area->x1,
                         area->y1,
                         static_cast<int16_t>(width),
                         static_cast<int16_t>(height),
                         tx_pixels);
    const bool dma_done = display.waitDmaComplete(kLvglFlushDmaWaitUs);
    display.endWrite();

    const uint32_t elapsed_us = micros() - started_us;
    self->graphics_stats_.flush_count += 1U;
    self->graphics_stats_.dma_flush_count += 1U;
    self->graphics_stats_.flush_time_total_us += elapsed_us;
    if (elapsed_us > self->graphics_stats_.flush_time_max_us) {
      self->graphics_stats_.flush_time_max_us = elapsed_us;
    }
    if (!dma_done && self->async_flush_enabled_) {
      self->graphics_stats_.flush_stall_count += 1U;
      self->graphics_stats_.flush_recover_count += 1U;
      self->graphics_stats_.async_fallback_count += 1U;
      self->async_flush_enabled_ = false;
      self->buffer_cfg_.dma_enabled = false;
      self->async_fallback_until_ms_ = millis() + kAsyncFallbackRecoverMs;
      self->pending_lvgl_flush_request_ = true;
      self->pending_full_repaint_request_ = true;
    }
    perfMonitor().noteUiFlush(true, elapsed_us);
    self->flush_pending_since_ms_ = 0U;
    self->flush_last_progress_ms_ = millis();
    lv_disp_flush_ready(disp);
    return;
  }

  if (!display.startWrite()) {
    self->graphics_stats_.flush_overflow_count += 1U;
    self->graphics_stats_.flush_blocked_count += 1U;
    self->pending_lvgl_flush_request_ = true;
    self->pending_full_repaint_request_ = true;
    lv_disp_flush_ready(disp);
    return;
  }
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
  self->flush_pending_since_ms_ = 0U;
  self->flush_last_progress_ms_ = millis();
  lv_disp_flush_ready(disp);
}

#endif  // UI_MANAGER_SPLIT_IMPL
