// qr_scene_controller.cpp

#include "ui/qr/qr_scene_controller.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>

#include <lvgl.h>

#include "ui/qr/qr_scan_controller.h"
#include "ui/qr/qr_validation_rules.h"

namespace ui {
namespace {

void copyTextSafe(char* out, size_t out_size, const char* value) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (value == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, value, out_size - 1U);
  out[out_size - 1U] = '\0';
}

}  // namespace

void QrSceneController::onSceneEnter(QrScanController* scanner, lv_obj_t* subtitle_label) {
  scene_active_ = true;
  bool qr_ready = false;
  if (scanner != nullptr) {
    qr_ready = scanner->begin();
    scanner->setEnabled(qr_ready);
  }
  last_decode_ms_ = 0U;
  feedback_until_ms_ = 0U;
  last_payload_[0] = '\0';
  last_match_ = false;
  pending_runtime_event_[0] = '\0';
  pending_runtime_event_valid_ = false;
  simulated_payload_[0] = '\0';
  simulated_pending_ = false;
  const bool has_reticle = LittleFS.exists("/ui/qr/reticle.png");
  const bool has_scanlines = LittleFS.exists("/ui/qr/scanlines.png");
  Serial.printf("[QR_UI] assets reticle=%u scanlines=%u\n", has_reticle ? 1U : 0U, has_scanlines ? 1U : 0U);
  if (subtitle_label != nullptr) {
    lv_label_set_text(subtitle_label, qr_ready ? "SCANNE UN QR CODE..." : "CAMERA/QR indisponible");
    lv_obj_clear_flag(subtitle_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
  }
}

void QrSceneController::onSceneExit(QrScanController* scanner) {
  scene_active_ = false;
  if (scanner != nullptr) {
    scanner->setEnabled(false);
  }
  feedback_until_ms_ = 0U;
  last_decode_ms_ = 0U;
  simulated_pending_ = false;
  simulated_payload_[0] = '\0';
  pending_runtime_event_valid_ = false;
  pending_runtime_event_[0] = '\0';
}

void QrSceneController::handleDecodedPayload(const char* payload,
                                             bool decoder_valid,
                                             uint32_t now_ms,
                                             const QrValidationRules& rules,
                                             lv_obj_t* subtitle_label,
                                             lv_obj_t* symbol_label) {
  const char* safe_payload = (payload != nullptr) ? payload : "";
  copyTextSafe(last_payload_, sizeof(last_payload_), safe_payload);
  last_decode_ms_ = now_ms;
  last_match_ = decoder_valid && rules.matches(safe_payload);
  feedback_until_ms_ = now_ms + (last_match_ ? 1800U : 900U);
  if (subtitle_label != nullptr) {
    if (last_match_) {
      lv_label_set_text(subtitle_label, "VALIDATION OK");
      lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0x70FF8A), LV_PART_MAIN);
    } else {
      lv_label_set_text(subtitle_label, "CODE INVALIDE");
      lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xFF6A6A), LV_PART_MAIN);
    }
    lv_obj_clear_flag(subtitle_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (symbol_label != nullptr) {
    lv_label_set_text(symbol_label, last_match_ ? "WINNER" : "QR");
  }
  copyTextSafe(pending_runtime_event_,
               sizeof(pending_runtime_event_),
               last_match_ ? "QR_OK" : "QR_INVALID");
  pending_runtime_event_valid_ = true;
  Serial.printf("[QR] %s payload=%s\n", last_match_ ? "OK" : "INVALID", safe_payload);
}

void QrSceneController::tick(uint32_t now_ms,
                             QrScanController* scanner,
                             const QrValidationRules& rules,
                             lv_obj_t* subtitle_label,
                             lv_obj_t* symbol_label) {
  if (!scene_active_ || scanner == nullptr) {
    return;
  }
  if (feedback_until_ms_ != 0U && now_ms > feedback_until_ms_) {
    feedback_until_ms_ = 0U;
    if (subtitle_label != nullptr) {
      lv_label_set_text(subtitle_label, "SCANNE UN QR CODE...");
      lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xE8F1FF), LV_PART_MAIN);
      lv_obj_clear_flag(subtitle_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (simulated_pending_) {
    simulated_pending_ = false;
    handleDecodedPayload(simulated_payload_, true, now_ms, rules, subtitle_label, symbol_label);
    simulated_payload_[0] = '\0';
    return;
  }
  if (last_decode_ms_ != 0U && (now_ms - last_decode_ms_) < 250U) {
    return;
  }
  QrScanResult result;
  if (!scanner->poll(&result, 0U)) {
    return;
  }
  handleDecodedPayload(result.payload, result.decoder_valid, now_ms, rules, subtitle_label, symbol_label);
}

bool QrSceneController::consumeRuntimeEvent(char* out_event, size_t capacity) {
  if (out_event == nullptr || capacity == 0U || !pending_runtime_event_valid_) {
    return false;
  }
  copyTextSafe(out_event, capacity, pending_runtime_event_);
  pending_runtime_event_[0] = '\0';
  pending_runtime_event_valid_ = false;
  return true;
}

bool QrSceneController::queueSimulatedPayload(const char* payload) {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }
  copyTextSafe(simulated_payload_, sizeof(simulated_payload_), payload);
  simulated_pending_ = true;
  return true;
}

}  // namespace ui
