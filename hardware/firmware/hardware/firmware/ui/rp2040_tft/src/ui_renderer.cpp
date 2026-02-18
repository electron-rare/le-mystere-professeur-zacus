#include "ui_renderer.h"

#include <cmath>

#include <TFT_eSPI.h>

#include "../include/ui_config.h"
#include "ui_screen_loader.h"
#include "ui_state.h"

namespace {

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kFg = TFT_WHITE;
constexpr uint16_t kAccent = 0x05FF;
constexpr uint16_t kWarn = 0xFD20;
constexpr uint16_t kOk = 0x07E0;
constexpr uint16_t kPanel = 0x1082;
constexpr uint16_t kPanelDark = 0x0841;

void splitTwoLines(const char* in, char* l1, size_t l1Len, char* l2, size_t l2Len) {
  if (l1Len > 0U) {
    l1[0] = '\0';
  }
  if (l2Len > 0U) {
    l2[0] = '\0';
  }
  if (in == nullptr || in[0] == '\0') {
    return;
  }
  const size_t n = strlen(in);
  if (n <= 36U) {
    snprintf(l1, l1Len, "%s", in);
    return;
  }
  size_t split = 36U;
  while (split > 10U && in[split] != ' ') {
    --split;
  }
  if (split <= 10U) {
    split = 36U;
  }
  snprintf(l1, l1Len, "%.*s", static_cast<int>(split), in);
  while (in[split] == ' ') {
    ++split;
  }
  snprintf(l2, l2Len, "%s", in + split);
}

}  // namespace

UiRenderer::UiRenderer(TFT_eSPI& tft) : tft_(tft) {}

void UiRenderer::showLocalScreen(TFT_eSPI& tft, const char* filename) {
  UiScreen screen;
  if (loadUiScreen(filename, screen)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(4);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(screen.id.c_str(), 240, 80);
    tft.setTextFont(2);
    tft.drawString(screen.description.c_str(), 240, 140);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Erreur ecran", 240, 120);
  tft.setTextDatum(TL_DATUM);
}

void UiRenderer::begin() {
  tft_.fillScreen(kBg);
  tft_.setTextColor(kFg, kBg);
}

void UiRenderer::drawBootScreen(const char* line1, const char* line2) {
  tft_.fillScreen(kBg);
  tft_.setTextColor(kAccent, kBg);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextFont(4);
  tft_.drawString("U-SON TOUCH UI", ui_config::kScreenWidth / 2, 70);
  tft_.setTextFont(2);
  tft_.setTextColor(kFg, kBg);
  tft_.drawString(line1 != nullptr ? line1 : "Booting...", ui_config::kScreenWidth / 2, 122);
  tft_.drawString(line2 != nullptr ? line2 : "", ui_config::kScreenWidth / 2, 146);
  tft_.setTextDatum(TL_DATUM);
}

void UiRenderer::drawBottomButtons(const char* labels[5], uint16_t color) {
  const int16_t y = 250;
  const int16_t h = 70;
  const int16_t w = ui_config::kScreenWidth / 5;
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  for (int i = 0; i < 5; ++i) {
    const int16_t x = i * w;
    tft_.fillRect(x + 1, y + 1, w - 2, h - 2, kPanelDark);
    tft_.drawRect(x + 1, y + 1, w - 2, h - 2, color);
    tft_.setTextColor(color, kPanelDark);
    tft_.drawString(labels[i], x + (w / 2), y + (h / 2));
  }
  tft_.setTextDatum(TL_DATUM);
}

void UiRenderer::drawHeader(const UiStateModel& ui) {
  tft_.fillRect(0, 0, ui_config::kScreenWidth, 38, kPanelDark);
  tft_.drawLine(0, 37, ui_config::kScreenWidth, 37, kPanel);

  const char* tabs[3] = {"LECTURE", "LISTE", "REGLAGES"};
  const int16_t tabW = ui_config::kScreenWidth / 3;
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  for (uint8_t i = 0U; i < 3U; ++i) {
    const bool active = (static_cast<uint8_t>(ui.page()) == i);
    const int16_t x = i * tabW;
    if (active) {
      tft_.fillRect(x + 2, 4, tabW - 4, 30, kAccent);
    } else {
      tft_.drawRect(x + 2, 4, tabW - 4, 30, kPanel);
    }
    tft_.setTextColor(active ? kBg : kFg, active ? kAccent : kPanelDark);
    tft_.drawString(tabs[i], x + (tabW / 2), 19);
  }
  tft_.setTextDatum(TL_DATUM);
}

const char* UiRenderer::wifiModeLabel(uint8_t mode) const {
  switch (mode % 3U) {
    case 0:
      return "STA";
    case 1:
      return "AP";
    default:
      return "AUTO";
  }
}

const char* UiRenderer::eqLabel(uint8_t preset) const {
  switch (preset % 4U) {
    case 0:
      return "FLAT";
    case 1:
      return "BASS";
    case 2:
      return "VOICE";
    default:
      return "TREBLE";
  }
}

const char* UiRenderer::brightnessLabel(uint8_t level) const {
  switch (level % 4U) {
    case 0:
      return "25%";
    case 1:
      return "50%";
    case 2:
      return "75%";
    default:
      return "100%";
  }
}

void UiRenderer::drawVuMeter(int16_t x, int16_t y, int16_t w, int16_t h, float vu) {
  tft_.drawRect(x, y, w, h, kPanel);
  const uint16_t fill = static_cast<uint16_t>((h - 2) * (vu < 0.0f ? 0.0f : (vu > 1.0f ? 1.0f : vu)));
  tft_.fillRect(x + 1, y + 1, w - 2, h - 2, kBg);
  if (fill > 0U) {
    const uint16_t c = (fill > (h * 3 / 4)) ? kWarn : kOk;
    tft_.fillRect(x + 1, y + h - 1 - fill, w - 2, fill, c);
  }
}

void UiRenderer::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int32_t pos, int32_t dur, bool live) {
  tft_.drawRect(x, y, w, h, kPanel);
  tft_.fillRect(x + 1, y + 1, w - 2, h - 2, kBg);
  if (live || dur <= 0) {
    tft_.setTextColor(kWarn, kBg);
    tft_.setTextFont(2);
    tft_.drawString("LIVE", x + 8, y + 4);
    return;
  }
  int32_t clampedPos = pos;
  if (clampedPos < 0) {
    clampedPos = 0;
  } else if (clampedPos > dur) {
    clampedPos = dur;
  }
  const uint16_t fill = static_cast<uint16_t>((static_cast<int64_t>(w - 2) * clampedPos) / dur);
  tft_.fillRect(x + 1, y + 1, fill, h - 2, kAccent);
}

void UiRenderer::drawWrappedTitle(const char* title, int16_t x, int16_t y, int16_t w, uint32_t nowMs) {
  if (title == nullptr) {
    title = "";
  }
  if (strcmp(marqueeTitle_, title) != 0) {
    snprintf(marqueeTitle_, sizeof(marqueeTitle_), "%s", title);
    marqueeOffset_ = 0U;
    marqueeStartedMs_ = nowMs;
    lastMarqueeStepMs_ = nowMs;
  }

  char line1[48] = {};
  char line2[48] = {};
  splitTwoLines(title, line1, sizeof(line1), line2, sizeof(line2));

  tft_.setTextColor(kFg, kBg);
  tft_.setTextFont(4);
  if (line2[0] == '\0') {
    // Marquee only when single long line.
    const uint16_t textW = tft_.textWidth(line1);
    if (textW > static_cast<uint16_t>(w) &&
        static_cast<int32_t>(nowMs - marqueeStartedMs_) > static_cast<int32_t>(ui_config::kTxtMarqueeStartDelayMs) &&
        static_cast<int32_t>(nowMs - lastMarqueeStepMs_) > static_cast<int32_t>(ui_config::kTxtMarqueeStepMs)) {
      ++marqueeOffset_;
      if (marqueeOffset_ > strlen(line1)) {
        marqueeOffset_ = 0U;
      }
      lastMarqueeStepMs_ = nowMs;
    }
    const char* start = line1 + marqueeOffset_;
    tft_.setViewport(x, y, w, 58, true);
    tft_.fillRect(0, 0, w, 58, kBg);
    tft_.drawString(start, 0, 0);
    if (tft_.textWidth(start) < static_cast<uint16_t>(w) && marqueeOffset_ > 0U) {
      tft_.drawString("   ", tft_.textWidth(start), 0);
      tft_.drawString(line1, tft_.textWidth(start) + 18, 0);
    }
    tft_.resetViewport();
  } else {
    tft_.fillRect(x, y, w, 58, kBg);
    tft_.drawString(line1, x, y);
    tft_.drawString(line2, x, y + 30);
  }
}

void UiRenderer::drawNowPlaying(const UiStateModel& ui, uint32_t nowMs, bool full) {
  if (full) {
    tft_.fillRect(0, 38, ui_config::kScreenWidth, 212, kBg);
  }
  tft_.fillRect(10, 46, 98, 26, kPanelDark);
  tft_.drawRect(10, 46, 98, 26, kAccent);
  tft_.setTextColor(kAccent, kPanelDark);
  tft_.setTextFont(2);
  tft_.drawString(ui.source() == UiSource::kRadio ? "RADIO" : "SD", 22, 53);

  tft_.fillRect(122, 46, 260, 20, kBg);
  tft_.setTextColor(kFg, kBg);
  tft_.setTextFont(2);
  tft_.drawString(ui.connected() ? "UART OK" : "CONNECTING...", 122, 49);

  tft_.fillRect(392, 46, 82, 20, kBg);
  tft_.setTextColor(kFg, kBg);
  tft_.setTextFont(2);
  tft_.drawString(String(ui.rssi()) + "dBm", 392, 49);

  drawWrappedTitle(ui.title(), 14, 80, 360, nowMs);

  tft_.fillRect(14, 144, 360, 24, kBg);
  tft_.setTextColor(kPanel == 0 ? kFg : 0xC618, kBg);
  tft_.setTextFont(2);
  if (ui.source() == UiSource::kRadio) {
    tft_.drawString(ui.station(), 14, 148);
  } else {
    tft_.drawString(ui.artist(), 14, 148);
  }

  drawProgressBar(16, 198, 360, 22, ui.posSec(), ui.durSec(), ui.source() == UiSource::kRadio);

  tft_.fillRect(16, 224, 360, 20, kBg);
  tft_.setTextColor(kFg, kBg);
  tft_.setTextFont(2);
  tft_.drawString(ui.playing() ? "PLAY" : "PAUSE", 16, 226);
  tft_.drawString("VOL " + String(ui.volume()) + "%", 120, 226);
  if (ui.bufferPercent() >= 0) {
    tft_.drawString("BUF " + String(ui.bufferPercent()) + "%", 236, 226);
  }

  drawVuMeter(402, 84, 58, 142, ui.vu());

  const char* labels[5] = {"PREV", "PLAY", "NEXT", "VOL-", "VOL+"};
  drawBottomButtons(labels, kAccent);
}

void UiRenderer::drawList(const UiStateModel& ui, bool full) {
  if (full) {
    tft_.fillRect(0, 38, ui_config::kScreenWidth, 212, kBg);
  }
  tft_.setTextColor(kAccent, kBg);
  tft_.setTextFont(2);
  tft_.fillRect(10, 44, 460, 22, kBg);
  tft_.drawString(String("Source: ") + (ui.source() == UiSource::kRadio ? "RADIO" : "SD"), 10, 46);

  const UiRemoteList& list = ui.list();
  const uint8_t rows = (list.count > 4U) ? 4U : list.count;
  for (uint8_t i = 0U; i < 4U; ++i) {
    const int16_t y = 72 + (i * 42);
    const bool active = (i == ui.listCursor() && i < rows);
    tft_.fillRect(10, y, 460, 36, active ? kAccent : kPanelDark);
    tft_.drawRect(10, y, 460, 36, active ? kAccent : kPanel);
    tft_.setTextColor(active ? kBg : kFg, active ? kAccent : kPanelDark);
    tft_.setTextFont(2);
    if (i < rows) {
      tft_.drawString(list.items[i], 18, y + 10);
    } else {
      tft_.drawString("-", 18, y + 10);
    }
  }

  tft_.fillRect(10, 240, 460, 10, kBg);
  tft_.setTextColor(0xC618, kBg);
  tft_.setTextFont(2);
  tft_.drawString(String("offset ") + list.offset + " / total " + list.total, 10, 240);

  const char* labels[5] = {"UP", "DOWN", "OK", "BACK", "MODE"};
  drawBottomButtons(labels, kAccent);
}

void UiRenderer::drawSettings(const UiStateModel& ui, bool full) {
  if (full) {
    tft_.fillRect(0, 38, ui_config::kScreenWidth, 212, kBg);
  }

  const char* keys[4] = {"Wi-Fi", "EQ", "Luminosite", "Screensaver"};
  String vals[4] = {
      wifiModeLabel(ui.wifiMode()),
      eqLabel(ui.eqPreset()),
      brightnessLabel(ui.brightness()),
      ui.screensaver() ? "ON" : "OFF",
  };

  for (uint8_t i = 0U; i < 4U; ++i) {
    const int16_t y = 62 + (i * 42);
    const bool active = (i == ui.settingsIndex());
    tft_.fillRect(10, y, 460, 36, active ? kAccent : kPanelDark);
    tft_.drawRect(10, y, 460, 36, active ? kAccent : kPanel);
    tft_.setTextColor(active ? kBg : kFg, active ? kAccent : kPanelDark);
    tft_.setTextFont(2);
    tft_.drawString(keys[i], 18, y + 10);
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString(vals[i], 460, y + 18);
    tft_.setTextDatum(TL_DATUM);
  }

  const char* labels[5] = {"UP", "DOWN", "APPLY", "BACK", "MODE"};
  drawBottomButtons(labels, kAccent);
}

void UiRenderer::drawFrame(UiStateModel& ui) {
  drawHeader(ui);
  switch (ui.page()) {
    case UiPage::kNowPlaying:
      drawNowPlaying(ui, millis(), true);
      break;
    case UiPage::kList:
      drawList(ui, true);
      break;
    case UiPage::kSettings:
      drawSettings(ui, true);
      break;
    default:
      break;
  }
}

void UiRenderer::render(UiStateModel& ui, uint32_t nowMs, bool forceFull) {
  const uint8_t currentPage = static_cast<uint8_t>(ui.page());
  bool full = forceFull || (currentPage != lastPage_);
  if (full) {
    drawFrame(ui);
    lastPage_ = currentPage;
    lastPosSec_ = -1;
    lastVol_ = -1;
    lastBuffer_ = -2;
    lastRssi_ = -255;
    lastVu_ = -1.0f;
    lastConnected_ = !ui.connected();
  }

  if (lastConnected_ != ui.connected()) {
    drawHeader(ui);
    lastConnected_ = ui.connected();
  }

  switch (ui.page()) {
    case UiPage::kNowPlaying:
      if (full || lastPosSec_ != ui.posSec() || lastVol_ != ui.volume() || lastBuffer_ != ui.bufferPercent() ||
          lastRssi_ != ui.rssi() || fabs(lastVu_ - ui.vu()) > 0.05f ||
          static_cast<int32_t>(nowMs - lastMarqueeStepMs_) > static_cast<int32_t>(ui_config::kTxtMarqueeStepMs)) {
        drawNowPlaying(ui, nowMs, false);
        lastPosSec_ = ui.posSec();
        lastVol_ = ui.volume();
        lastBuffer_ = ui.bufferPercent();
        lastRssi_ = ui.rssi();
        lastVu_ = ui.vu();
      }
      break;
    case UiPage::kList:
      if (full || static_cast<int32_t>(nowMs - lastMarqueeStepMs_) > 180) {
        drawList(ui, false);
        lastMarqueeStepMs_ = nowMs;
      }
      break;
    case UiPage::kSettings:
      if (full || static_cast<int32_t>(nowMs - lastMarqueeStepMs_) > 180) {
        drawSettings(ui, false);
        lastMarqueeStepMs_ = nowMs;
      }
      break;
    default:
      break;
  }
}
