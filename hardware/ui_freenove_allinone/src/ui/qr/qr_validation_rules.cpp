// qr_validation_rules.cpp

#include "ui/qr/qr_validation_rules.h"

#include <cctype>
#include <cstring>
#include <strings.h>

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

bool startsWithCaseInsensitive(const char* value, const char* prefix) {
  if (value == nullptr || prefix == nullptr) {
    return false;
  }
  while (*prefix != '\0') {
    if (*value == '\0') {
      return false;
    }
    const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(*value)));
    const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(*prefix)));
    if (lhs != rhs) {
      return false;
    }
    ++value;
    ++prefix;
  }
  return true;
}

bool containsCaseInsensitive(const char* haystack, const char* needle) {
  if (haystack == nullptr || needle == nullptr || needle[0] == '\0') {
    return false;
  }
  const size_t haystack_len = std::strlen(haystack);
  const size_t needle_len = std::strlen(needle);
  if (needle_len > haystack_len) {
    return false;
  }
  for (size_t index = 0U; index + needle_len <= haystack_len; ++index) {
    bool ok = true;
    for (size_t offset = 0U; offset < needle_len; ++offset) {
      const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(haystack[index + offset])));
      const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[offset])));
      if (lhs != rhs) {
        ok = false;
        break;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

bool isAsciiSpace(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

void trimAsciiWhitespaceInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  char* start = text;
  while (*start != '\0' && isAsciiSpace(*start)) {
    ++start;
  }
  if (start != text) {
    std::memmove(text, start, std::strlen(start) + 1U);
  }
  size_t length = std::strlen(text);
  while (length > 0U && isAsciiSpace(text[length - 1U])) {
    text[length - 1U] = '\0';
    --length;
  }
}

void asciiUpperInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  for (; *text != '\0'; ++text) {
    *text = static_cast<char>(std::toupper(static_cast<unsigned char>(*text)));
  }
}

uint16_t crc16CcittFalse(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFFu;
  for (size_t index = 0U; index < len; ++index) {
    crc ^= static_cast<uint16_t>(data[index]) << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = ((crc & 0x8000u) != 0U) ? static_cast<uint16_t>((crc << 1U) ^ 0x1021u)
                                    : static_cast<uint16_t>(crc << 1U);
    }
  }
  return crc;
}

bool parseHex16(const char* text, uint16_t* out) {
  if (text == nullptr || out == nullptr) {
    return false;
  }
  while (*text != '\0' && isAsciiSpace(*text)) {
    ++text;
  }
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text += 2;
  }
  uint32_t value = 0U;
  uint8_t digits = 0U;
  for (; *text != '\0'; ++text) {
    const char c = *text;
    if (isAsciiSpace(c)) {
      break;
    }
    uint8_t nibble = 0xFFU;
    if (c >= '0' && c <= '9') {
      nibble = static_cast<uint8_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      nibble = static_cast<uint8_t>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      nibble = static_cast<uint8_t>(c - 'A' + 10);
    } else {
      return false;
    }
    value = (value << 4U) | nibble;
    ++digits;
    if (digits > 4U) {
      return false;
    }
  }
  if (digits == 0U) {
    return false;
  }
  *out = static_cast<uint16_t>(value);
  return true;
}

}  // namespace

void QrValidationRules::clear() {
  expected_count_ = 0U;
  for (uint8_t index = 0U; index < kExpectedMax; ++index) {
    expected_values_[index][0] = '\0';
  }
  prefix_[0] = '\0';
  contains_[0] = '\0';
  case_insensitive_ = false;
  crc16_enabled_ = false;
  crc16_sep_ = '*';
}

void QrValidationRules::configureFromPayload(JsonVariantConst root) {
  clear();
  if (root.isNull()) {
    return;
  }
  const JsonObjectConst qr = root["qr"].as<JsonObjectConst>();
  if (qr.isNull()) {
    return;
  }
  case_insensitive_ = qr["caseInsensitive"] | false;
  if (qr["crc16"].is<JsonObjectConst>()) {
    const JsonObjectConst crc = qr["crc16"].as<JsonObjectConst>();
    crc16_enabled_ = crc["enabled"] | true;
    const char* sep = crc["sep"] | "*";
    if (sep != nullptr && sep[0] != '\0') {
      crc16_sep_ = sep[0];
    }
  } else {
    crc16_enabled_ = qr["crc16"] | false;
    const char* sep = qr["crcSep"] | "*";
    if (sep != nullptr && sep[0] != '\0') {
      crc16_sep_ = sep[0];
    }
  }

  if (qr["expected"].is<JsonArrayConst>()) {
    for (JsonVariantConst item : qr["expected"].as<JsonArrayConst>()) {
      const char* value = item | "";
      if (value[0] == '\0' || expected_count_ >= kExpectedMax) {
        continue;
      }
      copyTextSafe(expected_values_[expected_count_], sizeof(expected_values_[0]), value);
      ++expected_count_;
    }
  } else {
    const char* expected = qr["expected"] | "";
    if (expected[0] != '\0') {
      copyTextSafe(expected_values_[0], sizeof(expected_values_[0]), expected);
      expected_count_ = 1U;
    }
  }
  copyTextSafe(prefix_, sizeof(prefix_), qr["prefix"] | "");
  copyTextSafe(contains_, sizeof(contains_), qr["contains"] | "");
}

bool QrValidationRules::matches(const char* payload) const {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }
  char buffer[192] = {0};
  copyTextSafe(buffer, sizeof(buffer), payload);
  trimAsciiWhitespaceInPlace(buffer);
  if (buffer[0] == '\0') {
    return false;
  }
  const char* match_str = buffer;

  if (crc16_enabled_) {
    const char separator = (crc16_sep_ != '\0') ? crc16_sep_ : '*';
    char* sep_pos = std::strrchr(buffer, separator);
    if (sep_pos == nullptr) {
      return false;
    }
    *sep_pos = '\0';
    char* crc_text = sep_pos + 1;
    trimAsciiWhitespaceInPlace(crc_text);
    uint16_t expected_crc = 0U;
    if (!parseHex16(crc_text, &expected_crc)) {
      return false;
    }
    char crc_buffer[192] = {0};
    copyTextSafe(crc_buffer, sizeof(crc_buffer), buffer);
    if (case_insensitive_) {
      asciiUpperInPlace(crc_buffer);
    }
    const uint16_t actual_crc =
        crc16CcittFalse(reinterpret_cast<const uint8_t*>(crc_buffer), std::strlen(crc_buffer));
    if (actual_crc != expected_crc) {
      return false;
    }
    trimAsciiWhitespaceInPlace(buffer);
    if (buffer[0] == '\0') {
      return false;
    }
    match_str = buffer;
  }

  if (expected_count_ > 0U) {
    for (uint8_t index = 0U; index < expected_count_; ++index) {
      const char* expected = expected_values_[index];
      if (expected[0] == '\0') {
        continue;
      }
      if (case_insensitive_) {
        if (strcasecmp(match_str, expected) == 0) {
          return true;
        }
      } else if (std::strcmp(match_str, expected) == 0) {
        return true;
      }
    }
    return false;
  }

  if (prefix_[0] != '\0') {
    return case_insensitive_ ? startsWithCaseInsensitive(match_str, prefix_)
                             : (std::strncmp(match_str, prefix_, std::strlen(prefix_)) == 0);
  }
  if (contains_[0] != '\0') {
    return case_insensitive_ ? containsCaseInsensitive(match_str, contains_)
                             : (std::strstr(match_str, contains_) != nullptr);
  }
  return true;
}

}  // namespace ui

