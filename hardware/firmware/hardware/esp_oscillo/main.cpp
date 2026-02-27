#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp32-hal-rgb-led.h>

namespace {

constexpr char kDeviceName[] = "OSCILLO";

constexpr int kMorsePin = 4;
constexpr int kJoySwPin = 5;
constexpr int kJoyXPin = 6;
constexpr int kJoyYPin = 7;
constexpr int kServoXPin = 10;
constexpr int kServoYPin = 11;
constexpr int kPcm4BitPins[4] = {15, 16, 17, 18};
#if defined(LED_BUILTIN)
constexpr int kStatusLedPin = LED_BUILTIN;
#else
constexpr int kStatusLedPin = 48;
#endif
constexpr int kRgbLedPin = 48;

constexpr bool kStatusLedActiveHigh = true;
constexpr uint32_t kLedBlinkFastMs = 180;
constexpr uint32_t kLedBlinkSlowMs = 500;
constexpr uint32_t kLedHeartbeatPeriodMs = 1200;
constexpr uint32_t kLedHeartbeatOnMs = 80;
constexpr uint32_t kJoyModeToggleHoldMs = 900;
constexpr uint32_t kRgbRainbowUpdateMs = 20;   // 50 Hz refresh
constexpr uint32_t kRgbRainbowCycleMs = 1000;  // 1 Hz full color cycle
constexpr uint8_t kRgbRainbowMaxBrightness = 90;

constexpr int kServoChannelX = 0;
constexpr int kServoChannelY = 1;
constexpr int kServoFreqHz = 50;
constexpr int kServoResolutionBits = 14;
constexpr uint32_t kServoPeriodUs = 20000;
constexpr uint32_t kServoPulseMinUs = 500;
constexpr uint32_t kServoPulseMaxUs = 2500;

constexpr uint32_t kJoystickPollMs = 20;
constexpr uint32_t kLogPeriodMs = 150;
constexpr uint32_t kSineSamplePeriodUs = 220;
constexpr uint32_t kDiagPeriodMs = 1500;

constexpr uint32_t kStaConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kWifiScanCacheMs = 15000;
constexpr size_t kMaxScanEntries = 24;

constexpr uint32_t kEspNowHeartbeatMs = 60000;  // used as discovery broadcast period
constexpr uint32_t kEspNowPeerActiveMs = 180000;
constexpr size_t kEspNowMaxPeers = 16;
constexpr size_t kEspNowRxTextMax = 220;

constexpr char kPrefsNamespace[] = "oscillo";
constexpr char kPrefsSsidKey[] = "ssid";
constexpr char kPrefsPassKey[] = "pass";

constexpr char kDefaultSsid[] = "Les cils";
constexpr char kDefaultPass[] = "mascarade";

constexpr char kFallbackApSsid[] = "OSCILLO_AP";
constexpr char kFallbackApPass[] = "oscillo42";

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

struct WifiScanEntry {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  uint8_t bssid[6];
  wifi_auth_mode_t auth;
};

struct EspNowPeerEntry {
  uint8_t mac[6];
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint32_t rxCount;
  char name[20];
  char lastType[16];
};

Preferences gPrefs;
WebServer gServer(80);

String gStaSsid;
String gStaPass;

bool gStaConnecting = false;
bool gApActive = false;
bool gForceWifiReconnect = false;
uint32_t gStaConnectStartMs = 0;
uint32_t gLastWifiAttemptMs = 0;
uint32_t gLastScanMs = 0;

WifiScanEntry gScanEntries[kMaxScanEntries];
size_t gScanCount = 0;

uint16_t gJoyX = 2048;
uint16_t gJoyY = 2048;
bool gJoySwPressed = false;
uint16_t gJoyXMin = 4095;
uint16_t gJoyXMax = 0;
uint16_t gJoyYMin = 4095;
uint16_t gJoyYMax = 0;
uint8_t gServoXDeg = 90;
uint8_t gServoYDeg = 90;
uint16_t gMorseUnitMs = 350;
char gCurrentMorseLetter = 'L';
bool gMorseOutHigh = false;

uint32_t gLastJoystickMs = 0;
uint32_t gLastLogMs = 0;
uint32_t gLastDiagMs = 0;
uint32_t gMorseStepStartMs = 0;
uint32_t gMorseStepDurationMs = 0;
size_t gMorseStepIndex = 0;

uint32_t gLastSineUs = 0;
size_t gSineIndex = 0;

bool gEspNowReady = false;
uint32_t gLastEspNowTxMs = 0;
uint32_t gEspNowTxOk = 0;
uint32_t gEspNowTxFail = 0;
uint32_t gEspNowRxCount = 0;
uint32_t gEspNowLastRxMs = 0;
char gEspNowLastPeer[18] = "-";
EspNowPeerEntry gEspNowPeers[kEspNowMaxPeers];
size_t gEspNowPeerCount = 0;
bool gEspNowForceDiscovery = false;
String gSerialLine;
bool gStatusLedOn = false;
uint8_t gRgbR = 0;
uint8_t gRgbG = 0;
uint8_t gRgbB = 0;
bool gRgbJoyMode = false;
uint32_t gLastRgbRainbowUpdateMs = 0;
bool gJoySwPrev = false;
bool gJoyModeToggleArmed = false;
uint32_t gJoySwPressStartMs = 0;

const uint8_t kEspNowBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>OSCILLO Control Deck</title>
<style>
:root {
  --bg0: #071014;
  --bg1: #0b1c22;
  --panel: rgba(12, 27, 34, 0.82);
  --line: rgba(130, 244, 192, 0.24);
  --text: #d7fff0;
  --muted: #8ec8b6;
  --ok: #76f7ad;
  --warn: #ffd56a;
  --danger: #ff7f7f;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  color: var(--text);
  font-family: "Space Mono", "IBM Plex Sans", monospace;
  background:
    radial-gradient(circle at 20% 0%, #12303a 0, transparent 40%),
    radial-gradient(circle at 90% 100%, #2a2f1a 0, transparent 35%),
    linear-gradient(135deg, var(--bg0), var(--bg1));
  min-height: 100vh;
}
body::before {
  content: "";
  position: fixed;
  inset: 0;
  pointer-events: none;
  background:
    linear-gradient(rgba(130,244,192,0.07) 1px, transparent 1px) 0 0/100% 28px,
    linear-gradient(90deg, rgba(130,244,192,0.05) 1px, transparent 1px) 0 0/28px 100%;
}
main {
  max-width: 1080px;
  margin: 0 auto;
  padding: 18px;
  display: grid;
  gap: 14px;
}
.card {
  border: 1px solid var(--line);
  background: var(--panel);
  border-radius: 16px;
  padding: 14px;
  backdrop-filter: blur(2px);
  box-shadow: 0 16px 28px rgba(0,0,0,0.35);
}
.h {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}
h1 {
  font-size: 18px;
  margin: 0;
  letter-spacing: 0.12em;
  text-transform: uppercase;
}
h2 {
  font-size: 13px;
  margin: 0;
  color: var(--muted);
  letter-spacing: 0.1em;
  text-transform: uppercase;
}
.grid {
  display: grid;
  gap: 10px;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
}
.kv {
  display: flex;
  justify-content: space-between;
  padding: 6px 0;
  border-bottom: 1px dashed rgba(130,244,192,0.15);
  font-size: 13px;
}
.kv:last-child { border-bottom: 0; }
.bar {
  height: 10px;
  width: 100%;
  border-radius: 999px;
  border: 1px solid rgba(130,244,192,0.25);
  overflow: hidden;
  background: rgba(0,0,0,0.25);
  margin-top: 6px;
}
.fill {
  height: 100%;
  width: 0%;
  background: linear-gradient(90deg, #4cff9b, #d7ff7a);
  transition: width 0.15s linear;
}
.pill {
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 3px 10px;
  font-size: 12px;
}
.pill.ok { color: var(--ok); }
.pill.warn { color: var(--warn); }
.pill.danger { color: var(--danger); }
input, button {
  background: rgba(0,0,0,0.28);
  border: 1px solid rgba(130,244,192,0.25);
  color: var(--text);
  border-radius: 10px;
  padding: 9px 10px;
  font-family: inherit;
}
button {
  cursor: pointer;
  transition: transform 0.12s ease, border-color 0.12s ease;
}
button:hover {
  transform: translateY(-1px);
  border-color: rgba(130,244,192,0.6);
}
.row {
  display: grid;
  grid-template-columns: 1fr 1fr auto;
  gap: 8px;
}
.list {
  margin-top: 10px;
  display: grid;
  gap: 8px;
  max-height: 230px;
  overflow: auto;
}
.net {
  border: 1px solid rgba(130,244,192,0.16);
  border-radius: 10px;
  padding: 8px;
  display: grid;
  gap: 6px;
}
.net-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 12px;
}
@media (max-width: 700px) {
  .row { grid-template-columns: 1fr; }
}
</style>
</head>
<body>
<main>
  <div class="card">
    <div class="h">
      <h1>OSCILLO Control Deck</h1>
      <span id="wifiState" class="pill warn">BOOT</span>
    </div>
    <div class="grid">
      <section>
        <h2>Joystick</h2>
        <div class="kv"><span>VRx</span><b id="joyX">0</b></div>
        <div class="bar"><div id="joyXBar" class="fill"></div></div>
        <div class="kv"><span>VRy</span><b id="joyY">0</b></div>
        <div class="bar"><div id="joyYBar" class="fill"></div></div>
        <div class="kv"><span>SW</span><b id="joySw">UP</b></div>
      </section>
      <section>
        <h2>Morse / Servo</h2>
        <div class="kv"><span>Signal</span><b id="morseSignal">LOW</b></div>
        <div class="kv"><span>Letter</span><b id="morseLetter">L</b></div>
        <div class="kv"><span>Unit (ms)</span><b id="morseUnit">140</b></div>
        <div class="kv"><span>Servo X</span><b id="servoX">90°</b></div>
        <div class="kv"><span>Servo Y</span><b id="servoY">90°</b></div>
      </section>
      <section>
        <h2>Wi-Fi / ESP-NOW</h2>
        <div class="kv"><span>Mode</span><b id="wifiMode">-</b></div>
        <div class="kv"><span>STA SSID</span><b id="wifiSsid">-</b></div>
        <div class="kv"><span>STA IP</span><b id="wifiIp">-</b></div>
        <div class="kv"><span>RSSI</span><b id="wifiRssi">-</b></div>
        <div class="kv"><span>AP</span><b id="apInfo">-</b></div>
        <div class="kv"><span>ESP-NOW</span><b id="espnowInfo">-</b></div>
      </section>
    </div>
  </div>

  <div class="card">
    <div class="h">
      <h2>Selection Wi-Fi (NVS)</h2>
      <button onclick="scanWifi()">Scanner</button>
    </div>
    <div class="row">
      <input id="ssid" placeholder="SSID" />
      <input id="pass" placeholder="Password" type="password" />
      <button onclick="saveWifi()">Sauver + Connecter</button>
    </div>
    <div id="scanList" class="list"></div>
  </div>
</main>

<script>
const $ = (id) => document.getElementById(id);

function pct(v) {
  return Math.max(0, Math.min(100, (v / 4095) * 100));
}

function setWifiBadge(mode, connected) {
  const el = $('wifiState');
  el.textContent = connected ? `ONLINE ${mode}` : `OFFLINE ${mode}`;
  el.className = `pill ${connected ? 'ok' : 'warn'}`;
}

async function refreshStatus() {
  try {
    const res = await fetch('/api/status');
    const s = await res.json();

    $('joyX').textContent = s.joy.x;
    $('joyY').textContent = s.joy.y;
    $('joySw').textContent = s.joy.sw ? 'DOWN' : 'UP';
    $('joyXBar').style.width = `${pct(s.joy.x)}%`;
    $('joyYBar').style.width = `${pct(s.joy.y)}%`;

    $('morseSignal').textContent = s.morse.on ? 'HIGH' : 'LOW';
    $('morseLetter').textContent = s.morse.letter;
    $('morseUnit').textContent = s.morse.unit_ms;
    $('servoX').textContent = `${s.servo.x_deg}°`;
    $('servoY').textContent = `${s.servo.y_deg}°`;

    $('wifiMode').textContent = s.wifi.mode;
    $('wifiSsid').textContent = s.wifi.ssid || '-';
    $('wifiIp').textContent = s.wifi.ip || '-';
    $('wifiRssi').textContent = s.wifi.connected ? `${s.wifi.rssi} dBm` : '-';
    $('apInfo').textContent = s.wifi.ap_active ? `${s.wifi.ap_ssid} (${s.wifi.ap_ip})` : 'OFF';

    $('espnowInfo').textContent = `peers:${s.espnow.active_peers}/${s.espnow.peer_count} tx_ok:${s.espnow.tx_ok} rx:${s.espnow.rx_count}`;
    setWifiBadge(s.wifi.mode, s.wifi.connected);
  } catch (e) {
    $('wifiState').textContent = 'STATUS ERROR';
    $('wifiState').className = 'pill danger';
  }
}

async function saveWifi(ssidFromList = null) {
  const ssid = ssidFromList || $('ssid').value.trim();
  const pass = $('pass').value;
  if (!ssid) return;

  const body = new URLSearchParams({ ssid, pass });
  const res = await fetch('/api/wifi/select', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body
  });
  const payload = await res.json();
  if (!payload.ok) {
    alert(payload.error || 'Erreur wifi');
    return;
  }
  $('ssid').value = ssid;
  refreshStatus();
}

async function scanWifi() {
  const list = $('scanList');
  list.innerHTML = '<div class="net">Scan en cours...</div>';
  try {
    const res = await fetch('/api/wifi/scan');
    const payload = await res.json();
    list.innerHTML = '';
    payload.networks.forEach((n) => {
      const div = document.createElement('div');
      div.className = 'net';
      div.innerHTML = `
        <div class="net-top"><b>${n.ssid}</b><span>${n.rssi} dBm</span></div>
        <div class="net-top"><span>ch ${n.channel} | ${n.auth}</span><button>Utiliser</button></div>
      `;
      div.querySelector('button').onclick = () => saveWifi(n.ssid);
      list.appendChild(div);
    });
    if (!payload.networks.length) {
      list.innerHTML = '<div class="net">Aucun reseau detecte</div>';
    }
  } catch (e) {
    list.innerHTML = '<div class="net">Erreur de scan</div>';
  }
}

setInterval(refreshStatus, 500);
refreshStatus();
scanWifi();
</script>
</body>
</html>
)HTML";

const char* authToText(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    default:
      return "UNK";
  }
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

void formatMac(const uint8_t* mac, char* out, size_t outSize) {
  if (!mac || outSize < 18) {
    if (outSize > 0) out[0] = '\0';
    return;
  }
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
}

bool jsonExtractString(const char* text, const char* key, char* out, size_t outSize) {
  if (text == nullptr || key == nullptr || out == nullptr || outSize == 0) {
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

  EspNowPeerEntry& p = gEspNowPeers[static_cast<size_t>(idx)];
  p.lastSeenMs = nowMs;
  ++p.rxCount;
  if (type != nullptr && type[0] != '\0') {
    strncpy(p.lastType, type, sizeof(p.lastType) - 1);
    p.lastType[sizeof(p.lastType) - 1] = '\0';
  }
  if (name != nullptr && name[0] != '\0') {
    strncpy(p.name, name, sizeof(p.name) - 1);
    p.name[sizeof(p.name) - 1] = '\0';
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

void addEspNowPeerIfNeeded(const uint8_t* mac) {
  if (mac == nullptr) {
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  const esp_err_t rc = esp_now_add_peer(&peer);
  if (rc != ESP_OK && rc != ESP_ERR_ESPNOW_EXIST) {
    char macText[18] = "-";
    formatMac(mac, macText, sizeof(macText));
    Serial.printf("[ESPNOW] add peer %s failed err=%d\n", macText, static_cast<int>(rc));
  }
}

bool sendEspNowFrame(const uint8_t* mac, const char* type) {
  if (!gEspNowReady || mac == nullptr || type == nullptr) {
    return false;
  }

  char payload[220];
  const int n = snprintf(payload, sizeof(payload),
                         "{\"type\":\"%s\",\"device\":\"%s\",\"uptime_ms\":%lu}", type,
                         kDeviceName, static_cast<unsigned long>(millis()));
  if (n <= 0) {
    return false;
  }

  const esp_err_t rc =
      esp_now_send(mac, reinterpret_cast<const uint8_t*>(payload), strnlen(payload, sizeof(payload)));
  if (rc != ESP_OK) {
    ++gEspNowTxFail;
    return false;
  }
  return true;
}

const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT_RESET";
    case ESP_RST_SW:
      return "SW_RESET";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
#ifdef ESP_RST_USB
    case ESP_RST_USB:
      return "USB";
#endif
#ifdef ESP_RST_JTAG
    case ESP_RST_JTAG:
      return "JTAG";
#endif
#ifdef ESP_RST_EFUSE
    case ESP_RST_EFUSE:
      return "EFUSE";
#endif
#ifdef ESP_RST_PWR_GLITCH
    case ESP_RST_PWR_GLITCH:
      return "PWR_GLITCH";
#endif
#ifdef ESP_RST_CPU_LOCKUP
    case ESP_RST_CPU_LOCKUP:
      return "CPU_LOCKUP";
#endif
    default:
      return "OTHER";
  }
}

bool isAdc1Pin(int pin) { return pin >= 1 && pin <= 10; }

bool isAdc2Pin(int pin) { return pin >= 11 && pin <= 20; }

bool isBootStrapPin(int pin) { return pin == 0 || pin == 3 || pin == 45 || pin == 46; }

bool isUsbJtagPin(int pin) { return pin == 19 || pin == 20; }

const char* adcDomainText(int pin) {
  if (isAdc1Pin(pin)) return "ADC1";
  if (isAdc2Pin(pin)) return "ADC2";
  return "-";
}

void setStatusLed(bool on) {
  gStatusLedOn = on;
  if (kStatusLedPin == kRgbLedPin) return;
  const bool level = kStatusLedActiveHigh ? on : !on;
  digitalWrite(kStatusLedPin, level ? HIGH : LOW);
}

void setRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  gRgbR = r;
  gRgbG = g;
  gRgbB = b;
  neopixelWrite(static_cast<uint8_t>(kRgbLedPin), r, g, b);
}

void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (s == 0) {
    r = v;
    g = v;
    b = v;
    return;
  }

  const uint8_t region = h / 43;
  const uint8_t remainder = static_cast<uint8_t>((h - (region * 43)) * 6);
  const uint8_t p = static_cast<uint8_t>((v * (255 - s)) >> 8);
  const uint8_t q = static_cast<uint8_t>((v * (255 - ((s * remainder) >> 8))) >> 8);
  const uint8_t t = static_cast<uint8_t>((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

  switch (region) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
}

void refreshMorsePinOutput() {
  digitalWrite(kMorsePin, (!gRgbJoyMode && gMorseOutHigh) ? HIGH : LOW);
}

void printPinMappingAndRisks() {
  const int pins[] = {kMorsePin,      kJoySwPin,       kJoyXPin,        kJoyYPin,        kServoXPin,    kServoYPin,
                      kPcm4BitPins[0], kPcm4BitPins[1], kPcm4BitPins[2], kPcm4BitPins[3], kStatusLedPin, kRgbLedPin};
  const char* labels[] = {"MORSE",   "JOY_SW", "JOY_X", "JOY_Y", "SERVO_X", "SERVO_Y",
                          "PCM_D0", "PCM_D1", "PCM_D2", "PCM_D3", "LED_SYS", "RGB_LED"};
  constexpr size_t kCount = sizeof(pins) / sizeof(pins[0]);

  bool duplicate = false;
  for (size_t i = 0; i < kCount; ++i) {
    for (size_t j = i + 1; j < kCount; ++j) {
      if (pins[i] == pins[j]) {
        duplicate = true;
        Serial.printf("[PIN][CONFLICT] %s and %s both use GPIO%d\n", labels[i], labels[j], pins[i]);
      }
    }
  }
  if (!duplicate) {
    Serial.println("[PIN] map unique: no GPIO duplicates");
  }

  for (size_t i = 0; i < kCount; ++i) {
    Serial.printf("[PIN] %-7s GPIO%-2d adc=%s", labels[i], pins[i], adcDomainText(pins[i]));
    if (isBootStrapPin(pins[i])) Serial.print(" WARN:BOOTSTRAP");
    if (isUsbJtagPin(pins[i])) Serial.print(" WARN:USB-JTAG");
    Serial.println();
  }

  Serial.printf("[ADC] joystick pins: X=GPIO%d(%s) Y=GPIO%d(%s)\n", kJoyXPin, adcDomainText(kJoyXPin),
                kJoyYPin, adcDomainText(kJoyYPin));
  Serial.println("[ADC] note: analogRead on ADC2 pins can fail when WiFi is active");
}

uint32_t servoDutyFromAngle(uint8_t angleDeg) {
  const uint32_t pulseUs =
      map(static_cast<int32_t>(angleDeg), 0, 180, kServoPulseMinUs, kServoPulseMaxUs);
  const uint32_t maxDuty = (1UL << kServoResolutionBits) - 1UL;
  return (pulseUs * maxDuty) / kServoPeriodUs;
}

void writeServo(int channel, uint8_t angleDeg) {
  ledcWrite(channel, servoDutyFromAngle(angleDeg));
}

uint8_t mapJoystickToAngle(uint16_t raw) {
  const int32_t mapped = map(static_cast<int32_t>(raw), 0, 4095, 0, 180);
  return static_cast<uint8_t>(constrain(mapped, 0, 180));
}

uint16_t mapXToMorseUnitMs(uint16_t rawX) {
  const int32_t unit = map(static_cast<int32_t>(rawX), 0, 4095, 500, 180);
  return static_cast<uint16_t>(constrain(unit, 180, 500));
}

void writePcm4Bit(uint8_t nibble) {
  for (size_t i = 0; i < 4; ++i) {
    digitalWrite(kPcm4BitPins[i], ((nibble >> i) & 0x01U) ? HIGH : LOW);
  }
}

void startMorseStep(uint32_t nowMs) {
  const MorseStep& step = kMorseSequence[gMorseStepIndex];
  gCurrentMorseLetter = step.letter;
  gMorseOutHigh = step.on;
  refreshMorsePinOutput();
  gMorseStepDurationMs = static_cast<uint32_t>(step.units) * gMorseUnitMs;
  gMorseStepStartMs = nowMs;
}

void updateMorse(uint32_t nowMs) {
  if ((nowMs - gMorseStepStartMs) < gMorseStepDurationMs) return;
  gMorseStepIndex = (gMorseStepIndex + 1U) % kMorseSequenceLen;
  startMorseStep(nowMs);
}

void updatePseudoSine(uint32_t nowUs) {
  if ((nowUs - gLastSineUs) < kSineSamplePeriodUs) return;
  gLastSineUs = nowUs;
  writePcm4Bit(kSine4BitLut[gSineIndex]);
  gSineIndex = (gSineIndex + 1U) % kSine4BitLen;
}

void updateStatusLed(uint32_t nowMs) {
  // Hold joystick switch to mirror Morse output directly on onboard LED.
  if (gJoySwPressed) {
    setStatusLed(gMorseOutHigh);
    return;
  }

  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  if (staConnected) {
    const uint32_t phase = nowMs % kLedHeartbeatPeriodMs;
    setStatusLed(phase < kLedHeartbeatOnMs);
    return;
  }

  if (gApActive) {
    const uint32_t phase = nowMs % 1000;
    const bool on = (phase < 100) || (phase >= 220 && phase < 320);
    setStatusLed(on);
    return;
  }

  if (gStaConnecting) {
    setStatusLed(((nowMs / kLedBlinkSlowMs) % 2U) == 0U);
    return;
  }

  setStatusLed(((nowMs / kLedBlinkFastMs) % 2U) == 0U);
}

void updateRgbLed(uint32_t nowMs) {
  if (gRgbJoyMode) {
    if ((nowMs - gLastRgbRainbowUpdateMs) < kRgbRainbowUpdateMs) return;
    gLastRgbRainbowUpdateMs = nowMs;

    const uint32_t phase = nowMs % kRgbRainbowCycleMs;
    const uint8_t hue = static_cast<uint8_t>((phase * 255UL) / (kRgbRainbowCycleMs - 1UL));
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    hsvToRgb(hue, 255, kRgbRainbowMaxBrightness, r, g, b);
    if (gMorseOutHigh) {
      setRgbLed(255, 0, 0);
    } else {
      setRgbLed(r, g, b);
    }
    return;
  }

  if (gJoySwPressed) {
    if (gMorseOutHigh) {
      setRgbLed(180, 0, 200);
    } else {
      setRgbLed(0, 0, 0);
    }
    return;
  }

  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  if (staConnected) {
    const uint32_t phase = nowMs % kLedHeartbeatPeriodMs;
    if (phase < kLedHeartbeatOnMs) {
      setRgbLed(0, 60, 0);
    } else {
      setRgbLed(0, 0, 0);
    }
    return;
  }

  if (gApActive) {
    const uint32_t phase = nowMs % 1000;
    const bool on = (phase < 100) || (phase >= 220 && phase < 320);
    setRgbLed(on ? 60 : 0, on ? 40 : 0, 0);
    return;
  }

  if (gStaConnecting) {
    const bool on = ((nowMs / kLedBlinkSlowMs) % 2U) == 0U;
    setRgbLed(0, 0, on ? 60 : 0);
    return;
  }

  const bool on = ((nowMs / kLedBlinkFastMs) % 2U) == 0U;
  setRgbLed(on ? 60 : 0, 0, 0);
}

void updateJoystickAndServos(uint32_t nowMs) {
  if ((nowMs - gLastJoystickMs) < kJoystickPollMs) return;
  gLastJoystickMs = nowMs;

  gJoyX = static_cast<uint16_t>(analogRead(kJoyXPin));
  gJoyY = static_cast<uint16_t>(analogRead(kJoyYPin));
  gJoySwPressed = (digitalRead(kJoySwPin) == LOW);

  if (gJoySwPressed && !gJoySwPrev) {
    gJoySwPressStartMs = nowMs;
    gJoyModeToggleArmed = true;
  }
  if (!gJoySwPressed && gJoySwPrev) {
    if (gJoyModeToggleArmed && (nowMs - gJoySwPressStartMs) >= kJoyModeToggleHoldMs) {
      gRgbJoyMode = !gRgbJoyMode;
      gLastRgbRainbowUpdateMs = 0;
      Serial.printf("[RGB] mode=%s (long press SW)\n", gRgbJoyMode ? "JOY_RAINBOW" : "STATUS");
      refreshMorsePinOutput();
    }
    gJoyModeToggleArmed = false;
  }
  gJoySwPrev = gJoySwPressed;

  if (gJoyX < gJoyXMin) gJoyXMin = gJoyX;
  if (gJoyX > gJoyXMax) gJoyXMax = gJoyX;
  if (gJoyY < gJoyYMin) gJoyYMin = gJoyY;
  if (gJoyY > gJoyYMax) gJoyYMax = gJoyY;

  gServoXDeg = mapJoystickToAngle(gJoyX);
  gServoYDeg = mapJoystickToAngle(gJoyY);
  gMorseUnitMs = mapXToMorseUnitMs(gJoyX);

  writeServo(kServoChannelX, gServoXDeg);
  writeServo(kServoChannelY, gServoYDeg);
}

void loadWifiCredentialsFromNvs() {
  gPrefs.begin(kPrefsNamespace, false);
  gStaSsid = gPrefs.getString(kPrefsSsidKey, "");
  gStaPass = gPrefs.getString(kPrefsPassKey, "");

  if (gStaSsid.isEmpty()) {
    gStaSsid = kDefaultSsid;
    gStaPass = kDefaultPass;
    gPrefs.putString(kPrefsSsidKey, gStaSsid);
    gPrefs.putString(kPrefsPassKey, gStaPass);
  }
}

void saveWifiCredentialsToNvs(const String& ssid, const String& pass) {
  gPrefs.putString(kPrefsSsidKey, ssid);
  gPrefs.putString(kPrefsPassKey, pass);
}

void refreshWifiScanCache() {
  gScanCount = 0;
  const int total = WiFi.scanNetworks(false, true);
  if (total <= 0) {
    WiFi.scanDelete();
    gLastScanMs = millis();
    return;
  }

  for (int i = 0; i < total && gScanCount < kMaxScanEntries; ++i) {
    WifiScanEntry& e = gScanEntries[gScanCount++];
    e.ssid = WiFi.SSID(i);
    e.rssi = WiFi.RSSI(i);
    e.channel = static_cast<uint8_t>(WiFi.channel(i));
    e.auth = WiFi.encryptionType(i);
    const uint8_t* bssid = WiFi.BSSID(i);
    if (bssid) {
      memcpy(e.bssid, bssid, sizeof(e.bssid));
    } else {
      memset(e.bssid, 0, sizeof(e.bssid));
    }
  }

  WiFi.scanDelete();
  gLastScanMs = millis();
}

int findBestNetworkIndexForSsid(const String& targetSsid) {
  int bestIdx = -1;
  int32_t bestRssi = -1000;
  for (size_t i = 0; i < gScanCount; ++i) {
    if (gScanEntries[i].ssid != targetSsid) continue;
    if (gScanEntries[i].rssi > bestRssi) {
      bestRssi = gScanEntries[i].rssi;
      bestIdx = static_cast<int>(i);
    }
  }
  return bestIdx;
}

void ensureFallbackAp() {
  if (gApActive) return;
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(kFallbackApSsid, kFallbackApPass)) {
    gApActive = true;
    Serial.printf("[WIFI] fallback AP on ssid=%s ip=%s\n", kFallbackApSsid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[WIFI] fallback AP failed");
  }
}

void stopFallbackAp() {
  if (!gApActive) return;
  WiFi.softAPdisconnect(true);
  gApActive = false;
}

bool beginStaConnectWithBestRssi() {
  if (gStaSsid.isEmpty()) return false;

  const uint32_t now = millis();
  if ((now - gLastScanMs) > kWifiScanCacheMs || gScanCount == 0) {
    refreshWifiScanCache();
  }

  const int bestIdx = findBestNetworkIndexForSsid(gStaSsid);
  if (bestIdx < 0) {
    Serial.printf("[WIFI] ssid not found: %s\n", gStaSsid.c_str());
    return false;
  }

  const WifiScanEntry& best = gScanEntries[bestIdx];
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.begin(gStaSsid.c_str(), gStaPass.c_str(), best.channel, best.bssid, true);

  gStaConnecting = true;
  gStaConnectStartMs = millis();
  gLastWifiAttemptMs = gStaConnectStartMs;

  char mac[18];
  formatMac(best.bssid, mac, sizeof(mac));
  Serial.printf("[WIFI] connect ssid=%s best_rssi=%d ch=%u bssid=%s\n", gStaSsid.c_str(),
                static_cast<int>(best.rssi), static_cast<unsigned>(best.channel), mac);
  return true;
}

void serviceWifi(uint32_t nowMs) {
  const wl_status_t sta = WiFi.status();
  const bool connected = (sta == WL_CONNECTED);

  if (connected) {
    gStaConnecting = false;
    stopFallbackAp();
    return;
  }

  if (gStaConnecting) {
    if ((nowMs - gStaConnectStartMs) > kStaConnectTimeoutMs) {
      gStaConnecting = false;
      Serial.println("[WIFI] connect timeout -> AP fallback");
      ensureFallbackAp();
    }
    return;
  }

  if (gForceWifiReconnect || (nowMs - gLastWifiAttemptMs) > kWifiRetryMs) {
    gForceWifiReconnect = false;
    if (!beginStaConnectWithBestRssi()) {
      gLastWifiAttemptMs = nowMs;
      ensureFallbackAp();
    }
  }
}

void onEspNowSent(const uint8_t* macAddr, esp_now_send_status_t status) {
  (void)macAddr;
  if (status == ESP_NOW_SEND_SUCCESS) {
    ++gEspNowTxOk;
  } else {
    ++gEspNowTxFail;
  }
}

void onEspNowRecv(const uint8_t* macAddr, const uint8_t* data, int len) {
  if (macAddr == nullptr || data == nullptr || len <= 0) {
    return;
  }
  ++gEspNowRxCount;
  gEspNowLastRxMs = millis();
  formatMac(macAddr, gEspNowLastPeer, sizeof(gEspNowLastPeer));

  const size_t n = (len < static_cast<int>(kEspNowRxTextMax)) ? static_cast<size_t>(len) : kEspNowRxTextMax;
  char text[kEspNowRxTextMax + 1];
  memcpy(text, data, n);
  text[n] = '\0';
  sanitizeAsciiText(text);

  char type[16] = {0};
  char name[20] = {0};
  if (!jsonExtractString(text, "type", type, sizeof(type))) {
    return;
  }
  if (!jsonExtractString(text, "device", name, sizeof(name))) {
    jsonExtractString(text, "name", name, sizeof(name));
  }
  updateEspNowPeer(macAddr, type, name);

  if (strcmp(type, "discovery") == 0) {
    addEspNowPeerIfNeeded(macAddr);
    sendEspNowFrame(macAddr, "announce");
  }
}

void initEspNow() {
  if (esp_now_init() != ESP_OK) {
    gEspNowReady = false;
    Serial.println("[ESPNOW] init failed");
    return;
  }

  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowRecv);

  addEspNowPeerIfNeeded(kEspNowBroadcastMac);

  gEspNowReady = true;
  Serial.println("[ESPNOW] ready (broadcast+discovery)");
}

void serviceEspNow(uint32_t nowMs) {
  if (!gEspNowReady) return;
  if (!gEspNowForceDiscovery && (nowMs - gLastEspNowTxMs) < kEspNowHeartbeatMs) return;
  gEspNowForceDiscovery = false;
  gLastEspNowTxMs = nowMs;
  sendEspNowFrame(kEspNowBroadcastMac, "discovery");
}

String wifiModeText() {
  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  if (staConnected && gApActive) return "AP_STA";
  if (staConnected) return "STA";
  if (gApActive && gStaConnecting) return "AP+CONNECTING";
  if (gApActive) return "AP";
  if (gStaConnecting) return "STA_CONNECTING";
  return "IDLE";
}

void printJoystickDiag() {
  Serial.printf("[JOY] x=%u y=%u sw=%s | x[min=%u max=%u] y[min=%u max=%u] | servo=%u/%u | morse_unit=%ums\n",
                gJoyX, gJoyY, gJoySwPressed ? "DOWN" : "UP", gJoyXMin, gJoyXMax, gJoyYMin, gJoyYMax,
                gServoXDeg, gServoYDeg, gMorseUnitMs);
}

void printWifiDiag() {
  const bool connected = (WiFi.status() == WL_CONNECTED);
  Serial.printf("[WIFI] mode=%s connected=%s ssid=%s ip=%s rssi=%d ap=%s ap_ip=%s\n",
                wifiModeText().c_str(), connected ? "yes" : "no", gStaSsid.c_str(),
                connected ? WiFi.localIP().toString().c_str() : "-", connected ? WiFi.RSSI() : 0,
                gApActive ? "on" : "off", gApActive ? WiFi.softAPIP().toString().c_str() : "-");
}

void printEspNowDiag() {
  const uint32_t nowMs = millis();
  Serial.printf("[ESPNOW] ready=%s peers=%u active=%u tx_ok=%lu tx_fail=%lu rx=%lu last_peer=%s last_rx_ms=%lu\n",
                gEspNowReady ? "yes" : "no", static_cast<unsigned>(gEspNowPeerCount),
                static_cast<unsigned>(countEspNowActivePeers(nowMs)), static_cast<unsigned long>(gEspNowTxOk),
                static_cast<unsigned long>(gEspNowTxFail), static_cast<unsigned long>(gEspNowRxCount), gEspNowLastPeer,
                static_cast<unsigned long>(gEspNowLastRxMs));
}

void printEspNowPeers() {
  const uint32_t nowMs = millis();
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    char macText[18] = "-";
    formatMac(gEspNowPeers[i].mac, macText, sizeof(macText));
    const uint32_t ageMs = nowMs - gEspNowPeers[i].lastSeenMs;
    Serial.printf("  [%02u] mac=%s name=%s type=%s rx=%lu age=%lums %s\n", static_cast<unsigned>(i),
                  macText, gEspNowPeers[i].name, gEspNowPeers[i].lastType,
                  static_cast<unsigned long>(gEspNowPeers[i].rxCount), static_cast<unsigned long>(ageMs),
                  (ageMs <= kEspNowPeerActiveMs) ? "ACTIVE" : "STALE");
  }
}

void printRgbDiag() {
  Serial.printf("[RGB] mode=%s pin=%d rgb=(%u,%u,%u)\n", gRgbJoyMode ? "JOY_RAINBOW" : "STATUS",
                kRgbLedPin, static_cast<unsigned>(gRgbR), static_cast<unsigned>(gRgbG),
                static_cast<unsigned>(gRgbB));
}

void printSerialHelp() {
  Serial.println(
      "[CMD] help | status | pins | joy | joyreset | wifi | espnow | peers | discover | rgb | rgbjoy | rgbstatus | scan | reconnect | ap");
}

void handleSerialCommand(const String& cmdRaw) {
  String cmd = cmdRaw;
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.isEmpty()) return;

  Serial.printf("[CMD] %s\n", cmd.c_str());

  if (cmd == "help") {
    printSerialHelp();
    return;
  }
  if (cmd == "status") {
    printJoystickDiag();
    printWifiDiag();
    printEspNowDiag();
    printRgbDiag();
    return;
  }
  if (cmd == "pins") {
    printPinMappingAndRisks();
    return;
  }
  if (cmd == "joy") {
    printJoystickDiag();
    return;
  }
  if (cmd == "joyreset") {
    gJoyXMin = 4095;
    gJoyXMax = 0;
    gJoyYMin = 4095;
    gJoyYMax = 0;
    Serial.println("[JOY] min/max reset");
    return;
  }
  if (cmd == "wifi") {
    printWifiDiag();
    return;
  }
  if (cmd == "espnow") {
    printEspNowDiag();
    return;
  }
  if (cmd == "peers") {
    printEspNowDiag();
    printEspNowPeers();
    return;
  }
  if (cmd == "discover") {
    gEspNowForceDiscovery = true;
    Serial.println("[ESPNOW] forced discovery broadcast");
    return;
  }
  if (cmd == "rgb") {
    printRgbDiag();
    return;
  }
  if (cmd == "rgbjoy") {
    gRgbJoyMode = true;
    gLastRgbRainbowUpdateMs = 0;
    refreshMorsePinOutput();
    Serial.println("[RGB] mode set to JOY_RAINBOW");
    return;
  }
  if (cmd == "rgbstatus") {
    gRgbJoyMode = false;
    refreshMorsePinOutput();
    Serial.println("[RGB] mode set to STATUS");
    return;
  }
  if (cmd == "scan") {
    refreshWifiScanCache();
    Serial.printf("[WIFI] scan entries=%u\n", static_cast<unsigned>(gScanCount));
    for (size_t i = 0; i < gScanCount; ++i) {
      Serial.printf("  - ssid=%s rssi=%d ch=%u auth=%s\n", gScanEntries[i].ssid.c_str(),
                    static_cast<int>(gScanEntries[i].rssi), static_cast<unsigned>(gScanEntries[i].channel),
                    authToText(gScanEntries[i].auth));
    }
    return;
  }
  if (cmd == "reconnect") {
    gForceWifiReconnect = true;
    Serial.println("[WIFI] reconnect requested");
    return;
  }
  if (cmd == "ap") {
    ensureFallbackAp();
    printWifiDiag();
    return;
  }

  Serial.println("[CMD] unknown command");
  printSerialHelp();
}

void serviceSerialConsole() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      if (!gSerialLine.isEmpty()) {
        handleSerialCommand(gSerialLine);
        gSerialLine = "";
      }
      continue;
    }
    if (c >= 32 && c <= 126 && gSerialLine.length() < 96) {
      gSerialLine += c;
    }
  }
}

void handleRoot() {
  gServer.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleStatus() {
  const uint32_t nowMs = millis();
  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  const String staIp = staConnected ? WiFi.localIP().toString() : "";
  const String apIp = gApActive ? WiFi.softAPIP().toString() : "";

  String json;
  json.reserve(768);
  json += "{";
  json += "\"device\":\"OSCILLO\",";
  json += "\"uptime_ms\":" + String(nowMs) + ",";

  json += "\"joy\":{";
  json += "\"x\":" + String(gJoyX) + ",";
  json += "\"y\":" + String(gJoyY) + ",";
  json += "\"sw\":" + String(gJoySwPressed ? "true" : "false");
  json += "},";

  json += "\"servo\":{";
  json += "\"x_deg\":" + String(gServoXDeg) + ",";
  json += "\"y_deg\":" + String(gServoYDeg);
  json += "},";

  json += "\"morse\":{";
  json += "\"text\":\"LEFOU\",";
  json += "\"letter\":\"" + String(gCurrentMorseLetter) + "\",";
  json += "\"on\":" + String(gMorseOutHigh ? "true" : "false") + ",";
  json += "\"unit_ms\":" + String(gMorseUnitMs) + ",";
  json += "\"step\":" + String(gMorseStepIndex);
  json += "},";

  json += "\"wifi\":{";
  json += "\"mode\":\"" + wifiModeText() + "\",";
  json += "\"connected\":" + String(staConnected ? "true" : "false") + ",";
  json += "\"ssid\":\"" + jsonEscape(gStaSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(staIp) + "\",";
  json += "\"rssi\":" + String(staConnected ? WiFi.RSSI() : 0) + ",";
  json += "\"ap_active\":" + String(gApActive ? "true" : "false") + ",";
  json += "\"ap_ssid\":\"" + String(kFallbackApSsid) + "\",";
  json += "\"ap_ip\":\"" + jsonEscape(apIp) + "\"";
  json += "},";

  json += "\"espnow\":{";
  json += "\"ready\":" + String(gEspNowReady ? "true" : "false") + ",";
  json += "\"tx_ok\":" + String(gEspNowTxOk) + ",";
  json += "\"tx_fail\":" + String(gEspNowTxFail) + ",";
  json += "\"rx_count\":" + String(gEspNowRxCount) + ",";
  json += "\"peer_count\":" + String(static_cast<unsigned>(gEspNowPeerCount)) + ",";
  json += "\"active_peers\":" + String(static_cast<unsigned>(countEspNowActivePeers(nowMs))) + ",";
  json += "\"last_peer\":\"" + String(gEspNowLastPeer) + "\",";
  json += "\"last_rx_ms\":" + String(gEspNowLastRxMs);
  json += "},";

  json += "\"led\":{";
  json += "\"pin\":" + String(kStatusLedPin) + ",";
  json += "\"on\":" + String(gStatusLedOn ? "true" : "false");
  json += "},";

  json += "\"rgb\":{";
  json += "\"pin\":" + String(kRgbLedPin) + ",";
  json += "\"mode\":\"" + String(gRgbJoyMode ? "JOY_RAINBOW" : "STATUS") + "\",";
  json += "\"r\":" + String(gRgbR) + ",";
  json += "\"g\":" + String(gRgbG) + ",";
  json += "\"b\":" + String(gRgbB);
  json += "}";

  json += "}";

  gServer.send(200, "application/json", json);
}

void handleWifiScan() {
  refreshWifiScanCache();

  String json;
  json.reserve(1024);
  json += "{\"ok\":true,\"count\":" + String(gScanCount) + ",\"networks\":[";

  for (size_t i = 0; i < gScanCount; ++i) {
    if (i > 0) json += ",";
    const WifiScanEntry& e = gScanEntries[i];
    json += "{";
    json += "\"ssid\":\"" + jsonEscape(e.ssid) + "\",";
    json += "\"rssi\":" + String(e.rssi) + ",";
    json += "\"channel\":" + String(e.channel) + ",";
    json += "\"auth\":\"" + String(authToText(e.auth)) + "\"";
    json += "}";
  }

  json += "]}";
  gServer.send(200, "application/json", json);
}

void handleWifiSelect() {
  const String ssid = gServer.arg("ssid");
  const String pass = gServer.arg("pass");

  if (ssid.isEmpty()) {
    gServer.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
    return;
  }

  gStaSsid = ssid;
  gStaPass = pass;
  saveWifiCredentialsToNvs(gStaSsid, gStaPass);

  gForceWifiReconnect = true;
  gServer.send(200, "application/json",
               "{\"ok\":true,\"message\":\"credentials saved, reconnect started\"}");
}

void handleEspNowPeers() {
  const uint32_t nowMs = millis();
  String json;
  json.reserve(1200);
  json += "{";
  json += "\"ok\":true,";
  json += "\"count\":" + String(static_cast<unsigned>(gEspNowPeerCount)) + ",";
  json += "\"active\":" + String(static_cast<unsigned>(countEspNowActivePeers(nowMs))) + ",";
  json += "\"peers\":[";
  for (size_t i = 0; i < gEspNowPeerCount; ++i) {
    if (i > 0) json += ",";
    char macText[18] = "-";
    formatMac(gEspNowPeers[i].mac, macText, sizeof(macText));
    const uint32_t ageMs = nowMs - gEspNowPeers[i].lastSeenMs;
    const bool active = ageMs <= kEspNowPeerActiveMs;
    json += "{";
    json += "\"mac\":\"" + String(macText) + "\",";
    json += "\"name\":\"" + String(gEspNowPeers[i].name) + "\",";
    json += "\"type\":\"" + String(gEspNowPeers[i].lastType) + "\",";
    json += "\"rx_count\":" + String(gEspNowPeers[i].rxCount) + ",";
    json += "\"last_seen_ms\":" + String(gEspNowPeers[i].lastSeenMs) + ",";
    json += "\"age_ms\":" + String(ageMs) + ",";
    json += "\"active\":" + String(active ? "true" : "false");
    json += "}";
  }
  json += "]}";
  gServer.send(200, "application/json", json);
}

void startWebServer() {
  gServer.on("/", HTTP_GET, handleRoot);
  gServer.on("/api/status", HTTP_GET, handleStatus);
  gServer.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  gServer.on("/api/wifi/select", HTTP_POST, handleWifiSelect);
  gServer.on("/api/espnow/peers", HTTP_GET, handleEspNowPeers);
  gServer.onNotFound([]() {
    gServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });
  gServer.begin();
}

void logRuntimeStatus(uint32_t nowMs) {
  if ((nowMs - gLastLogMs) < kLogPeriodMs) return;
  gLastLogMs = nowMs;

  Serial.printf(
      "JOY X=%4u[%4u..%4u] Y=%4u[%4u..%4u] SW=%s | SERVO X=%3u Y=%3u | MORSE %c %s unit=%3ums | WIFI=%s RSSI=%d | ESPNOW peers=%u/%u ok=%lu fail=%lu rx=%lu\n",
      gJoyX, gJoyXMin, gJoyXMax, gJoyY, gJoyYMin, gJoyYMax, gJoySwPressed ? "DOWN" : "UP", gServoXDeg,
      gServoYDeg, gCurrentMorseLetter, gMorseOutHigh ? "ON" : "OFF", gMorseUnitMs, wifiModeText().c_str(),
      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0, static_cast<unsigned>(countEspNowActivePeers(nowMs)),
      static_cast<unsigned>(gEspNowPeerCount), static_cast<unsigned long>(gEspNowTxOk),
      static_cast<unsigned long>(gEspNowTxFail), static_cast<unsigned long>(gEspNowRxCount));

  if ((nowMs - gLastDiagMs) >= kDiagPeriodMs) {
    gLastDiagMs = nowMs;
    const uint16_t rangeX = (gJoyXMax >= gJoyXMin) ? (gJoyXMax - gJoyXMin) : 0;
    const uint16_t rangeY = (gJoyYMax >= gJoyYMin) ? (gJoyYMax - gJoyYMin) : 0;
    if (rangeX < 250) {
      Serial.printf("[WARN] joystick X low dynamic range: %u (check wiring or 3V3/GND)\n",
                    static_cast<unsigned>(rangeX));
    }
    if (rangeY < 250) {
      Serial.printf("[WARN] joystick Y low dynamic range: %u (check wiring or 3V3/GND)\n",
                    static_cast<unsigned>(rangeY));
    }
  }
}

void initIo() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  pinMode(kMorsePin, OUTPUT);
  if (kStatusLedPin != kRgbLedPin) {
    pinMode(kStatusLedPin, OUTPUT);
  }
  pinMode(kJoySwPin, INPUT_PULLUP);
  pinMode(kJoyXPin, INPUT);
  pinMode(kJoyYPin, INPUT);
  setStatusLed(false);
  setRgbLed(0, 0, 0);

  for (size_t i = 0; i < 4; ++i) {
    pinMode(kPcm4BitPins[i], OUTPUT);
    digitalWrite(kPcm4BitPins[i], LOW);
  }

  ledcSetup(kServoChannelX, kServoFreqHz, kServoResolutionBits);
  ledcSetup(kServoChannelY, kServoFreqHz, kServoResolutionBits);
  ledcAttachPin(kServoXPin, kServoChannelX);
  ledcAttachPin(kServoYPin, kServoChannelY);

  gJoyX = static_cast<uint16_t>(analogRead(kJoyXPin));
  gJoyY = static_cast<uint16_t>(analogRead(kJoyYPin));
  gServoXDeg = mapJoystickToAngle(gJoyX);
  gServoYDeg = mapJoystickToAngle(gJoyY);
  gMorseUnitMs = mapXToMorseUnitMs(gJoyX);

  writeServo(kServoChannelX, gServoXDeg);
  writeServo(kServoChannelY, gServoYDeg);

  gMorseStepIndex = 0;
  startMorseStep(millis());
  gLastSineUs = micros();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  const esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("[BOOT] OSCILLO startup reset_reason=%d (%s)\n", static_cast<int>(rr), resetReasonText(rr));
  Serial.printf("[BOOT] chip=%s rev=%d flash=%uMB psram=%uMB\n", ESP.getChipModel(),
                static_cast<int>(ESP.getChipRevision()), ESP.getFlashChipSize() / (1024UL * 1024UL),
                ESP.getPsramSize() / (1024UL * 1024UL));
  printPinMappingAndRisks();

  initIo();

  loadWifiCredentialsFromNvs();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(kDeviceName);
  WiFi.setSleep(false);

  if (!beginStaConnectWithBestRssi()) {
    ensureFallbackAp();
  }

  initEspNow();
  startWebServer();

  printSerialHelp();
  printJoystickDiag();
  printWifiDiag();
  printEspNowDiag();
  Serial.println("[BOOT] web ui ready");
}

void loop() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  updateJoystickAndServos(nowMs);
  updateMorse(nowMs);
  updatePseudoSine(nowUs);
  updateStatusLed(nowMs);
  updateRgbLed(nowMs);
  refreshMorsePinOutput();

  serviceWifi(nowMs);
  serviceEspNow(nowMs);

  serviceSerialConsole();
  gServer.handleClient();
  logRuntimeStatus(nowMs);
}
