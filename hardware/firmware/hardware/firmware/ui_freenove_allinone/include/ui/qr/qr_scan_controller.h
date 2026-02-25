// qr_scan_controller.h - QR code scan helper for Freenove camera scene.
#pragma once

#include <Arduino.h>

namespace ui {

struct QrScanResult {
  uint32_t at_ms = 0U;
  bool decoder_valid = false;
  uint16_t payload_len = 0U;
  char payload[192] = {0};
};

class QrScanController {
 public:
  QrScanController() = default;
  ~QrScanController();

  QrScanController(const QrScanController&) = delete;
  QrScanController& operator=(const QrScanController&) = delete;

  bool begin();
  bool ready() const { return ready_; }
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  bool poll(QrScanResult* out, uint32_t timeout_ms = 0U);

 private:
  bool ready_ = false;
  bool enabled_ = false;
  void* reader_ = nullptr;
};

}  // namespace ui
