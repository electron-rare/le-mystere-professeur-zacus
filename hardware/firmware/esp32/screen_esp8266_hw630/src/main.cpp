#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint8_t kLinkRx = D6;    // ESP8266 RX <- ESP32 TX (GPIO22)
constexpr uint8_t kLinkTx = D5;    // Non utilise dans le profil actuel
constexpr uint32_t kLinkBaud = 57600;

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr int8_t kOledReset = -1;

constexpr uint16_t kRenderPeriodMs = 250;
constexpr uint16_t kLinkTimeoutMs = 3000;
constexpr uint16_t kDiagPeriodMs = 5000;
constexpr uint16_t kBootVisualTestMs = 250;
constexpr uint16_t kUnlockBadgeMs = 1200;
constexpr uint8_t kInvalidPin = 0xFF;
constexpr uint8_t kScopeHistoryLen = 64;

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
  bool mp3Mode = false;
  bool uLockMode = false;
  bool uLockListening = false;
  bool uSonFunctional = false;
  uint32_t uptimeMs = 0;
  uint8_t key = 0;
  uint16_t track = 0;
  uint16_t trackCount = 0;
  uint8_t volumePercent = 0;
  uint8_t micLevelPercent = 0;   // 0..100
  bool micScopeEnabled = false;  // scope render only when source supports it
  int8_t tuningOffset = 0;      // -8..+8 (left/right around LA)
  uint8_t tuningConfidence = 0; // 0..100
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
uint8_t g_scopeHistory[kScopeHistoryLen] = {};
uint8_t g_scopeHead = 0;
bool g_scopeFilled = false;
uint32_t g_unlockBadgeUntilMs = 0;

bool isLinkAlive(uint32_t nowMs) {
  if (!g_linkEnabled) {
    return false;
  }
  return g_hasValidState && ((nowMs - g_state.lastRxMs) <= kLinkTimeoutMs);
}

int16_t textWidth(const char* text, uint8_t textSize) {
  return static_cast<int16_t>(strlen(text)) * 6 * textSize;
}

void drawCenteredText(const char* text, int16_t y, uint8_t textSize) {
  const int16_t w = textWidth(text, textSize);
  int16_t x = (kScreenWidth - w) / 2;
  if (x < 0) {
    x = 0;
  }
  g_display.setTextSize(textSize);
  g_display.setCursor(x, y);
  g_display.print(text);
}

void drawTitleBar(const char* title) {
  g_display.fillRect(0, 0, kScreenWidth, 12, SSD1306_WHITE);
  g_display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  drawCenteredText(title, 2, 1);
  g_display.setTextColor(SSD1306_WHITE);
}

void drawHorizontalGauge(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent) {
  if (percent > 100U) {
    percent = 100U;
  }
  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  const int16_t fill = ((w - 2) * static_cast<int16_t>(percent)) / 100;
  g_display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void drawTuningBar(int8_t tuningOffset, uint8_t tuningConfidence, int16_t y) {
  const int16_t x = 8;
  const int16_t w = 112;
  const int16_t h = 10;
  const int16_t centerX = x + (w / 2);

  int16_t clampedOffset = tuningOffset;
  if (clampedOffset < -8) {
    clampedOffset = -8;
  } else if (clampedOffset > 8) {
    clampedOffset = 8;
  }

  uint8_t clampedConfidence = tuningConfidence;
  if (clampedConfidence > 100U) {
    clampedConfidence = 100U;
  }

  const int16_t markerHalfSpan = (w / 2) - 3;
  const int16_t markerX = centerX + (clampedOffset * markerHalfSpan) / 8;

  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  g_display.drawFastVLine(centerX, y - 2, h + 4, SSD1306_WHITE);
  g_display.fillRect(markerX - 1, y + 1, 3, h - 2, SSD1306_WHITE);

  drawHorizontalGauge(8, y + h + 3, 112, 7, clampedConfidence);
}

void drawBrokenIcon(int16_t cx, int16_t cy) {
  g_display.drawCircle(cx, cy, 12, SSD1306_WHITE);
  g_display.drawLine(cx - 9, cy + 9, cx + 9, cy - 9, SSD1306_WHITE);
  g_display.drawLine(cx - 4, cy - 11, cx - 1, cy - 6, SSD1306_WHITE);
  g_display.drawLine(cx + 2, cy - 3, cx + 6, cy + 3, SSD1306_WHITE);
}

void drawCheckIcon(int16_t cx, int16_t cy) {
  g_display.drawCircle(cx, cy, 12, SSD1306_WHITE);
  g_display.drawLine(cx - 6, cy + 1, cx - 1, cy + 6, SSD1306_WHITE);
  g_display.drawLine(cx - 1, cy + 6, cx + 7, cy - 5, SSD1306_WHITE);
}

void pushScopeSample(uint8_t levelPercent) {
  if (levelPercent > 100U) {
    levelPercent = 100U;
  }
  g_scopeHistory[g_scopeHead] = levelPercent;
  g_scopeHead = static_cast<uint8_t>((g_scopeHead + 1U) % kScopeHistoryLen);
  if (g_scopeHead == 0) {
    g_scopeFilled = true;
  }
}

void drawScope(int16_t x, int16_t y, int16_t w, int16_t h) {
  g_display.drawRect(x, y, w, h, SSD1306_WHITE);

  const uint8_t sampleCount = g_scopeFilled ? kScopeHistoryLen : g_scopeHead;
  if (sampleCount < 2 || w < 3 || h < 3) {
    return;
  }

  const int16_t plotW = w - 2;
  const int16_t plotH = h - 2;
  const uint8_t start = g_scopeFilled ? g_scopeHead : 0;

  int16_t prevX = x + 1;
  uint8_t firstValue = g_scopeHistory[start];
  int16_t prevY =
      y + 1 + (plotH - 1) - ((static_cast<int16_t>(firstValue) * (plotH - 1)) / 100);

  for (int16_t i = 1; i < plotW; ++i) {
    const uint8_t sampleIndex =
        static_cast<uint8_t>((start + (i * sampleCount) / plotW) % kScopeHistoryLen);
    const uint8_t value = g_scopeHistory[sampleIndex];
    const int16_t currX = x + 1 + i;
    const int16_t currY =
        y + 1 + (plotH - 1) - ((static_cast<int16_t>(value) * (plotH - 1)) / 100);
    g_display.drawLine(prevX, prevY, currX, currY, SSD1306_WHITE);
    prevX = currX;
    prevY = currY;
  }
}

void renderMp3Screen() {
  drawTitleBar("LECTEUR U-SON");

  drawCenteredText(g_state.mp3Playing ? "PLAY" : "PAUSE", 14, 2);

  char trackLine[20];
  if (g_state.trackCount == 0) {
    snprintf(trackLine, sizeof(trackLine), "-- / --");
  } else {
    snprintf(trackLine,
             sizeof(trackLine),
             "PISTE %u/%u",
             static_cast<unsigned int>(g_state.track),
             static_cast<unsigned int>(g_state.trackCount));
  }
  drawCenteredText(trackLine, 33, 1);

  char infoLine[32];
  if (g_state.key == 0) {
    snprintf(infoLine,
             sizeof(infoLine),
             "VOL %u%%  SD %s",
             static_cast<unsigned int>(g_state.volumePercent),
             g_state.sdReady ? "OK" : "ERR");
  } else {
    snprintf(infoLine,
             sizeof(infoLine),
             "VOL %u%%  K%u",
             static_cast<unsigned int>(g_state.volumePercent),
             static_cast<unsigned int>(g_state.key));
  }
  drawCenteredText(infoLine, 43, 1);
  drawHorizontalGauge(12, 54, 104, 8, g_state.volumePercent);
}

void renderULockWaitingScreen() {
  drawTitleBar("MODE U_LOCK");
  drawBrokenIcon(64, 28);
  drawCenteredText("Pictogramme casse", 43, 1);
  drawCenteredText("Appuyez une touche", 53, 1);
}

void renderULockDetectScreen() {
  drawTitleBar("MODE U_LOCK");
  drawCenteredText("Detection LA 440Hz", 14, 1);
  drawHorizontalGauge(8, 23, 112, 7, g_state.micLevelPercent);
  drawTuningBar(g_state.tuningOffset, g_state.tuningConfidence, 33);
  if (g_state.micScopeEnabled) {
    drawScope(8, 47, 112, 16);
  }
}

void renderULockScreen() {
  if (!g_state.uLockListening) {
    renderULockWaitingScreen();
    return;
  }
  renderULockDetectScreen();
}

void renderUnlockBadgeScreen() {
  drawTitleBar("U-SON FONCTIONNEL");
  drawCheckIcon(64, 30);
  drawCenteredText("Validation LA OK", 50, 1);
}

void renderUSonFunctionalScreen() {
  drawTitleBar("U-SON FONCTIONNEL");
  drawCenteredText(g_state.laDetected ? "LA OK" : "LA --", 15, 2);

  char statusLine[28];
  if (g_state.key == 0) {
    snprintf(statusLine, sizeof(statusLine), "Pret");
  } else {
    snprintf(statusLine, sizeof(statusLine), "Derniere touche K%u", static_cast<unsigned int>(g_state.key));
  }
  drawCenteredText(statusLine, 39, 1);

  char upLine[20];
  snprintf(upLine, sizeof(upLine), "Uptime %lus", static_cast<unsigned long>(g_state.uptimeMs / 1000UL));
  drawCenteredText(upLine, 51, 1);
}

void renderLinkDownScreen(uint32_t nowMs) {
  const uint32_t ageMs = g_hasValidState ? (nowMs - g_state.lastRxMs) : 0;

  drawTitleBar("U-SON SCREEN");
  drawCenteredText("LINK DOWN", 18, 2);

  char ageLine[26];
  snprintf(ageLine, sizeof(ageLine), "Derniere trame %lus", static_cast<unsigned long>(ageMs / 1000UL));
  drawCenteredText(ageLine, 43, 1);

  char lossLine[22];
  snprintf(lossLine, sizeof(lossLine), "Pertes %lu", static_cast<unsigned long>(g_linkLossCount));
  drawCenteredText(lossLine, 54, 1);
}

void renderScreen(uint32_t nowMs, bool linkAlive) {
  if (!g_displayReady) {
    return;
  }

  g_display.clearDisplay();
  g_display.setTextColor(SSD1306_WHITE);

  if (!g_linkEnabled) {
    drawTitleBar("U-SON SCREEN");
    drawCenteredText("Liaison indisponible", 22, 1);
    drawCenteredText("Verifier cablage", 34, 1);
  } else if (!g_hasValidState) {
    drawTitleBar("U-SON SCREEN");
    drawCenteredText("Demarrage...", 18, 2);
    drawCenteredText("En attente des donnees", 45, 1);
  } else if (!linkAlive) {
    renderLinkDownScreen(nowMs);
  } else {
    if (g_state.mp3Mode) {
      renderMp3Screen();
    } else if (g_state.uLockMode) {
      renderULockScreen();
    } else if (g_state.uSonFunctional && (nowMs < g_unlockBadgeUntilMs)) {
      renderUnlockBadgeScreen();
    } else if (g_state.uSonFunctional) {
      renderUSonFunctionalScreen();
    } else {
      drawTitleBar("U-SON SCREEN");
      drawCenteredText("Mode signal", 20, 1);
      drawCenteredText("En attente...", 34, 1);
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
  unsigned int mode = 0;
  unsigned int track = 0;
  unsigned int trackCount = 0;
  unsigned int volumePercent = 0;
  unsigned int uLockMode = 0;
  unsigned int uSonFunctional = 0;
  int tuningOffset = 0;
  unsigned int tuningConfidence = 0;
  unsigned int uLockListening = 0;
  unsigned int micLevelPercent = 0;
  unsigned int micScopeEnabled = 0;

  const int parsed = sscanf(frame,
                            "STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u",
                            &la,
                            &mp3,
                            &sd,
                            &up,
                            &key,
                            &mode,
                            &track,
                            &trackCount,
                            &volumePercent,
                            &uLockMode,
                            &uSonFunctional,
                            &tuningOffset,
                            &tuningConfidence,
                            &uLockListening,
                            &micLevelPercent,
                            &micScopeEnabled);
  if (parsed < 5) {
    return false;
  }

  out->laDetected = (la != 0U);
  out->mp3Playing = (mp3 != 0U);
  out->sdReady = (sd != 0U);
  out->uptimeMs = static_cast<uint32_t>(up);
  out->key = static_cast<uint8_t>(key);
  out->mp3Mode = (parsed >= 6) ? (mode != 0U) : false;
  out->track = (parsed >= 7) ? static_cast<uint16_t>(track) : 0;
  out->trackCount = (parsed >= 8) ? static_cast<uint16_t>(trackCount) : 0;
  out->volumePercent = (parsed >= 9) ? static_cast<uint8_t>(volumePercent) : 0;
  out->uLockMode = (parsed >= 10) ? (uLockMode != 0U) : false;
  out->uSonFunctional = (parsed >= 11) ? (uSonFunctional != 0U) : false;
  if (parsed >= 12) {
    if (tuningOffset < -8) {
      tuningOffset = -8;
    } else if (tuningOffset > 8) {
      tuningOffset = 8;
    }
    out->tuningOffset = static_cast<int8_t>(tuningOffset);
  } else {
    out->tuningOffset = 0;
  }
  if (parsed >= 13) {
    if (tuningConfidence > 100U) {
      tuningConfidence = 100U;
    }
    out->tuningConfidence = static_cast<uint8_t>(tuningConfidence);
  } else {
    out->tuningConfidence = 0;
  }
  out->uLockListening = (parsed >= 14) ? (uLockListening != 0U) : false;
  if (parsed >= 15) {
    if (micLevelPercent > 100U) {
      micLevelPercent = 100U;
    }
    out->micLevelPercent = static_cast<uint8_t>(micLevelPercent);
  } else {
    out->micLevelPercent = 0;
  }
  out->micScopeEnabled = (parsed >= 16) ? (micScopeEnabled != 0U) : false;
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
        if (!g_state.uSonFunctional && parsed.uSonFunctional) {
          g_unlockBadgeUntilMs = millis() + kUnlockBadgeMs;
        }
        pushScopeSample(parsed.micLevelPercent);
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
    renderScreen(nowMs, linkAlive);
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
