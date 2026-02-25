#include "ui/camera_capture/camera_capture_service.h"

#include <algorithm>

namespace ui::camera {

CameraManager::RecorderSaveFormat CameraCaptureService::toRecorderFormat(CaptureFormat format) {
  switch (format) {
    case CaptureFormat::Bmp24:
      return CameraManager::RecorderSaveFormat::kBmp24;
    case CaptureFormat::Jpeg:
      return CameraManager::RecorderSaveFormat::kJpeg;
    case CaptureFormat::RawRGB565:
      return CameraManager::RecorderSaveFormat::kRawRgb565;
    case CaptureFormat::Auto:
    default:
      return CameraManager::RecorderSaveFormat::kAuto;
  }
}

bool CameraCaptureService::begin(const Config& cfg) {
  cfg_ = cfg;
  return cfg_.camera != nullptr;
}

bool CameraCaptureService::ready() const {
  return cfg_.camera != nullptr;
}

bool CameraCaptureService::update_preview_rgb565(uint16_t* dst, int dst_w, int dst_h) {
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderUpdatePreviewRgb565(dst, dst_w, dst_h);
}

bool CameraCaptureService::snap_freeze(uint16_t* preview_dst, int preview_w, int preview_h) {
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderSnapFreeze(preview_dst, preview_w, preview_h);
}

bool CameraCaptureService::has_frozen() const {
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderHasFrozen();
}

bool CameraCaptureService::save_frozen(String& out_path, CaptureFormat fmt) {
  out_path = "";
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderSaveFrozen(&out_path, toRecorderFormat(fmt));
}

void CameraCaptureService::discard_frozen() {
  if (cfg_.camera == nullptr) {
    return;
  }
  cfg_.camera->recorderDiscardFrozen();
}

bool CameraCaptureService::capture_next(String& out_path, CaptureFormat fmt) {
  out_path = "";
  if (cfg_.camera == nullptr) {
    return false;
  }
  if (fmt == CaptureFormat::Auto || fmt == CaptureFormat::Jpeg) {
    return cfg_.camera->snapshotToFile(nullptr, &out_path);
  }
  if (!cfg_.camera->recorderSnapFreeze(nullptr, 0, 0)) {
    return false;
  }
  return cfg_.camera->recorderSaveFrozen(&out_path, toRecorderFormat(fmt));
}

int CameraCaptureService::list_photos(String* out, int max_items, bool newest_first) const {
  if (cfg_.camera == nullptr) {
    return 0;
  }
  return cfg_.camera->recorderListPhotos(out, max_items, newest_first);
}

bool CameraCaptureService::remove_file(const char* path) {
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderRemoveFile(path);
}

bool CameraCaptureService::select_next_photo(String* in_out_path) const {
  if (cfg_.camera == nullptr) {
    return false;
  }
  return cfg_.camera->recorderSelectNextPhoto(in_out_path);
}

bool CameraCaptureService::get_sensor_size(int& w, int& h) const {
  if (cfg_.camera == nullptr) {
    return false;
  }
  const CameraManager::Snapshot snapshot = cfg_.camera->snapshot();
  w = snapshot.width;
  h = snapshot.height;
  return (w > 0 && h > 0);
}

void CameraCaptureService::downscale_rgb565_nearest(const uint16_t* src,
                                                    int src_w,
                                                    int src_h,
                                                    int src_stride_px,
                                                    uint16_t* dst,
                                                    int dst_w,
                                                    int dst_h,
                                                    bool aligned16_hint) {
  (void)aligned16_hint;
  if (src == nullptr || dst == nullptr || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
    return;
  }
  if (src_stride_px < src_w) {
    src_stride_px = src_w;
  }

  const uint32_t src_w_span = static_cast<uint32_t>(src_w - 1);
  const uint32_t src_h_span = static_cast<uint32_t>(src_h - 1);
  const uint32_t dst_w_span = static_cast<uint32_t>(std::max(dst_w - 1, 1));
  const uint32_t dst_h_span = static_cast<uint32_t>(std::max(dst_h - 1, 1));

  for (int y = 0; y < dst_h; ++y) {
    const uint32_t sy = (dst_h == 1) ? static_cast<uint32_t>(src_h / 2) : (static_cast<uint32_t>(y) * src_h_span) / dst_h_span;
    const uint16_t* src_row = src + static_cast<size_t>(sy) * static_cast<size_t>(src_stride_px);
    uint16_t* dst_row = dst + static_cast<size_t>(y) * static_cast<size_t>(dst_w);
    for (int x = 0; x < dst_w; ++x) {
      const uint32_t sx =
          (dst_w == 1) ? static_cast<uint32_t>(src_w / 2) : (static_cast<uint32_t>(x) * src_w_span) / dst_w_span;
      dst_row[x] = src_row[sx];
    }
  }
}

}  // namespace ui::camera
