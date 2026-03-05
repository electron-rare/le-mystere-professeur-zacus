#include "props/EspNowBridge.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <algorithm>

EspNowBridge* EspNowBridge::instance_ = nullptr;

namespace {
constexpr size_t kEspNowMaxPayloadBytes = 240;
constexpr size_t kEspNowMaxPeers = 16;
constexpr const char* kDefaultEspNowDeviceName = "HOTLINE_PHONE";

void enforceEspNowCoexPolicy() {
    WiFi.setSleep(true);
    const esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
        Serial.printf("[EspNowBridge] warn: esp_wifi_set_ps(min_modem) failed err=0x%04x\n",
                      static_cast<unsigned>(err));
    }
}

bool isBroadcastTarget(const String& target) {
    return target.equalsIgnoreCase("broadcast");
}

bool parseTargetMac(const String& target, uint8_t out[6], bool& is_broadcast) {
    const String normalized = A252ConfigStore::normalizeMac(target);
    is_broadcast = false;
    if (isBroadcastTarget(target)) {
        is_broadcast = true;
        return true;
    }
    if (normalized.isEmpty()) {
        return false;
    }
    return A252ConfigStore::parseMac(normalized, out);
}

String normalizeOrDefaultDeviceName(const String& name) {
    const String normalized = A252ConfigStore::normalizeDeviceName(name);
    return normalized.isEmpty() ? String(kDefaultEspNowDeviceName) : normalized;
}

bool ensurePeerRegistered(const uint8_t mac[6]) {
    if (mac == nullptr) {
        return false;
    }
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    const esp_err_t err = esp_now_add_peer(&peer_info);
    return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}
}

EspNowBridge::EspNowBridge() {
    instance_ = this;
}

bool EspNowBridge::begin(const EspNowPeerStore& initial_peers) {
    if (ready_) {
        return true;
    }

    store_ = initial_peers;
    store_.device_name = normalizeOrDefaultDeviceName(store_.device_name);

    const wifi_mode_t current_mode = WiFi.getMode();
    if (current_mode == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
        delay(5);
    } else if (current_mode == WIFI_MODE_AP) {
        WiFi.mode(WIFI_AP_STA);
        delay(5);
    }
    enforceEspNowCoexPolicy();
    if (esp_now_init() != ESP_OK) {
        ready_ = false;
        return false;
    }
    enforceEspNowCoexPolicy();

    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    ready_ = true;

    std::vector<String> peers_copy = store_.peers;
    store_.peers.clear();
    for (const String& mac : peers_copy) {
        addPeerInternal(mac, false);
    }
    return true;
}

bool EspNowBridge::stop() {
    if (!ready_) {
        return true;
    }

    const esp_err_t err = esp_now_deinit();
    ready_ = false;
    return err == ESP_OK;
}

void EspNowBridge::tick() {
    // ESP-NOW uses callbacks, no polling required.
}

bool EspNowBridge::addPeer(const String& mac) {
    return addPeerInternal(mac, true);
}

bool EspNowBridge::deletePeer(const String& mac) {
    return deletePeerInternal(mac, true);
}

const std::vector<String>& EspNowBridge::peers() const {
    return store_.peers;
}

const String& EspNowBridge::deviceName() const {
    return store_.device_name;
}

bool EspNowBridge::setDeviceName(const String& name, bool persist) {
    const String normalized = A252ConfigStore::normalizeDeviceName(name);
    if (normalized.isEmpty()) {
        return false;
    }
    store_.device_name = normalized;
    if (persist) {
        return A252ConfigStore::saveEspNowPeers(store_);
    }
    return true;
}

bool EspNowBridge::sendJson(const String& target, const String& json_payload) {
    if (!ready_) {
        Serial.printf("[EspNowBridge] send rejected: bridge not started\n");
        tx_fail_++;
        return false;
    }

    String normalized_target = target;
    normalized_target.trim();
    if (normalized_target.isEmpty()) {
        Serial.printf("[EspNowBridge] send rejected: empty target\n");
        tx_fail_++;
        return false;
    }

    if (json_payload.length() > kEspNowMaxPayloadBytes) {
        Serial.printf("[EspNowBridge] send rejected: payload too large=%u bytes\n",
                      static_cast<unsigned>(json_payload.length()));
        tx_fail_++;
        return false;
    }

    bool is_broadcast = false;
    uint8_t target_mac[6] = {0};
    if (!parseTargetMac(normalized_target, target_mac, is_broadcast)) {
        Serial.printf("[EspNowBridge] send rejected: invalid target '%s'\n", normalized_target.c_str());
        tx_fail_++;
        return false;
    }

    if (!is_broadcast) {
        const String normalized_source = A252ConfigStore::normalizeMac(normalized_target);
        if (std::find(store_.peers.begin(), store_.peers.end(), normalized_source) == store_.peers.end()) {
            Serial.printf("[EspNowBridge] send rejected: target not configured '%s'\n", normalized_source.c_str());
            tx_fail_++;
            return false;
        }
    }

    if (is_broadcast) {
        const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return sendToMac(broadcast_mac, json_payload);
    }

    return sendToMac(target_mac, json_payload);
}

bool EspNowBridge::isReady() const {
    return ready_;
}

void EspNowBridge::setCommandCallback(std::function<void(const String&, const JsonVariantConst&)> cb) {
    command_callback_ = std::move(cb);
}

void EspNowBridge::statusToJson(JsonObject obj) const {
    obj["ready"] = ready_;
    obj["device_name"] = store_.device_name;
    obj["peer_count"] = static_cast<uint32_t>(store_.peers.size());
    obj["tx_ok"] = tx_ok_;
    obj["tx_fail"] = tx_fail_;
    obj["rx_count"] = rx_count_;
    obj["last_rx_mac"] = last_rx_mac_;
    obj["last_rx_payload"] = last_rx_payload_;

    JsonArray peers = obj["peers"].to<JsonArray>();
    for (const String& peer : store_.peers) {
        peers.add(peer);
    }
}

bool EspNowBridge::addPeerInternal(const String& mac, bool persist) {
    if (!ready_) {
        return false;
    }

    const String normalized = A252ConfigStore::normalizeMac(mac);
    if (normalized.isEmpty()) {
        return false;
    }

    if (std::find(store_.peers.begin(), store_.peers.end(), normalized) != store_.peers.end()) {
        return true;
    }

    if (store_.peers.size() >= kEspNowMaxPeers) {
        Serial.println("[EspNowBridge] peer rejected: max peers reached");
        return false;
    }

    uint8_t peer_mac[6] = {0};
    if (!A252ConfigStore::parseMac(normalized, peer_mac)) {
        return false;
    }

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, peer_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        return false;
    }

    store_.peers.push_back(normalized);
    if (persist) {
        A252ConfigStore::saveEspNowPeers(store_);
    }
    return true;
}

bool EspNowBridge::deletePeerInternal(const String& mac, bool persist) {
    if (!ready_) {
        return false;
    }

    const String normalized = A252ConfigStore::normalizeMac(mac);
    if (normalized.isEmpty()) {
        return false;
    }

    uint8_t peer_mac[6] = {0};
    if (!A252ConfigStore::parseMac(normalized, peer_mac)) {
        return false;
    }

    esp_now_del_peer(peer_mac);

    const auto it = std::remove(store_.peers.begin(), store_.peers.end(), normalized);
    const bool removed = it != store_.peers.end();
    store_.peers.erase(it, store_.peers.end());
    if (removed && persist) {
        A252ConfigStore::saveEspNowPeers(store_);
    }
    return removed;
}

bool EspNowBridge::sendToMac(const uint8_t mac[6], const String& payload) {
    if (!ready_) {
        return false;
    }

    if (payload.length() > kEspNowMaxPayloadBytes) {
        tx_fail_++;
        return false;
    }

    if (!ensurePeerRegistered(mac)) {
        tx_fail_++;
        return false;
    }

    const esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length());
    if (err != ESP_OK) {
        tx_fail_++;
        return false;
    }
    return true;
}

void EspNowBridge::onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (!instance_) {
        return;
    }

    char mac_buf[18] = {0};
    snprintf(mac_buf,
             sizeof(mac_buf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);

    if (len <= 0 || len > static_cast<int>(kEspNowMaxPayloadBytes)) {
        Serial.printf("[EspNowBridge] rx dropped: invalid len=%d (max=%u)\n",
                      len,
                      static_cast<unsigned>(kEspNowMaxPayloadBytes));
        return;
    }

    String payload;
    payload.reserve(len + 1);
    for (int i = 0; i < len; ++i) {
        payload += static_cast<char>(data[i]);
    }

    instance_->rx_count_++;
    instance_->last_rx_mac_ = mac_buf;
    instance_->last_rx_payload_ = payload;

    if (!instance_->command_callback_) {
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        doc.clear();
        doc["raw"] = payload;
    }
    instance_->command_callback_(String(mac_buf), doc.as<JsonVariantConst>());
}

void EspNowBridge::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (!instance_) {
        return;
    }
    if (status == ESP_NOW_SEND_SUCCESS) {
        instance_->tx_ok_++;
    } else {
        instance_->tx_fail_++;
    }
}
