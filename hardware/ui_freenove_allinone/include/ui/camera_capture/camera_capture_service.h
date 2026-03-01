#pragma once

#include <cstdint>

#include <Arduino.h>

#include "camera_manager.h"

namespace ui::camera {

class CameraCaptureService {
 public:
  enum class CaptureFormat : uint8_t {
    Auto = 0,
    Jpeg,
    Bmp24,
    RawRGB565,
  };

  struct Config {
    CameraManager* camera = nullptr;
    const char* base_dir = "/picture";
  };

  bool begin(const Config& cfg);
  bool ready() const;

  bool update_preview_rgb565(uint16_t* dst, int dst_w, int dst_h);

  bool snap_freeze(uint16_t* preview_dst, int preview_w, int preview_h);
  bool has_frozen() const;
  bool save_frozen(String& out_path, CaptureFormat fmt = CaptureFormat::Auto);
  void discard_frozen();

  bool capture_next(String& out_path, CaptureFormat fmt = CaptureFormat::Auto);

  int list_photos(String* out, int max_items, bool newest_first = true) const;
  bool remove_file(const char* path);
  bool select_next_photo(String* in_out_path) const;
  bool get_sensor_size(int& w, int& h) const;

  const Config& config() const { return cfg_; }

  static void downscale_rgb565_nearest(const uint16_t* src,
                                        int src_w,
                                        int src_h,
                                        int src_stride_px,
                                        uint16_t* dst,
                                        int dst_w,
                                        int dst_h,
                                        bool aligned16_hint);

 private:
  static CameraManager::RecorderSaveFormat toRecorderFormat(CaptureFormat format);

  Config cfg_ = {};
};

}  // namespace ui::camera
