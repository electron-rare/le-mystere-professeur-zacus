#include "ui/camera_capture/win311_camera_ui.h"

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cstring>

#if defined(ESP32) && __has_include("esp_heap_caps.h")
  #include "esp_heap_caps.h"
#endif

namespace ui::camera {

static lv_color_t C_GREY()   { return lv_color_hex(0xC0C0C0); }
static lv_color_t C_BLUE()   { return lv_color_hex(0x000080); }
static lv_color_t C_WHITE()  { return lv_color_hex(0xFFFFFF); }
static lv_color_t C_BLACK()  { return lv_color_hex(0x000000); }
static lv_color_t C_DARK()   { return lv_color_hex(0x404040); }
static lv_color_t C_MID()    { return lv_color_hex(0x808080); }

static void apply_btn_style(lv_obj_t* btn, lv_style_t* st_btn, lv_style_t* st_btn_pr)
{
  lv_obj_add_style(btn, st_btn, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_style(btn, st_btn_pr, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
}

static inline bool aligned16_(const void* p) {
  return (((uintptr_t)p) & 15u) == 0u;
}

bool Win311CameraUI::begin(const UiConfig& ui_cfg, const CameraCaptureService::Config& svc_cfg)
{
  ui_cfg_ = ui_cfg;
  if (ui_cfg_.parent == nullptr) ui_cfg_.parent = lv_layer_top();

  // Prepare service config.
  CameraCaptureService::Config cfg = svc_cfg;
  if (cfg.camera == nullptr) cfg.camera = ui_cfg_.camera;
  if (cfg.base_dir == nullptr) cfg.base_dir = ui_cfg_.base_dir;

  if (!service_.begin(cfg)) {
    return false;
  }

  auto align16 = [](void* p) -> void* {
    uintptr_t u = (uintptr_t)p;
    u = (u + 15u) & ~((uintptr_t)15u);
    return (void*)u;
  };

  // Allocate preview buffer (force 16-byte alignment for SIMD-friendly paths).
  const size_t preview_bytes = (size_t)ui_cfg_.preview_w * (size_t)ui_cfg_.preview_h * sizeof(uint16_t);
#if defined(ESP32) && __has_include("esp_heap_caps.h")
  preview_alloc_ = heap_caps_malloc(preview_bytes + 15u, MALLOC_CAP_8BIT);
#else
  preview_alloc_ = malloc(preview_bytes + 15u);
#endif
  preview_buf_ = preview_alloc_ ? (uint16_t*)align16(preview_alloc_) : nullptr;

  // Allocate thumbnail buffer (force 16-byte alignment).
  const size_t thumb_bytes = (size_t)ui_cfg_.thumb_w * (size_t)ui_cfg_.thumb_h * sizeof(uint16_t);
#if defined(ESP32) && __has_include("esp_heap_caps.h")
  thumb_alloc_ = heap_caps_malloc(thumb_bytes + 15u, MALLOC_CAP_8BIT);
#else
  thumb_alloc_ = malloc(thumb_bytes + 15u);
#endif
  thumb_buf_ = thumb_alloc_ ? (uint16_t*)align16(thumb_alloc_) : nullptr;

  if (!preview_buf_ || !thumb_buf_) {
    // UI can still exist, but without full features.
    return false;
  }

  // Prepare LVGL image descriptors.
  preview_dsc_ = {};
  preview_dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
  preview_dsc_.header.w = (uint32_t)ui_cfg_.preview_w;
  preview_dsc_.header.h = (uint32_t)ui_cfg_.preview_h;
  preview_dsc_.data = (const uint8_t*)preview_buf_;
  preview_dsc_.data_size = (uint32_t)preview_bytes;

  thumb_dsc_ = {};
  thumb_dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
  thumb_dsc_.header.w = (uint32_t)ui_cfg_.thumb_w;
  thumb_dsc_.header.h = (uint32_t)ui_cfg_.thumb_h;
  thumb_dsc_.data = (const uint8_t*)thumb_buf_;
  thumb_dsc_.data_size = (uint32_t)thumb_bytes;

  build_styles_();
  build_ui_();

  const int hz = (ui_cfg_.preview_hz <= 0) ? 10 : ui_cfg_.preview_hz;
  const uint32_t period_ms = (uint32_t)std::max(50, 1000 / hz);
  timer_ = lv_timer_create(timer_cb_, period_ms, this);

  set_frozen_(false);

  if (!ui_cfg_.start_visible) {
    hide();
  } else {
    show();
  }

  return true;
}

void Win311CameraUI::show()
{
  if (!win_) return;
  lv_obj_clear_flag(win_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(win_);
  set_status_("READY");
}

void Win311CameraUI::hide()
{
  if (!win_) return;
  service_.discard_frozen();
  set_frozen_(false);
  lv_obj_add_flag(win_, LV_OBJ_FLAG_HIDDEN);
}

void Win311CameraUI::toggle()
{
  if (visible()) hide(); else show();
}

bool Win311CameraUI::visible() const
{
  if (!win_) return false;
  return (lv_obj_has_flag(win_, LV_OBJ_FLAG_HIDDEN) == false);
}

void Win311CameraUI::build_styles_()
{
  // Window
  lv_style_init(&st_win_);
  lv_style_set_bg_color(&st_win_, C_GREY());
  lv_style_set_border_width(&st_win_, 2);
  lv_style_set_border_color(&st_win_, C_BLACK());
  lv_style_set_radius(&st_win_, 0);
  lv_style_set_pad_all(&st_win_, 0);

  // Title bar
  lv_style_init(&st_title_);
  lv_style_set_bg_color(&st_title_, C_BLUE());
  lv_style_set_text_color(&st_title_, C_WHITE());
  lv_style_set_radius(&st_title_, 0);
  lv_style_set_pad_left(&st_title_, 4);
  lv_style_set_pad_right(&st_title_, 4);
  lv_style_set_pad_top(&st_title_, 2);
  lv_style_set_pad_bottom(&st_title_, 2);

  // Buttons (raised)
  lv_style_init(&st_btn_);
  lv_style_set_bg_color(&st_btn_, C_GREY());
  lv_style_set_text_color(&st_btn_, C_BLACK());
  lv_style_set_radius(&st_btn_, 0);
  lv_style_set_border_width(&st_btn_, 1);
  lv_style_set_border_color(&st_btn_, C_BLACK());
  lv_style_set_shadow_width(&st_btn_, 0);
  lv_style_set_pad_left(&st_btn_, 6);
  lv_style_set_pad_right(&st_btn_, 6);
  lv_style_set_pad_top(&st_btn_, 3);
  lv_style_set_pad_bottom(&st_btn_, 3);

  // Pressed button (inset feel)
  lv_style_init(&st_btn_pr_);
  lv_style_set_bg_color(&st_btn_pr_, C_MID());
  lv_style_set_text_color(&st_btn_pr_, C_BLACK());
  lv_style_set_radius(&st_btn_pr_, 0);
  lv_style_set_border_width(&st_btn_pr_, 1);
  lv_style_set_border_color(&st_btn_pr_, C_BLACK());
  lv_style_set_translate_y(&st_btn_pr_, 1);

  // Sunken panel (preview frame)
  lv_style_init(&st_sunken_);
  lv_style_set_bg_color(&st_sunken_, C_GREY());
  lv_style_set_radius(&st_sunken_, 0);
  lv_style_set_border_width(&st_sunken_, 1);
  lv_style_set_border_color(&st_sunken_, C_DARK());

  // Status bar
  lv_style_init(&st_status_);
  lv_style_set_bg_color(&st_status_, C_GREY());
  lv_style_set_text_color(&st_status_, C_BLACK());
  lv_style_set_radius(&st_status_, 0);
  lv_style_set_border_width(&st_status_, 1);
  lv_style_set_border_color(&st_status_, C_DARK());
  lv_style_set_pad_left(&st_status_, 4);
  lv_style_set_pad_right(&st_status_, 4);
  lv_style_set_pad_top(&st_status_, 2);
  lv_style_set_pad_bottom(&st_status_, 2);
}

void Win311CameraUI::build_ui_()
{
  // Root window
  win_ = lv_obj_create(ui_cfg_.parent);
  lv_obj_add_style(win_, &st_win_, 0);
  lv_obj_set_size(win_, ui_cfg_.window_w, ui_cfg_.window_h);
  lv_obj_center(win_);
  lv_obj_clear_flag(win_, LV_OBJ_FLAG_SCROLLABLE);

  // Title bar
  titlebar_ = lv_obj_create(win_);
  lv_obj_add_style(titlebar_, &st_title_, 0);
  lv_obj_set_size(titlebar_, ui_cfg_.window_w, 20);
  lv_obj_align(titlebar_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(titlebar_, LV_OBJ_FLAG_SCROLLABLE);

  title_label_ = lv_label_create(titlebar_);
  lv_label_set_text(title_label_, ui_cfg_.title ? ui_cfg_.title : "CAMERA");
  lv_obj_align(title_label_, LV_ALIGN_LEFT_MID, 4, 0);

  btn_close_ = lv_btn_create(titlebar_);
  apply_btn_style(btn_close_, &st_btn_, &st_btn_pr_);
  lv_obj_set_size(btn_close_, 18, 16);
  lv_obj_align(btn_close_, LV_ALIGN_RIGHT_MID, -2, 0);
  lv_obj_add_event_cb(btn_close_, event_cb_, LV_EVENT_CLICKED, this);
  lv_obj_t* lblx = lv_label_create(btn_close_);
  lv_label_set_text(lblx, "X");
  lv_obj_center(lblx);

  // Layout constants
  const int pad = 6;
  const int content_y = 20 + pad;
  const int preview_x = pad;
  const int preview_y = content_y;

  // Preview frame (outer)
  frame_preview_outer_ = lv_obj_create(win_);
  lv_obj_add_style(frame_preview_outer_, &st_sunken_, 0);
  lv_obj_set_size(frame_preview_outer_, ui_cfg_.preview_w + 8, ui_cfg_.preview_h + 8);
  lv_obj_set_pos(frame_preview_outer_, preview_x, preview_y);
  lv_obj_clear_flag(frame_preview_outer_, LV_OBJ_FLAG_SCROLLABLE);

  // Inner (gives a second border = more Win3.11 feel)
  frame_preview_inner_ = lv_obj_create(frame_preview_outer_);
  lv_obj_add_style(frame_preview_inner_, &st_sunken_, 0);
  lv_obj_set_size(frame_preview_inner_, ui_cfg_.preview_w + 4, ui_cfg_.preview_h + 4);
  lv_obj_align(frame_preview_inner_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(frame_preview_inner_, LV_OBJ_FLAG_SCROLLABLE);

  img_preview_ = lv_img_create(frame_preview_inner_);
  lv_img_set_src(img_preview_, &preview_dsc_);
  lv_obj_set_size(img_preview_, ui_cfg_.preview_w, ui_cfg_.preview_h);
  lv_obj_align(img_preview_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(img_preview_, LV_OBJ_FLAG_CLICKABLE);

  label_no_preview_ = lv_label_create(frame_preview_inner_);
  lv_label_set_text(label_no_preview_, "NO PREVIEW");
  lv_obj_set_style_text_color(label_no_preview_, C_BLACK(), 0);
  lv_obj_align(label_no_preview_, LV_ALIGN_CENTER, 0, 0);

  // Right column buttons
  const int col_x = preview_x + (ui_cfg_.preview_w + 8) + pad;
  const int col_w = ui_cfg_.window_w - col_x - pad;
  const int btn_h = 22;
  const int btn_gap = 6;

  btn_snap_ = lv_btn_create(win_);
  apply_btn_style(btn_snap_, &st_btn_, &st_btn_pr_);
  lv_obj_set_size(btn_snap_, col_w, btn_h);
  lv_obj_set_pos(btn_snap_, col_x, preview_y);
  lv_obj_add_event_cb(btn_snap_, event_cb_, LV_EVENT_CLICKED, this);
  lbl_snap_ = lv_label_create(btn_snap_);
  lv_label_set_text(lbl_snap_, "SNAP");

  btn_save_ = lv_btn_create(win_);
  apply_btn_style(btn_save_, &st_btn_, &st_btn_pr_);
  lv_obj_set_size(btn_save_, col_w, btn_h);
  lv_obj_set_pos(btn_save_, col_x, preview_y + (btn_h + btn_gap) * 1);
  lv_obj_add_event_cb(btn_save_, event_cb_, LV_EVENT_CLICKED, this);
  lbl_save_ = lv_label_create(btn_save_);
  lv_label_set_text(lbl_save_, "SAVE");

  btn_gallery_ = lv_btn_create(win_);
  apply_btn_style(btn_gallery_, &st_btn_, &st_btn_pr_);
  lv_obj_set_size(btn_gallery_, col_w, btn_h);
  lv_obj_set_pos(btn_gallery_, col_x, preview_y + (btn_h + btn_gap) * 2);
  lv_obj_add_event_cb(btn_gallery_, event_cb_, LV_EVENT_CLICKED, this);
  lv_label_set_text(lv_label_create(btn_gallery_), "GALLERY");

  btn_delete_ = lv_btn_create(win_);
  apply_btn_style(btn_delete_, &st_btn_, &st_btn_pr_);
  lv_obj_set_size(btn_delete_, col_w, btn_h);
  lv_obj_set_pos(btn_delete_, col_x, preview_y + (btn_h + btn_gap) * 3);
  lv_obj_add_event_cb(btn_delete_, event_cb_, LV_EVENT_CLICKED, this);
  lv_label_set_text(lv_label_create(btn_delete_), "DELETE");

  // Disable delete until something selected
  lv_obj_add_state(btn_delete_, LV_STATE_DISABLED);

  // Info label under buttons
  info_label_ = lv_label_create(win_);
  lv_label_set_text(info_label_, "--");
  lv_obj_set_style_text_color(info_label_, C_BLACK(), 0);
  lv_obj_set_pos(info_label_, col_x, preview_y + (btn_h + btn_gap) * 4 + 4);

  // "Last shot" thumbnail
  const int last_y = preview_y + (btn_h + btn_gap) * 4 + 22;
  label_last_ = lv_label_create(win_);
  lv_label_set_text(label_last_, "LAST:");
  lv_obj_set_style_text_color(label_last_, C_BLACK(), 0);
  lv_obj_set_pos(label_last_, col_x, last_y);

  frame_thumb_outer_ = lv_obj_create(win_);
  lv_obj_add_style(frame_thumb_outer_, &st_sunken_, 0);
  lv_obj_set_size(frame_thumb_outer_, ui_cfg_.thumb_w + 8, ui_cfg_.thumb_h + 8);
  lv_obj_set_pos(frame_thumb_outer_, col_x, last_y + 14);
  lv_obj_clear_flag(frame_thumb_outer_, LV_OBJ_FLAG_SCROLLABLE);

  frame_thumb_inner_ = lv_obj_create(frame_thumb_outer_);
  lv_obj_add_style(frame_thumb_inner_, &st_sunken_, 0);
  lv_obj_set_size(frame_thumb_inner_, ui_cfg_.thumb_w + 4, ui_cfg_.thumb_h + 4);
  lv_obj_align(frame_thumb_inner_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(frame_thumb_inner_, LV_OBJ_FLAG_SCROLLABLE);

  img_thumb_ = lv_img_create(frame_thumb_inner_);
  lv_img_set_src(img_thumb_, &thumb_dsc_);
  lv_obj_set_size(img_thumb_, ui_cfg_.thumb_w, ui_cfg_.thumb_h);
  lv_obj_align(img_thumb_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(img_thumb_, LV_OBJ_FLAG_CLICKABLE);

  label_no_thumb_ = lv_label_create(frame_thumb_inner_);
  lv_label_set_text(label_no_thumb_, "(none)");
  lv_obj_set_style_text_color(label_no_thumb_, C_BLACK(), 0);
  lv_obj_align(label_no_thumb_, LV_ALIGN_CENTER, 0, 0);

  // Status bar (bottom)
  statusbar_ = lv_obj_create(win_);
  lv_obj_add_style(statusbar_, &st_status_, 0);
  lv_obj_set_size(statusbar_, ui_cfg_.window_w - 2, 18);
  lv_obj_align(statusbar_, LV_ALIGN_BOTTOM_MID, 0, -1);
  lv_obj_clear_flag(statusbar_, LV_OBJ_FLAG_SCROLLABLE);

  status_label_ = lv_label_create(statusbar_);
  lv_label_set_text(status_label_, "READY");
  lv_obj_align(status_label_, LV_ALIGN_LEFT_MID, 2, 0);

  refresh_sensor_info_();
}

void Win311CameraUI::refresh_sensor_info_()
{
  int w = 0, h = 0;
  if (service_.get_sensor_size(w, h)) {
    char buf[64];
    snprintf(buf, sizeof(buf), "SENSOR %dx%d", w, h);
    lv_label_set_text(info_label_, buf);
  } else {
    lv_label_set_text(info_label_, "SENSOR ?");
  }
}

void Win311CameraUI::set_status_(const char* fmt, ...)
{
  if (!status_label_ || !fmt) return;
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  lv_label_set_text(status_label_, buf);
}

void Win311CameraUI::flash_preview_()
{
  if (!frame_preview_inner_) return;

  lv_obj_t* flash = lv_obj_create(frame_preview_inner_);
  lv_obj_set_size(flash, ui_cfg_.preview_w, ui_cfg_.preview_h);
  lv_obj_align(flash, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(flash, C_WHITE(), 0);
  lv_obj_set_style_bg_opa(flash, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(flash, 0, 0);
  lv_obj_clear_flag(flash, LV_OBJ_FLAG_SCROLLABLE);

  // Fade out
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, flash);
  lv_anim_set_time(&a, 180);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
  });
  lv_anim_set_ready_cb(&a, [](lv_anim_t* anim) {
    lv_obj_del((lv_obj_t*)anim->var);
  });
  lv_anim_start(&a);
}

void Win311CameraUI::set_frozen_(bool en)
{
  frozen_ = en;

  // SAVE only works when frozen.
  if (btn_save_) {
    if (frozen_) lv_obj_clear_state(btn_save_, LV_STATE_DISABLED);
    else lv_obj_add_state(btn_save_, LV_STATE_DISABLED);
  }

  // SNAP becomes LIVE when frozen.
  if (lbl_snap_) {
    lv_label_set_text(lbl_snap_, frozen_ ? "LIVE" : "SNAP");
  }
}

void Win311CameraUI::update_thumb_from_preview_()
{
  if (!thumb_buf_ || !preview_buf_) return;

  const bool hint = aligned16_(thumb_buf_) && aligned16_(preview_buf_);

  CameraCaptureService::downscale_rgb565_nearest(
      preview_buf_, ui_cfg_.preview_w, ui_cfg_.preview_h, ui_cfg_.preview_w,
      thumb_buf_, ui_cfg_.thumb_w, ui_cfg_.thumb_h,
      hint);

  if (label_no_thumb_) lv_obj_add_flag(label_no_thumb_, LV_OBJ_FLAG_HIDDEN);
  if (img_thumb_) lv_obj_invalidate(img_thumb_);
}

void Win311CameraUI::update_preview_()
{
  if (!preview_buf_) return;

  const bool ok = service_.update_preview_rgb565(preview_buf_, ui_cfg_.preview_w, ui_cfg_.preview_h);

  if (ok) {
    lv_obj_add_flag(label_no_preview_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(label_no_preview_, LV_OBJ_FLAG_HIDDEN);
  }

  // Force redraw
  if (img_preview_) lv_obj_invalidate(img_preview_);
}

void Win311CameraUI::rebuild_gallery_()
{
  if (!win_) return;

  // Create a modal "File list" window the first time.
  if (!list_gallery_) {
    // Modal container (looks like a small dialog)
    lv_obj_t* dlg = lv_obj_create(win_);
    lv_obj_add_style(dlg, &st_win_, 0);
    lv_obj_set_size(dlg, ui_cfg_.window_w - 40, ui_cfg_.window_h - 60);
    lv_obj_center(dlg);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t* tb = lv_obj_create(dlg);
    lv_obj_add_style(tb, &st_title_, 0);
    lv_obj_set_size(tb, lv_obj_get_width(dlg), 20);
    lv_obj_align(tb, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* tl = lv_label_create(tb);
    lv_label_set_text(tl, "PHOTOS");
    lv_obj_align(tl, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* close = lv_btn_create(tb);
    apply_btn_style(close, &st_btn_, &st_btn_pr_);
    lv_obj_set_size(close, 18, 16);
    lv_obj_align(close, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_add_event_cb(close, event_cb_, LV_EVENT_CLICKED, this);
    lv_label_set_text(lv_label_create(close), "X");

    // List
    list_gallery_ = lv_list_create(dlg);
    lv_obj_set_size(list_gallery_, lv_obj_get_width(dlg) - 12, lv_obj_get_height(dlg) - 20 - 34);
    lv_obj_set_pos(list_gallery_, 6, 24);

    // Delete button
    lv_obj_t* del = lv_btn_create(dlg);
    apply_btn_style(del, &st_btn_, &st_btn_pr_);
    lv_obj_set_size(del, 80, 22);
    lv_obj_align(del, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    lv_obj_add_event_cb(del, event_cb_, LV_EVENT_CLICKED, this);
    lv_label_set_text(lv_label_create(del), "DELETE");

    // Close button
    lv_obj_t* ok = lv_btn_create(dlg);
    apply_btn_style(ok, &st_btn_, &st_btn_pr_);
    lv_obj_set_size(ok, 80, 22);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
    lv_obj_add_event_cb(ok, event_cb_, LV_EVENT_CLICKED, this);
    lv_label_set_text(lv_label_create(ok), "CLOSE");

    // Make sure dialog is on top.
    lv_obj_move_foreground(dlg);
  } else {
    // Show existing dialog
    lv_obj_t* dlg = lv_obj_get_parent(list_gallery_);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dlg);
  }

  // Populate list
  lv_obj_clean(list_gallery_);

  String items[64];
  const int n = service_.list_photos(items, 64, true);
  for (int i = 0; i < n; i++) {
    lv_obj_t* b = lv_list_add_btn(list_gallery_, nullptr, items[i].c_str());
    lv_obj_add_event_cb(b, event_cb_, LV_EVENT_CLICKED, this);
  }

  if (n == 0) {
    lv_list_add_text(list_gallery_, "(empty)");
  }
}

bool Win311CameraUI::handleInputAction(InputAction action)
{
  if (!visible()) {
    return false;
  }
  switch (action) {
    case InputAction::kSnapToggle:
      on_button_(btn_snap_);
      return true;
    case InputAction::kSave:
      on_button_(btn_save_);
      return true;
    case InputAction::kGalleryToggle:
      on_button_(btn_gallery_);
      return true;
    case InputAction::kGalleryNext:
      if (service_.select_next_photo(&selected_path_)) {
        set_status_("SELECTED %s", selected_path_.c_str());
        if (btn_delete_ != nullptr) {
          lv_obj_clear_state(btn_delete_, LV_STATE_DISABLED);
        }
      } else {
        set_status_("GALLERY EMPTY");
      }
      return true;
    case InputAction::kDeleteSelected:
      on_button_(btn_delete_);
      return true;
    case InputAction::kClose:
      hide();
      return true;
    default:
      return false;
  }
}

void Win311CameraUI::on_button_(lv_obj_t* btn)
{
  if (!btn) return;

  // Close main window
  if (btn == btn_close_) {
    hide();
    return;
  }

  // SNAP = freeze / LIVE
  if (btn == btn_snap_) {
    if (!frozen_) {
      const bool ok = service_.snap_freeze(preview_buf_, ui_cfg_.preview_w, ui_cfg_.preview_h);
      if (ok) {
        set_frozen_(true);
        set_status_("FROZEN - PRESS SAVE");
        flash_preview_();
      } else {
        set_status_("SNAP FAILED");
      }
    } else {
      service_.discard_frozen();
      set_frozen_(false);
      set_status_("LIVE");
    }
    return;
  }

  // SAVE = writes frozen frame
  if (btn == btn_save_) {
    if (!frozen_) {
      set_status_("SNAP FIRST");
      return;
    }

    String path;
    const bool ok = service_.save_frozen(path, CameraCaptureService::CaptureFormat::Bmp24);
    if (ok) {
      last_saved_path_ = path;
      set_status_("SAVED %s", path.c_str());
      update_thumb_from_preview_();
      flash_preview_();
      // Return to live immediately (classic workflow)
      set_frozen_(false);
    } else {
      set_status_("SAVE FAILED");
    }
    return;
  }

  // Gallery: open dialog
  if (btn == btn_gallery_) {
    rebuild_gallery_();
    return;
  }

  // Delete (main): deletes selected from gallery if any, else last saved
  if (btn == btn_delete_) {
    const String target = (selected_path_.length() ? selected_path_ : last_saved_path_);
    if (target.length() == 0) {
      set_status_("NOTHING TO DELETE");
      return;
    }

    const bool ok = service_.remove_file(target.c_str());
    if (ok) {
      set_status_("DELETED %s", target.c_str());
      selected_path_ = "";
      // If gallery dialog open, refresh
      if (list_gallery_) {
        rebuild_gallery_();
      }
    } else {
      set_status_("DELETE FAILED");
    }
    return;
  }

  // Gallery dialog buttons & list items
  if (list_gallery_) {
    lv_obj_t* parent1 = lv_obj_get_parent(btn);
    lv_obj_t* parent2 = parent1 ? lv_obj_get_parent(parent1) : nullptr;

    // List item clicked
    if (parent2 == list_gallery_) {
      const char* txt = lv_list_get_btn_text(list_gallery_, btn);
      if (txt && txt[0]) {
        selected_path_ = txt;
        set_status_("SELECTED %s", selected_path_.c_str());
        lv_obj_clear_state(btn_delete_, LV_STATE_DISABLED);
      }
      return;
    }

    // Dialog buttons (identify by their label)
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
      const char* t = lv_label_get_text(lbl);
      if (t) {
        if (strcmp(t, "CLOSE") == 0) {
          lv_obj_t* dlg = lv_obj_get_parent(list_gallery_);
          if (dlg) lv_obj_add_flag(dlg, LV_OBJ_FLAG_HIDDEN);
          return;
        }
        if (strcmp(t, "DELETE") == 0) {
          if (selected_path_.length() == 0) {
            set_status_("SELECT A FILE");
            return;
          }
          const bool ok = service_.remove_file(selected_path_.c_str());
          if (ok) {
            set_status_("DELETED %s", selected_path_.c_str());
            selected_path_ = "";
            rebuild_gallery_();
          } else {
            set_status_("DELETE FAILED");
          }
          return;
        }
        if (strcmp(t, "X") == 0) {
          lv_obj_t* dlg = lv_obj_get_parent(list_gallery_);
          if (dlg) lv_obj_add_flag(dlg, LV_OBJ_FLAG_HIDDEN);
          return;
        }
      }
    }
  }
}

void Win311CameraUI::timer_cb_(lv_timer_t* t)
{
  Win311CameraUI* self = static_cast<Win311CameraUI*>(t->user_data);
  if (!self || !self->visible()) return;

  if (!self->frozen_) {
    self->update_preview_();
  }

  // Refresh sensor info occasionally
  static uint32_t ctr = 0;
  ctr++;
  if ((ctr % 10u) == 0u) {
    self->refresh_sensor_info_();
  }
}

void Win311CameraUI::event_cb_(lv_event_t* e)
{
  Win311CameraUI* self = static_cast<Win311CameraUI*>(lv_event_get_user_data(e));
  if (!self) return;

  lv_obj_t* target = lv_event_get_target(e);
  if (!target) return;

  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    self->on_button_(target);
  }
}

}  // namespace ui::camera
