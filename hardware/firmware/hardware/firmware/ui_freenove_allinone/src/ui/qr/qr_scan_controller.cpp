// qr_scan_controller.cpp
#include "ui/qr/qr_scan_controller.h"

#include <cstring>

#if defined(USE_CAM) && (USE_CAM != 0) && __has_include(<ESP32QRCodeReader.h>) && __has_include(<ESP32CameraPins.h>)
#define ZACUS_QR_READER_AVAILABLE 1
#include <ESP32CameraPins.h>
#include <ESP32QRCodeReader.h>
#else
#define ZACUS_QR_READER_AVAILABLE 0
#endif

namespace ui {
namespace {

template <typename T>
auto qrPayloadLength(const T& qr, int) -> decltype(qr.payloadLen) {
  return qr.payloadLen;
}

template <typename T>
auto qrPayloadLength(const T& qr, long) -> decltype(qr.payload_len) {
  return qr.payload_len;
}

}  // namespace

QrScanController::~QrScanController() {
#if ZACUS_QR_READER_AVAILABLE
  if (reader_ != nullptr) {
    delete reinterpret_cast<ESP32QRCodeReader*>(reader_);
    reader_ = nullptr;
  }
#endif
}

bool QrScanController::begin() {
  if (ready_) {
    return true;
  }
#if !ZACUS_QR_READER_AVAILABLE
  Serial.println("[QR] scanner unavailable (missing dependency or USE_CAM=0)");
  return false;
#else
  CameraPins pins = {};
  pins.PWDN_GPIO_NUM = FREENOVE_CAM_PWDN;
  pins.RESET_GPIO_NUM = FREENOVE_CAM_RESET;
  pins.XCLK_GPIO_NUM = FREENOVE_CAM_XCLK;
  pins.SIOD_GPIO_NUM = FREENOVE_CAM_SIOD;
  pins.SIOC_GPIO_NUM = FREENOVE_CAM_SIOC;
  pins.Y9_GPIO_NUM = FREENOVE_CAM_Y9;
  pins.Y8_GPIO_NUM = FREENOVE_CAM_Y8;
  pins.Y7_GPIO_NUM = FREENOVE_CAM_Y7;
  pins.Y6_GPIO_NUM = FREENOVE_CAM_Y6;
  pins.Y5_GPIO_NUM = FREENOVE_CAM_Y5;
  pins.Y4_GPIO_NUM = FREENOVE_CAM_Y4;
  pins.Y3_GPIO_NUM = FREENOVE_CAM_Y3;
  pins.Y2_GPIO_NUM = FREENOVE_CAM_Y2;
  pins.VSYNC_GPIO_NUM = FREENOVE_CAM_VSYNC;
  pins.HREF_GPIO_NUM = FREENOVE_CAM_HREF;
  pins.PCLK_GPIO_NUM = FREENOVE_CAM_PCLK;
  auto* reader = new ESP32QRCodeReader(pins);
  if (reader == nullptr) {
    Serial.println("[QR] scanner alloc failed");
    return false;
  }
  reader->setup();
  reader->beginOnCore(0);
  reader_ = reinterpret_cast<void*>(reader);
  ready_ = true;
  enabled_ = false;
  Serial.println("[QR] scanner ready");
  return true;
#endif
}

bool QrScanController::poll(QrScanResult* out, uint32_t timeout_ms) {
  if (!ready_ || !enabled_ || out == nullptr) {
    return false;
  }
#if !ZACUS_QR_READER_AVAILABLE
  (void)timeout_ms;
  return false;
#else
  auto* reader = reinterpret_cast<ESP32QRCodeReader*>(reader_);
  if (reader == nullptr) {
    return false;
  }
  QRCodeData qr;
  if (!reader->receiveQrCode(&qr, timeout_ms)) {
    return false;
  }

  out->at_ms = millis();
  out->decoder_valid = qr.valid;
  out->payload_len = 0U;
  out->payload[0] = '\0';
  const int payload_len = static_cast<int>(qrPayloadLength(qr, 0));
  if (!qr.valid || payload_len <= 0) {
    return true;
  }

  const size_t max_len = sizeof(out->payload) - 1U;
  const size_t copy_len = (static_cast<size_t>(payload_len) < max_len) ? static_cast<size_t>(payload_len) : max_len;
  std::memcpy(out->payload, qr.payload, copy_len);
  out->payload[copy_len] = '\0';
  out->payload_len = static_cast<uint16_t>(copy_len);
  return true;
#endif
}

}  // namespace ui
