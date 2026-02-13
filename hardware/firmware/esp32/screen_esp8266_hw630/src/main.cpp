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
constexpr uint32_t kLinkBaud = 19200;
constexpr int kLinkRxBufferBytes = 256;
constexpr int kLinkIsrBufferBytes = 2048;

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr int8_t kOledReset = -1;

constexpr uint16_t kRenderPeriodMs = 250;
constexpr uint16_t kLinkTimeoutMs = 15000;
constexpr uint16_t kLinkDownConfirmMs = 2500;
constexpr uint32_t kLinkRecoverGraceMs = 30000;
constexpr uint32_t kPeerRebootGraceMs = 8000;
constexpr uint32_t kPeerUptimeRollbackSlackMs = 2000;
constexpr uint16_t kDiagPeriodMs = 5000;
constexpr uint16_t kBootVisualTestMs = 400;
constexpr uint16_t kBootSplashMinMs = 3600;
constexpr uint8_t kOledInitRetries = 3;
constexpr uint16_t kOledInitRetryDelayMs = 80;
constexpr uint16_t kUnlockFrameMs = 2500;
constexpr uint8_t kUnlockFrameCount = 6;
constexpr uint8_t kInvalidPin = 0xFF;
constexpr uint8_t kScopeHistoryLen = 64;

constexpr uint8_t kStartupStageInactive = 0;
constexpr uint8_t kStartupStageBootValidation = 1;

constexpr uint8_t kAppStageULockWaiting = 0;
constexpr uint8_t kAppStageULockListening = 1;
constexpr uint8_t kAppStageUSonFunctional = 2;
constexpr uint8_t kAppStageMp3 = 3;

constexpr uint8_t kSpriteChip[8] = {0x3C, 0x7E, 0xDB, 0xA5, 0xA5, 0xDB, 0x7E, 0x3C};
constexpr uint8_t kSpriteLock[8] = {0x18, 0x24, 0x24, 0x7E, 0x42, 0x5A, 0x42, 0x7E};
constexpr uint8_t kSpriteStar[8] = {0x18, 0x99, 0x5A, 0x3C, 0x3C, 0x5A, 0x99, 0x18};
constexpr uint8_t kSpritePhone[8] = {0x60, 0x70, 0x38, 0x1C, 0x0E, 0x87, 0xC3, 0x66};
constexpr uint8_t kSpriteSkull[8] = {0x3C, 0x7E, 0xA5, 0x81, 0xA5, 0xDB, 0x24, 0x18};

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
  uint8_t startupStage = kStartupStageInactive;
  uint8_t appStage = kAppStageULockWaiting;
  uint32_t frameSeq = 0;
  uint8_t uiPage = 0;
  uint8_t repeatMode = 0;
  bool fxActive = false;
  uint8_t backendMode = 0;
  bool scanBusy = false;
  uint8_t errorCode = 0;
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
char g_lineBuffer[220];
uint8_t g_lineLen = 0;
uint8_t g_oledSdaPin = kInvalidPin;
uint8_t g_oledSclPin = kInvalidPin;
uint8_t g_oledAddress = 0;
uint8_t g_scopeHistory[kScopeHistoryLen] = {};
uint8_t g_scopeHead = 0;
bool g_scopeFilled = false;
uint32_t g_unlockSequenceStartMs = 0;
uint32_t g_lastByteMs = 0;
uint32_t g_linkDownSinceMs = 0;
uint32_t g_linkLostSinceMs = 0;
uint32_t g_peerRebootUntilMs = 0;
uint32_t g_bootSplashUntilMs = 0;

uint32_t latestLinkTickMs() {
  if (g_state.lastRxMs > g_lastByteMs) {
    return g_state.lastRxMs;
  }
  return g_lastByteMs;
}

uint32_t safeAgeMs(uint32_t nowMs, uint32_t tickMs) {
  if (tickMs == 0U || nowMs < tickMs) {
    return 0U;
  }
  return nowMs - tickMs;
}

bool isPhysicalLinkAlive(uint32_t nowMs) {
  if (!g_linkEnabled) {
    return false;
  }

  const uint32_t lastTickMs = latestLinkTickMs();
  if (lastTickMs == 0) {
    return false;
  }
  if (nowMs < lastTickMs) {
    return true;
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

bool isPeerRebootGraceActive(uint32_t nowMs) {
  return g_peerRebootUntilMs != 0U && static_cast<int32_t>(nowMs - g_peerRebootUntilMs) < 0;
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

void drawSprite8(const uint8_t sprite[8], int16_t x, int16_t y, uint16_t color = SSD1306_WHITE) {
  for (uint8_t row = 0; row < 8; ++row) {
    const uint8_t bits = sprite[row];
    for (uint8_t col = 0; col < 8; ++col) {
      if (((bits >> (7U - col)) & 0x01U) != 0U) {
        g_display.drawPixel(x + col, y + row, color);
      }
    }
  }
}

void drawCenteredDemoText(const char* text,
                          int16_t y,
                          uint8_t textSize,
                          uint32_t nowMs,
                          bool wobble,
                          uint16_t color = SSD1306_WHITE) {
  const size_t len = strlen(text);
  const int16_t charW = 6 * textSize;
  const int16_t w = static_cast<int16_t>(len) * charW;
  int16_t x = (kScreenWidth - w) / 2;
  if (x < 0) {
    x = 0;
  }

  g_display.setTextSize(textSize);
  g_display.setTextColor(color);
  for (size_t i = 0; i < len; ++i) {
    int16_t yOffset = 0;
    if (wobble) {
      const uint8_t phase =
          static_cast<uint8_t>(((nowMs / 95U) + static_cast<uint32_t>(i * 3U)) % 4U);
      yOffset = static_cast<int16_t>(phase);
      if (yOffset > 2) {
        yOffset = static_cast<int16_t>(4 - yOffset);
      }
      yOffset -= 1;
    }
    const int16_t cx = x + static_cast<int16_t>(i) * charW;
    g_display.setCursor(cx, y + yOffset);
    g_display.print(text[i]);

    if (((nowMs / 200U) + i) % 9U == 0U) {
      g_display.drawPixel(cx + (charW / 2), y + yOffset - 1, color);
    }
  }

  if (((nowMs / 170U) % 3U) == 0U) {
    const int16_t scanY =
        y + static_cast<int16_t>((nowMs / 80U) % static_cast<uint32_t>(8U * textSize));
    if (scanY >= 0 && scanY < kScreenHeight) {
      for (int16_t sx = x; sx < (x + w); sx += 2) {
        g_display.drawPixel(sx, scanY, color);
      }
    }
  }
  g_display.setTextColor(SSD1306_WHITE);
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
  for (uint8_t i = 0; i < 5; ++i) {
    const int16_t sy = y + 3 + static_cast<int16_t>((nowMs / 23U + i * 9U) % (h - 6));
    const int16_t len = 16 + static_cast<int16_t>((nowMs / 21U + i * 11U) % 44U);
    const int16_t sx = x + 2 + static_cast<int16_t>((nowMs / 13U + i * 23U) % (w - len - 4));
    const int8_t dx = static_cast<int8_t>((nowMs / 31U + i * 5U) % 5U) - 2;
    g_display.drawFastHLine(sx + dx, sy, len, SSD1306_WHITE);
    if ((i % 4U) == 0U) {
      g_display.drawFastHLine(sx, sy + 1, len / 2, SSD1306_WHITE);
    }
  }

  // Sparse static/noise all over the screen.
  for (uint8_t i = 0; i < 14; ++i) {
    if (((nowMs / 52U) + i) % 3U != 0U) {
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

void drawUnlockWaveform(uint32_t nowMs, int16_t x, int16_t y, int16_t w, int16_t h, bool semiStable) {
  if (w < 6 || h < 6) {
    return;
  }

  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  const int16_t plotX = x + 1;
  const int16_t plotY = y + 1;
  const int16_t plotW = w - 2;
  const int16_t plotH = h - 2;
  const int16_t midY = plotY + (plotH / 2);
  const int16_t maxAmp = (plotH - 2) / 2;

  for (int16_t i = 0; i < plotW; i += 2) {
    g_display.drawPixel(plotX + i, midY, SSD1306_WHITE);
  }

  int16_t prevX = plotX;
  int16_t prevY = midY;
  for (int16_t i = 1; i < plotW; ++i) {
    const int16_t currX = plotX + i;
    const uint32_t t = (nowMs / (semiStable ? 45U : 20U)) + static_cast<uint32_t>(i * (semiStable ? 5U : 11U));
    int16_t swing = static_cast<int16_t>(t % (semiStable ? 24U : 46U));
    swing -= semiStable ? 12 : 23;

    int16_t jitter = static_cast<int16_t>(((nowMs / (semiStable ? 73U : 29U)) + i * (semiStable ? 3U : 7U)) %
                                          (semiStable ? 7U : 19U));
    jitter -= semiStable ? 3 : 9;

    int16_t amp = semiStable ? ((swing / 2) + jitter) : (swing + jitter);
    if (amp > maxAmp) {
      amp = maxAmp;
    } else if (amp < -maxAmp) {
      amp = -maxAmp;
    }

    const int16_t currY = midY - amp;
    g_display.drawLine(prevX, prevY, currX, currY, SSD1306_WHITE);

    if (!semiStable && ((i % 9) == 0)) {
      g_display.drawFastVLine(currX, currY - 1, 3, SSD1306_WHITE);
    }

    prevX = currX;
    prevY = currY;
  }
}

void drawGamingCorners(uint32_t nowMs) {
  constexpr int16_t k = 9;
  g_display.drawFastHLine(0, 12, k, SSD1306_WHITE);
  g_display.drawFastVLine(0, 12, k, SSD1306_WHITE);
  g_display.drawFastHLine(kScreenWidth - k, 12, k, SSD1306_WHITE);
  g_display.drawFastVLine(kScreenWidth - 1, 12, k, SSD1306_WHITE);
  g_display.drawFastHLine(0, kScreenHeight - 1, k, SSD1306_WHITE);
  g_display.drawFastVLine(0, kScreenHeight - k, k, SSD1306_WHITE);
  g_display.drawFastHLine(kScreenWidth - k, kScreenHeight - 1, k, SSD1306_WHITE);
  g_display.drawFastVLine(kScreenWidth - 1, kScreenHeight - k, k, SSD1306_WHITE);

  const int16_t sweep = 2 + static_cast<int16_t>((nowMs / 65U) % (kScreenWidth - 4));
  g_display.drawPixel(sweep, 13, SSD1306_WHITE);
  g_display.drawPixel(kScreenWidth - sweep, kScreenHeight - 2, SSD1306_WHITE);
}

void drawGamingScanlines(uint32_t nowMs, int16_t yStart, int16_t yEnd) {
  if (yEnd <= yStart + 1) {
    return;
  }
  const int16_t phase = static_cast<int16_t>((nowMs / 55U) % 6U);
  for (int16_t y = yStart + phase; y <= yEnd; y += 6) {
    for (int16_t x = 4; x < (kScreenWidth - 4); x += 3) {
      g_display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
}

void drawReticle(int16_t cx, int16_t cy, int16_t r, uint32_t nowMs) {
  const int16_t pulse = static_cast<int16_t>((nowMs / 130U) % 3U);
  const int16_t rr = r + pulse;
  g_display.drawCircle(cx, cy, rr, SSD1306_WHITE);
  g_display.drawFastHLine(cx - rr - 4, cy, 4, SSD1306_WHITE);
  g_display.drawFastHLine(cx + rr + 1, cy, 4, SSD1306_WHITE);
  g_display.drawFastVLine(cx, cy - rr - 4, 4, SSD1306_WHITE);
  g_display.drawFastVLine(cx, cy + rr + 1, 4, SSD1306_WHITE);
}

void drawPulseRays(uint32_t nowMs, int16_t cx, int16_t cy) {
  const int16_t l = 8 + static_cast<int16_t>((nowMs / 70U) % 6U);
  g_display.drawLine(cx - l, cy, cx - 2, cy, SSD1306_WHITE);
  g_display.drawLine(cx + 2, cy, cx + l, cy, SSD1306_WHITE);
  g_display.drawLine(cx, cy - l, cx, cy - 2, SSD1306_WHITE);
  g_display.drawLine(cx, cy + 2, cx, cy + l, SSD1306_WHITE);
  g_display.drawLine(cx - (l - 2), cy - (l - 2), cx - 2, cy - 2, SSD1306_WHITE);
  g_display.drawLine(cx + 2, cy + 2, cx + (l - 2), cy + (l - 2), SSD1306_WHITE);
  g_display.drawLine(cx - (l - 2), cy + (l - 2), cx - 2, cy + 2, SSD1306_WHITE);
  g_display.drawLine(cx + 2, cy - 2, cx + (l - 2), cy - (l - 2), SSD1306_WHITE);
}

void drawDataRain(uint32_t nowMs, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (w < 12 || h < 8) {
    return;
  }

  g_display.drawRect(x, y, w, h, SSD1306_WHITE);
  const int16_t columns = w / 8;
  for (int16_t c = 0; c < columns; ++c) {
    const int16_t cx = x + 2 + c * 8;
    const uint32_t speed = 33U + static_cast<uint32_t>(c * 9U);
    const int16_t head = y + 1 + static_cast<int16_t>(((nowMs / speed) + c * 7U) % (h - 2));
    for (int8_t t = 0; t < 4; ++t) {
      int16_t py = head - t * 3;
      while (py < (y + 1)) {
        py += (h - 2);
      }
      g_display.drawPixel(cx, py, SSD1306_WHITE);
      if (((c + t) % 2) == 0) {
        g_display.drawPixel(cx + 1, py, SSD1306_WHITE);
      }
    }
  }
}

void drawRadarSweep(uint32_t nowMs, int16_t cx, int16_t cy, int16_t r) {
  static constexpr int8_t kDirX[16] = {8, 7, 6, 3, 0, -3, -6, -7, -8, -7, -6, -3, 0, 3, 6, 7};
  static constexpr int8_t kDirY[16] = {0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7, -8, -7, -6, -3};

  g_display.drawCircle(cx, cy, r, SSD1306_WHITE);
  g_display.drawCircle(cx, cy, r - 4, SSD1306_WHITE);
  g_display.drawFastHLine(cx - r, cy, 2 * r, SSD1306_WHITE);
  g_display.drawFastVLine(cx, cy - r, 2 * r, SSD1306_WHITE);

  const uint8_t idx = static_cast<uint8_t>((nowMs / 95U) % 16U);
  const int16_t ex = cx + (kDirX[idx] * r) / 8;
  const int16_t ey = cy + (kDirY[idx] * r) / 8;
  g_display.drawLine(cx, cy, ex, ey, SSD1306_WHITE);

  const uint8_t ping = static_cast<uint8_t>((idx + 5U) % 16U);
  const int16_t px = cx + (kDirX[ping] * (r - 2)) / 8;
  const int16_t py = cy + (kDirY[ping] * (r - 2)) / 8;
  g_display.drawCircle(px, py, 1, SSD1306_WHITE);
}

void drawMissionGrid(uint32_t nowMs, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (w < 10 || h < 10) {
    return;
  }
  g_display.drawRect(x, y, w, h, SSD1306_WHITE);

  for (int16_t gx = x + 4; gx < x + w - 2; gx += 8) {
    for (int16_t gy = y + 2; gy < y + h - 2; gy += 4) {
      g_display.drawPixel(gx, gy, SSD1306_WHITE);
    }
  }
  for (int16_t gy = y + 4; gy < y + h - 2; gy += 8) {
    for (int16_t gx = x + 2; gx < x + w - 2; gx += 4) {
      g_display.drawPixel(gx, gy, SSD1306_WHITE);
    }
  }

  const int16_t pathY = y + h / 2;
  g_display.drawLine(x + 6, pathY + 6, x + 26, pathY, SSD1306_WHITE);
  g_display.drawLine(x + 26, pathY, x + 48, pathY - 5, SSD1306_WHITE);
  g_display.drawLine(x + 48, pathY - 5, x + 72, pathY + 2, SSD1306_WHITE);
  g_display.drawLine(x + 72, pathY + 2, x + 96, pathY - 3, SSD1306_WHITE);
  g_display.drawLine(x + 96, pathY - 3, x + w - 10, pathY + 5, SSD1306_WHITE);

  const int16_t cursor =
      x + 6 + static_cast<int16_t>((nowMs / 38U) % static_cast<uint32_t>(w - 16));
  g_display.drawRect(cursor - 1, pathY - 1, 3, 3, SSD1306_WHITE);
}

const char* uiPageShortLabel(uint8_t page) {
  switch (page) {
    case 1:
      return "BRW";
    case 2:
      return "QUE";
    case 3:
      return "SET";
    case 0:
    default:
      return "NOW";
  }
}

const char* repeatShortLabel(uint8_t repeatMode) {
  return (repeatMode == 1U) ? "ONE" : "ALL";
}

const char* backendShortLabel(uint8_t backendMode) {
  switch (backendMode) {
    case 1:
      return "AT";
    case 2:
      return "LEG";
    case 0:
    default:
      return "AUTO";
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

  char modeLine[32];
  snprintf(modeLine,
           sizeof(modeLine),
           "%s %s %s%s%s",
           uiPageShortLabel(g_state.uiPage),
           repeatShortLabel(g_state.repeatMode),
           backendShortLabel(g_state.backendMode),
           g_state.fxActive ? " FX" : "",
           g_state.scanBusy ? " SC" : "");
  drawCenteredText(modeLine, 52, 1);
  drawHorizontalGauge(12, 58, 104, 5, g_state.volumePercent);

  if (g_state.errorCode != 0U) {
    char errLine[10];
    snprintf(errLine, sizeof(errLine), "E%u", static_cast<unsigned int>(g_state.errorCode));
    g_display.setCursor(103, 0);
    g_display.setTextSize(1);
    g_display.print(errLine);
  }
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

void renderStartupBootScreen(uint32_t nowMs) {
  drawBrokenModuleGlitch(nowMs, 64, 32);

  g_display.fillRect(0, 0, kScreenWidth, 12, SSD1306_WHITE);
  g_display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  drawCenteredText("U-SON SCREEN", 2, 1);
  g_display.setTextColor(SSD1306_WHITE);

  g_display.fillRect(6, 15, 116, 30, SSD1306_BLACK);
  g_display.drawRect(6, 15, 116, 30, SSD1306_WHITE);
  drawCenteredDemoText("DECOUVERTE MODULE", 19, 1, nowMs, true);

  char waitLine[24] = {};
  const uint8_t dots = static_cast<uint8_t>((nowMs / 280U) % 4U);
  snprintf(waitLine, sizeof(waitLine), "EN ATTENTE%.*s", static_cast<int>(dots), "...");
  drawCenteredText(waitLine, 32, 1);

  drawCenteredText("K1..K6 -> U_LOCK", 46, 1);
  const uint16_t sweepPhase = static_cast<uint16_t>((nowMs / 35U) % 200U);
  const uint8_t sweep = static_cast<uint8_t>((sweepPhase <= 100U) ? sweepPhase : (200U - sweepPhase));
  drawHorizontalGauge(12, 54, 104, 8, sweep);
}

void renderUnlockSequenceScreen(uint32_t nowMs) {
  if (g_unlockSequenceStartMs == 0) {
    g_unlockSequenceStartMs = nowMs;
  }

  const uint32_t elapsedMs = nowMs - g_unlockSequenceStartMs;
  const uint32_t cycleDurationMs = static_cast<uint32_t>(kUnlockFrameMs) * kUnlockFrameCount;
  const uint32_t cycleMs = (cycleDurationMs > 0U) ? (elapsedMs % cycleDurationMs) : 0U;
  const uint8_t frameIndex = (kUnlockFrameMs > 0U)
                                 ? static_cast<uint8_t>(cycleMs / kUnlockFrameMs)
                                 : 0U;

  drawGamingCorners(nowMs);
  drawGamingScanlines(nowMs, 14, 62);

  if (frameIndex == 0U) {
    drawTitleBar("BRIGADE Z - ANALYSE");
    drawCenteredDemoText("BRIGADE Z - ANALYSE", 2, 1, nowMs, false, SSD1306_BLACK);
    drawSprite8(kSpriteLock, 3, 2, SSD1306_BLACK);
    drawSprite8(kSpriteChip, 117, 2, SSD1306_BLACK);
    drawUnlockWaveform(nowMs, 8, 16, 112, 32, false);
    drawReticle(64, 32, 8, nowMs);
    for (uint8_t i = 0; i < 4; ++i) {
      if (((nowMs / 55U) + i) % 2U == 0U) {
        continue;
      }
      const int16_t px = 6 + static_cast<int16_t>((nowMs + i * 19U) % 116U);
      const int16_t py = 16 + static_cast<int16_t>(((nowMs / 2U) + i * 13U) % 30U);
      g_display.drawPixel(px, py, SSD1306_WHITE);
    }
    drawSprite8(kSpriteStar, 10, 53);
    drawSprite8(kSpriteStar, 110, 53);
    drawCenteredDemoText("CALIBRATION...", 54, 1, nowMs, true);
    return;
  }

  if (frameIndex == 1U) {
    drawTitleBar("OSCILLA VOLT - SYNC");
    drawCenteredDemoText("OSCILLA VOLT - SYNC", 2, 1, nowMs, false, SSD1306_BLACK);
    drawSprite8(kSpriteChip, 3, 2, SSD1306_BLACK);
    drawSprite8(kSpriteStar, 117, 2, SSD1306_BLACK);
    drawUnlockWaveform(nowMs, 8, 16, 112, 32, true);
    drawReticle(64, 32, 10, nowMs);
    g_display.drawRoundRect(26, 22, 76, 20, 3, SSD1306_WHITE);
    g_display.drawFastVLine(64, 22, 20, SSD1306_WHITE);
    drawCenteredDemoText("VERIF SIGNATURE", 54, 1, nowMs, true);
    return;
  }

  if (frameIndex == 2U) {
    drawTitleBar("CRYPTO CLEF - LOCK");
    drawCenteredDemoText("CRYPTO CLEF - LOCK", 2, 1, nowMs, false, SSD1306_BLACK);
    drawSprite8(kSpriteSkull, 3, 2, SSD1306_BLACK);
    drawSprite8(kSpriteLock, 117, 2, SSD1306_BLACK);
    drawDataRain(nowMs, 8, 16, 112, 32);
    drawRadarSweep(nowMs, 64, 32, 12);
    drawCenteredDemoText("ECOUTE CANAL Z", 54, 1, nowMs, true);
    return;
  }

  if (frameIndex == 3U) {
    drawTitleBar("ACCES AUTORISE");
    drawCenteredDemoText("ACCES AUTORISE", 2, 1, nowMs, false, SSD1306_BLACK);
    drawSprite8(kSpriteStar, 3, 2, SSD1306_BLACK);
    drawSprite8(kSpriteStar, 117, 2, SSD1306_BLACK);
    drawPulseRays(nowMs, 64, 34);
    drawSprite8(kSpriteChip, 16, 26);
    drawSprite8(kSpriteChip, 104, 26);
    drawCenteredDemoText("LA CONFIRME", 24, 2, nowMs, true);
    drawCenteredDemoText("VERROU 01 : OUVERT", 54, 1, nowMs, false);
    return;
  }

  if (frameIndex == 4U) {
    drawTitleBar("NOUVEAU DROIT");
    drawCenteredDemoText("NOUVEAU DROIT", 2, 1, nowMs, false, SSD1306_BLACK);
    drawSprite8(kSpriteLock, 3, 2, SSD1306_BLACK);
    drawSprite8(kSpriteLock, 117, 2, SSD1306_BLACK);
    g_display.drawRoundRect(8, 18, 112, 28, 4, SSD1306_WHITE);
    drawSprite8(kSpritePhone, 16, 27);
    drawSprite8(kSpriteStar, 104, 27);
    drawCenteredDemoText("APPELER HOTLINE", 28, 1, nowMs, true);
    drawCenteredDemoText("BRAVO", 54, 1, nowMs, true);
    return;
  }

  drawTitleBar("MISSION ACTIVE");
  drawCenteredDemoText("MISSION ACTIVE", 2, 1, nowMs, false, SSD1306_BLACK);
  drawSprite8(kSpriteChip, 3, 2, SSD1306_BLACK);
  drawSprite8(kSpriteSkull, 117, 2, SSD1306_BLACK);
  drawMissionGrid(nowMs, 8, 16, 112, 32);
  drawCenteredDemoText("SCAN CAMPUS / INDICES", 54, 1, nowMs, true);
}

void renderLinkDownScreen(uint32_t nowMs) {
  const uint32_t lastTickMs = latestLinkTickMs();
  const uint32_t ageMs = safeAgeMs(nowMs, lastTickMs);

  drawTitleBar("U-SON SCREEN");
  drawCenteredText("LINK DOWN", 18, 2);

  char ageLine[26];
  snprintf(ageLine, sizeof(ageLine), "Derniere trame %lus", static_cast<unsigned long>(ageMs / 1000UL));
  drawCenteredText(ageLine, 43, 1);

  char lossLine[22];
  snprintf(lossLine, sizeof(lossLine), "Pertes %lu", static_cast<unsigned long>(g_linkLossCount));
  drawCenteredText(lossLine, 54, 1);
}

void renderLinkRecoveringScreen(uint32_t nowMs) {
  const uint32_t lastTickMs = latestLinkTickMs();
  const uint32_t ageMs = safeAgeMs(nowMs, lastTickMs);

  drawBrokenModuleGlitch(nowMs, 64, 32);
  g_display.fillRect(0, 0, kScreenWidth, 12, SSD1306_WHITE);
  g_display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  drawCenteredText("U-SON SCREEN", 2, 1);
  g_display.setTextColor(SSD1306_WHITE);

  drawCenteredText("RECONNEXION MODULE", 18, 1);

  char ageLine[26];
  snprintf(ageLine, sizeof(ageLine), "Derniere trame %lus", static_cast<unsigned long>(ageMs / 1000UL));
  drawCenteredText(ageLine, 32, 1);

  char retryLine[22];
  snprintf(retryLine, sizeof(retryLine), "Pertes %lu", static_cast<unsigned long>(g_linkLossCount));
  drawCenteredText(retryLine, 43, 1);

  const uint16_t sweepPhase = static_cast<uint16_t>((nowMs / 35U) % 200U);
  const uint8_t sweep = static_cast<uint8_t>((sweepPhase <= 100U) ? sweepPhase : (200U - sweepPhase));
  drawHorizontalGauge(12, 54, 104, 8, sweep);
}

void renderScreen(uint32_t nowMs, bool linkAlive) {
  if (!g_displayReady) {
    return;
  }

  g_display.clearDisplay();
  g_display.setTextColor(SSD1306_WHITE);

  if (g_bootSplashUntilMs != 0U && static_cast<int32_t>(nowMs - g_bootSplashUntilMs) < 0) {
    drawTitleBar("U-SON SCREEN");

    char line[22] = {};
    const uint8_t dots = static_cast<uint8_t>((nowMs / 280U) % 4U);
    snprintf(line, sizeof(line), "Demarrage%.*s", static_cast<int>(dots), "...");
    drawCenteredText(line, 20, 2);
    drawCenteredText(g_linkEnabled ? "Init OLED + lien ESP32" : "Init OLED", 43, 1);

    const uint16_t sweepPhase = static_cast<uint16_t>((nowMs / 35U) % 200U);
    const uint8_t sweep = static_cast<uint8_t>((sweepPhase <= 100U) ? sweepPhase : (200U - sweepPhase));
    drawHorizontalGauge(12, 54, 104, 8, sweep);

    g_display.display();
    return;
  }

  if (!g_linkEnabled) {
    drawTitleBar("U-SON SCREEN");
    drawCenteredText("Liaison indisponible", 22, 1);
    drawCenteredText("Verifier cablage", 34, 1);
  } else if (!g_hasValidState) {
    renderStartupBootScreen(nowMs);
  } else if (!linkAlive) {
    const bool recovering = isPeerRebootGraceActive(nowMs) || g_linkLostSinceMs == 0U ||
                            (nowMs - g_linkLostSinceMs) < kLinkRecoverGraceMs;
    if (recovering) {
      renderLinkRecoveringScreen(nowMs);
    } else {
      renderLinkDownScreen(nowMs);
    }
  } else {
    if (g_state.startupStage == kStartupStageBootValidation) {
      renderStartupBootScreen(nowMs);
    } else if (g_state.appStage == kAppStageMp3) {
      renderMp3Screen();
    } else if (g_state.appStage == kAppStageULockWaiting ||
               g_state.appStage == kAppStageULockListening) {
      renderULockScreen(nowMs);
    } else if (g_state.appStage == kAppStageUSonFunctional) {
      renderUnlockSequenceScreen(nowMs);
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
  unsigned int startupStage = 0;
  unsigned int appStage = 0;
  unsigned long frameSeq = 0;
  unsigned int uiPage = 0;
  unsigned int repeatMode = 0;
  unsigned int fxActive = 0;
  unsigned int backendMode = 0;
  unsigned int scanBusy = 0;
  unsigned int errorCode = 0;

  const int parsed = sscanf(frame,
                            "STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u",
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
                            &unlockHoldPercent,
                            &startupStage,
                            &appStage,
                            &frameSeq,
                            &uiPage,
                            &repeatMode,
                            &fxActive,
                            &backendMode,
                            &scanBusy,
                            &errorCode);
  if (parsed < 19) {
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

  if (parsed >= 18) {
    out->startupStage =
        (startupStage == kStartupStageBootValidation) ? kStartupStageBootValidation
                                                      : kStartupStageInactive;
  } else {
    out->startupStage = kStartupStageInactive;
  }

  if (parsed >= 19) {
    if (appStage > kAppStageMp3) {
      appStage = kAppStageULockWaiting;
    }
    out->appStage = static_cast<uint8_t>(appStage);
  } else if (out->mp3Mode) {
    out->appStage = kAppStageMp3;
  } else if (out->uSonFunctional) {
    out->appStage = kAppStageUSonFunctional;
  } else if (out->uLockMode && out->uLockListening) {
    out->appStage = kAppStageULockListening;
  } else {
    out->appStage = kAppStageULockWaiting;
  }

  if (parsed >= 20) {
    out->frameSeq = static_cast<uint32_t>(frameSeq);
  }
  out->uiPage = (parsed >= 21) ? static_cast<uint8_t>(uiPage) : 0U;
  out->repeatMode = (parsed >= 22) ? static_cast<uint8_t>(repeatMode) : 0U;
  out->fxActive = (parsed >= 23) ? (fxActive != 0U) : false;
  out->backendMode = (parsed >= 24) ? static_cast<uint8_t>(backendMode) : 0U;
  out->scanBusy = (parsed >= 25) ? (scanBusy != 0U) : false;
  out->errorCode = (parsed >= 26) ? static_cast<uint8_t>(errorCode) : 0U;

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
        if (g_hasValidState &&
            (parsed.uptimeMs + kPeerUptimeRollbackSlackMs) < g_state.uptimeMs) {
          g_peerRebootUntilMs = millis() + kPeerRebootGraceMs;
          Serial.printf("[SCREEN] Peer reboot detecte: uptime %lu -> %lu\n",
                        static_cast<unsigned long>(g_state.uptimeMs),
                        static_cast<unsigned long>(parsed.uptimeMs));
        }
        if (g_state.appStage != kAppStageUSonFunctional &&
            parsed.appStage == kAppStageUSonFunctional) {
          g_unlockSequenceStartMs = millis();
        } else if (g_state.appStage == kAppStageUSonFunctional &&
                   parsed.appStage != kAppStageUSonFunctional) {
          g_unlockSequenceStartMs = 0;
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
    for (uint8_t attempt = 1U; attempt <= kOledInitRetries; ++attempt) {
      Serial.printf("[SCREEN] Test I2C %s try=%u/%u\n",
                    candidate.label,
                    static_cast<unsigned int>(attempt),
                    static_cast<unsigned int>(kOledInitRetries));
      if (initDisplayOnPins(candidate.sda, candidate.scl)) {
        g_displayReady = true;
        g_oledSdaPin = candidate.sda;
        g_oledSclPin = candidate.scl;
        break;
      }
      delay(kOledInitRetryDelayMs);
    }
    if (g_displayReady) {
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
    g_bootSplashUntilMs = millis() + static_cast<uint32_t>(kBootSplashMinMs);

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
  if (linkAlive) {
    if (g_linkLostSinceMs != 0U) {
      g_stateDirty = true;
    }
    g_linkLostSinceMs = 0U;
  } else if (g_linkLostSinceMs == 0U) {
    g_linkLostSinceMs = nowMs;
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
    const uint32_t ageMs = safeAgeMs(nowMs, lastTickMs);
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
