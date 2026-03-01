// camera_manager.cpp - camera lifecycle + JPEG snapshots + recorder session.
#include "camera_manager.h"

#include <FS.h>
#include <LittleFS.h>

#include <algorithm>
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
  while (normalized.length() > 1U && normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1U);
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
  for (size_t index = 0U; index < base.length(); ++index) {
    const char ch = base[index];
    const bool keep = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == '.';
    if (!keep) {
      base.setCharAt(index, '_');
    }
  }
  return base;
}

bool hasSuffix(const char* text, const char* suffix) {
  if (text == nullptr || suffix == nullptr) {
    return false;
  }
  const size_t text_len = std::strlen(text);
  const size_t suffix_len = std::strlen(suffix);
  if (text_len < suffix_len) {
    return false;
  }
  return std::strcmp(text + (text_len - suffix_len), suffix) == 0;
}

void writeLe16(File& file, uint16_t value) {
  uint8_t bytes[2] = {static_cast<uint8_t>(value & 0xFFU), static_cast<uint8_t>((value >> 8U) & 0xFFU)};
  file.write(bytes, sizeof(bytes));
}

void writeLe32(File& file, uint32_t value) {
  uint8_t bytes[4] = {
      static_cast<uint8_t>(value & 0xFFU),
      static_cast<uint8_t>((value >> 8U) & 0xFFU),
      static_cast<uint8_t>((value >> 16U) & 0xFFU),
      static_cast<uint8_t>((value >> 24U) & 0xFFU),
  };
  file.write(bytes, sizeof(bytes));
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

const char* frameSizeToText(framesize_t value) {
  switch (value) {
    case FRAMESIZE_QQVGA:
      return "QQVGA";
    case FRAMESIZE_HQVGA:
      return "HQVGA";
    case FRAMESIZE_QVGA:
      return "QVGA";
    case FRAMESIZE_CIF:
      return "CIF";
    case FRAMESIZE_VGA:
      return "VGA";
    case FRAMESIZE_SVGA:
      return "SVGA";
    case FRAMESIZE_XGA:
      return "XGA";
    case FRAMESIZE_SXGA:
      return "SXGA";
    case FRAMESIZE_UXGA:
      return "UXGA";
    case FRAMESIZE_HD:
      return "HD";
    case FRAMESIZE_FHD:
      return "FHD";
    default:
      return "VGA";
  }
}

uint16_t frameSizeWidth(framesize_t value) {
  switch (value) {
    case FRAMESIZE_QQVGA:
      return 160U;
    case FRAMESIZE_HQVGA:
      return 240U;
    case FRAMESIZE_QVGA:
      return 320U;
    case FRAMESIZE_CIF:
      return 352U;
    case FRAMESIZE_VGA:
      return 640U;
    case FRAMESIZE_SVGA:
      return 800U;
    case FRAMESIZE_XGA:
      return 1024U;
    case FRAMESIZE_SXGA:
      return 1280U;
    case FRAMESIZE_UXGA:
      return 1600U;
    case FRAMESIZE_HD:
      return 1280U;
    case FRAMESIZE_FHD:
      return 1920U;
    default:
      return 0U;
  }
}

uint16_t frameSizeHeight(framesize_t value) {
  switch (value) {
    case FRAMESIZE_QQVGA:
      return 120U;
    case FRAMESIZE_HQVGA:
      return 176U;
    case FRAMESIZE_QVGA:
      return 240U;
    case FRAMESIZE_CIF:
      return 288U;
    case FRAMESIZE_VGA:
      return 480U;
    case FRAMESIZE_SVGA:
      return 600U;
    case FRAMESIZE_XGA:
      return 768U;
    case FRAMESIZE_SXGA:
      return 1024U;
    case FRAMESIZE_UXGA:
      return 1200U;
    case FRAMESIZE_HD:
      return 720U;
    case FRAMESIZE_FHD:
      return 1080U;
    default:
      return 0U;
  }
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

  snapshot_ = {};
  snapshot_.supported = (ZACUS_HAS_CAMERA != 0);
  snapshot_.jpeg_quality = config_.jpeg_quality;
  snapshot_.fb_count = config_.fb_count;
  snapshot_.xclk_hz = config_.xclk_hz;
  copyText(snapshot_.frame_size, sizeof(snapshot_.frame_size), config_.frame_size);
  copyText(snapshot_.snapshot_dir, sizeof(snapshot_.snapshot_dir), config_.snapshot_dir);
  recorder_mode_ = false;
  recorder_frozen_ = false;
  recorder_frozen_fb_ = nullptr;
  preview_map_src_w_ = 0;
  preview_map_src_h_ = 0;
  preview_map_dst_w_ = 0;
  preview_map_dst_h_ = 0;
  preview_x_map_.clear();
  preview_y_map_.clear();
  return true;
}

bool CameraManager::ensureSnapshotDir() {
  const String dir = normalizeDir(config_.snapshot_dir);
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
  const String dir = normalizeDir(config_.snapshot_dir);
  String file = sanitizeFileBasename(filename_hint);
  if (!file.startsWith("/")) {
    file = dir + "/" + file;
  }
  return file;
}

bool CameraManager::initCameraForMode(bool recorder_mode) {
#if !ZACUS_HAS_CAMERA
  (void)recorder_mode;
  setLastError("camera_not_supported");
  return false;
#else
  if (snapshot_.initialized && recorder_mode_ == recorder_mode) {
    snapshot_.enabled = true;
    snapshot_.recorder_session_active = recorder_mode;
    return true;
  }

  recorderDiscardFrozen();
  if (snapshot_.initialized) {
    esp_camera_deinit();
    snapshot_.initialized = false;
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
  cfg.pin_sccb_sda = FREENOVE_CAM_SIOD;
  cfg.pin_sccb_scl = FREENOVE_CAM_SIOC;
  cfg.pin_pwdn = FREENOVE_CAM_PWDN;
  cfg.pin_reset = FREENOVE_CAM_RESET;
  cfg.xclk_freq_hz = config_.xclk_hz;
  cfg.fb_count = recorder_mode ? 1U : config_.fb_count;
#if defined(CAMERA_GRAB_LATEST)
  cfg.grab_mode = CAMERA_GRAB_LATEST;
#endif
#if defined(CAMERA_FB_IN_PSRAM) && (UI_CAMERA_FB_IN_PSRAM != 0)
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
#endif

  if (recorder_mode) {
    cfg.pixel_format = PIXFORMAT_RGB565;
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 12U;
  } else {
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = frameSizeFromText(config_.frame_size);
    cfg.jpeg_quality = config_.jpeg_quality;
  }

  esp_err_t status = esp_camera_init(&cfg);
  if (status != ESP_OK) {
    Serial.printf("[CAM] init failed mode=%s err=0x%x\n", recorder_mode ? "recorder" : "default", static_cast<unsigned int>(status));
    camera_config_t fallback = cfg;
    if (recorder_mode) {
      fallback.frame_size = FRAMESIZE_QQVGA;
      fallback.fb_count = 1U;
    } else {
      fallback.frame_size = FRAMESIZE_QVGA;
      fallback.jpeg_quality = (fallback.jpeg_quality < 20U) ? 20U : fallback.jpeg_quality;
      fallback.fb_count = 1U;
    }
#if defined(CAMERA_FB_IN_DRAM)
    fallback.fb_location = CAMERA_FB_IN_DRAM;
#endif
    status = esp_camera_init(&fallback);
    if (status == ESP_OK) {
      cfg = fallback;
    }
  }

  if (status != ESP_OK) {
    snapshot_.enabled = false;
    snapshot_.initialized = false;
    snapshot_.recorder_session_active = false;
    recorder_mode_ = false;
    setLastError("camera_init_failed");
    return false;
  }

  snapshot_.enabled = true;
  snapshot_.initialized = true;
  snapshot_.jpeg_quality = static_cast<uint8_t>(cfg.jpeg_quality);
  snapshot_.fb_count = static_cast<uint8_t>(cfg.fb_count);
  copyText(snapshot_.frame_size, sizeof(snapshot_.frame_size), frameSizeToText(cfg.frame_size));
  snapshot_.width = frameSizeWidth(cfg.frame_size);
  snapshot_.height = frameSizeHeight(cfg.frame_size);
  recorder_mode_ = recorder_mode;
  snapshot_.recorder_session_active = recorder_mode;
  snapshot_.recorder_frozen = false;
  snapshot_.recorder_preview_width = recorder_mode ? snapshot_.width : 0U;
  snapshot_.recorder_preview_height = recorder_mode ? snapshot_.height : 0U;
  clearLastError();
  Serial.printf("[CAM] ready mode=%s frame=%s quality=%u fb=%u xclk=%lu\n",
                recorder_mode ? "recorder" : "default",
                snapshot_.frame_size,
                static_cast<unsigned int>(snapshot_.jpeg_quality),
                static_cast<unsigned int>(snapshot_.fb_count),
                static_cast<unsigned long>(snapshot_.xclk_hz));
  return true;
#endif
}

bool CameraManager::start() {
  return initCameraForMode(false);
}

bool CameraManager::startRecorderSession() {
  return initCameraForMode(true);
}

void CameraManager::stopRecorderSession() {
  recorderDiscardFrozen();
  if (!snapshot_.supported) {
    return;
  }
  if (recorder_mode_) {
    stop();
    (void)start();
  } else {
    snapshot_.recorder_session_active = false;
    snapshot_.recorder_frozen = false;
  }
}

void CameraManager::stop() {
  recorderDiscardFrozen();
#if ZACUS_HAS_CAMERA
  if (snapshot_.initialized) {
    esp_camera_deinit();
  }
#endif
  snapshot_.initialized = false;
  snapshot_.enabled = false;
  snapshot_.recorder_session_active = false;
  snapshot_.recorder_frozen = false;
  snapshot_.recorder_preview_width = 0U;
  snapshot_.recorder_preview_height = 0U;
  recorder_mode_ = false;
}

bool CameraManager::isEnabled() const {
  return snapshot_.enabled;
}

bool CameraManager::recorderSessionActive() const {
  return recorder_mode_ && snapshot_.enabled;
}

bool CameraManager::saveRgb565AsBmp24(const char* path,
                                      const uint16_t* rgb565,
                                      int w,
                                      int h,
                                      int stride_px) {
  if (path == nullptr || rgb565 == nullptr || w <= 0 || h <= 0 || stride_px < w) {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const uint32_t row_bytes = static_cast<uint32_t>(w) * 3U;
  const uint32_t row_padded = (row_bytes + 3U) & ~3U;
  const uint32_t pixel_bytes = row_padded * static_cast<uint32_t>(h);
  const uint32_t file_size = 54U + pixel_bytes;

  file.write(reinterpret_cast<const uint8_t*>("BM"), 2U);
  writeLe32(file, file_size);
  writeLe16(file, 0U);
  writeLe16(file, 0U);
  writeLe32(file, 54U);

  writeLe32(file, 40U);
  writeLe32(file, static_cast<uint32_t>(w));
  writeLe32(file, static_cast<uint32_t>(h));
  writeLe16(file, 1U);
  writeLe16(file, 24U);
  writeLe32(file, 0U);
  writeLe32(file, pixel_bytes);
  writeLe32(file, 2835U);
  writeLe32(file, 2835U);
  writeLe32(file, 0U);
  writeLe32(file, 0U);

  std::vector<uint8_t> row(static_cast<size_t>(row_padded), 0U);
  for (int y = h - 1; y >= 0; --y) {
    const uint16_t* src = rgb565 + static_cast<size_t>(y) * static_cast<size_t>(stride_px);
    uint8_t* out = row.data();
    for (int x = 0; x < w; ++x) {
      const uint16_t c = src[x];
      const uint8_t r = static_cast<uint8_t>(((c >> 11U) & 0x1FU) * 255U / 31U);
      const uint8_t g = static_cast<uint8_t>(((c >> 5U) & 0x3FU) * 255U / 63U);
      const uint8_t b = static_cast<uint8_t>((c & 0x1FU) * 255U / 31U);
      *out++ = b;
      *out++ = g;
      *out++ = r;
    }
    file.write(row.data(), row_padded);
  }
  file.close();
  return true;
}

bool CameraManager::saveRgb565Raw(const char* path,
                                  const uint16_t* rgb565,
                                  int w,
                                  int h,
                                  int stride_px) {
  if (path == nullptr || rgb565 == nullptr || w <= 0 || h <= 0 || stride_px < w) {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const uint8_t*>("R565"), 4U);
  writeLe16(file, static_cast<uint16_t>(w));
  writeLe16(file, static_cast<uint16_t>(h));
  for (int y = 0; y < h; ++y) {
    const uint16_t* row = rgb565 + static_cast<size_t>(y) * static_cast<size_t>(stride_px);
    file.write(reinterpret_cast<const uint8_t*>(row), static_cast<size_t>(w) * sizeof(uint16_t));
  }
  file.close();
  return true;
}

void CameraManager::ensurePreviewMap(int src_w, int src_h, int dst_w, int dst_h) const {
  if (src_w == preview_map_src_w_ && src_h == preview_map_src_h_ && dst_w == preview_map_dst_w_ &&
      dst_h == preview_map_dst_h_ && static_cast<int>(preview_x_map_.size()) == dst_w &&
      static_cast<int>(preview_y_map_.size()) == dst_h) {
    return;
  }
  preview_map_src_w_ = src_w;
  preview_map_src_h_ = src_h;
  preview_map_dst_w_ = dst_w;
  preview_map_dst_h_ = dst_h;
  preview_x_map_.assign(static_cast<size_t>(dst_w), 0U);
  preview_y_map_.assign(static_cast<size_t>(dst_h), 0U);

  if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) {
    return;
  }

  if (dst_w == 1) {
    preview_x_map_[0] = static_cast<uint16_t>(src_w / 2);
  } else {
    const uint32_t denom = static_cast<uint32_t>(dst_w - 1);
    const uint32_t num_max = static_cast<uint32_t>(src_w - 1);
    for (int x = 0; x < dst_w; ++x) {
      uint32_t sx = static_cast<uint32_t>(x) * num_max / denom;
      if (sx >= static_cast<uint32_t>(src_w)) {
        sx = static_cast<uint32_t>(src_w - 1);
      }
      preview_x_map_[static_cast<size_t>(x)] = static_cast<uint16_t>(sx);
    }
  }

  if (dst_h == 1) {
    preview_y_map_[0] = static_cast<uint16_t>(src_h / 2);
  } else {
    const uint32_t denom = static_cast<uint32_t>(dst_h - 1);
    const uint32_t num_max = static_cast<uint32_t>(src_h - 1);
    for (int y = 0; y < dst_h; ++y) {
      uint32_t sy = static_cast<uint32_t>(y) * num_max / denom;
      if (sy >= static_cast<uint32_t>(src_h)) {
        sy = static_cast<uint32_t>(src_h - 1);
      }
      preview_y_map_[static_cast<size_t>(y)] = static_cast<uint16_t>(sy);
    }
  }
}

bool CameraManager::downscaleToRgb565(const uint16_t* src,
                                      int src_w,
                                      int src_h,
                                      int src_stride_px,
                                      uint16_t* dst,
                                      int dst_w,
                                      int dst_h) const {
  if (src == nullptr || dst == nullptr || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
    return false;
  }
  if (src_stride_px < src_w) {
    src_stride_px = src_w;
  }
  ensurePreviewMap(src_w, src_h, dst_w, dst_h);
  if (static_cast<int>(preview_x_map_.size()) != dst_w || static_cast<int>(preview_y_map_.size()) != dst_h) {
    return false;
  }
  for (int y = 0; y < dst_h; ++y) {
    const uint16_t* src_row = src + static_cast<size_t>(preview_y_map_[static_cast<size_t>(y)]) *
                                        static_cast<size_t>(src_stride_px);
    uint16_t* dst_row = dst + static_cast<size_t>(y) * static_cast<size_t>(dst_w);
    for (int x = 0; x < dst_w; ++x) {
      dst_row[x] = src_row[preview_x_map_[static_cast<size_t>(x)]];
    }
  }
  return true;
}

bool CameraManager::snapshotToFile(const char* filename_hint, String* out_path) {
  if (out_path != nullptr) {
    out_path->remove(0);
  }
  if (recorder_mode_) {
    ++snapshot_.fail_count;
    setLastError("camera_busy_recorder_owner");
    return false;
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

  String path = buildSnapshotPath(filename_hint);
  if (!hasSuffix(path.c_str(), ".jpg") && !hasSuffix(path.c_str(), ".jpeg") && !hasSuffix(path.c_str(), ".bmp") &&
      !hasSuffix(path.c_str(), ".rgb565")) {
    path += ".jpg";
  }

  bool ok = false;
  if (frame->format == PIXFORMAT_JPEG) {
    File file = LittleFS.open(path.c_str(), "w");
    if (file) {
      ok = (file.write(frame->buf, frame->len) == frame->len);
      file.close();
    }
  } else if (frame->format == PIXFORMAT_RGB565) {
    if (hasSuffix(path.c_str(), ".rgb565")) {
      ok = saveRgb565Raw(path.c_str(), reinterpret_cast<const uint16_t*>(frame->buf), frame->width, frame->height, frame->width);
    } else {
      if (!hasSuffix(path.c_str(), ".bmp")) {
        path = buildSnapshotPath((sanitizeFileBasename(filename_hint) + ".bmp").c_str());
      }
      ok = saveRgb565AsBmp24(path.c_str(), reinterpret_cast<const uint16_t*>(frame->buf), frame->width, frame->height, frame->width);
    }
  }

  if (!ok) {
    ++snapshot_.fail_count;
    setLastError("snapshot_write_failed");
    esp_camera_fb_return(frame);
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

bool CameraManager::recorderUpdatePreviewRgb565(uint16_t* dst, int dst_w, int dst_h) {
  if (dst == nullptr || dst_w <= 0 || dst_h <= 0) {
    return false;
  }
  if (!startRecorderSession()) {
    return false;
  }

#if ZACUS_HAS_CAMERA
  if (recorder_frozen_) {
    camera_fb_t* frame = reinterpret_cast<camera_fb_t*>(recorder_frozen_fb_);
    if (frame == nullptr || frame->format != PIXFORMAT_RGB565) {
      return false;
    }
    snapshot_.recorder_preview_width = static_cast<uint16_t>(frame->width);
    snapshot_.recorder_preview_height = static_cast<uint16_t>(frame->height);
    return downscaleToRgb565(reinterpret_cast<const uint16_t*>(frame->buf),
                             frame->width,
                             frame->height,
                             frame->width,
                             dst,
                             dst_w,
                             dst_h);
  }

  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    setLastError("camera_capture_failed");
    return false;
  }

  bool ok = false;
  if (frame->format == PIXFORMAT_RGB565) {
    snapshot_.recorder_preview_width = static_cast<uint16_t>(frame->width);
    snapshot_.recorder_preview_height = static_cast<uint16_t>(frame->height);
    snapshot_.width = static_cast<uint16_t>(frame->width);
    snapshot_.height = static_cast<uint16_t>(frame->height);
    ok = downscaleToRgb565(reinterpret_cast<const uint16_t*>(frame->buf),
                           frame->width,
                           frame->height,
                           frame->width,
                           dst,
                           dst_w,
                           dst_h);
  }
  esp_camera_fb_return(frame);
  if (!ok) {
    setLastError("camera_preview_unavailable");
  }
  return ok;
#else
  (void)dst;
  (void)dst_w;
  (void)dst_h;
  setLastError("camera_not_supported");
  return false;
#endif
}

bool CameraManager::recorderSnapFreeze(uint16_t* preview_dst, int preview_w, int preview_h) {
  if (!startRecorderSession()) {
    return false;
  }
#if ZACUS_HAS_CAMERA
  if (recorder_frozen_) {
    if (preview_dst != nullptr && preview_w > 0 && preview_h > 0) {
      (void)recorderUpdatePreviewRgb565(preview_dst, preview_w, preview_h);
    }
    return true;
  }

  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    ++snapshot_.fail_count;
    setLastError("camera_capture_failed");
    return false;
  }
  recorder_frozen_fb_ = frame;
  recorder_frozen_ = true;
  snapshot_.recorder_frozen = true;
  snapshot_.width = static_cast<uint16_t>(frame->width);
  snapshot_.height = static_cast<uint16_t>(frame->height);
  snapshot_.recorder_preview_width = static_cast<uint16_t>(frame->width);
  snapshot_.recorder_preview_height = static_cast<uint16_t>(frame->height);
  if (preview_dst != nullptr && preview_w > 0 && preview_h > 0 && frame->format == PIXFORMAT_RGB565) {
    (void)downscaleToRgb565(reinterpret_cast<const uint16_t*>(frame->buf),
                            frame->width,
                            frame->height,
                            frame->width,
                            preview_dst,
                            preview_w,
                            preview_h);
  }
  clearLastError();
  return true;
#else
  (void)preview_dst;
  (void)preview_w;
  (void)preview_h;
  setLastError("camera_not_supported");
  return false;
#endif
}

bool CameraManager::recorderHasFrozen() const {
  return recorder_frozen_;
}

void CameraManager::recorderDiscardFrozen() {
#if ZACUS_HAS_CAMERA
  if (recorder_frozen_fb_ != nullptr) {
    esp_camera_fb_return(reinterpret_cast<camera_fb_t*>(recorder_frozen_fb_));
  }
#endif
  recorder_frozen_fb_ = nullptr;
  recorder_frozen_ = false;
  snapshot_.recorder_frozen = false;
}

CameraManager::RecorderSaveFormat CameraManager::parseSaveFormatToken(const char* token) {
  if (token == nullptr || token[0] == '\0') {
    return RecorderSaveFormat::kAuto;
  }
  String normalized(token);
  normalized.trim();
  normalized.toLowerCase();
  if (normalized == "bmp") {
    return RecorderSaveFormat::kBmp24;
  }
  if (normalized == "jpg" || normalized == "jpeg") {
    return RecorderSaveFormat::kJpeg;
  }
  if (normalized == "raw" || normalized == "rgb565") {
    return RecorderSaveFormat::kRawRgb565;
  }
  return RecorderSaveFormat::kAuto;
}

bool CameraManager::recorderSaveFrozen(String* out_path, RecorderSaveFormat format) {
  if (out_path != nullptr) {
    out_path->remove(0);
  }
#if !ZACUS_HAS_CAMERA
  (void)format;
  setLastError("camera_not_supported");
  return false;
#else
  if (!recorder_frozen_ || recorder_frozen_fb_ == nullptr) {
    setLastError("camera_not_frozen");
    return false;
  }
  if (!ensureSnapshotDir()) {
    return false;
  }

  camera_fb_t* frame = reinterpret_cast<camera_fb_t*>(recorder_frozen_fb_);
  RecorderSaveFormat actual = format;
  if (frame->format == PIXFORMAT_JPEG) {
    actual = RecorderSaveFormat::kJpeg;
  } else if (frame->format == PIXFORMAT_RGB565) {
    if (actual == RecorderSaveFormat::kAuto || actual == RecorderSaveFormat::kJpeg) {
      actual = RecorderSaveFormat::kBmp24;
    }
  }

  const char* ext = ".bmp";
  if (actual == RecorderSaveFormat::kJpeg) {
    ext = ".jpg";
  } else if (actual == RecorderSaveFormat::kRawRgb565) {
    ext = ".rgb565";
  }

  String base_name = String("rec_") + String(static_cast<unsigned long>(millis())) + ext;
  String path = buildSnapshotPath(base_name.c_str());
  for (uint8_t attempt = 0U; LittleFS.exists(path.c_str()) && attempt < 20U; ++attempt) {
    base_name = String("rec_") + String(static_cast<unsigned long>(millis())) + "_" + String(attempt + 1U) + ext;
    path = buildSnapshotPath(base_name.c_str());
  }

  bool ok = false;
  if (actual == RecorderSaveFormat::kJpeg) {
    File file = LittleFS.open(path.c_str(), "w");
    if (file) {
      ok = (file.write(frame->buf, frame->len) == frame->len);
      file.close();
    }
  } else if (actual == RecorderSaveFormat::kBmp24) {
    ok = saveRgb565AsBmp24(path.c_str(),
                           reinterpret_cast<const uint16_t*>(frame->buf),
                           frame->width,
                           frame->height,
                           frame->width);
  } else if (actual == RecorderSaveFormat::kRawRgb565) {
    ok = saveRgb565Raw(path.c_str(),
                       reinterpret_cast<const uint16_t*>(frame->buf),
                       frame->width,
                       frame->height,
                       frame->width);
  }

  if (!ok) {
    ++snapshot_.fail_count;
    setLastError("snapshot_write_failed");
    return false;
  }

  snapshot_.last_snapshot_ok = true;
  snapshot_.last_capture_ms = millis();
  ++snapshot_.capture_count;
  copyText(snapshot_.last_file, sizeof(snapshot_.last_file), path.c_str());
  copyText(snapshot_.recorder_selected_file, sizeof(snapshot_.recorder_selected_file), path.c_str());
  clearLastError();
  if (out_path != nullptr) {
    *out_path = path;
  }
  recorderDiscardFrozen();
  return true;
#endif
}

bool CameraManager::isPhotoExtension(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".bmp") || lower.endsWith(".rgb565");
}

int CameraManager::recorderListPhotos(String* out, int max_items, bool newest_first) const {
  if (out == nullptr || max_items <= 0) {
    return 0;
  }
  const String dir = normalizeDir(config_.snapshot_dir);
  File root = LittleFS.open(dir.c_str());
  if (!root || !root.isDirectory()) {
    return 0;
  }

  std::vector<String> photos;
  photos.reserve(static_cast<size_t>(max_items));
  File entry = root.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (isPhotoExtension(name)) {
        photos.push_back(name);
      }
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();

  std::sort(photos.begin(), photos.end(), [](const String& a, const String& b) { return a.compareTo(b) < 0; });
  if (newest_first) {
    std::reverse(photos.begin(), photos.end());
  }

  const int count = std::min<int>(max_items, static_cast<int>(photos.size()));
  for (int index = 0; index < count; ++index) {
    out[index] = photos[static_cast<size_t>(index)];
  }
  return count;
}

bool CameraManager::recorderRemoveFile(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  const bool ok = LittleFS.remove(path);
  if (!ok) {
    setLastError("snapshot_remove_failed");
  }
  return ok;
}

bool CameraManager::recorderSelectNextPhoto(String* in_out_path) const {
  if (in_out_path == nullptr) {
    return false;
  }
  String items[64];
  const int count = recorderListPhotos(items, 64, true);
  if (count <= 0) {
    in_out_path->remove(0);
    return false;
  }
  if (in_out_path->isEmpty()) {
    *in_out_path = items[0];
    return true;
  }
  int current_index = -1;
  for (int i = 0; i < count; ++i) {
    if (items[i] == *in_out_path) {
      current_index = i;
      break;
    }
  }
  const int next_index = (current_index < 0) ? 0 : ((current_index + 1) % count);
  *in_out_path = items[next_index];
  return true;
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
