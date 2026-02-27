#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <espnow.h>

extern "C" {
#include <user_interface.h>
}

namespace {

constexpr char kDeviceName[] = "OSCILLO-8266";

constexpr char kDefaultSsid[] = "Les cils";
constexpr char kDefaultPass[] = "mascarade";

constexpr char kFallbackApSsid[] = "OSCILLO_AP";
constexpr char kFallbackApPass[] = "oscillo42";
constexpr char kMorseText[] = "LEFOU";

// ESP8266 has one ADC channel only.
constexpr uint8_t kJoyAdcPin = A0;
constexpr uint8_t kJoySwPin = D3;
// Reserve D4 for Morse output, disable dedicated status LED pin.
constexpr uint8_t kStatusLedPin = 0xFF;
constexpr bool kStatusLedActiveLow = true;
// Morse output pin (bootstrap pin: keep external load high-impedance on boot).
constexpr uint8_t kMorseOutPin = D4;
// 4-bit pseudo DAC outputs (R-2R/resistor summer + RC filter recommended).
constexpr uint8_t kPcm4BitPins[4] = {D5, D6, D7, D8};

#if defined(OLED_ALT_D5_D6)
constexpr uint8_t kOledSdaPin = D5;
constexpr uint8_t kOledSclPin = D6;
constexpr char kOledBusLabel[] = "D5/D6";
#elif defined(OLED_SDA_PIN) && defined(OLED_SCL_PIN)
constexpr uint8_t kOledSdaPin = OLED_SDA_PIN;
constexpr uint8_t kOledSclPin = OLED_SCL_PIN;
constexpr char kOledBusLabel[] = "DA(D1)/D2";
#else
constexpr uint8_t kOledSdaPin = D1;
constexpr uint8_t kOledSclPin = D2;
constexpr char kOledBusLabel[] = "DA(D1)/D2";
#endif

constexpr uint8_t kOledWidth = 128;
constexpr uint8_t kOledHeight = 64;

constexpr uint32_t kStaConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kWifiScanCacheMs = 15000;
constexpr size_t kMaxScanEntries = 24;

constexpr uint32_t kJoystickPollMs = 200;
constexpr uint32_t kOledRefreshMs = 200;
constexpr uint32_t kLedBlinkFastMs = 180;
constexpr uint32_t kLedBlinkSlowMs = 500;
constexpr uint32_t kLedHeartbeatPeriodMs = 1200;
constexpr uint32_t kLedHeartbeatOnMs = 70;
constexpr uint32_t kLogPeriodMs = 1500;
constexpr uint16_t kMorseUnitMs = 350;
constexpr uint32_t kSineSamplePeriodUs = 300;
constexpr uint32_t kScopeSampleMs = 40;
constexpr uint8_t kSineAmpMin = 5;
constexpr uint8_t kSineAmpMax = 18;
constexpr uint32_t kSineAmpMinStepMs = 120;
constexpr uint32_t kSineAmpMaxStepMs = 420;
constexpr uint32_t kEspNowDiscoveryPeriodMs = 60000;
constexpr uint32_t kEspNowPeerActiveMs = 180000;
constexpr size_t kEspNowMaxPeers = 16;
constexpr size_t kEspNowRxTextMax = 180;
constexpr size_t kEspNowNameMax = 20;

struct MorseStep {
  char letter;
  bool on;
  uint8_t units;
};

constexpr MorseStep kMorseSequence[] = {
    // L .-..
    {'L', true, 1},  {'L', false, 1}, {'L', true, 3},  {'L', false, 1},
    {'L', true, 1},  {'L', false, 1}, {'L', true, 1},  {'L', false, 8},
    // E .
    {'E', true, 1},  {'E', false, 8},
    // F ..-.
    {'F', true, 1},  {'F', false, 1}, {'F', true, 1},  {'F', false, 1},
    {'F', true, 3},  {'F', false, 1}, {'F', true, 1},  {'F', false, 8},
    // O ---
    {'O', true, 3},  {'O', false, 1}, {'O', true, 3},  {'O', false, 1},
    {'O', true, 3},  {'O', false, 8},
    // U ..-
    {'U', true, 1},  {'U', false, 1}, {'U', true, 1},  {'U', false, 1},
    {'U', true, 3},  {'U', false, 12},
};
constexpr size_t kMorseSequenceLen = sizeof(kMorseSequence) / sizeof(kMorseSequence[0]);
constexpr uint8_t kSine4BitLut[] = {
    8,  9,  11, 12, 13, 14, 15, 15,
    15, 14, 13, 12, 11, 9,  8,  6,
    5,  3,  2,  1,  0,  0,  0,  1,
    2,  3,  5,  6,  8,  9,  11, 12,
};
constexpr size_t kSine4BitLen = sizeof(kSine4BitLut) / sizeof(kSine4BitLut[0]);

constexpr uint16_t kEepromSize = 256;
constexpr uint32_t kCfgMagic = 0x4F534338;  // 'OSC8'

struct WifiConfigBlob {
  uint32_t magic;
  char ssid[33];
  char pass[65];
  uint32_t checksum;
};

struct WifiScanEntry {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  uint8_t bssid[6];
};

struct EspNowPeerEntry {
  uint8_t mac[6];
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint32_t rxCount;
  char name[kEspNowNameMax + 1];
  char lastType[16];
};

ESP8266WebServer gServer(80);
Adafruit_SSD1306 gDisplay(kOledWidth, kOledHeight, &Wire, -1);

WifiConfigBlob gCfg{};
String gStaSsid;
String gStaPass;

WifiScanEntry gScanEntries[kMaxScanEntries];
size_t gScanCount = 0;
uint32_t gLastScanMs = 0;

bool gStaConnecting = false;
bool gApActive = false;
uint32_t gStaConnectStartMs = 0;
uint32_t gLastWifiAttemptMs = 0;

uint16_t gJoyRaw = 0;
float gJoySmooth = 512.0f;
bool gJoySwPressed = false;

bool gOledReady = false;
uint8_t gOledAddr = 0;

char gMorseLetter = 'L';
bool gMorseOn = false;
size_t gMorseStepIndex = 0;
uint32_t gMorseStepStartMs = 0;
uint32_t gMorseStepDurationMs = 0;

uint32_t gLastJoystickMs = 0;
uint32_t gLastOledMs = 0;
uint32_t gLastLogMs = 0;
uint32_t gLastSineUs = 0;
size_t gSineIndex = 0;
uint32_t gLastScopeSampleMs = 0;
uint32_t gLastSineAmpUpdateMs = 0;
uint32_t gNextSineAmpDelayMs = kSineAmpMinStepMs;
uint8_t gSineGlitchAmp = 10;
uint8_t gMorseScope[kOledWidth] = {0};
size_t gMorseScopeHead = 0;

bool gEspNowReady = false;
uint32_t gEspNowTxOk = 0;
uint32_t gEspNowTxFail = 0;
uint32_t gEspNowRxCount = 0;
uint32_t gEspNowLastTxMs = 0;
uint32_t gEspNowLastRxMs = 0;
char gEspNowLastPeer[18] = "-";
bool gEspNowForceDiscovery = false;
EspNowPeerEntry gEspNowPeers[kEspNowMaxPeers];
size_t gEspNowPeerCount = 0;

String gSerialLine;

const uint8_t kEspNowBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t fnv1a32(const uint8_t* data, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t configChecksum(const WifiConfigBlob& cfg) {
  return fnv1a32(reinterpret_cast<const uint8_t*>(&cfg),
                 sizeof(WifiConfigBlob) - sizeof(cfg.checksum));
}

void safeStringCopy(char* dst, size_t dstSize, const String& src) {
  if (dstSize == 0) {
    return;
  }
  size_t n = src.length();
  if (n >= dstSize) {
    n = dstSize - 1;
  }
  memcpy(dst, src.c_str(), n);
  dst[n] = '\0';
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in.charAt(i);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void formatMac(const uint8_t* mac, char* out, size_t outSize) {
  if (outSize < 18) {
    if (outSize > 0) {
      out[0] = '\0';
    }
    return;
  }
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
}

bool jsonExtractString(const char* text, const char* key, char* out, size_t outSize) {
  if (outSize == 0 || text == nullptr || key == nullptr) {
    return false;
  }
  out[0] = '\0';

  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* p = strstr(text, pattern);
  if (p == nullptr) {
    return false;
  }
  p += strlen(pattern);

  size_t i = 0;
  while (*p != '\0' && *p != '"' && (i + 1) < outSize) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

void sanitizeAsciiText(char* text) {
  if (text == nullptr) {
    return;
  }
  for (size_t i = 0; text[i] != '\0'; ++i) {
    const char c = text[i];
    if (c < 32 || c > 126) {
      text[i] = '_';
    }
  }
}

int findEspNowPeerIndex(const uint8_t* mac) {
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    if (memcmp(gEspNowPeers[i].mac, mac, 6) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int allocEspNowPeerIndex(const uint8_t* mac, uint32_t nowMs) {
  if (gEspNowPeerCount < kEspNowMaxPeers) {
    const size_t idx = gEspNowPeerCount++;
    memset(&gEspNowPeers[idx], 0, sizeof(gEspNowPeers[idx]));
    memcpy(gEspNowPeers[idx].mac, mac, 6);
    gEspNowPeers[idx].firstSeenMs = nowMs;
    gEspNowPeers[idx].lastSeenMs = nowMs;
    gEspNowPeers[idx].rxCount = 0;
    strncpy(gEspNowPeers[idx].name, "?", sizeof(gEspNowPeers[idx].name) - 1);
    strncpy(gEspNowPeers[idx].lastType, "?", sizeof(gEspNowPeers[idx].lastType) - 1);
    return static_cast<int>(idx);
  }

  size_t oldestIdx = 0;
  uint32_t oldestSeen = gEspNowPeers[0].lastSeenMs;
  for (size_t i = 1; i < gEspNowPeerCount; ++i) {
    if (gEspNowPeers[i].lastSeenMs < oldestSeen) {
      oldestSeen = gEspNowPeers[i].lastSeenMs;
      oldestIdx = i;
    }
  }
  memset(&gEspNowPeers[oldestIdx], 0, sizeof(gEspNowPeers[oldestIdx]));
  memcpy(gEspNowPeers[oldestIdx].mac, mac, 6);
  gEspNowPeers[oldestIdx].firstSeenMs = nowMs;
  gEspNowPeers[oldestIdx].lastSeenMs = nowMs;
  strncpy(gEspNowPeers[oldestIdx].name, "?", sizeof(gEspNowPeers[oldestIdx].name) - 1);
  strncpy(gEspNowPeers[oldestIdx].lastType, "?", sizeof(gEspNowPeers[oldestIdx].lastType) - 1);
  return static_cast<int>(oldestIdx);
}

void updateEspNowPeer(const uint8_t* mac, const char* type, const char* name) {
  const uint32_t nowMs = millis();
  int idx = findEspNowPeerIndex(mac);
  if (idx < 0) {
    idx = allocEspNowPeerIndex(mac, nowMs);
  }
  if (idx < 0) {
    return;
  }

  EspNowPeerEntry& peer = gEspNowPeers[static_cast<size_t>(idx)];
  peer.lastSeenMs = nowMs;
  ++peer.rxCount;
  if (type != nullptr && type[0] != '\0') {
    strncpy(peer.lastType, type, sizeof(peer.lastType) - 1);
    peer.lastType[sizeof(peer.lastType) - 1] = '\0';
  }
  if (name != nullptr && name[0] != '\0') {
    strncpy(peer.name, name, sizeof(peer.name) - 1);
    peer.name[sizeof(peer.name) - 1] = '\0';
  }
}

size_t countEspNowActivePeers(uint32_t nowMs) {
  size_t count = 0;
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    if ((nowMs - gEspNowPeers[i].lastSeenMs) <= kEspNowPeerActiveMs) {
      ++count;
    }
  }
  return count;
}

bool sendEspNowFrame(const uint8_t* mac, const char* type) {
  if (!gEspNowReady || mac == nullptr || type == nullptr) {
    return false;
  }

  char payload[200];
  const int n = snprintf(payload, sizeof(payload),
                         "{\"type\":\"%s\",\"device\":\"%s\",\"uptime_ms\":%lu}", type,
                         kDeviceName, static_cast<unsigned long>(millis()));
  if (n <= 0) {
    return false;
  }

  const uint8_t rc =
      esp_now_send(const_cast<uint8_t*>(mac), reinterpret_cast<uint8_t*>(payload),
                   static_cast<uint8_t>(strnlen(payload, sizeof(payload))));
  if (rc != 0) {
    ++gEspNowTxFail;
    return false;
  }
  return true;
}

void onEspNowSent(uint8_t* macAddr, uint8_t status) {
  (void)macAddr;
  if (status == 0) {
    ++gEspNowTxOk;
  } else {
    ++gEspNowTxFail;
  }
}

void onEspNowRecv(uint8_t* macAddr, uint8_t* data, uint8_t len) {
  if (macAddr == nullptr || data == nullptr || len == 0) {
    return;
  }

  ++gEspNowRxCount;
  gEspNowLastRxMs = millis();
  formatMac(macAddr, gEspNowLastPeer, sizeof(gEspNowLastPeer));

  const size_t n = (len < kEspNowRxTextMax) ? len : kEspNowRxTextMax;
  char text[kEspNowRxTextMax + 1];
  memcpy(text, data, n);
  text[n] = '\0';
  sanitizeAsciiText(text);

  char type[16] = {0};
  char name[kEspNowNameMax + 1] = {0};
  if (!jsonExtractString(text, "type", type, sizeof(type))) {
    return;
  }
  if (!jsonExtractString(text, "device", name, sizeof(name))) {
    jsonExtractString(text, "name", name, sizeof(name));
  }

  updateEspNowPeer(macAddr, type, name);
  if (strcmp(type, "discovery") == 0) {
    esp_now_add_peer(macAddr, ESP_NOW_ROLE_COMBO, 0, nullptr, 0);
    sendEspNowFrame(macAddr, "announce");
  }
}

void initEspNow() {
  if (esp_now_init() != 0) {
    gEspNowReady = false;
    Serial.println("[ESPNOW] init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_add_peer(const_cast<uint8_t*>(kEspNowBroadcastMac), ESP_NOW_ROLE_COMBO, 0, nullptr, 0);

  gEspNowReady = true;
  Serial.println("[ESPNOW] ready (broadcast+discovery)");
}

void serviceEspNow(uint32_t nowMs) {
  if (!gEspNowReady) {
    return;
  }
  if (!gEspNowForceDiscovery && (nowMs - gEspNowLastTxMs) < kEspNowDiscoveryPeriodMs) {
    return;
  }
  gEspNowForceDiscovery = false;
  gEspNowLastTxMs = nowMs;
  sendEspNowFrame(kEspNowBroadcastMac, "discovery");
}

void setStatusLed(bool on) {
  if (kStatusLedPin == 0xFF) {
    return;
  }
  const bool active = kStatusLedActiveLow ? !on : on;
  digitalWrite(kStatusLedPin, active ? HIGH : LOW);
}

void setWifiConfigDefaults() {
  memset(&gCfg, 0, sizeof(gCfg));
  gCfg.magic = kCfgMagic;
  safeStringCopy(gCfg.ssid, sizeof(gCfg.ssid), String(kDefaultSsid));
  safeStringCopy(gCfg.pass, sizeof(gCfg.pass), String(kDefaultPass));
  gCfg.checksum = configChecksum(gCfg);
}

void saveWifiCredentialsToEeprom(const String& ssid, const String& pass) {
  gCfg.magic = kCfgMagic;
  safeStringCopy(gCfg.ssid, sizeof(gCfg.ssid), ssid);
  safeStringCopy(gCfg.pass, sizeof(gCfg.pass), pass);
  gCfg.checksum = configChecksum(gCfg);
  EEPROM.put(0, gCfg);
  EEPROM.commit();
  gStaSsid = String(gCfg.ssid);
  gStaPass = String(gCfg.pass);
}

void loadWifiCredentialsFromEeprom() {
  EEPROM.get(0, gCfg);
  const bool magicOk = (gCfg.magic == kCfgMagic);
  const bool csumOk = (gCfg.checksum == configChecksum(gCfg));
  const bool ssidOk = (gCfg.ssid[0] != '\0');

  if (!magicOk || !csumOk || !ssidOk) {
    setWifiConfigDefaults();
    EEPROM.put(0, gCfg);
    EEPROM.commit();
  }

  gStaSsid = String(gCfg.ssid);
  gStaPass = String(gCfg.pass);
}

void refreshWifiScanCache(bool force) {
  const uint32_t nowMs = millis();
  if (!force && (nowMs - gLastScanMs) < kWifiScanCacheMs) {
    return;
  }

  gScanCount = 0;
  const int found = WiFi.scanNetworks();
  if (found <= 0) {
    gLastScanMs = nowMs;
    return;
  }

  const size_t limit = (found < static_cast<int>(kMaxScanEntries)) ? static_cast<size_t>(found)
                                                                    : kMaxScanEntries;
  for (size_t i = 0; i < limit; ++i) {
    WifiScanEntry& entry = gScanEntries[i];
    entry.ssid = WiFi.SSID(i);
    entry.rssi = WiFi.RSSI(i);
    entry.channel = static_cast<uint8_t>(WiFi.channel(i));
    const uint8_t* bssid = WiFi.BSSID(i);
    if (bssid != nullptr) {
      memcpy(entry.bssid, bssid, sizeof(entry.bssid));
    } else {
      memset(entry.bssid, 0, sizeof(entry.bssid));
    }
    ++gScanCount;
  }

  gLastScanMs = nowMs;
}

int findBestNetworkIndexForSsid(const String& ssid) {
  int bestIndex = -1;
  int32_t bestRssi = -127;
  for (size_t i = 0; i < gScanCount; ++i) {
    if (gScanEntries[i].ssid == ssid && gScanEntries[i].rssi > bestRssi) {
      bestRssi = gScanEntries[i].rssi;
      bestIndex = static_cast<int>(i);
    }
  }
  return bestIndex;
}

void ensureFallbackAp() {
  if (gApActive) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  const bool ok = WiFi.softAP(kFallbackApSsid, kFallbackApPass);
  gApActive = ok;
  if (ok) {
    Serial.printf("[WIFI] AP fallback active ssid=%s ip=%s\n", kFallbackApSsid,
                  ipToString(WiFi.softAPIP()).c_str());
  } else {
    Serial.println("[WIFI] Failed to start AP fallback");
  }
}

void stopFallbackAp() {
  if (!gApActive) {
    return;
  }
  WiFi.softAPdisconnect(true);
  gApActive = false;
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
  }
}

bool beginStaConnectWithBestRssi() {
  if (gStaSsid.isEmpty()) {
    return false;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  gApActive = false;
  refreshWifiScanCache(true);

  const int bestIdx = findBestNetworkIndexForSsid(gStaSsid);
  if (bestIdx >= 0) {
    const WifiScanEntry& e = gScanEntries[static_cast<size_t>(bestIdx)];
    WiFi.begin(gStaSsid.c_str(), gStaPass.c_str(), e.channel, e.bssid, true);
    char macText[18] = "-";
    formatMac(e.bssid, macText, sizeof(macText));
    Serial.printf("[WIFI] STA connect to '%s' rssi=%ld ch=%u bssid=%s\n", e.ssid.c_str(),
                  static_cast<long>(e.rssi), e.channel, macText);
  } else {
    WiFi.begin(gStaSsid.c_str(), gStaPass.c_str());
    Serial.printf("[WIFI] STA connect to '%s' (best RSSI candidate not found in scan)\n",
                  gStaSsid.c_str());
  }

  gStaConnecting = true;
  gStaConnectStartMs = millis();
  gLastWifiAttemptMs = gStaConnectStartMs;
  return true;
}

String wifiModeText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "STA";
  }
  if (gStaConnecting) {
    return "STA_CONNECTING";
  }
  if (gApActive) {
    return "AP_FALLBACK";
  }
  return "DISCONNECTED";
}

void serviceWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    gStaConnecting = false;
    stopFallbackAp();
    return;
  }

  if (gStaConnecting && (nowMs - gStaConnectStartMs) > kStaConnectTimeoutMs) {
    gStaConnecting = false;
    ensureFallbackAp();
  }

  if (!gStaConnecting && (nowMs - gLastWifiAttemptMs) >= kWifiRetryMs) {
    beginStaConnectWithBestRssi();
  }
}

void sampleJoystick(uint32_t nowMs) {
  if ((nowMs - gLastJoystickMs) < kJoystickPollMs) {
    return;
  }
  gLastJoystickMs = nowMs;

  gJoyRaw = analogRead(kJoyAdcPin);
  gJoySmooth = (0.8f * gJoySmooth) + (0.2f * static_cast<float>(gJoyRaw));
  gJoySwPressed = (digitalRead(kJoySwPin) == LOW);
}

void writePcm4Bit(uint8_t nibble) {
  for (size_t i = 0; i < 4; ++i) {
    digitalWrite(kPcm4BitPins[i], ((nibble >> i) & 0x01U) ? HIGH : LOW);
  }
}

void servicePseudoSine(uint32_t nowUs) {
  if ((nowUs - gLastSineUs) < kSineSamplePeriodUs) {
    return;
  }
  gLastSineUs = nowUs;
  writePcm4Bit(kSine4BitLut[gSineIndex]);
  gSineIndex = (gSineIndex + 1U) % kSine4BitLen;
}

void refreshMorseOutput() { digitalWrite(kMorseOutPin, gMorseOn ? HIGH : LOW); }

void pushMorseScopeSample(bool levelHigh) {
  gMorseScope[gMorseScopeHead] = levelHigh ? 1 : 0;
  gMorseScopeHead = (gMorseScopeHead + 1U) % kOledWidth;
}

void serviceMorseScope(uint32_t nowMs) {
  if ((nowMs - gLastScopeSampleMs) < kScopeSampleMs) {
    return;
  }
  gLastScopeSampleMs = nowMs;
  pushMorseScopeSample(gMorseOn);
}

void drawMorseScope(uint32_t nowMs) {
  constexpr int scopeX = 0;
  constexpr int scopeY = 0;
  constexpr int scopeW = 128;
  constexpr int scopeH = 64;
  constexpr int yHigh = scopeY + 8;
  constexpr int yLow = scopeY + scopeH - 8;

  int prevX = scopeX + 1;
  int prevY = yLow;
  for (int x = 0; x < (scopeW - 2); ++x) {
    const size_t idx = (gMorseScopeHead + static_cast<size_t>(x)) % kOledWidth;
    const bool on = (gMorseScope[idx] != 0);
    const int curX = scopeX + 1 + x;
    const int curY = on ? yHigh : yLow;
    if (x > 0) {
      gDisplay.drawLine(prevX, prevY, curX, curY, SSD1306_WHITE);
    }
    prevX = curX;
    prevY = curY;
  }

  // Overlay a synthetic sine with deterministic glitches.
  constexpr int yCenter = scopeY + (scopeH / 2);
  const int amp = static_cast<int>(gSineGlitchAmp);
  int prevSineX = scopeX;
  int prevSineY = yCenter;
  for (int x = 0; x < scopeW; ++x) {
    const size_t lutIdx = (gSineIndex + static_cast<size_t>(x * 2U) +
                           static_cast<size_t>((nowMs >> 4U) & 0x1FU)) %
                          kSine4BitLen;
    const int base = static_cast<int>(kSine4BitLut[lutIdx]) - 8;  // -8..+7
    int y = yCenter - ((base * amp) / 8);

    const uint8_t noise = static_cast<uint8_t>((x * 37U) ^ (nowMs >> 3U) ^ (gSineIndex * 13U));
    if ((noise & 0x1F) == 0x03 || (noise & 0x1F) == 0x11) {
      const int spike = 6 + static_cast<int>(noise & 0x07);
      y += ((noise & 0x80) != 0) ? spike : -spike;
    }
    y = constrain(y, scopeY, scopeY + scopeH - 1);

    if (x > 0) {
      gDisplay.drawLine(prevSineX, prevSineY, scopeX + x, y, SSD1306_WHITE);
    }
    prevSineX = scopeX + x;
    prevSineY = y;
  }
}

void serviceGlitchSineAmplitude(uint32_t nowMs) {
  if ((nowMs - gLastSineAmpUpdateMs) < gNextSineAmpDelayMs) {
    return;
  }
  gLastSineAmpUpdateMs = nowMs;
  gNextSineAmpDelayMs =
      static_cast<uint32_t>(random(static_cast<long>(kSineAmpMinStepMs), static_cast<long>(kSineAmpMaxStepMs + 1U)));
  gSineGlitchAmp = static_cast<uint8_t>(
      random(static_cast<long>(kSineAmpMin), static_cast<long>(kSineAmpMax + 1U)));
}

void startMorseStep(uint32_t nowMs) {
  const MorseStep& step = kMorseSequence[gMorseStepIndex];
  gMorseLetter = step.letter;
  gMorseOn = step.on;
  refreshMorseOutput();
  gMorseStepDurationMs = static_cast<uint32_t>(step.units) * kMorseUnitMs;
  gMorseStepStartMs = nowMs;
}

void serviceMorse(uint32_t nowMs) {
  if ((nowMs - gMorseStepStartMs) < gMorseStepDurationMs) {
    return;
  }
  gMorseStepIndex = (gMorseStepIndex + 1U) % kMorseSequenceLen;
  startMorseStep(nowMs);
}

char morsePulseSymbol() {
  if (!gMorseOn) {
    return ' ';
  }
  return kMorseSequence[gMorseStepIndex].units >= 3 ? '-' : '.';
}

void updateStatusLed(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    const uint32_t t = nowMs % kLedHeartbeatPeriodMs;
    setStatusLed(t < kLedHeartbeatOnMs);
    return;
  }

  if (gStaConnecting) {
    setStatusLed((nowMs / kLedBlinkFastMs) % 2 == 0);
    return;
  }

  if (gApActive) {
    const uint32_t phase = nowMs % 1000;
    const bool on = (phase < 80) || (phase >= 160 && phase < 240);
    setStatusLed(on);
    return;
  }

  setStatusLed((nowMs / kLedBlinkSlowMs) % 2 == 0);
}

bool probeI2cAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2cBusToSerial() {
  Serial.printf("[I2C] Scan on SDA=%u SCL=%u\n", kOledSdaPin, kOledSclPin);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    if (probeI2cAddress(addr)) {
      Serial.printf("[I2C] Found device at 0x%02X\n", addr);
      ++found;
    }
  }
  if (found == 0) {
    Serial.println("[I2C] No device found");
  }
}

void initOled() {
  Wire.begin(kOledSdaPin, kOledSclPin);
  Wire.setClock(400000);

  const uint8_t addresses[] = {0x3C, 0x3D};
  for (size_t i = 0; i < sizeof(addresses); ++i) {
    const uint8_t address = addresses[i];
    if (!probeI2cAddress(address)) {
      continue;
    }

    if (gDisplay.begin(SSD1306_SWITCHCAPVCC, address)) {
      gOledReady = true;
      gOledAddr = address;
      break;
    }
  }

  if (!gOledReady) {
    Serial.println("[OLED] SSD1306 not found on 0x3C/0x3D");
    return;
  }

  gDisplay.clearDisplay();
  gDisplay.setTextSize(1);
  gDisplay.setTextColor(SSD1306_WHITE);
  gDisplay.setCursor(0, 0);
  gDisplay.println(F("OSCILLO ESP8266"));
  gDisplay.println(F("OLED online"));
  gDisplay.printf("I2C: %s\n", kOledBusLabel);
  gDisplay.printf("ADDR: 0x%02X\n", gOledAddr);
  gDisplay.display();
  Serial.printf("[OLED] Ready addr=0x%02X SDA=%u SCL=%u\n", gOledAddr, kOledSdaPin, kOledSclPin);
}

void drawOled(uint32_t nowMs) {
  if (!gOledReady || (nowMs - gLastOledMs) < kOledRefreshMs) {
    return;
  }
  gLastOledMs = nowMs;

  gDisplay.clearDisplay();
  drawMorseScope(nowMs);
  gDisplay.display();
}

String buildStatusJson() {
  const uint32_t nowMs = millis();
  String out = "{";
  out += "\"device\":\"" + String(kDeviceName) + "\",";
  out += "\"uptime_ms\":" + String(nowMs) + ",";
  out += "\"wifi_mode\":\"" + wifiModeText() + "\",";
  out += "\"ssid\":\"" + jsonEscape(gStaSsid) + "\",";
  out += "\"ip\":\"" + jsonEscape(ipToString(WiFi.localIP())) + "\",";
  out += "\"ap_ip\":\"" + jsonEscape(ipToString(WiFi.softAPIP())) + "\",";
  out += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  out += "\"joy\":{";
  out += "\"a0_raw\":" + String(gJoyRaw) + ",";
  out += "\"a0_smooth\":" + String(static_cast<uint16_t>(gJoySmooth)) + ",";
  out += "\"sw\":" + String(gJoySwPressed ? "true" : "false");
  out += "},";
  out += "\"morse\":{";
  out += "\"text\":\"" + String(kMorseText) + "\",";
  out += "\"letter\":\"" + String(gMorseLetter) + "\",";
  out += "\"on\":" + String(gMorseOn ? "true" : "false") + ",";
  out += "\"symbol\":\"" + String(morsePulseSymbol()) + "\",";
  out += "\"unit_ms\":" + String(kMorseUnitMs) + ",";
  out += "\"step\":" + String(static_cast<unsigned>(gMorseStepIndex));
  out += "},";
  out += "\"espnow\":{";
  out += "\"ready\":" + String(gEspNowReady ? "true" : "false") + ",";
  out += "\"tx_ok\":" + String(gEspNowTxOk) + ",";
  out += "\"tx_fail\":" + String(gEspNowTxFail) + ",";
  out += "\"rx_count\":" + String(gEspNowRxCount) + ",";
  out += "\"peer_count\":" + String(static_cast<unsigned>(gEspNowPeerCount)) + ",";
  out += "\"active_peers\":" + String(static_cast<unsigned>(countEspNowActivePeers(nowMs))) + ",";
  out += "\"last_peer\":\"" + String(gEspNowLastPeer) + "\",";
  out += "\"last_rx_ms\":" + String(gEspNowLastRxMs);
  out += "},";
  out += "\"oled\":{";
  out += "\"ready\":" + String(gOledReady ? "true" : "false") + ",";
  out += "\"addr\":\"0x" + String(gOledAddr, HEX) + "\",";
  out += "\"sda\":" + String(kOledSdaPin) + ",";
  out += "\"scl\":" + String(kOledSclPin);
  out += "}";
  out += "}";
  return out;
}

void handleRoot() {
  static const char kHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>OSCILLO ESP8266</title>
<style>
:root {
  --bg: #0b1418;
  --panel: #122028;
  --line: #2f5363;
  --text: #e4f3f9;
  --muted: #9bbac8;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: "IBM Plex Mono", monospace;
  background: radial-gradient(circle at 20% 0%, #1e2e38, #0b1418 55%);
  color: var(--text);
}
main { max-width: 820px; margin: 0 auto; padding: 18px; }
.card {
  border: 1px solid var(--line);
  border-radius: 14px;
  background: var(--panel);
  padding: 14px;
  margin-bottom: 12px;
}
h1 { margin: 0 0 12px; font-size: 18px; letter-spacing: .08em; text-transform: uppercase; }
.row { display: flex; justify-content: space-between; border-bottom: 1px dashed #24414f; padding: 6px 0; }
.label { color: var(--muted); }
input {
  width: 100%;
  background: #0d1a21;
  color: var(--text);
  border: 1px solid #2a4959;
  border-radius: 8px;
  padding: 8px;
  margin: 6px 0;
}
button {
  width: 100%;
  border: 0;
  border-radius: 8px;
  padding: 10px;
  background: linear-gradient(120deg, #2f9eb4, #4cb4a0);
  color: #021015;
  font-weight: bold;
}
small { color: var(--muted); }
</style>
</head>
<body>
<main>
  <div class="card">
    <h1>OSCILLO ESP8266 OLED</h1>
    <div class="row"><span class="label">Mode</span><span id="mode">-</span></div>
    <div class="row"><span class="label">IP</span><span id="ip">-</span></div>
    <div class="row"><span class="label">RSSI</span><span id="rssi">-</span></div>
    <div class="row"><span class="label">A0</span><span id="a0">-</span></div>
    <div class="row"><span class="label">Switch</span><span id="sw">-</span></div>
    <div class="row"><span class="label">ESP-NOW</span><span id="espnow">-</span></div>
    <div class="row"><span class="label">OLED</span><span id="oled">-</span></div>
  </div>
  <div class="card">
    <small>Update Wi-Fi credentials (saved to EEPROM):</small>
    <input id="ssid" placeholder="SSID" />
    <input id="pass" placeholder="Password" type="password" />
    <button id="saveBtn">Save And Reconnect</button>
  </div>
</main>
<script>
async function refresh(){
  const r = await fetch('/api/status');
  const s = await r.json();
  mode.textContent = s.wifi_mode;
  ip.textContent = s.ip && s.ip !== '0.0.0.0' ? s.ip : s.ap_ip;
  rssi.textContent = s.wifi_mode === 'STA' ? s.rssi + ' dBm' : '-';
  a0.textContent = `${s.joy.a0_raw} (smooth ${s.joy.a0_smooth})`;
  sw.textContent = s.joy.sw ? 'ON' : 'OFF';
  espnow.textContent = s.espnow ? `${s.espnow.active_peers}/${s.espnow.peer_count} peers` : '-';
  oled.textContent = `${s.oled.ready ? 'OK' : 'OFF'} @ ${s.oled.addr}`;
}
saveBtn.onclick = async () => {
  const fd = new URLSearchParams();
  fd.set('ssid', ssid.value);
  fd.set('pass', pass.value);
  const r = await fetch('/api/wifi/select', {method:'POST', body: fd});
  const j = await r.json();
  alert(j.ok ? 'Saved' : ('Error: ' + (j.error || 'unknown')));
};
setInterval(refresh, 1000);
refresh();
</script>
</body>
</html>
)HTML";

  gServer.send_P(200, "text/html", kHtml);
}

void handleStatus() {
  gServer.send(200, "application/json", buildStatusJson());
}

void handleWifiScan() {
  refreshWifiScanCache(true);
  String out = "{";
  out += "\"count\":" + String(gScanCount) + ",\"networks\":[";
  for (size_t i = 0; i < gScanCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    char macText[18] = "-";
    formatMac(gScanEntries[i].bssid, macText, sizeof(macText));
    out += "{";
    out += "\"ssid\":\"" + jsonEscape(gScanEntries[i].ssid) + "\",";
    out += "\"rssi\":" + String(gScanEntries[i].rssi) + ",";
    out += "\"channel\":" + String(gScanEntries[i].channel) + ",";
    out += "\"bssid\":\"" + String(macText) + "\"";
    out += "}";
  }
  out += "]}";
  gServer.send(200, "application/json", out);
}

void handleWifiSelect() {
  if (!gServer.hasArg("ssid")) {
    gServer.send(400, "application/json", "{\"ok\":false,\"error\":\"missing ssid\"}");
    return;
  }

  const String ssid = gServer.arg("ssid");
  const String pass = gServer.arg("pass");
  if (ssid.isEmpty()) {
    gServer.send(400, "application/json", "{\"ok\":false,\"error\":\"empty ssid\"}");
    return;
  }

  saveWifiCredentialsToEeprom(ssid, pass);
  beginStaConnectWithBestRssi();
  gServer.send(200, "application/json", "{\"ok\":true}");
}

void handleEspNowPeers() {
  const uint32_t nowMs = millis();
  String out = "{";
  out += "\"ok\":true,";
  out += "\"count\":" + String(static_cast<unsigned>(gEspNowPeerCount)) + ",";
  out += "\"active\":" + String(static_cast<unsigned>(countEspNowActivePeers(nowMs))) + ",";
  out += "\"peers\":[";
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    char macText[18] = "-";
    formatMac(gEspNowPeers[i].mac, macText, sizeof(macText));
    const uint32_t ageMs = nowMs - gEspNowPeers[i].lastSeenMs;
    const bool active = ageMs <= kEspNowPeerActiveMs;
    out += "{";
    out += "\"mac\":\"" + String(macText) + "\",";
    out += "\"name\":\"" + String(gEspNowPeers[i].name) + "\",";
    out += "\"type\":\"" + String(gEspNowPeers[i].lastType) + "\",";
    out += "\"rx_count\":" + String(gEspNowPeers[i].rxCount) + ",";
    out += "\"last_seen_ms\":" + String(gEspNowPeers[i].lastSeenMs) + ",";
    out += "\"age_ms\":" + String(ageMs) + ",";
    out += "\"active\":" + String(active ? "true" : "false");
    out += "}";
  }
  out += "]}";
  gServer.send(200, "application/json", out);
}

void startWebServer() {
  gServer.on("/", HTTP_GET, handleRoot);
  gServer.on("/api/status", HTTP_GET, handleStatus);
  gServer.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  gServer.on("/api/wifi/select", HTTP_POST, handleWifiSelect);
  gServer.on("/api/espnow/peers", HTTP_GET, handleEspNowPeers);
  gServer.begin();
  Serial.println("[WEB] HTTP server started on :80");
}

void printStatusSerial() {
  const uint32_t nowMs = millis();
  Serial.printf(
      "[STATUS] mode=%s ip=%s ap_ip=%s rssi=%d a0=%u sm=%u sw=%s morse=%c:%s(%c) dac=D5..D8 espnow=%s peers=%u/%u tx=%lu/%lu rx=%lu oled=%s(0x%02X)\n",
                wifiModeText().c_str(), ipToString(WiFi.localIP()).c_str(),
                ipToString(WiFi.softAPIP()).c_str(), WiFi.RSSI(), gJoyRaw,
                static_cast<uint16_t>(gJoySmooth), gJoySwPressed ? "ON" : "OFF",
                gMorseLetter, gMorseOn ? "ON" : "OFF", morsePulseSymbol(),
                gEspNowReady ? "ON" : "OFF", static_cast<unsigned>(countEspNowActivePeers(nowMs)),
                static_cast<unsigned>(gEspNowPeerCount), static_cast<unsigned long>(gEspNowTxOk),
                static_cast<unsigned long>(gEspNowTxFail), static_cast<unsigned long>(gEspNowRxCount),
                gOledReady ? "ON" : "OFF", gOledAddr);
}

void printEspNowPeersSerial() {
  const uint32_t nowMs = millis();
  Serial.printf("[ESPNOW] peers=%u active=%u last_peer=%s last_rx_ms=%lu\n",
                static_cast<unsigned>(gEspNowPeerCount),
                static_cast<unsigned>(countEspNowActivePeers(nowMs)), gEspNowLastPeer,
                static_cast<unsigned long>(gEspNowLastRxMs));
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    char macText[18] = "-";
    formatMac(gEspNowPeers[i].mac, macText, sizeof(macText));
    const uint32_t ageMs = nowMs - gEspNowPeers[i].lastSeenMs;
    Serial.printf("  [%02u] mac=%s name=%s type=%s rx=%lu age=%lums %s\n", static_cast<unsigned>(i),
                  macText, gEspNowPeers[i].name, gEspNowPeers[i].lastType,
                  static_cast<unsigned long>(gEspNowPeers[i].rxCount),
                  static_cast<unsigned long>(ageMs), (ageMs <= kEspNowPeerActiveMs) ? "ACTIVE" : "STALE");
  }
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help");
  Serial.println("  status");
  Serial.println("  scan");
  Serial.println("  wifi");
  Serial.println("  espnow");
  Serial.println("  peers");
  Serial.println("  discover");
  Serial.println("  reconnect");
  Serial.println("  setwifi <ssid> <pass>");
  Serial.println("  i2c");
  Serial.println("  oled");
}

void handleSerialCommand(const String& raw) {
  String cmd = raw;
  cmd.trim();
  if (cmd.isEmpty()) {
    return;
  }

  if (cmd.equalsIgnoreCase("help")) {
    printHelp();
    return;
  }

  if (cmd.equalsIgnoreCase("status") || cmd.equalsIgnoreCase("wifi")) {
    printStatusSerial();
    return;
  }

  if (cmd.equalsIgnoreCase("espnow")) {
    printEspNowPeersSerial();
    return;
  }

  if (cmd.equalsIgnoreCase("peers")) {
    printEspNowPeersSerial();
    return;
  }

  if (cmd.equalsIgnoreCase("discover")) {
    gEspNowForceDiscovery = true;
    Serial.println("[ESPNOW] forced discovery broadcast");
    return;
  }

  if (cmd.equalsIgnoreCase("scan")) {
    refreshWifiScanCache(true);
    for (size_t i = 0; i < gScanCount; ++i) {
      char macText[18] = "-";
      formatMac(gScanEntries[i].bssid, macText, sizeof(macText));
      Serial.printf("[%02u] ssid=%s rssi=%ld ch=%u bssid=%s\n", static_cast<unsigned>(i),
                    gScanEntries[i].ssid.c_str(), static_cast<long>(gScanEntries[i].rssi),
                    gScanEntries[i].channel, macText);
    }
    if (gScanCount == 0) {
      Serial.println("[WIFI] No networks in scan cache");
    }
    return;
  }

  if (cmd.equalsIgnoreCase("reconnect")) {
    beginStaConnectWithBestRssi();
    return;
  }

  if (cmd.equalsIgnoreCase("i2c")) {
    scanI2cBusToSerial();
    return;
  }

  if (cmd.equalsIgnoreCase("oled")) {
    if (!gOledReady) {
      Serial.println("[OLED] not ready");
    } else {
      Serial.printf("[OLED] ready addr=0x%02X sda=%u scl=%u\n", gOledAddr, kOledSdaPin,
                    kOledSclPin);
    }
    return;
  }

  const String prefix = "setwifi ";
  if (cmd.startsWith(prefix)) {
    const String payload = cmd.substring(prefix.length());
    const int split = payload.indexOf(' ');
    if (split <= 0) {
      Serial.println("[WIFI] Usage: setwifi <ssid> <pass>");
      return;
    }
    const String ssid = payload.substring(0, split);
    const String pass = payload.substring(split + 1);
    saveWifiCredentialsToEeprom(ssid, pass);
    Serial.printf("[WIFI] Saved credentials for ssid='%s'\n", ssid.c_str());
    beginStaConnectWithBestRssi();
    return;
  }

  Serial.printf("[SERIAL] Unknown command: %s\n", cmd.c_str());
}

void serviceSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!gSerialLine.isEmpty()) {
        handleSerialCommand(gSerialLine);
        gSerialLine = "";
      }
    } else {
      gSerialLine += c;
      if (gSerialLine.length() > 180) {
        gSerialLine = "";
      }
    }
  }
}

void appSetup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("[BOOT] OSCILLO ESP8266 OLED starting"));

  if (kStatusLedPin != 0xFF) {
    pinMode(kStatusLedPin, OUTPUT);
  }
  setStatusLed(false);
  pinMode(kJoySwPin, INPUT_PULLUP);
  pinMode(kMorseOutPin, OUTPUT);
  digitalWrite(kMorseOutPin, LOW);
  for (size_t i = 0; i < 4; ++i) {
    pinMode(kPcm4BitPins[i], OUTPUT);
    digitalWrite(kPcm4BitPins[i], LOW);
  }
  randomSeed(micros() ^ static_cast<uint32_t>(analogRead(kJoyAdcPin)));

  EEPROM.begin(kEepromSize);
  loadWifiCredentialsFromEeprom();

  Serial.printf("[CFG] STA ssid='%s'\n", gStaSsid.c_str());
  Serial.printf("[CFG] OLED SDA=%u SCL=%u (%s)\n", kOledSdaPin, kOledSclPin, kOledBusLabel);
  Serial.printf("[CFG] MORSE_OUT=D4 DAC=D5,D6,D7,D8 SW=D3 A0=A0\n");

  initOled();
  scanI2cBusToSerial();
  startMorseStep(millis());
  gLastSineUs = micros();
  gLastScopeSampleMs = millis();
  gLastSineAmpUpdateMs = millis();
  gNextSineAmpDelayMs = kSineAmpMinStepMs;
  gSineGlitchAmp = static_cast<uint8_t>(
      random(static_cast<long>(kSineAmpMin), static_cast<long>(kSineAmpMax + 1U)));
  for (size_t i = 0; i < kOledWidth; ++i) {
    gMorseScope[i] = 0;
  }

  beginStaConnectWithBestRssi();
  initEspNow();
  startWebServer();
  printHelp();
}

void appLoop() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  serviceSerial();
  gServer.handleClient();
  sampleJoystick(nowMs);
  serviceMorse(nowMs);
  serviceMorseScope(nowMs);
  serviceGlitchSineAmplitude(nowMs);
  servicePseudoSine(nowUs);
  serviceEspNow(nowMs);
  serviceWifi(nowMs);
  updateStatusLed(nowMs);
  drawOled(nowMs);

  if ((nowMs - gLastLogMs) >= kLogPeriodMs) {
    gLastLogMs = nowMs;
    printStatusSerial();
  }
}

}  // namespace

void setup() { appSetup(); }

void loop() { appLoop(); }

#endif  // ARDUINO_ARCH_ESP8266
