#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstdio>

namespace {

constexpr uint8_t kLinkRx = D6;    // ESP8266 RX <- ESP32 TX (GPIO33)
constexpr uint8_t kLinkTx = D5;    // ESP8266 TX -> ESP32 RX (GPIO21)
constexpr uint32_t kLinkBaud = 57600;

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr int8_t kOledReset = -1;

constexpr uint16_t kRenderPeriodMs = 250;
constexpr uint16_t kLinkTimeoutMs = 3000;
constexpr uint16_t kDiagPeriodMs = 5000;
constexpr uint16_t kBootVisualTestMs = 250;
constexpr uint8_t kInvalidPin = 0xFF;

SoftwareSerial g_link(kLinkRx, kLinkTx);  // RX, TX
Adafruit_SSD1306 g_display(kScreenWidth, kScreenHeight, &Wire, kOledReset);

struct I2cCandidate {
  uint8_t sda;
  uint8_t scl;
  const char* label;
};

constexpr I2cCandidate kI2cCandidates[] = {
    // Priority requested: SDA=D1(GPIO5), SCL=D2(GPIO4)
    {5, 4, "GPIO5/GPIO4 (D1/D2)"},
    {4, 5, "GPIO4/GPIO5 (D2/D1)"},
    {12, 14, "GPIO12/GPIO14 (D6/D5)"},
    {14, 12, "GPIO14/GPIO12 (swappe)"},
};

struct TelemetryState {
  bool laDetected = false;
  bool mp3Playing = false;
  bool sdReady = false;
  uint32_t uptimeMs = 0;
  uint8_t key = 0;
  uint32_t lastRxMs = 0;
};

TelemetryState g_state;
bool g_displayReady = false;
bool g_linkEnabled = true;
bool g_stateDirty = true;
uint32_t g_lastRenderMs = 0;
uint32_t g_lastDiagMs = 0;
bool g_hasValidState = false;
bool g_linkWasAlive = false;
uint32_t g_linkLossCount = 0;
char g_lineBuffer[96];
uint8_t g_lineLen = 0;
uint8_t g_oledSdaPin = kInvalidPin;
uint8_t g_oledSclPin = kInvalidPin;
uint8_t g_oledAddress = 0;

bool isLinkAlive(uint32_t nowMs) {
  if (!g_linkEnabled) {
    return false;
  }
  return g_hasValidState && ((nowMs - g_state.lastRxMs) <= kLinkTimeoutMs);
}

void renderScreen(uint32_t nowMs) {
  if (!g_displayReady) {
    return;
  }

  g_display.clearDisplay();
  g_display.setTextSize(1);
  g_display.setTextColor(SSD1306_WHITE);
  g_display.setCursor(0, 0);
  g_display.println("U-SON SCREEN");
  if (!g_linkEnabled) {
    g_display.println("LINK: OFF");
    g_display.println("PINS OLED/LINK");
    g_display.print("SDA:");
    g_display.print(g_oledSdaPin);
    g_display.print(" SCL:");
    g_display.println(g_oledSclPin);
  } else
  if (!g_hasValidState) {
    g_display.println("LINK: ATTENTE");
    g_display.println("AUCUNE TRAME");
  } else {
    const bool linkAlive = isLinkAlive(nowMs);
    if (linkAlive) {
      g_display.println("LINK: OK");
    } else {
      g_display.print("LINK: PERDUE x");
      g_display.println(g_linkLossCount);
    }
    g_display.print("AGE: ");
    g_display.print((nowMs - g_state.lastRxMs) / 1000UL);
    g_display.println("s");
    g_display.print("LA: ");
    g_display.println(g_state.laDetected ? "DETECTE" : "---");
    g_display.print("MP3: ");
    g_display.println(g_state.mp3Playing ? "PLAY" : "STOP");
    g_display.print("SD: ");
    g_display.println(g_state.sdReady ? "OK" : "ERR");
    g_display.print("UP: ");
    g_display.print(g_state.uptimeMs / 1000UL);
    g_display.println("s");
    g_display.print("KEY: ");
    if (g_state.key == 0) {
      g_display.println("-");
    } else {
      g_display.print("K");
      g_display.println(g_state.key);
    }
  }

  g_display.display();
}

bool hasI2cDevice(uint8_t address) {
  Wire.beginTransmission(address);
  const uint8_t error = Wire.endTransmission();
  return (error == 0);
}

bool initDisplayOnPins(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);
  delay(5);

  if (hasI2cDevice(0x3C)) {
    if (g_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      g_oledAddress = 0x3C;
      return true;
    }
  }
  if (hasI2cDevice(0x3D)) {
    if (g_display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      g_oledAddress = 0x3D;
      return true;
    }
  }
  return false;
}

bool parseFrame(const char* frame, TelemetryState* out) {
  unsigned int la = 0;
  unsigned int mp3 = 0;
  unsigned int sd = 0;
  unsigned long up = 0;
  unsigned int key = 0;

  const int parsed = sscanf(frame, "STAT,%u,%u,%u,%lu,%u", &la, &mp3, &sd, &up, &key);
  if (parsed != 5 && parsed != 4) {
    return false;
  }

  out->laDetected = (la != 0U);
  out->mp3Playing = (mp3 != 0U);
  out->sdReady = (sd != 0U);
  out->uptimeMs = static_cast<uint32_t>(up);
  out->key = (parsed == 5) ? static_cast<uint8_t>(key) : 0;
  out->lastRxMs = millis();
  return true;
}

void handleIncoming() {
  while (g_link.available() > 0) {
    const char c = static_cast<char>(g_link.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      g_lineBuffer[g_lineLen] = '\0';
      TelemetryState parsed = g_state;
      if (parseFrame(g_lineBuffer, &parsed)) {
        g_state = parsed;
        g_hasValidState = true;
        g_stateDirty = true;
      }
      g_lineLen = 0;
      continue;
    }

    if (g_lineLen < (sizeof(g_lineBuffer) - 1U)) {
      g_lineBuffer[g_lineLen++] = c;
    } else {
      g_lineLen = 0;
    }
  }
}

void initDisplay() {
  Serial.println("[SCREEN] OLED init...");
  for (const auto& candidate : kI2cCandidates) {
    Serial.printf("[SCREEN] Test I2C %s\n", candidate.label);
    if (initDisplayOnPins(candidate.sda, candidate.scl)) {
      g_displayReady = true;
      g_oledSdaPin = candidate.sda;
      g_oledSclPin = candidate.scl;
      break;
    }
  }

  if (g_displayReady) {
    Serial.printf("[SCREEN] OLED OK @0x%02X on SDA=%u SCL=%u\n",
                  static_cast<unsigned int>(g_oledAddress),
                  static_cast<unsigned int>(g_oledSdaPin),
                  static_cast<unsigned int>(g_oledSclPin));
    // Quick visual confirmation that panel + contrast are physically working.
    g_display.clearDisplay();
    g_display.fillRect(0, 0, kScreenWidth, kScreenHeight, SSD1306_WHITE);
    g_display.display();
    delay(kBootVisualTestMs);
    g_display.clearDisplay();
    g_display.setTextSize(1);
    g_display.setTextColor(SSD1306_WHITE);
    g_display.setCursor(0, 0);
    g_display.println("U-SON SCREEN");
    g_display.println("Boot...");
    g_display.display();

    if (g_oledSdaPin == kLinkRx || g_oledSdaPin == kLinkTx ||
        g_oledSclPin == kLinkRx || g_oledSclPin == kLinkTx) {
      g_linkEnabled = false;
      Serial.println("[SCREEN] LINK desactive (conflit de broches avec OLED).");
      Serial.println("[SCREEN] Utiliser d'autres broches pour le lien ESP32.");
    }
  } else {
    Serial.println("[SCREEN] OLED introuvable (0x3C/0x3D) sur GPIO12/14 ou GPIO4/5.");
    Serial.println("[SCREEN] Verifier cablage + alim, puis retester.");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  initDisplay();
  if (g_linkEnabled) {
    g_link.begin(kLinkBaud);
  }
  Serial.println("[SCREEN] Ready.");
}

void loop() {
  const uint32_t nowMs = millis();
  if (g_linkEnabled) {
    handleIncoming();
  }
  const bool linkAlive = isLinkAlive(nowMs);

  if (!linkAlive && g_linkWasAlive) {
    ++g_linkLossCount;
    g_stateDirty = true;
  }
  if (linkAlive != g_linkWasAlive) {
    g_stateDirty = true;
  }
  g_linkWasAlive = linkAlive;

  if (g_stateDirty || (nowMs - g_lastRenderMs) >= kRenderPeriodMs) {
    renderScreen(nowMs);
    g_stateDirty = false;
    g_lastRenderMs = nowMs;
  }

  if ((nowMs - g_lastDiagMs) >= kDiagPeriodMs) {
    const uint32_t ageMs = g_hasValidState ? (nowMs - g_state.lastRxMs) : 0;
    Serial.printf("[SCREEN] oled=%s link=%s valid=%u age_ms=%lu losses=%lu sda=%u scl=%u addr=0x%02X\n",
                  g_displayReady ? "OK" : "KO",
                  g_linkEnabled ? (linkAlive ? "OK" : "DOWN") : "OFF",
                  g_hasValidState ? 1U : 0U,
                  static_cast<unsigned long>(ageMs),
                  static_cast<unsigned long>(g_linkLossCount),
                  static_cast<unsigned int>(g_oledSdaPin),
                  static_cast<unsigned int>(g_oledSclPin),
                  static_cast<unsigned int>(g_oledAddress));
    g_lastDiagMs = nowMs;
  }
}
