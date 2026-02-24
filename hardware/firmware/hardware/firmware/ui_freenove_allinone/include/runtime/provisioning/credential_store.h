// credential_store.h - NVS-backed Wi-Fi/WebUI credential persistence.
#pragma once

#include <Arduino.h>

class CredentialStore {
 public:
  bool loadStaCredentials(char* out_ssid,
                          size_t ssid_capacity,
                          char* out_password,
                          size_t password_capacity) const;
  bool saveStaCredentials(const char* ssid, const char* password) const;
  bool clearStaCredentials() const;

  bool loadWebToken(char* out_token, size_t token_capacity) const;
  bool saveWebToken(const char* token) const;
  bool clearWebToken() const;

  bool isProvisioned() const;
  bool setProvisioned(bool provisioned) const;

  bool generateWebToken(char* out_token, size_t token_capacity) const;
};
