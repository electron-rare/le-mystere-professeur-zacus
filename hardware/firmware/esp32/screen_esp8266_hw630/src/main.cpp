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
constexpr uint32_t kLinkBaud = 38400;
constexpr int kLinkRxBufferBytes = 256;
constexpr int kLinkIsrBufferBytes = 2048;

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr int8_t kOledReset = -1;

constexpr uint16_t kRenderPeriodMs = 250;
constexpr uint16_t kLinkTimeoutMs = 10000;
constexpr uint16_t kLinkDownConfirmMs = 1500;
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
  uint8_t unlockHoldPercent = 0; // 0..100
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
uint32_t g_parseErrorCount = 0;
uint32_t g_rxOverflowCount = 0;
char g_lineBuffer[128];
uint8_t g_lineLen = 0;
uint8_t g_oledSdaPin = kInvalidPin;
uint8_t g_oledSclPin = kInvalidPin;
uint8_t g_oledAddress = 0;
uint8_t g_scopeHistory[kScopeHistoryLen] = {};
uint8_t g_scopeHead = 0;
bool g_scopeFilled = false;
uint32_t g_unlockBadgeUntilMs = 0;
uint32_t g_lastByteMs = 0;
uint32_t g_linkDownSinceMs = 0;

uint32_t latestLinkTickMs() {
  if (g_state.lastRxMs > g_lastByteMs) {
    return g_state.lastRxMs;
  }
  return g_lastByteMs;
}

bool isPhysicalLinkAlive(uint32_t nowMs) {
  if (!g_linkEnabled) {
    return false;
  }

  const uint32_t lastTickMs = latestLinkTickMs();
  if (lastTickMs == 0) {
    return false;
  }
  return (nowMs - lastTickMs) <= kLinkTimeoutMs;
}

bool isLinkAlive(uint32_t nowMs) {
  if (!g_linkEnabled) {
    return false;
  }

  if (latestLinkTickMs() == 0) {
    return false;
  }

  if (isPhysicalLinkAlive(nowMs)) {
    g_linkDownSinceMs = 0;
    return true;
  }

  if (g_linkDownSinceMs == 0) {
    g_linkDownSinceMs = nowMs;
    return true;
  }

  return (nowMs - g_linkDownSinceMs) < kLinkDownConfirmMs;
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

void drawTinyLock(int16_t x, int16_t y, uint16_t color) {
  // 7x8 lock icon (body + shackle)
  g_display.drawRect(x, y + 3, 7, 5, color);
  g_display.drawLine(x + 2, y + 3, x + 2, y + 1, color);
  g_display.drawLine(x + 4, y + 3, x + 4, y + 1, color);
  g_display.drawPixel(x + 3, y + 0, color);
}

void drawProtoTitleBar() {
  g_display.fillRect(0, 0, kScreenWidth, 12, SSD1306_WHITE);
  drawTinyLock(6, 2, SSD1306_BLACK);
  drawTinyLock(kScreenWidth - 13, 2, SSD1306_BLACK);
  g_display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  drawCenteredText("U-SON PROTO", 2, 1);
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
  const int16_t h = 8;
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
  const int16_t markerW = 1 + (static_cast<int16_t>(clampedConfidence) / 30);

  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  g_display.drawFastVLine(centerX, y - 2, h + 4, SSD1306_WHITE);
  g_display.fillRect(markerX - markerW, y + 1, (markerW * 2) + 1, h - 2, SSD1306_WHITE);

  // Left/right graduation ticks for a more "instrument" feel.
  for (uint8_t i = 1; i < 4; ++i) {
    const int16_t step = static_cast<int16_t>((w / 2) * i / 4);
    g_display.drawPixel(centerX - step, y + h + 1, SSD1306_WHITE);
    g_display.drawPixel(centerX + step, y + h + 1, SSD1306_WHITE);
  }
}

void drawUnlockProgressBar(uint8_t unlockHoldPercent, int16_t y) {
  if (unlockHoldPercent > 100U) {
    unlockHoldPercent = 100U;
  }
  drawHorizontalGauge(8, y, 112, 8, unlockHoldPercent);
}

void drawMiniEqualizer(uint32_t nowMs, uint8_t levelPercent, int16_t x, int16_t y) {
  if (levelPercent > 100U) {
    levelPercent = 100U;
  }

  constexpr uint8_t kBars = 10;
  constexpr int16_t kBarW = 3;
  constexpr int16_t kBarGap = 1;
  constexpr int16_t kMaxH = 9;

  for (uint8_t i = 0; i < kBars; ++i) {
    const uint8_t phase = static_cast<uint8_t>((nowMs / 90U) + (i * 17U));
    const uint8_t wave = static_cast<uint8_t>((phase % 20U) * 5U);
    const uint8_t mixed = static_cast<uint8_t>((levelPercent + wave) / 2U);
    const int16_t barH = 1 + ((static_cast<int16_t>(mixed) * kMaxH) / 100);
    const int16_t bx = x + (i * (kBarW + kBarGap));
    g_display.fillRect(bx, y + (kMaxH - barH), kBarW, barH, SSD1306_WHITE);
  }
}

void drawBrokenIcon(int16_t cx, int16_t cy);

void drawBrokenModuleGlitch(uint32_t nowMs, int16_t cx, int16_t cy) {
  const int16_t x = 0;
  const int16_t y = 0;
  const int16_t w = kScreenWidth;
  const int16_t h = kScreenHeight;

  const int8_t jitterX = static_cast<int8_t>((nowMs / 90U) % 3U) - 1;
  const int8_t jitterY = static_cast<int8_t>((nowMs / 130U) % 3U) - 1;

  // Full-screen shell: the intro now owns the complete OLED surface.
  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  g_display.drawRect(x + 1 + jitterX, y + 1 + jitterY, w - 2, h - 2, SSD1306_WHITE);
  g_display.drawRect(x + 3 - jitterX, y + 3, w - 6, h - 6, SSD1306_WHITE);

  // Main fracture map across the whole panel.
  const int16_t crackY1 = 8 + static_cast<int16_t>((nowMs / 170U) % 3U);
  g_display.drawLine(x + 4, crackY1, x + (w / 3), y + (h / 2) - 2, SSD1306_WHITE);
  g_display.drawLine(x + (w / 3), y + (h / 2) - 2, x + ((w * 2) / 3), y + (h / 3), SSD1306_WHITE);
  g_display.drawLine(x + ((w * 2) / 3), y + (h / 3), x + w - 5, y + h - 10, SSD1306_WHITE);
  g_display.drawLine(x + (w / 2), y + 4, x + (w / 2) - 8, y + h - 8, SSD1306_WHITE);

  // Animated glitch slices distributed over almost full width.
  for (uint8_t i = 0; i < 10; ++i) {
    const int16_t sy = y + 3 + static_cast<int16_t>((nowMs / 23U + i * 9U) % (h - 6));
    const int16_t len = 20 + static_cast<int16_t>((nowMs / 17U + i * 11U) % 70U);
    const int16_t sx = x + 2 + static_cast<int16_t>((nowMs / 13U + i * 23U) % (w - len - 4));
    const int8_t dx = static_cast<int8_t>((nowMs / 31U + i * 5U) % 9U) - 4;
    g_display.drawFastHLine(sx + dx, sy, len, SSD1306_WHITE);
    if ((i % 3U) == 0U) {
      g_display.drawFastHLine(x + 2, sy + 1, w - 4, SSD1306_WHITE);
    }
  }

  // Sparse static/noise all over the screen.
  for (uint8_t i = 0; i < 42; ++i) {
    if (((nowMs / 37U) + i) % 2U != 0U) {
      continue;
    }
    const int16_t px = x + static_cast<int16_t>((nowMs + i * 29U) % w);
    const int16_t py = y + static_cast<int16_t>(((nowMs / 2U) + i * 17U) % h);
    g_display.drawPixel(px, py, SSD1306_WHITE);
  }

  drawBrokenIcon(cx, cy);
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
  const int16_t plotX = x + 1;
  const int16_t plotY = y + 1;
  const uint8_t start = g_scopeFilled ? g_scopeHead : 0;
  const int16_t centerY = plotY + (plotH / 2);
  const int16_t maxAmp = (plotH - 1) / 2;

  // Midline reference for the mirror effect.
  for (int16_t i = 0; i < plotW; i += 2) {
    g_display.drawPixel(plotX + i, centerY, SSD1306_WHITE);
  }

  int16_t prevX = plotX;
  uint8_t firstValue = g_scopeHistory[start];
  int16_t firstAmp = (static_cast<int16_t>(firstValue) * maxAmp) / 100;
  int16_t prevTopY = centerY - firstAmp;
  int16_t prevBottomY = centerY + firstAmp;

  for (int16_t i = 1; i < plotW; ++i) {
    const uint8_t sampleIndex =
        static_cast<uint8_t>((start + (i * sampleCount) / plotW) % kScopeHistoryLen);
    const uint8_t value = g_scopeHistory[sampleIndex];
    const int16_t currX = plotX + i;
    const int16_t currAmp = (static_cast<int16_t>(value) * maxAmp) / 100;
    const int16_t currTopY = centerY - currAmp;
    const int16_t currBottomY = centerY + currAmp;

    g_display.drawLine(prevX, prevTopY, currX, currTopY, SSD1306_WHITE);
    g_display.drawLine(prevX, prevBottomY, currX, currBottomY, SSD1306_WHITE);

    // Light bridges to emphasize mirrored "energy".
    if ((i % 7) == 0) {
      g_display.drawLine(currX, currTopY, currX, currBottomY, SSD1306_WHITE);
    }

    prevX = currX;
    prevTopY = currTopY;
    prevBottomY = currBottomY;
  }
}

void renderMp3Screen() {
  drawTitleBar("LECTEUR U-SON");

  drawCenteredText(g_state.mp3Playing ? "PLAY" : "PAUSE", 14, 2);
  drawMiniEqualizer(g_state.uptimeMs,
                    g_state.mp3Playing ? g_state.volumePercent : static_cast<uint8_t>(g_state.volumePercent / 3U),
                    84,
                    15);

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

void renderULockWaitingScreen(uint32_t nowMs) {
  drawBrokenModuleGlitch(nowMs, 64, 32);
}

void renderULockDetectScreen() {
  drawProtoTitleBar();
  drawHorizontalGauge(8, 15, 112, 7, g_state.micLevelPercent);
  drawTuningBar(g_state.tuningOffset, g_state.tuningConfidence, 24);
  drawUnlockProgressBar(g_state.unlockHoldPercent, 34);
  if (g_state.micScopeEnabled) {
    drawScope(8, 44, 112, 19);
  }
}

void renderULockScreen(uint32_t nowMs) {
  if (!g_state.uLockListening) {
    renderULockWaitingScreen(nowMs);
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
  const uint32_t lastTickMs = latestLinkTickMs();
  const uint32_t ageMs = (lastTickMs > 0) ? (nowMs - lastTickMs) : 0;

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
      renderULockScreen(nowMs);
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
  if (strncmp(frame, "STAT,", 5) != 0) {
    return false;
  }

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
  unsigned int unlockHoldPercent = 0;

  const int parsed = sscanf(frame,
                            "STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u",
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
                            &micScopeEnabled,
                            &unlockHoldPercent);
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
  if (parsed >= 17) {
    if (unlockHoldPercent > 100U) {
      unlockHoldPercent = 100U;
    }
    out->unlockHoldPercent = static_cast<uint8_t>(unlockHoldPercent);
  } else {
    out->unlockHoldPercent = 0;
  }
  out->lastRxMs = millis();
  return true;
}

void handleIncoming() {
  while (g_link.available() > 0) {
    g_lastByteMs = millis();
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
      } else if (g_lineLen > 0) {
        ++g_parseErrorCount;
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

  if (g_link.overflow()) {
    ++g_rxOverflowCount;
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
    g_link.begin(kLinkBaud,
                 SWSERIAL_8N1,
                 kLinkRx,
                 kLinkTx,
                 false,
                 kLinkRxBufferBytes,
                 kLinkIsrBufferBytes);
    g_link.enableRxGPIOPullUp(true);
    g_link.enableIntTx(false);
  }
  Serial.println("[SCREEN] Ready.");
}

void loop() {
  const uint32_t nowMs = millis();
  if (g_linkEnabled) {
    handleIncoming();
  }
  const bool physicalAlive = isPhysicalLinkAlive(nowMs);
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
    const uint32_t lastTickMs = latestLinkTickMs();
    const uint32_t ageMs = (lastTickMs > 0) ? (nowMs - lastTickMs) : 0;
    Serial.printf("[SCREEN] oled=%s link=%s phys=%s valid=%u age_ms=%lu losses=%lu parse_err=%lu rx_ovf=%lu sda=%u scl=%u addr=0x%02X\n",
                  g_displayReady ? "OK" : "KO",
                  g_linkEnabled ? (linkAlive ? "OK" : "DOWN") : "OFF",
                  g_linkEnabled ? (physicalAlive ? "OK" : "DOWN") : "OFF",
                  g_hasValidState ? 1U : 0U,
                  static_cast<unsigned long>(ageMs),
                  static_cast<unsigned long>(g_linkLossCount),
                  static_cast<unsigned long>(g_parseErrorCount),
                  static_cast<unsigned long>(g_rxOverflowCount),
                  static_cast<unsigned int>(g_oledSdaPin),
                  static_cast<unsigned int>(g_oledSclPin),
                  static_cast<unsigned int>(g_oledAddress));
    g_lastDiagMs = nowMs;
  }
}
