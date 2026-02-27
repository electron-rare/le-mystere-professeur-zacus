#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>
#include <ArduinoJson.h>

struct WifiStatusSnapshot {
    bool connected = false;
    bool has_credentials = false;
    String ssid;
    String ip;
    int32_t rssi = 0;
    int32_t channel = 0;
    String bssid;
    String state;
    bool ap_active = false;
    String ap_ssid;
    String ap_ip;
    String mode;
};

class WifiManager {
public:
    WifiManager();

    bool begin(const char* ssid, const char* password, uint32_t timeout_ms = 10000);
    bool connect(const String& ssid, const String& password, uint32_t timeout_ms = 10000,
                 bool persist = true);
    bool reconnect(uint32_t timeout_ms = 10000);
    void disconnect(bool erase_credentials = false);
    void loop();
    void ensureFallbackAp();

    bool isConnected() const;
    bool hasCredentials() const;
    WifiStatusSnapshot status() const;
    void statusToJson(JsonObject obj) const;
    void scanToJson(JsonArray arr, int max_networks = 20) const;

private:
    void enforceCoexPolicy() const;

    bool connected_;
    String ssid_;
    String password_;
    bool ap_active_;
    String ap_ssid_;
    String ap_password_;
    mutable uint32_t next_auto_reconnect_ms_;
    uint32_t reconnect_backoff_ms_;
    uint32_t next_coex_reassert_ms_;

    bool startFallbackAp();
    void stopFallbackAp();
    bool waitForConnection(uint32_t timeout_ms);
};

#endif  // WIFIMANAGER_H
