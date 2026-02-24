// credential_store.cpp - NVS-backed Wi-Fi/WebUI credential persistence.
#include "runtime/provisioning/credential_store.h"

#include <Preferences.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

#include <cstdint>
#include <cstring>

namespace {

constexpr const char* kNamespace = "zacus_net";
constexpr const char* kKeyStaSsid = "sta_ssid";
constexpr const char* kKeyStaPass = "sta_pass";
constexpr const char* kKeyWebToken = "web_token";
constexpr const char* kKeyProvisioned = "provisioned";
constexpr size_t kTokenBytes = 16U;

void clearBuffer(char* out, size_t capacity) {
  if (out == nullptr || capacity == 0U) {
    return;
  }
  out[0] = '\0';
}

uint32_t nextRandomWord() {
#if defined(ARDUINO_ARCH_ESP32)
  return esp_random();
#else
  return static_cast<uint32_t>(micros());
#endif
}

}  // namespace

bool CredentialStore::loadStaCredentials(char* out_ssid,
                                         size_t ssid_capacity,
                                         char* out_password,
                                         size_t password_capacity) const {
  clearBuffer(out_ssid, ssid_capacity);
  clearBuffer(out_password, password_capacity);
  if (out_ssid == nullptr || ssid_capacity == 0U || out_password == nullptr || password_capacity == 0U) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  prefs.getString(kKeyStaSsid, out_ssid, ssid_capacity);
  prefs.getString(kKeyStaPass, out_password, password_capacity);
  prefs.end();
  return out_ssid[0] != '\0';
}

bool CredentialStore::saveStaCredentials(const char* ssid, const char* password) const {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t ssid_len = prefs.putString(kKeyStaSsid, ssid);
  const size_t pass_len = prefs.putString(kKeyStaPass, (password != nullptr) ? password : "");
  prefs.putBool(kKeyProvisioned, true);
  prefs.end();
  return ssid_len > 0U || pass_len > 0U;
}

bool CredentialStore::clearStaCredentials() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.remove(kKeyStaSsid);
  prefs.remove(kKeyStaPass);
  prefs.putBool(kKeyProvisioned, false);
  prefs.end();
  return true;
}

bool CredentialStore::loadWebToken(char* out_token, size_t token_capacity) const {
  clearBuffer(out_token, token_capacity);
  if (out_token == nullptr || token_capacity == 0U) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  prefs.getString(kKeyWebToken, out_token, token_capacity);
  prefs.end();
  return out_token[0] != '\0';
}

bool CredentialStore::saveWebToken(const char* token) const {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = prefs.putString(kKeyWebToken, token);
  prefs.end();
  return written > 0U;
}

bool CredentialStore::clearWebToken() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.remove(kKeyWebToken);
  prefs.end();
  return true;
}

bool CredentialStore::isProvisioned() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  const bool provisioned = prefs.getBool(kKeyProvisioned, false);
  prefs.end();
  return provisioned;
}

bool CredentialStore::setProvisioned(bool provisioned) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.putBool(kKeyProvisioned, provisioned);
  prefs.end();
  return true;
}

bool CredentialStore::generateWebToken(char* out_token, size_t token_capacity) const {
  clearBuffer(out_token, token_capacity);
  if (out_token == nullptr || token_capacity < (kTokenBytes * 2U + 1U)) {
    return false;
  }
  static constexpr char kHex[] = "0123456789abcdef";
  uint32_t random_word = 0U;
  uint8_t random_bytes_left = 0U;
  size_t cursor = 0U;
  for (size_t index = 0U; index < kTokenBytes; ++index) {
    if (random_bytes_left == 0U) {
      random_word = nextRandomWord();
      random_bytes_left = 4U;
    }
    const uint8_t byte_value = static_cast<uint8_t>(random_word & 0xFFU);
    random_word >>= 8U;
    --random_bytes_left;
    out_token[cursor++] = kHex[(byte_value >> 4U) & 0x0FU];
    out_token[cursor++] = kHex[byte_value & 0x0FU];
  }
  out_token[cursor] = '\0';
  return true;
}
