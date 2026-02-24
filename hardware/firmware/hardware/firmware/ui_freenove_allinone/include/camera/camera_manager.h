// camera_manager.h - minimal esp_camera wrapper for snapshots.
#pragma once

#include <Arduino.h>

class CameraManager {
 public:
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
  };

  CameraManager();

  bool begin(const Config& config);
  bool start();
  void stop();
  bool isEnabled() const;
  bool snapshotToFile(const char* filename_hint, String* out_path);
  Snapshot snapshot() const;

 private:
  void setLastError(const char* message);
  void clearLastError();
  bool ensureSnapshotDir();
  String buildSnapshotPath(const char* filename_hint) const;

  Config config_;
  Snapshot snapshot_;
};
