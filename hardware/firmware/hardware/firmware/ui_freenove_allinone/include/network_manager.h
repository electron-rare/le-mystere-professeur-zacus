// network_manager.h - WiFi + ESP-NOW runtime helpers for Freenove all-in-one.
#pragma once

#include <Arduino.h>
#include <esp_now.h>

class NetworkManager {
 public:
  struct Snapshot {
    bool ready = false;
    bool sta_connected = false;
    bool sta_connecting = false;
    bool ap_enabled = false;
    bool espnow_enabled = false;
    bool fallback_ap_active = false;
    bool local_match = false;
    bool local_retry_paused = false;
    char state[16] = "idle";
    char mode[12] = "OFF";
    char sta_ssid[33] = {0};
    char ap_ssid[33] = {0};
    char local_target[33] = {0};
    char ip[20] = "0.0.0.0";
    int32_t rssi = 0;
    uint8_t ap_clients = 0U;
    uint8_t espnow_peer_count = 0U;
    uint32_t espnow_rx_packets = 0U;
    uint32_t espnow_tx_ok = 0U;
    uint32_t espnow_tx_fail = 0U;
    uint32_t espnow_drop_packets = 0U;
    char last_peer[18] = {0};
    char last_rx_peer[18] = {0};
    char last_payload[128] = {0};
  };

  bool begin(const char* hostname);
  void update(uint32_t now_ms);

  void configureFallbackAp(const char* ssid, const char* password);
  void configureLocalPolicy(const char* ssid,
                            const char* password,
                            bool force_if_not_local,
                            uint32_t retry_ms,
                            bool pause_retry_when_ap_client);
  bool connectSta(const char* ssid, const char* password);
  void disconnectSta();
  bool startAp(const char* ssid, const char* password);
  void stopAp();

  bool enableEspNow();
  void disableEspNow();
  bool parseMac(const char* text, uint8_t out_mac[6]) const;
  bool addEspNowPeer(const char* mac_text);
  bool removeEspNowPeer(const char* mac_text);
  uint8_t espNowPeerCount() const;
  bool espNowPeerAt(uint8_t index, char* out_mac, size_t out_capacity) const;
  bool sendEspNowText(const uint8_t mac[6], const char* text);
  bool sendEspNowTarget(const char* target, const char* text);

  Snapshot snapshot() const;
  bool consumeEspNowMessage(char* out_payload,
                            size_t payload_capacity,
                            char* out_peer,
                            size_t peer_capacity);

 private:
  static void onEspNowRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len);
  static void onEspNowSend(const uint8_t* mac_addr, esp_now_send_status_t status);

  static uint8_t parseHexByte(char high, char low, bool* ok);
  static void copyText(char* out, size_t out_size, const char* text);
  static void formatMac(const uint8_t* mac, char* out, size_t out_size);
  static bool equalsIgnoreCase(const char* lhs, const char* rhs);
  static const char* wifiModeLabel(uint8_t mode);
  static const char* networkStateLabel(bool sta_connected,
                                       bool sta_connecting,
                                       bool ap_enabled,
                                       bool fallback_ap_active);

  bool startApInternal(const char* ssid, const char* password, bool manual_request);
  bool isConnectedToSelfAp() const;
  bool isConnectedToLocalTarget() const;
  bool shouldForceFallbackAp() const;
  bool ensureEspNowReady();
  bool addEspNowPeerInternal(const uint8_t mac[6]);
  bool removeEspNowPeerInternal(const uint8_t mac[6]);
  void cachePeer(const uint8_t mac[6]);
  void forgetPeer(const uint8_t mac[6]);
  bool queueEspNowMessage(const char* payload, const char* peer);
  void refreshSnapshot();
  void handleEspNowRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len);
  void handleEspNowSend(const uint8_t* mac_addr, esp_now_send_status_t status);

  static constexpr uint8_t kMaxPeerCache = 16U;
  static constexpr uint8_t kRxQueueSize = 6U;
  static constexpr size_t kPayloadCapacity = 128U;
  static constexpr uint32_t kStaConnectTimeoutMs = 12000U;

  bool started_ = false;
  bool espnow_enabled_ = false;
  bool sta_connecting_ = false;
  bool manual_ap_active_ = false;
  bool fallback_ap_active_ = false;
  bool force_ap_if_not_local_ = true;
  bool pause_local_retry_when_ap_client_ = false;
  bool local_retry_paused_ = false;
  uint32_t last_refresh_ms_ = 0U;
  uint32_t sta_connect_requested_at_ms_ = 0U;
  uint32_t next_local_retry_at_ms_ = 0U;
  uint32_t espnow_rx_packets_ = 0U;
  uint32_t espnow_tx_ok_ = 0U;
  uint32_t espnow_tx_fail_ = 0U;
  uint32_t espnow_drop_packets_ = 0U;
  uint32_t local_retry_ms_ = 15000U;

  char local_target_ssid_[33] = "Les cils";
  char local_target_password_[65] = "mascarade";
  char fallback_ap_ssid_[33] = "Les cils";
  char fallback_ap_password_[65] = "mascarade";

  char peer_cache_[kMaxPeerCache][18] = {};
  uint8_t peer_cache_count_ = 0U;

  struct EspNowMessage {
    char payload[kPayloadCapacity] = {0};
    char peer[18] = {0};
  };
  EspNowMessage rx_queue_[kRxQueueSize];
  uint8_t rx_queue_head_ = 0U;
  uint8_t rx_queue_tail_ = 0U;
  uint8_t rx_queue_count_ = 0U;

  Snapshot snapshot_;
};
