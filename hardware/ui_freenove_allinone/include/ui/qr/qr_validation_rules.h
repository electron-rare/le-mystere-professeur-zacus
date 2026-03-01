// qr_validation_rules.h - QR payload rule parsing + matching helpers.
#pragma once

#include <ArduinoJson.h>
#include <stdint.h>

namespace ui {

class QrValidationRules {
 public:
  static constexpr uint8_t kExpectedMax = 4U;

  void clear();
  void configureFromPayload(JsonVariantConst root);
  bool matches(const char* payload) const;

 private:
  bool case_insensitive_ = false;
  char expected_values_[kExpectedMax][64] = {};
  uint8_t expected_count_ = 0U;
  char prefix_[64] = {0};
  char contains_[64] = {0};
  bool crc16_enabled_ = false;
  char crc16_sep_ = '*';
};

}  // namespace ui

