// camera_manager.h - minimal esp_camera wrapper for snapshots.
#pragma once

#include <Arduino.h>
#include <vector>

class CameraManager {
 public:
  enum class RecorderSaveFormat : uint8_t {
    kAuto = 0,
    kBmp24,
    kJpeg,
    kRawRgb565,
  };

  struct Config {
    bool enabled_on_boot = false;
    char frame_size[16] = "VGA";
    uint8_t jpeg_quality = 12U;
    uint8_t fb_count = 1U;
    uint32_t xclk_hz = 20000000UL;
    char snapshot_dir[32] = "/picture";
  };

  struct Snapshot {
    bool supported = false;
    bool enabled = false;
    bool initialized = false;
    bool last_snapshot_ok = false;
    uint32_t capture_count = 0U;
    uint32_t fail_count = 0U;
    uint32_t last_capture_ms = 0U;
    uint16_t width = 0U;
    uint16_t height = 0U;
    uint8_t jpeg_quality = 12U;
    uint8_t fb_count = 1U;
    uint32_t xclk_hz = 20000000UL;
    char frame_size[16] = "VGA";
    char snapshot_dir[32] = "/picture";
    char last_file[96] = "";
    char last_error[64] = "";
    bool recorder_session_active = false;
    bool recorder_frozen = false;
    uint16_t recorder_preview_width = 0U;
    uint16_t recorder_preview_height = 0U;
    char recorder_selected_file[96] = "";
  };

  CameraManager();
  ~CameraManager() = default;
  CameraManager(const CameraManager&) = delete;
  CameraManager& operator=(const CameraManager&) = delete;
  CameraManager(CameraManager&&) = delete;
  CameraManager& operator=(CameraManager&&) = delete;

  bool begin(const Config& config);
  bool start();
  void stop();
  bool isEnabled() const;
  bool snapshotToFile(const char* filename_hint, String* out_path);
  bool startRecorderSession();
  void stopRecorderSession();
  bool recorderSessionActive() const;
  bool recorderSnapFreeze(uint16_t* preview_dst, int preview_w, int preview_h);
  bool recorderHasFrozen() const;
  bool recorderSaveFrozen(String* out_path, RecorderSaveFormat format);
  void recorderDiscardFrozen();
  bool recorderUpdatePreviewRgb565(uint16_t* dst, int dst_w, int dst_h);
  int recorderListPhotos(String* out, int max_items, bool newest_first = true) const;
  bool recorderRemoveFile(const char* path);
  bool recorderSelectNextPhoto(String* in_out_path) const;
  Snapshot snapshot() const;

 private:
  void setLastError(const char* message);
  void clearLastError();
  bool ensureSnapshotDir();
  String buildSnapshotPath(const char* filename_hint) const;
  bool initCameraForMode(bool recorder_mode);
  bool saveRgb565AsBmp24(const char* path, const uint16_t* rgb565, int w, int h, int stride_px);
  bool saveRgb565Raw(const char* path, const uint16_t* rgb565, int w, int h, int stride_px);
  bool downscaleToRgb565(const uint16_t* src,
                         int src_w,
                         int src_h,
                         int src_stride_px,
                         uint16_t* dst,
                         int dst_w,
                         int dst_h) const;
  void ensurePreviewMap(int src_w, int src_h, int dst_w, int dst_h) const;
  static bool isPhotoExtension(const String& name);
  static RecorderSaveFormat parseSaveFormatToken(const char* token);

  Config config_;
  Snapshot snapshot_;
  bool recorder_mode_ = false;
  bool recorder_frozen_ = false;
  void* recorder_frozen_fb_ = nullptr;
  mutable int preview_map_src_w_ = 0;
  mutable int preview_map_src_h_ = 0;
  mutable int preview_map_dst_w_ = 0;
  mutable int preview_map_dst_h_ = 0;
  mutable std::vector<uint16_t> preview_x_map_ = {};
  mutable std::vector<uint16_t> preview_y_map_ = {};
};
