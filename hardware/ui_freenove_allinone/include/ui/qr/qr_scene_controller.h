// qr_scene_controller.h - QR scene lifecycle + runtime event helper.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace ui {

class QrScanController;
class QrValidationRules;

class QrSceneController {
 public:
  void onSceneEnter(QrScanController* scanner, lv_obj_t* subtitle_label);
  void onSceneExit(QrScanController* scanner);
  void tick(uint32_t now_ms,
            QrScanController* scanner,
            const QrValidationRules& rules,
            lv_obj_t* subtitle_label,
            lv_obj_t* symbol_label);

  bool consumeRuntimeEvent(char* out_event, size_t capacity);
  bool queueSimulatedPayload(const char* payload);

 private:
  void handleDecodedPayload(const char* payload,
                            bool decoder_valid,
                            uint32_t now_ms,
                            const QrValidationRules& rules,
                            lv_obj_t* subtitle_label,
                            lv_obj_t* symbol_label);

  bool scene_active_ = false;
  bool last_match_ = false;
  uint32_t last_decode_ms_ = 0U;
  uint32_t feedback_until_ms_ = 0U;
  char last_payload_[192] = {0};
  char pending_runtime_event_[24] = {0};
  bool pending_runtime_event_valid_ = false;
  char simulated_payload_[192] = {0};
  bool simulated_pending_ = false;
};

}  // namespace ui
