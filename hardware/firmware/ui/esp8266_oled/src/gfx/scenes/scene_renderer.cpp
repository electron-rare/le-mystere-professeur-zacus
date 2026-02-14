#include "scene_renderer.h"

#include "../layout_metrics.h"
#include "../widgets/basic_widgets.h"

namespace screen_gfx {

namespace {

const char* slotText(const screen_core::TextSlots* text, screen_core::TextSlotId id) {
  if (text == nullptr) {
    return "";
  }
  return screen_core::textSlotValue(*text, id);
}

uint8_t safePercent(uint8_t v) {
  return (v > 100U) ? 100U : v;
}

const char* sourceLabel(const screen_core::TelemetryState* state) {
  if (state == nullptr) {
    return "SD";
  }
  return (state->uiSource == 1U) ? "RADIO" : "SD";
}

}  // namespace

void renderMp3LectureScene(const SceneRenderContext& ctx) {
  if (ctx.display == nullptr || ctx.state == nullptr) {
    return;
  }
  DisplayBackend& d = *ctx.display;
  const screen_core::TelemetryState& s = *ctx.state;
  drawHeader(d, "LECTURE", sourceLabel(&s));

  d.setTextSize(1);
  d.setCursor(2, 22);
  d.print(slotText(ctx.text, screen_core::TextSlotId::kNowTitle1));
  d.setCursor(2, 32);
  d.print(slotText(ctx.text, screen_core::TextSlotId::kNowTitle2));
  d.setCursor(2, 42);
  d.print(slotText(ctx.text, screen_core::TextSlotId::kNowSub));

  const uint8_t progress = (s.trackCount > 0U)
                               ? static_cast<uint8_t>((static_cast<uint32_t>(s.track) * 100U) / s.trackCount)
                               : static_cast<uint8_t>((ctx.nowMs / 200U) % 100U);
  drawProgressBar(d, 2, 50, 96, 8, progress);
  drawVuMini(d, 104, 50, safePercent(s.micLevelPercent), ctx.nowMs);
  drawProgressBar(d, 2, 60, 126, 4, safePercent(s.volumePercent));
}

void renderMp3ListeScene(const SceneRenderContext& ctx) {
  if (ctx.display == nullptr || ctx.state == nullptr) {
    return;
  }
  DisplayBackend& d = *ctx.display;
  const screen_core::TelemetryState& s = *ctx.state;

  char right[24] = {};
  snprintf(right,
           sizeof(right),
           "%s %u/%u",
           sourceLabel(&s),
           static_cast<unsigned int>((s.uiCount == 0U) ? 0U : (s.uiCursor + 1U)),
           static_cast<unsigned int>(s.uiCount));
  drawHeader(d, "LISTE", right);

  d.setTextSize(1);
  d.setCursor(2, 20);
  d.print(slotText(ctx.text, screen_core::TextSlotId::kListPath));

  const uint16_t row0Idx = s.uiOffset;
  const uint16_t row1Idx = static_cast<uint16_t>(s.uiOffset + 1U);
  const uint16_t row2Idx = static_cast<uint16_t>(s.uiOffset + 2U);
  drawListRow(d,
              2,
              32,
              124,
              slotText(ctx.text, screen_core::TextSlotId::kListRow0),
              s.uiCursor == row0Idx);
  drawListRow(d,
              2,
              42,
              124,
              slotText(ctx.text, screen_core::TextSlotId::kListRow1),
              s.uiCursor == row1Idx);
  drawListRow(d,
              2,
              52,
              124,
              slotText(ctx.text, screen_core::TextSlotId::kListRow2),
              s.uiCursor == row2Idx);
}

void renderMp3ReglagesScene(const SceneRenderContext& ctx) {
  if (ctx.display == nullptr || ctx.state == nullptr) {
    return;
  }
  DisplayBackend& d = *ctx.display;
  const screen_core::TelemetryState& s = *ctx.state;
  drawHeader(d, "REGLAGES", sourceLabel(&s));

  d.setTextSize(1);
  d.setCursor(2, 22);
  d.print("K2/K3: item   K4/K5: val");
  d.setCursor(2, 32);
  d.print("K1: appliquer  K6: mode");
  d.setCursor(2, 44);
  d.print(slotText(ctx.text, screen_core::TextSlotId::kSetHint));

  char line[20] = {};
  snprintf(line, sizeof(line), "item=%u", static_cast<unsigned int>(s.uiCursor));
  d.setCursor(2, 56);
  d.print(line);
}

void renderMp3SceneV3(const SceneRenderContext& ctx) {
  if (ctx.state == nullptr) {
    return;
  }
  switch (ctx.state->uiPage) {
    case 1:
      renderMp3ListeScene(ctx);
      break;
    case 2:
      renderMp3ReglagesScene(ctx);
      break;
    case 0:
    default:
      renderMp3LectureScene(ctx);
      break;
  }
}

}  // namespace screen_gfx
