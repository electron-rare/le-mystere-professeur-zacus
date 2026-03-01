#pragma once

#include "ui/camera_capture/camera_capture_service.h"

#include <Arduino.h>
#include <FS.h>

// LVGL include (project usually provides it)
#include "lvgl.h"

namespace ui::camera {

class Win311CameraUI {
public:
  enum class InputAction : uint8_t {
    kSnapToggle = 0,
    kSave,
    kGalleryToggle,
    kGalleryNext,
    kDeleteSelected,
    kClose,
  };

  struct UiConfig {
    lv_obj_t* parent = nullptr;  // default: lv_layer_top()
    bool start_visible = false;

    // Window size (for 320x240 you can keep defaults)
    int window_w = 316;
    int window_h = 236;

    // Preview area (RGB565)
    int preview_w = 220;
    int preview_h = 160;

    // Thumbnail of last saved photo (RGB565)
    int thumb_w = 64;
    int thumb_h = 48;

    // Preview refresh (Hz)
    int preview_hz = 10;

    // Filesystem + base dir where photos are saved
    fs::FS* fs = nullptr;
    const char* base_dir = "/picture";
    CameraManager* camera = nullptr;

    const char* title = "CAMERA";

    // If true, UI captures arrow/enter/esc when visible.
    bool capture_keys_when_visible = true;
  };

  bool begin(const UiConfig& ui_cfg, const CameraCaptureService::Config& svc_cfg = {});

  void show();
  void hide();
  void toggle();
  bool visible() const;
  bool handleInputAction(InputAction action);

  CameraCaptureService& service() { return service_; }
  const CameraCaptureService& service() const { return service_; }

private:
  UiConfig ui_cfg_{};
  CameraCaptureService service_{};

  // Styles
  lv_style_t st_win_{};
  lv_style_t st_title_{};
  lv_style_t st_btn_{};
  lv_style_t st_btn_pr_{};
  lv_style_t st_sunken_{};
  lv_style_t st_status_{};

  // Objects
  lv_obj_t* win_ = nullptr;
  lv_obj_t* titlebar_ = nullptr;
  lv_obj_t* title_label_ = nullptr;
  lv_obj_t* btn_close_ = nullptr;

  lv_obj_t* frame_preview_outer_ = nullptr;
  lv_obj_t* frame_preview_inner_ = nullptr;
  lv_obj_t* img_preview_ = nullptr;
  lv_obj_t* label_no_preview_ = nullptr;

  // Thumbnail
  lv_obj_t* label_last_ = nullptr;
  lv_obj_t* frame_thumb_outer_ = nullptr;
  lv_obj_t* frame_thumb_inner_ = nullptr;
  lv_obj_t* img_thumb_ = nullptr;
  lv_obj_t* label_no_thumb_ = nullptr;

  lv_obj_t* btn_snap_ = nullptr;
  lv_obj_t* btn_save_ = nullptr;
  lv_obj_t* btn_gallery_ = nullptr;
  lv_obj_t* btn_delete_ = nullptr;

  lv_obj_t* lbl_snap_ = nullptr;
  lv_obj_t* lbl_save_ = nullptr;

  lv_obj_t* list_gallery_ = nullptr;
  lv_obj_t* statusbar_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* info_label_ = nullptr;

  lv_timer_t* timer_ = nullptr;
  // Raw allocations (for manual 16-byte alignment)
  void* preview_alloc_ = nullptr;
  void* thumb_alloc_ = nullptr;


  uint16_t* preview_buf_ = nullptr;
  lv_img_dsc_t preview_dsc_{};

  uint16_t* thumb_buf_ = nullptr;
  lv_img_dsc_t thumb_dsc_{};

  String selected_path_;
  String last_saved_path_;
  bool frozen_ = false;

  void build_styles_();
  void build_ui_();

  void update_preview_();
  void refresh_sensor_info_();
  void rebuild_gallery_();

  void set_status_(const char* fmt, ...);
  void flash_preview_();

  void update_thumb_from_preview_();
  void set_frozen_(bool en);

  void on_button_(lv_obj_t* btn);

  static void timer_cb_(lv_timer_t* t);
  static void event_cb_(lv_event_t* e);
};

}  // namespace ui::camera
