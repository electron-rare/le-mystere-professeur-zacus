#ifndef PROPS_ESPNOW_BRIDGE_H
#define PROPS_ESPNOW_BRIDGE_H


#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_now.h>

#include <functional>
#include <vector>

#include "config/A252ConfigStore.h"

class EspNowBridge {
public:
    EspNowBridge();

    bool begin(const EspNowPeerStore& initial_peers);
    bool stop();
    void tick();

    bool addPeer(const String& mac);
    bool deletePeer(const String& mac);
    const std::vector<String>& peers() const;
    const String& deviceName() const;
    bool setDeviceName(const String& name, bool persist = true);

    bool sendJson(const String& target, const String& json_payload);
    bool isReady() const;

    void setCommandCallback(std::function<void(const String&, const JsonVariantConst&)> cb);
    void statusToJson(JsonObject obj) const;

private:
    bool addPeerInternal(const String& normalized_mac, bool persist);
    bool deletePeerInternal(const String& normalized_mac, bool persist);
    bool sendToMac(const uint8_t mac[6], const String& payload);

    static void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len);
    static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);

    static EspNowBridge* instance_;

    bool ready_ = false;
    EspNowPeerStore store_;
    std::function<void(const String&, const JsonVariantConst&)> command_callback_;

    uint32_t tx_ok_ = 0;
    uint32_t tx_fail_ = 0;
    uint32_t rx_count_ = 0;
    String last_rx_mac_;
    String last_rx_payload_;
};

#endif  // PROPS_ESPNOW_BRIDGE_H
