// network_manager.cpp - WiFi + ESP-NOW runtime helpers for Freenove all-in-one.
#include "network_manager.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

NetworkManager* g_network_instance = nullptr;
constexpr uint8_t kBroadcastMac[6] = {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU};
constexpr char kBroadcastTarget[] = "broadcast";

void trimAsciiInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  const size_t len = std::strlen(text);
  size_t begin = 0U;
  while (begin < len && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = len;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U]))) {
    --end;
  }
  const size_t out_len = (end > begin) ? (end - begin) : 0U;
  if (begin > 0U && out_len > 0U) {
    std::memmove(text, &text[begin], out_len);
  }
  text[out_len] = '\0';
}

void enterCritical(portMUX_TYPE* mux, bool from_isr) {
  if (from_isr) {
    portENTER_CRITICAL_ISR(mux);
    return;
  }
  portENTER_CRITICAL(mux);
}

void exitCritical(portMUX_TYPE* mux, bool from_isr) {
  if (from_isr) {
    portEXIT_CRITICAL_ISR(mux);
    return;
  }
  portEXIT_CRITICAL(mux);
}

bool inIsrContext() {
#if defined(ARDUINO_ARCH_ESP32)
  return xPortInIsrContext();
#else
  return false;
#endif
}

bool timeReached(uint32_t now_ms, uint32_t target_ms) {
  return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

bool isBroadcastMac(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }
  for (uint8_t index = 0U; index < 6U; ++index) {
    if (mac[index] != 0xFFU) {
      return false;
    }
  }
  return true;
}

const char* inferEnvelopeType(const char* payload) {
  if (payload == nullptr || payload[0] == '\0') {
    return "empty";
  }
  if (std::strncmp(payload, "SC_EVENT", 8U) == 0 || std::strncmp(payload, "SERIAL:", 7U) == 0 ||
      std::strncmp(payload, "TIMER:", 6U) == 0 || std::strncmp(payload, "ACTION:", 7U) == 0 ||
      std::strcmp(payload, "UNLOCK") == 0 || std::strcmp(payload, "AUDIO_DONE") == 0) {
    return "story_event";
  }
  if (payload[0] == '{' || payload[0] == '[') {
    return "json";
  }
  return "text";
}

bool looksLikeEspNowEnvelope(JsonVariantConst root) {
  if (!root.is<JsonObjectConst>()) {
    return false;
  }
  JsonObjectConst object = root.as<JsonObjectConst>();
  return object["msg_id"].is<const char*>() && object["seq"].is<uint32_t>() && object["type"].is<const char*>() &&
         object.containsKey("payload") && object["ack"].is<bool>();
}

}  // namespace

bool NetworkManager::begin(const char* hostname) {
  if (started_) {
    return true;
  }

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  if (hostname != nullptr && hostname[0] != '\0') {
    WiFi.setHostname(hostname);
  }

  g_network_instance = this;
  started_ = true;
  refreshSnapshot();
  Serial.printf("[NET] wifi ready hostname=%s\n", (hostname != nullptr) ? hostname : "none");
  return true;
}

void NetworkManager::update(uint32_t now_ms) {
  if (!started_) {
    return;
  }

  const bool connected_to_local = isConnectedToLocalTarget();
  const bool was_retry_paused = local_retry_paused_;
  bool force_refresh = false;

  if (sta_connecting_) {
    if (WiFi.status() == WL_CONNECTED) {
      sta_connecting_ = false;
      force_refresh = true;
    } else if ((now_ms - sta_connect_requested_at_ms_) >= kStaConnectTimeoutMs) {
      sta_connecting_ = false;
      force_refresh = true;
    }
  }

  const bool should_force_fallback = shouldForceFallbackAp();
  if (should_force_fallback && !fallback_ap_active_ && fallback_ap_ssid_[0] != '\0') {
    fallback_ap_active_ = startApInternal(fallback_ap_ssid_, fallback_ap_password_, false);
    force_refresh = true;
  } else if (!should_force_fallback && fallback_ap_active_ && !manual_ap_active_) {
    WiFi.softAPdisconnect(true);
    fallback_ap_active_ = false;
    WiFi.mode(WIFI_STA);
    force_refresh = true;
  }

  const bool should_retry_local = local_target_ssid_[0] != '\0' &&
                                  (force_ap_if_not_local_ ? !connected_to_local : (WiFi.status() != WL_CONNECTED));
  const uint8_t ap_clients = (fallback_ap_active_ && !manual_ap_active_) ? WiFi.softAPgetStationNum() : 0U;
  local_retry_paused_ =
      should_retry_local && fallback_ap_active_ && pause_local_retry_when_ap_client_ && (ap_clients > 0U);
  if (local_retry_paused_ != was_retry_paused) {
    force_refresh = true;
  }

  if (should_retry_local) {
    if (local_retry_paused_) {
      if (next_local_retry_at_ms_ == 0U || timeReached(now_ms, next_local_retry_at_ms_)) {
        next_local_retry_at_ms_ = now_ms + local_retry_ms_;
        Serial.printf("[NET] local retry paused ap_clients=%u\n", ap_clients);
      }
    } else if (!sta_connecting_ && (next_local_retry_at_ms_ == 0U || timeReached(now_ms, next_local_retry_at_ms_))) {
      if (fallback_ap_active_ && equalsIgnoreCase(fallback_ap_ssid_, local_target_ssid_)) {
        // Avoid self-association when fallback AP and local target share the same SSID.
        WiFi.softAPdisconnect(true);
        fallback_ap_active_ = false;
        WiFi.mode(WIFI_STA);
        Serial.println("[NET] local retry paused fallback AP (same ssid)");
      }
      const bool started = connectSta(local_target_ssid_, local_target_password_);
      next_local_retry_at_ms_ = now_ms + local_retry_ms_;
      force_refresh = true;
      Serial.printf("[NET] local retry target=%s started=%u\n", local_target_ssid_, started ? 1U : 0U);
    }
  } else {
    next_local_retry_at_ms_ = 0U;
    local_retry_paused_ = false;
  }

  if (!force_refresh && (now_ms - last_refresh_ms_) < 350U) {
    return;
  }
  last_refresh_ms_ = now_ms;
  refreshSnapshot();
}

void NetworkManager::configureFallbackAp(const char* ssid, const char* password) {
  if (ssid != nullptr) {
    copyText(fallback_ap_ssid_, sizeof(fallback_ap_ssid_), ssid);
  }
  if (password != nullptr) {
    copyText(fallback_ap_password_, sizeof(fallback_ap_password_), password);
  }
  Serial.printf("[NET] fallback AP configured ssid=%s\n", fallback_ap_ssid_);
}

void NetworkManager::configureLocalPolicy(const char* ssid,
                                          const char* password,
                                          bool force_if_not_local,
                                          uint32_t retry_ms,
                                          bool pause_retry_when_ap_client) {
  if (ssid != nullptr) {
    copyText(local_target_ssid_, sizeof(local_target_ssid_), ssid);
  }
  if (password != nullptr) {
    copyText(local_target_password_, sizeof(local_target_password_), password);
  }
  force_ap_if_not_local_ = force_if_not_local;
  pause_local_retry_when_ap_client_ = pause_retry_when_ap_client;
  if (retry_ms >= 1000U) {
    local_retry_ms_ = retry_ms;
  }
  local_retry_paused_ = false;
  next_local_retry_at_ms_ = 0U;
  refreshSnapshot();
  Serial.printf("[NET] local policy target=%s force_ap_if_not_local=%u retry_ms=%lu pause_retry_on_ap_client=%u\n",
                local_target_ssid_,
                force_ap_if_not_local_ ? 1U : 0U,
                static_cast<unsigned long>(local_retry_ms_),
                pause_local_retry_when_ap_client_ ? 1U : 0U);
}

bool NetworkManager::connectSta(const char* ssid, const char* password) {
  if (!started_ && !begin(nullptr)) {
    return false;
  }
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }

  if (WiFi.status() == WL_CONNECTED && equalsIgnoreCase(WiFi.SSID().c_str(), ssid)) {
    sta_connecting_ = false;
    refreshSnapshot();
    return true;
  }

  const uint8_t mode = (manual_ap_active_ || fallback_ap_active_) ? WIFI_MODE_APSTA : WIFI_MODE_STA;
  WiFi.mode(static_cast<wifi_mode_t>(mode));
  WiFi.begin(ssid, (password != nullptr) ? password : "");
  copyText(snapshot_.sta_ssid, sizeof(snapshot_.sta_ssid), ssid);
  sta_connecting_ = true;
  sta_connect_requested_at_ms_ = millis();
  refreshSnapshot();
  Serial.printf("[NET] wifi connect requested ssid=%s\n", ssid);
  return true;
}

void NetworkManager::disconnectSta() {
  if (!started_) {
    return;
  }
  WiFi.disconnect(true, false);
  sta_connecting_ = false;
  local_retry_paused_ = false;
  next_local_retry_at_ms_ = 0U;
  snapshot_.sta_ssid[0] = '\0';
  if (shouldForceFallbackAp() && !manual_ap_active_ && fallback_ap_ssid_[0] != '\0') {
    fallback_ap_active_ = startApInternal(fallback_ap_ssid_, fallback_ap_password_, false);
  }
  refreshSnapshot();
  Serial.println("[NET] wifi disconnected");
}

bool NetworkManager::startAp(const char* ssid, const char* password) {
  return startApInternal(ssid, password, true);
}

bool NetworkManager::isConnectedToLocalTarget() const {
  if (local_target_ssid_[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (!equalsIgnoreCase(WiFi.SSID().c_str(), local_target_ssid_)) {
    return false;
  }
  return !isConnectedToSelfAp();
}

bool NetworkManager::isConnectedToSelfAp() const {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  const uint8_t* sta_bssid = WiFi.BSSID();
  if (sta_bssid == nullptr) {
    return false;
  }
  uint8_t ap_mac[6] = {0};
  WiFi.softAPmacAddress(ap_mac);
  return std::memcmp(sta_bssid, ap_mac, 6U) == 0;
}

bool NetworkManager::shouldForceFallbackAp() const {
  if (manual_ap_active_ || fallback_ap_ssid_[0] == '\0') {
    return false;
  }
  if (force_ap_if_not_local_ && local_target_ssid_[0] != '\0') {
    if (sta_connecting_) {
      return false;
    }
    return !isConnectedToLocalTarget();
  }
  if (sta_connecting_) {
    return false;
  }
  return WiFi.status() != WL_CONNECTED;
}

bool NetworkManager::startApInternal(const char* ssid, const char* password, bool manual_request) {
  if (!started_ && !begin(nullptr)) {
    return false;
  }
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  if (password != nullptr && password[0] != '\0' && std::strlen(password) < 8U) {
    Serial.println("[NET] AP password must be >= 8 chars");
    return false;
  }

  WiFi.mode(WIFI_AP_STA);
  bool ok = false;
  if (password != nullptr && password[0] != '\0') {
    ok = WiFi.softAP(ssid, password);
  } else {
    ok = WiFi.softAP(ssid);
  }
  if (ok) {
    copyText(snapshot_.ap_ssid, sizeof(snapshot_.ap_ssid), ssid);
    if (manual_request) {
      manual_ap_active_ = true;
      fallback_ap_active_ = false;
    } else {
      fallback_ap_active_ = true;
    }
  }
  refreshSnapshot();
  Serial.printf("[NET] AP %s ssid=%s mode=%s\n",
                ok ? "on" : "failed",
                ssid,
                manual_request ? "manual" : "fallback");
  return ok;
}

void NetworkManager::stopAp() {
  if (!started_) {
    return;
  }
  WiFi.softAPdisconnect(true);
  manual_ap_active_ = false;
  fallback_ap_active_ = false;
  local_retry_paused_ = false;
  if (WiFi.status() == WL_CONNECTED || sta_connecting_) {
    WiFi.mode(WIFI_STA);
  }
  snapshot_.ap_ssid[0] = '\0';
  refreshSnapshot();
  Serial.println("[NET] AP off");
}

bool NetworkManager::enableEspNow() {
  if (!started_ && !begin(nullptr)) {
    return false;
  }
  if (espnow_enabled_) {
    return true;
  }

  if (WiFi.getMode() == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("[NET] esp_now_init failed");
    return false;
  }
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_register_send_cb(onEspNowSend);
  espnow_enabled_ = true;
  refreshSnapshot();
  Serial.println("[NET] ESP-NOW ready");
  return true;
}

void NetworkManager::disableEspNow() {
  if (!espnow_enabled_) {
    return;
  }
  esp_now_deinit();
  enterCritical(&rx_queue_mux_, false);
  espnow_enabled_ = false;
  peer_cache_count_ = 0U;
  rx_queue_head_ = 0U;
  rx_queue_tail_ = 0U;
  rx_queue_count_ = 0U;
  snapshot_.last_peer[0] = '\0';
  snapshot_.last_rx_peer[0] = '\0';
  snapshot_.last_msg_id[0] = '\0';
  snapshot_.last_type[0] = '\0';
  snapshot_.last_payload[0] = '\0';
  snapshot_.espnow_last_seq = 0U;
  snapshot_.espnow_last_ack = false;
  exitCritical(&rx_queue_mux_, false);
  refreshSnapshot();
  Serial.println("[NET] ESP-NOW off");
}

bool NetworkManager::parseMac(const char* text, uint8_t out_mac[6]) const {
  if (text == nullptr || out_mac == nullptr) {
    return false;
  }

  char compact[13] = {0};
  size_t cursor = 0U;
  for (size_t index = 0U; text[index] != '\0'; ++index) {
    const char ch = text[index];
    if (std::isxdigit(static_cast<unsigned char>(ch))) {
      if (cursor >= 12U) {
        return false;
      }
      compact[cursor++] = ch;
      continue;
    }
    if (ch == ':' || ch == '-' || ch == ' ') {
      continue;
    }
    return false;
  }
  if (cursor != 12U) {
    return false;
  }

  bool ok = true;
  for (uint8_t idx = 0U; idx < 6U; ++idx) {
    out_mac[idx] = parseHexByte(compact[idx * 2U], compact[idx * 2U + 1U], &ok);
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool NetworkManager::addEspNowPeer(const char* mac_text) {
  if (mac_text == nullptr || mac_text[0] == '\0') {
    return false;
  }
  if (!ensureEspNowReady()) {
    return false;
  }
  uint8_t mac[6] = {0};
  if (!parseMac(mac_text, mac)) {
    return false;
  }
  if (!addEspNowPeerInternal(mac)) {
    return false;
  }
  cachePeer(mac);
  refreshSnapshot();
  return true;
}

bool NetworkManager::removeEspNowPeer(const char* mac_text) {
  if (mac_text == nullptr || mac_text[0] == '\0') {
    return false;
  }
  if (!ensureEspNowReady()) {
    return false;
  }
  uint8_t mac[6] = {0};
  if (!parseMac(mac_text, mac)) {
    return false;
  }
  if (!removeEspNowPeerInternal(mac)) {
    return false;
  }
  forgetPeer(mac);
  refreshSnapshot();
  return true;
}

uint8_t NetworkManager::espNowPeerCount() const {
  enterCritical(&rx_queue_mux_, false);
  const uint8_t count = peer_cache_count_;
  exitCritical(&rx_queue_mux_, false);
  return count;
}

bool NetworkManager::espNowPeerAt(uint8_t index, char* out_mac, size_t out_capacity) const {
  if (out_mac == nullptr || out_capacity == 0U) {
    return false;
  }
  bool ok = false;
  enterCritical(&rx_queue_mux_, false);
  if (index < peer_cache_count_) {
    copyText(out_mac, out_capacity, peer_cache_[index]);
    ok = true;
  }
  exitCritical(&rx_queue_mux_, false);
  return ok;
}

bool NetworkManager::sendEspNowText(const uint8_t mac[6], const char* text) {
  if (!ensureEspNowReady()) {
    return false;
  }
  if (mac == nullptr || text == nullptr || text[0] == '\0') {
    return false;
  }
  const size_t payload_len = std::strlen(text);
  if (payload_len == 0U || payload_len > kEspNowFrameCapacity) {
    Serial.printf("[NET] ESP-NOW payload too large: %u bytes\n", static_cast<unsigned int>(payload_len));
    return false;
  }

  if (!isBroadcastMac(mac)) {
    if (!addEspNowPeerInternal(mac)) {
      if (ensureEspNowReady() && addEspNowPeerInternal(mac)) {
        // recovered
      } else {
        Serial.println("[NET] ESP-NOW add peer failed");
        return false;
      }
    }
  } else {
    // ESP-NOW broadcast still needs an explicit peer on some SDK versions.
    if (!addEspNowPeerInternal(mac)) {
      if (!(ensureEspNowReady() && addEspNowPeerInternal(mac))) {
        Serial.println("[NET] ESP-NOW add peer failed");
      }
    }
  }

  esp_err_t err = esp_now_send(mac,
                               reinterpret_cast<const uint8_t*>(text),
                               payload_len);
  if (err == ESP_ERR_ESPNOW_NOT_INIT) {
    // WiFi mode switches can deinit ESP-NOW internally: recover once, then retry the same payload.
    espnow_enabled_ = false;
    if (enableEspNow()) {
      addEspNowPeerInternal(mac);
      err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(text), payload_len);
    }
  }
  if (err != ESP_OK) {
    enterCritical(&rx_queue_mux_, false);
    ++espnow_tx_fail_;
    exitCritical(&rx_queue_mux_, false);
    Serial.printf("[NET] ESP-NOW send failed err=%d\n", static_cast<int>(err));
    return false;
  }
  cachePeer(mac);
  return true;
}

bool NetworkManager::ensureEspNowReady() {
  if (!espnow_enabled_) {
    return enableEspNow();
  }

  esp_now_peer_num_t peer_num = {};
  const esp_err_t err = esp_now_get_peer_num(&peer_num);
  if (err == ESP_OK) {
    return true;
  }
  if (err == ESP_ERR_ESPNOW_NOT_INIT) {
    espnow_enabled_ = false;
    return enableEspNow();
  }
  Serial.printf("[NET] ESP-NOW health check err=%d\n", static_cast<int>(err));
  return false;
}

bool NetworkManager::sendEspNowTarget(const char* target, const char* text) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  char frame[kEspNowFrameCapacity + 1U] = {0};
  copyText(frame, sizeof(frame), text);
  trimAsciiInPlace(frame);
  if (frame[0] == '\0') {
    return false;
  }

  bool is_envelope = false;
  if (frame[0] == '{') {
    StaticJsonDocument<512> document;
    if (!deserializeJson(document, frame) && looksLikeEspNowEnvelope(document.as<JsonVariantConst>())) {
      is_envelope = true;
    }
  }
  if (!is_envelope) {
    StaticJsonDocument<512> envelope;
    ++espnow_tx_seq_;
    char msg_id[32] = {0};
    snprintf(msg_id,
             sizeof(msg_id),
             "M%08lX%06lu",
             static_cast<unsigned long>(millis()),
             static_cast<unsigned long>(espnow_tx_seq_));
    envelope["msg_id"] = msg_id;
    envelope["seq"] = espnow_tx_seq_;
    envelope["type"] = inferEnvelopeType(frame);
    envelope["payload"] = frame;
    envelope["ack"] = false;
    char encoded[kEspNowFrameCapacity + 1U] = {0};
    const size_t encoded_size = serializeJson(envelope, encoded, sizeof(encoded));
    if (encoded_size == 0U || encoded_size >= sizeof(encoded)) {
      Serial.println("[NET] ESP-NOW envelope too large");
      return false;
    }
    copyText(frame, sizeof(frame), encoded);
  }

  if (target != nullptr && target[0] != '\0' && !equalsIgnoreCase(target, kBroadcastTarget)) {
    Serial.printf("[NET] ESP-NOW target ignored; using broadcast target=%s\n", target);
  }
  return sendEspNowText(kBroadcastMac, frame);
}

NetworkManager::Snapshot NetworkManager::snapshot() const {
  enterCritical(&rx_queue_mux_, false);
  const Snapshot snapshot_copy = snapshot_;
  exitCritical(&rx_queue_mux_, false);
  return snapshot_copy;
}

bool NetworkManager::consumeEspNowMessage(char* out_payload,
                                          size_t payload_capacity,
                                          char* out_peer,
                                          size_t peer_capacity,
                                          char* out_msg_id,
                                          size_t msg_id_capacity,
                                          uint32_t* out_seq,
                                          char* out_type,
                                          size_t type_capacity,
                                          bool* out_ack_requested) {
  for (;;) {
    EspNowMessage entry = {};
    enterCritical(&rx_queue_mux_, false);
    if (rx_queue_count_ == 0U) {
      exitCritical(&rx_queue_mux_, false);
      return false;
    }
    entry = rx_queue_[rx_queue_head_];
    rx_queue_head_ = static_cast<uint8_t>((rx_queue_head_ + 1U) % kRxQueueSize);
    --rx_queue_count_;
    exitCritical(&rx_queue_mux_, false);

    char normalized_payload[kPayloadCapacity] = {0};
    copyText(normalized_payload, sizeof(normalized_payload), entry.payload);
    char msg_id[32] = {0};
    uint32_t seq = 0U;
    char envelope_type[24] = {0};
    bool ack_requested = false;

    if (entry.payload[0] == '{') {
      StaticJsonDocument<512> document;
      if (!deserializeJson(document, entry.payload) && looksLikeEspNowEnvelope(document.as<JsonVariantConst>())) {
        JsonVariantConst root = document.as<JsonVariantConst>();
        copyText(msg_id, sizeof(msg_id), root["msg_id"] | "");
        seq = root["seq"] | 0U;
        copyText(envelope_type, sizeof(envelope_type), root["type"] | "");
        const bool envelope_ack = root["ack"] | false;
        const bool ack_response = envelope_ack && std::strcmp(envelope_type, "ack") == 0;
        if (ack_response) {
          continue;
        }
        ack_requested = envelope_ack;
        if (root["payload"].is<const char*>()) {
          copyText(normalized_payload, sizeof(normalized_payload), root["payload"].as<const char*>());
        } else if (!root["payload"].isNull()) {
          char payload_text[kPayloadCapacity] = {0};
          const size_t payload_size = serializeJson(root["payload"], payload_text, sizeof(payload_text));
          if (payload_size > 0U) {
            copyText(normalized_payload, sizeof(normalized_payload), payload_text);
          }
        }
        if (envelope_type[0] == '\0') {
          copyText(envelope_type, sizeof(envelope_type), inferEnvelopeType(normalized_payload));
        }
      }
    }
    if (envelope_type[0] == '\0') {
      copyText(envelope_type, sizeof(envelope_type), inferEnvelopeType(normalized_payload));
    }

    if (out_payload != nullptr && payload_capacity > 0U) {
      copyText(out_payload, payload_capacity, normalized_payload);
    }
    if (out_peer != nullptr && peer_capacity > 0U) {
      copyText(out_peer, peer_capacity, entry.peer);
    }
    if (out_msg_id != nullptr && msg_id_capacity > 0U) {
      copyText(out_msg_id, msg_id_capacity, msg_id);
    }
    if (out_seq != nullptr) {
      *out_seq = seq;
    }
    if (out_type != nullptr && type_capacity > 0U) {
      copyText(out_type, type_capacity, envelope_type);
    }
    if (out_ack_requested != nullptr) {
      *out_ack_requested = ack_requested;
    }
    return true;
  }
}

void NetworkManager::onEspNowRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
  if (g_network_instance == nullptr) {
    return;
  }
  g_network_instance->handleEspNowRecv(mac_addr, data, data_len);
}

void NetworkManager::onEspNowSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
  if (g_network_instance == nullptr) {
    return;
  }
  g_network_instance->handleEspNowSend(mac_addr, status);
}

uint8_t NetworkManager::parseHexByte(char high, char low, bool* ok) {
  auto nibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
      return 10 + (ch - 'A');
    }
    if (ch >= 'a' && ch <= 'f') {
      return 10 + (ch - 'a');
    }
    return -1;
  };
  const int hi = nibble(high);
  const int lo = nibble(low);
  if (hi < 0 || lo < 0) {
    if (ok != nullptr) {
      *ok = false;
    }
    return 0U;
  }
  if (ok != nullptr) {
    *ok = true;
  }
  return static_cast<uint8_t>((hi << 4) | lo);
}

void NetworkManager::copyText(char* out, size_t out_size, const char* text) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (text == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, text, out_size - 1U);
  out[out_size - 1U] = '\0';
}

void NetworkManager::formatMac(const uint8_t* mac, char* out, size_t out_size) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (mac == nullptr) {
    copyText(out, out_size, "00:00:00:00:00:00");
    return;
  }
  snprintf(out,
           out_size,
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]);
}

bool NetworkManager::equalsIgnoreCase(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  size_t index = 0U;
  while (lhs[index] != '\0' && rhs[index] != '\0') {
    const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[index])));
    const char r = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[index])));
    if (l != r) {
      return false;
    }
    ++index;
  }
  return lhs[index] == '\0' && rhs[index] == '\0';
}

const char* NetworkManager::wifiModeLabel(uint8_t mode) {
  switch (mode) {
    case WIFI_MODE_STA:
      return "STA";
    case WIFI_MODE_AP:
      return "AP";
    case WIFI_MODE_APSTA:
      return "AP_STA";
    default:
      return "OFF";
  }
}

const char* NetworkManager::networkStateLabel(bool sta_connected,
                                              bool sta_connecting,
                                              bool ap_enabled,
                                              bool fallback_ap_active) {
  if (sta_connected) {
    return "connected";
  }
  if (sta_connecting) {
    return "connecting";
  }
  if (ap_enabled && fallback_ap_active) {
    return "ap_fallback";
  }
  if (ap_enabled) {
    return "ap";
  }
  return "idle";
}

bool NetworkManager::addEspNowPeerInternal(const uint8_t mac[6]) {
  if (!espnow_enabled_ || mac == nullptr) {
    return false;
  }
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peer = {};
  std::memcpy(peer.peer_addr, mac, 6U);
  peer.channel = 0U;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

bool NetworkManager::removeEspNowPeerInternal(const uint8_t mac[6]) {
  if (!espnow_enabled_ || mac == nullptr) {
    return false;
  }
  if (!esp_now_is_peer_exist(mac)) {
    return true;
  }
  const esp_err_t err = esp_now_del_peer(mac);
  return err == ESP_OK;
}

void NetworkManager::cachePeer(const uint8_t mac[6], bool from_isr) {
  if (mac == nullptr) {
    return;
  }
  // Do not treat FF:FF:FF:FF:FF:FF as a discovered runtime peer.
  if (isBroadcastMac(mac)) {
    return;
  }
  char peer_text[18] = {0};
  formatMac(mac, peer_text, sizeof(peer_text));
  if (peer_text[0] == '\0') {
    return;
  }
  enterCritical(&rx_queue_mux_, from_isr);
  for (uint8_t index = 0U; index < peer_cache_count_; ++index) {
    if (std::strcmp(peer_cache_[index], peer_text) == 0) {
      exitCritical(&rx_queue_mux_, from_isr);
      return;
    }
  }
  if (peer_cache_count_ < kMaxPeerCache) {
    copyText(peer_cache_[peer_cache_count_], sizeof(peer_cache_[peer_cache_count_]), peer_text);
    ++peer_cache_count_;
    exitCritical(&rx_queue_mux_, from_isr);
    return;
  }
  for (uint8_t index = 1U; index < kMaxPeerCache; ++index) {
    copyText(peer_cache_[index - 1U], sizeof(peer_cache_[index - 1U]), peer_cache_[index]);
  }
  copyText(peer_cache_[kMaxPeerCache - 1U], sizeof(peer_cache_[kMaxPeerCache - 1U]), peer_text);
  exitCritical(&rx_queue_mux_, from_isr);
}

void NetworkManager::forgetPeer(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return;
  }
  char peer_text[18] = {0};
  formatMac(mac, peer_text, sizeof(peer_text));
  if (peer_text[0] == '\0') {
    return;
  }
  enterCritical(&rx_queue_mux_, false);
  if (peer_cache_count_ == 0U) {
    exitCritical(&rx_queue_mux_, false);
    return;
  }
  for (uint8_t index = 0U; index < peer_cache_count_; ++index) {
    if (std::strcmp(peer_cache_[index], peer_text) != 0) {
      continue;
    }
    for (uint8_t move = index + 1U; move < peer_cache_count_; ++move) {
      copyText(peer_cache_[move - 1U], sizeof(peer_cache_[move - 1U]), peer_cache_[move]);
    }
    peer_cache_[peer_cache_count_ - 1U][0] = '\0';
    --peer_cache_count_;
    exitCritical(&rx_queue_mux_, false);
    return;
  }
  exitCritical(&rx_queue_mux_, false);
}

bool NetworkManager::queueEspNowMessage(const char* payload,
                                        const char* peer,
                                        const char* msg_id,
                                        uint32_t seq,
                                        const char* type,
                                        bool ack_requested,
                                        bool from_isr) {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }
  enterCritical(&rx_queue_mux_, from_isr);
  if (rx_queue_count_ >= kRxQueueSize) {
    rx_queue_head_ = static_cast<uint8_t>((rx_queue_head_ + 1U) % kRxQueueSize);
    --rx_queue_count_;
    ++espnow_drop_packets_;
  }
  EspNowMessage& slot = rx_queue_[rx_queue_tail_];
  copyText(slot.payload, sizeof(slot.payload), payload);
  copyText(slot.peer, sizeof(slot.peer), peer);
  copyText(slot.msg_id, sizeof(slot.msg_id), msg_id);
  copyText(slot.type, sizeof(slot.type), type);
  slot.seq = seq;
  slot.ack_requested = ack_requested;
  rx_queue_tail_ = static_cast<uint8_t>((rx_queue_tail_ + 1U) % kRxQueueSize);
  ++rx_queue_count_;
  exitCritical(&rx_queue_mux_, from_isr);
  return true;
}

void NetworkManager::refreshSnapshot() {
  const wl_status_t wifi_status = WiFi.status();
  const wifi_mode_t mode = WiFi.getMode();
  const bool local_match = isConnectedToLocalTarget();
  const bool sta_connected = (wifi_status == WL_CONNECTED);
  const bool ap_enabled = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  const bool fallback_ap_active =
      fallback_ap_active_ && !manual_ap_active_ && ap_enabled && !local_match;
  const int32_t rssi = sta_connected ? WiFi.RSSI() : 0;

  char sta_ssid[33] = {0};
  char ap_ssid[33] = {0};
  char local_target[33] = {0};
  char ip[20] = "0.0.0.0";
  char mode_label[12] = {0};
  char state_label[16] = {0};
  uint8_t ap_clients = 0U;

  copyText(local_target, sizeof(local_target), local_target_ssid_);
  copyText(mode_label, sizeof(mode_label), wifiModeLabel(static_cast<uint8_t>(mode)));
  copyText(state_label,
           sizeof(state_label),
           networkStateLabel(sta_connected, sta_connecting_, ap_enabled, fallback_ap_active));
  if (sta_connected) {
    copyText(sta_ssid, sizeof(sta_ssid), WiFi.SSID().c_str());
    copyText(ip, sizeof(ip), WiFi.localIP().toString().c_str());
  } else if (ap_enabled) {
    copyText(ip, sizeof(ip), WiFi.softAPIP().toString().c_str());
  }
  if (ap_enabled) {
    copyText(ap_ssid, sizeof(ap_ssid), WiFi.softAPSSID().c_str());
    ap_clients = WiFi.softAPgetStationNum();
  }

  enterCritical(&rx_queue_mux_, false);
  snapshot_.ready = started_;
  snapshot_.sta_connected = sta_connected;
  snapshot_.sta_connecting = sta_connecting_;
  snapshot_.ap_enabled = ap_enabled;
  snapshot_.espnow_enabled = espnow_enabled_;
  snapshot_.local_match = local_match;
  snapshot_.fallback_ap_active = fallback_ap_active;
  snapshot_.local_retry_paused = local_retry_paused_;
  snapshot_.rssi = rssi;
  copyText(snapshot_.local_target, sizeof(snapshot_.local_target), local_target);
  copyText(snapshot_.mode, sizeof(snapshot_.mode), mode_label);
  copyText(snapshot_.state, sizeof(snapshot_.state), state_label);
  copyText(snapshot_.sta_ssid, sizeof(snapshot_.sta_ssid), sta_ssid);
  copyText(snapshot_.ap_ssid, sizeof(snapshot_.ap_ssid), ap_ssid);
  copyText(snapshot_.ip, sizeof(snapshot_.ip), ip);
  snapshot_.ap_clients = ap_clients;
  snapshot_.espnow_peer_count = peer_cache_count_;
  snapshot_.espnow_rx_packets = espnow_rx_packets_;
  snapshot_.espnow_tx_ok = espnow_tx_ok_;
  snapshot_.espnow_tx_fail = espnow_tx_fail_;
  snapshot_.espnow_drop_packets = espnow_drop_packets_;
  exitCritical(&rx_queue_mux_, false);
}

void NetworkManager::handleEspNowRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
  const bool from_isr = inIsrContext();
  char peer_text[18] = {0};
  formatMac(mac_addr, peer_text, sizeof(peer_text));

  char payload[kPayloadCapacity] = {0};
  const int safe_len = (data_len > 0) ? data_len : 0;
  const size_t copy_len = (static_cast<size_t>(safe_len) < (sizeof(payload) - 1U)) ? static_cast<size_t>(safe_len)
                                                                                    : (sizeof(payload) - 1U);
  if (data != nullptr && copy_len > 0U) {
    std::memcpy(payload, data, copy_len);
  }
  payload[copy_len] = '\0';
  cachePeer(mac_addr, from_isr);
  enterCritical(&rx_queue_mux_, from_isr);
  ++espnow_rx_packets_;
  copyText(snapshot_.last_peer, sizeof(snapshot_.last_peer), peer_text);
  copyText(snapshot_.last_rx_peer, sizeof(snapshot_.last_rx_peer), peer_text);
  snapshot_.espnow_last_seq = 0U;
  snapshot_.espnow_last_ack = false;
  snapshot_.last_msg_id[0] = '\0';
  copyText(snapshot_.last_type, sizeof(snapshot_.last_type), inferEnvelopeType(payload));
  copyText(snapshot_.last_payload, sizeof(snapshot_.last_payload), payload);
  exitCritical(&rx_queue_mux_, from_isr);
  queueEspNowMessage(payload, peer_text, "", 0U, "", false, from_isr);
}

void NetworkManager::handleEspNowSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
  const bool from_isr = inIsrContext();
  cachePeer(mac_addr, from_isr);
  enterCritical(&rx_queue_mux_, from_isr);
  if (status == ESP_NOW_SEND_SUCCESS) {
    ++espnow_tx_ok_;
  } else {
    ++espnow_tx_fail_;
  }
  formatMac(mac_addr, snapshot_.last_peer, sizeof(snapshot_.last_peer));
  exitCritical(&rx_queue_mux_, from_isr);
}
