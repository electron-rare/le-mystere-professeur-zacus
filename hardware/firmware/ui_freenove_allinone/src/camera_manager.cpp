// camera_manager.cpp - camera lifecycle + JPEG snapshots.
#include "camera_manager.h"

#include <FS.h>
#include <LittleFS.h>
#include <cctype>
#include <cstring>

#include "ui_freenove_config.h"

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<esp_camera.h>) && FREENOVE_CAM_ENABLE
#include <esp_camera.h>
#define ZACUS_HAS_CAMERA 1
#else
#define ZACUS_HAS_CAMERA 0
#endif

namespace {

void copyText(char* out, size_t out_size, const char* text) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (text == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, text, out_size - 1U);
  out[out_size - 1U] = '\0';
}

String normalizeDir(const char* dir) {
  if (dir == nullptr || dir[0] == '\0') {
    return String("/picture");
  }
  String normalized = dir;
  normalized.trim();
  if (normalized.isEmpty()) {
    normalized = "/picture";
  }
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  if (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1U);
  }
  if (normalized.isEmpty()) {
    normalized = "/picture";
  }
  return normalized;
}

String sanitizeFileBasename(const char* filename_hint) {
  String base = (filename_hint != nullptr) ? filename_hint : "";
  base.trim();
  if (base.isEmpty()) {
    base = "story_";
    base += String(static_cast<unsigned long>(millis()));
  }

  // Keep names portable across FS/web flows.
  for (size_t index = 0U; index < base.length(); ++index) {
    const char ch = base[index];
    const bool keep = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == '.';
    if (!keep) {
      base.setCharAt(index, '_');
    }
  }
  if (!base.endsWith(".jpg") && !base.endsWith(".jpeg")) {
    base += ".jpg";
  }
  return base;
}

#if ZACUS_HAS_CAMERA
framesize_t frameSizeFromText(const char* text) {
  if (text == nullptr || text[0] == '\0') {
    return FRAMESIZE_VGA;
  }
  char normalized[20] = {0};
  std::strncpy(normalized, text, sizeof(normalized) - 1U);
  for (size_t index = 0U; normalized[index] != '\0'; ++index) {
    normalized[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(normalized[index])));
  }
  if (std::strcmp(normalized, "QQVGA") == 0) {
    return FRAMESIZE_QQVGA;
  }
  if (std::strcmp(normalized, "HQVGA") == 0) {
    return FRAMESIZE_HQVGA;
  }
  if (std::strcmp(normalized, "QVGA") == 0) {
    return FRAMESIZE_QVGA;
  }
  if (std::strcmp(normalized, "CIF") == 0) {
    return FRAMESIZE_CIF;
  }
  if (std::strcmp(normalized, "VGA") == 0) {
    return FRAMESIZE_VGA;
  }
  if (std::strcmp(normalized, "SVGA") == 0) {
    return FRAMESIZE_SVGA;
  }
  if (std::strcmp(normalized, "XGA") == 0) {
    return FRAMESIZE_XGA;
  }
  if (std::strcmp(normalized, "SXGA") == 0) {
    return FRAMESIZE_SXGA;
  }
  if (std::strcmp(normalized, "UXGA") == 0) {
    return FRAMESIZE_UXGA;
  }
  if (std::strcmp(normalized, "HD") == 0) {
    return FRAMESIZE_HD;
  }
  if (std::strcmp(normalized, "FHD") == 0) {
    return FRAMESIZE_FHD;
  }
  return FRAMESIZE_VGA;
}
#endif

}  // namespace

CameraManager::CameraManager() {
  snapshot_.supported = (ZACUS_HAS_CAMERA != 0);
}

bool CameraManager::begin(const Config& config) {
  config_ = config;
  copyText(config_.snapshot_dir, sizeof(config_.snapshot_dir), normalizeDir(config.snapshot_dir).c_str());
  if (config_.jpeg_quality < 4U) {
    config_.jpeg_quality = 4U;
  }
  if (config_.jpeg_quality > 63U) {
    config_.jpeg_quality = 63U;
  }
  if (config_.fb_count == 0U) {
    config_.fb_count = 1U;
  }
  if (config_.fb_count > 2U) {
    config_.fb_count = 2U;
  }
  if (config_.xclk_hz < 1000000UL) {
    config_.xclk_hz = 10000000UL;
  }

  snapshot_.supported = (ZACUS_HAS_CAMERA != 0);
  snapshot_.enabled = false;
  snapshot_.initialized = false;
  snapshot_.last_snapshot_ok = false;
  snapshot_.capture_count = 0U;
  snapshot_.fail_count = 0U;
  snapshot_.last_capture_ms = 0U;
  snapshot_.width = 0U;
  snapshot_.height = 0U;
  snapshot_.jpeg_quality = config_.jpeg_quality;
  snapshot_.fb_count = config_.fb_count;
  snapshot_.xclk_hz = config_.xclk_hz;
  copyText(snapshot_.frame_size, sizeof(snapshot_.frame_size), config_.frame_size);
  copyText(snapshot_.snapshot_dir, sizeof(snapshot_.snapshot_dir), config_.snapshot_dir);
  snapshot_.last_file[0] = '\0';
  snapshot_.last_error[0] = '\0';
  return true;
}

bool CameraManager::start() {
  snapshot_.enabled = false;
#if ZACUS_HAS_CAMERA
  if (snapshot_.initialized) {
    snapshot_.enabled = true;
    return true;
  }

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = FREENOVE_CAM_Y2;
  cfg.pin_d1 = FREENOVE_CAM_Y3;
  cfg.pin_d2 = FREENOVE_CAM_Y4;
  cfg.pin_d3 = FREENOVE_CAM_Y5;
  cfg.pin_d4 = FREENOVE_CAM_Y6;
  cfg.pin_d5 = FREENOVE_CAM_Y7;
  cfg.pin_d6 = FREENOVE_CAM_Y8;
  cfg.pin_d7 = FREENOVE_CAM_Y9;
  cfg.pin_xclk = FREENOVE_CAM_XCLK;
  cfg.pin_pclk = FREENOVE_CAM_PCLK;
  cfg.pin_vsync = FREENOVE_CAM_VSYNC;
  cfg.pin_href = FREENOVE_CAM_HREF;
  cfg.pin_sscb_sda = FREENOVE_CAM_SIOD;
  cfg.pin_sscb_scl = FREENOVE_CAM_SIOC;
  cfg.pin_pwdn = FREENOVE_CAM_PWDN;
  cfg.pin_reset = FREENOVE_CAM_RESET;
  cfg.xclk_freq_hz = config_.xclk_hz;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = frameSizeFromText(config_.frame_size);
  cfg.jpeg_quality = config_.jpeg_quality;
  cfg.fb_count = config_.fb_count;
#if defined(CAMERA_GRAB_LATEST)
  cfg.grab_mode = CAMERA_GRAB_LATEST;
#endif
#if defined(CAMERA_FB_IN_PSRAM)
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
#endif

  const esp_err_t status = esp_camera_init(&cfg);
  if (status != ESP_OK) {
    setLastError("camera_init_failed");
    Serial.printf("[CAM] init failed err=0x%x\n", static_cast<unsigned int>(status));
    return false;
  }

  snapshot_.initialized = true;
  snapshot_.enabled = true;
  clearLastError();
  Serial.printf("[CAM] ready frame=%s quality=%u fb=%u xclk=%lu\n",
                snapshot_.frame_size,
                static_cast<unsigned int>(snapshot_.jpeg_quality),
                static_cast<unsigned int>(snapshot_.fb_count),
                static_cast<unsigned long>(snapshot_.xclk_hz));
  return true;
#else
  setLastError("camera_not_supported");
  return false;
#endif
}

void CameraManager::stop() {
#if ZACUS_HAS_CAMERA
  if (snapshot_.initialized) {
    esp_camera_deinit();
  }
#endif
  snapshot_.initialized = false;
  snapshot_.enabled = false;
}

bool CameraManager::isEnabled() const {
  return snapshot_.enabled;
}

bool CameraManager::ensureSnapshotDir() {
  String dir = normalizeDir(config_.snapshot_dir);
  copyText(config_.snapshot_dir, sizeof(config_.snapshot_dir), dir.c_str());
  copyText(snapshot_.snapshot_dir, sizeof(snapshot_.snapshot_dir), dir.c_str());
  if (LittleFS.exists(dir.c_str())) {
    return true;
  }
  if (LittleFS.mkdir(dir.c_str())) {
    return true;
  }
  setLastError("snapshot_dir_error");
  return false;
}

String CameraManager::buildSnapshotPath(const char* filename_hint) const {
  String dir = normalizeDir(config_.snapshot_dir);
  String file = sanitizeFileBasename(filename_hint);
  if (file.startsWith("/")) {
    return file;
  }
  return dir + "/" + file;
}

bool CameraManager::snapshotToFile(const char* filename_hint, String* out_path) {
  if (out_path != nullptr) {
    out_path->remove(0);
  }
  if (!start()) {
    ++snapshot_.fail_count;
    return false;
  }
  if (!ensureSnapshotDir()) {
    ++snapshot_.fail_count;
    return false;
  }

#if ZACUS_HAS_CAMERA
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    ++snapshot_.fail_count;
    setLastError("camera_capture_failed");
    return false;
  }

  const String path = buildSnapshotPath(filename_hint);
  File file = LittleFS.open(path.c_str(), "w");
  if (!file) {
    esp_camera_fb_return(frame);
    ++snapshot_.fail_count;
    setLastError("snapshot_write_failed");
    return false;
  }
  const size_t written = file.write(frame->buf, frame->len);
  file.close();
  if (written != frame->len) {
    esp_camera_fb_return(frame);
    ++snapshot_.fail_count;
    setLastError("snapshot_write_incomplete");
    return false;
  }

  snapshot_.last_snapshot_ok = true;
  snapshot_.last_capture_ms = millis();
  ++snapshot_.capture_count;
  snapshot_.width = frame->width;
  snapshot_.height = frame->height;
  copyText(snapshot_.last_file, sizeof(snapshot_.last_file), path.c_str());
  clearLastError();
  if (out_path != nullptr) {
    *out_path = path;
  }
  esp_camera_fb_return(frame);
  return true;
#else
  (void)filename_hint;
  ++snapshot_.fail_count;
  setLastError("camera_not_supported");
  return false;
#endif
}

CameraManager::Snapshot CameraManager::snapshot() const {
  return snapshot_;
}

void CameraManager::setLastError(const char* message) {
  copyText(snapshot_.last_error, sizeof(snapshot_.last_error), message);
}

void CameraManager::clearLastError() {
  snapshot_.last_error[0] = '\0';
}
