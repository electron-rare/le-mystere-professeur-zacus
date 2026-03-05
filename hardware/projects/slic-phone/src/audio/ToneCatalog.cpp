#include "audio/ToneCatalog.h"

namespace {

#define TONE_ON1(freq_hz, ms) \
    ToneStep { static_cast<uint16_t>(freq_hz), 0U, static_cast<uint16_t>(ms), false }
#define TONE_ON2(freq_a_hz, freq_b_hz, ms) \
    ToneStep { static_cast<uint16_t>(freq_a_hz), static_cast<uint16_t>(freq_b_hz), static_cast<uint16_t>(ms), false }
#define TONE_OFF(ms) \
    ToneStep { 0U, 0U, static_cast<uint16_t>(ms), true }

constexpr ToneStep kEtsiDial[] = {TONE_ON1(425, 1000)};
constexpr ToneStep kEtsiSecondaryDial[] = {TONE_ON1(425, 1000)};
constexpr ToneStep kEtsiSpecialDialStutter[] = {TONE_ON1(425, 500), TONE_OFF(50)};
constexpr ToneStep kEtsiRecallDial[] = {TONE_ON1(425, 1000)};
constexpr ToneStep kEtsiRingback[] = {TONE_ON1(425, 1000), TONE_OFF(4000)};
constexpr ToneStep kEtsiBusy[] = {TONE_ON1(425, 500), TONE_OFF(500)};
constexpr ToneStep kEtsiCongestion[] = {TONE_ON1(425, 250), TONE_OFF(250)};
constexpr ToneStep kEtsiCallWaiting[] = {TONE_ON1(425, 200), TONE_OFF(200), TONE_ON1(425, 200), TONE_OFF(3000)};
constexpr ToneStep kEtsiConfirmation[] = {
    TONE_ON1(425, 100), TONE_OFF(100), TONE_ON1(425, 100), TONE_OFF(100), TONE_ON1(425, 100), TONE_OFF(1000)};
constexpr ToneStep kEtsiSitIntercept[] = {
    TONE_ON1(950, 330), TONE_OFF(30), TONE_ON1(1400, 330), TONE_OFF(30), TONE_ON1(1800, 330), TONE_OFF(1000)};

constexpr ToneStep kFrDial[] = {TONE_ON1(440, 1000)};
constexpr ToneStep kFrSecondaryDial[] = {TONE_ON1(440, 1000)};
constexpr ToneStep kFrSpecialDialStutter[] = {TONE_ON1(440, 500), TONE_OFF(50)};
constexpr ToneStep kFrRecallDial[] = {TONE_ON1(440, 1000)};
constexpr ToneStep kFrRingback[] = {TONE_ON1(440, 1500), TONE_OFF(3500)};
constexpr ToneStep kFrBusy[] = {TONE_ON1(440, 500), TONE_OFF(500)};
constexpr ToneStep kFrCongestion[] = {TONE_ON1(440, 250), TONE_OFF(250)};
constexpr ToneStep kFrCallWaiting[] = {TONE_ON1(440, 300), TONE_OFF(10000)};
constexpr ToneStep kFrConfirmation[] = {
    TONE_ON1(440, 100), TONE_OFF(100), TONE_ON1(440, 100), TONE_OFF(100), TONE_ON1(440, 100), TONE_OFF(1000)};
constexpr ToneStep kFrSitIntercept[] = {
    TONE_ON1(950, 300), TONE_OFF(30), TONE_ON1(1400, 300), TONE_OFF(30), TONE_ON1(1800, 300), TONE_OFF(1000)};

constexpr ToneStep kUkDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kUkSecondaryDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kUkSpecialDialStutter[] = {TONE_ON2(350, 440, 100), TONE_OFF(100)};
constexpr ToneStep kUkRecallDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kUkRingback[] = {TONE_ON2(400, 450, 400), TONE_OFF(200), TONE_ON2(400, 450, 400), TONE_OFF(2000)};
constexpr ToneStep kUkBusy[] = {TONE_ON2(400, 450, 375), TONE_OFF(375)};
constexpr ToneStep kUkCongestion[] = {TONE_ON2(400, 450, 400), TONE_OFF(400)};
constexpr ToneStep kUkCallWaiting[] = {TONE_ON2(400, 450, 100), TONE_OFF(100), TONE_ON2(400, 450, 100), TONE_OFF(9700)};
constexpr ToneStep kUkSitIntercept[] = {
    TONE_ON1(950, 330), TONE_OFF(30), TONE_ON1(1400, 330), TONE_OFF(30), TONE_ON1(1800, 330), TONE_OFF(1000)};

constexpr ToneStep kNaDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kNaSecondaryDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kNaSpecialDialStutter[] = {TONE_ON2(350, 440, 100), TONE_OFF(100)};
constexpr ToneStep kNaRecallDial[] = {TONE_ON2(350, 440, 1000)};
constexpr ToneStep kNaRingback[] = {TONE_ON2(440, 480, 2000), TONE_OFF(4000)};
constexpr ToneStep kNaBusy[] = {TONE_ON2(480, 620, 500), TONE_OFF(500)};
constexpr ToneStep kNaCongestion[] = {TONE_ON2(480, 620, 250), TONE_OFF(250)};
constexpr ToneStep kNaCallWaiting[] = {TONE_ON1(440, 300), TONE_OFF(9700)};
constexpr ToneStep kNaConfirmation[] = {TONE_ON2(350, 440, 100), TONE_OFF(100), TONE_ON2(350, 440, 100), TONE_OFF(900)};
constexpr ToneStep kNaSitIntercept[] = {
    TONE_ON1(950, 330), TONE_OFF(30), TONE_ON1(1400, 330), TONE_OFF(30), TONE_ON1(1800, 330), TONE_OFF(1000)};

struct PatternEntry {
    ToneProfile profile;
    ToneEvent event;
    const ToneStep* steps;
    uint8_t count;
    bool loop;
    uint8_t loop_start;
};

#define ENTRY(profile, event, arr, should_loop) \
    PatternEntry { profile, event, arr, static_cast<uint8_t>(sizeof(arr) / sizeof(arr[0])), should_loop, 0U }

constexpr PatternEntry kPatternTable[] = {
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::DIAL, kEtsiDial, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::SECONDARY_DIAL, kEtsiSecondaryDial, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::SPECIAL_DIAL_STUTTER, kEtsiSpecialDialStutter, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::RECALL_DIAL, kEtsiRecallDial, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::RINGBACK, kEtsiRingback, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::BUSY, kEtsiBusy, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::CONGESTION, kEtsiCongestion, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::CALL_WAITING, kEtsiCallWaiting, true),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::CONFIRMATION, kEtsiConfirmation, false),
    ENTRY(ToneProfile::ETSI_EU, ToneEvent::SIT_INTERCEPT, kEtsiSitIntercept, true),

    ENTRY(ToneProfile::FR_FR, ToneEvent::DIAL, kFrDial, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::SECONDARY_DIAL, kFrSecondaryDial, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::SPECIAL_DIAL_STUTTER, kFrSpecialDialStutter, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::RECALL_DIAL, kFrRecallDial, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::RINGBACK, kFrRingback, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::BUSY, kFrBusy, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::CONGESTION, kFrCongestion, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::CALL_WAITING, kFrCallWaiting, true),
    ENTRY(ToneProfile::FR_FR, ToneEvent::CONFIRMATION, kFrConfirmation, false),
    ENTRY(ToneProfile::FR_FR, ToneEvent::SIT_INTERCEPT, kFrSitIntercept, true),

    ENTRY(ToneProfile::UK_GB, ToneEvent::DIAL, kUkDial, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::SECONDARY_DIAL, kUkSecondaryDial, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::SPECIAL_DIAL_STUTTER, kUkSpecialDialStutter, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::RECALL_DIAL, kUkRecallDial, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::RINGBACK, kUkRingback, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::BUSY, kUkBusy, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::CONGESTION, kUkCongestion, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::CALL_WAITING, kUkCallWaiting, true),
    ENTRY(ToneProfile::UK_GB, ToneEvent::SIT_INTERCEPT, kUkSitIntercept, true),

    ENTRY(ToneProfile::NA_US, ToneEvent::DIAL, kNaDial, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::SECONDARY_DIAL, kNaSecondaryDial, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::SPECIAL_DIAL_STUTTER, kNaSpecialDialStutter, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::RECALL_DIAL, kNaRecallDial, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::RINGBACK, kNaRingback, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::BUSY, kNaBusy, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::CONGESTION, kNaCongestion, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::CALL_WAITING, kNaCallWaiting, true),
    ENTRY(ToneProfile::NA_US, ToneEvent::CONFIRMATION, kNaConfirmation, false),
    ENTRY(ToneProfile::NA_US, ToneEvent::SIT_INTERCEPT, kNaSitIntercept, true),
};

bool lookupPattern(ToneProfile profile, ToneEvent event, TonePattern& out_pattern) {
    for (const PatternEntry& entry : kPatternTable) {
        if (entry.profile != profile || entry.event != event) {
            continue;
        }
        out_pattern.steps = entry.steps;
        out_pattern.step_count = entry.count;
        out_pattern.loop = entry.loop;
        out_pattern.loop_start_index = entry.loop_start;
        return true;
    }
    return false;
}

}  // namespace

bool ToneCatalog::resolve(ToneProfile profile, ToneEvent event, TonePattern& out_pattern) {
    out_pattern = TonePattern{};
    if (event == ToneEvent::NONE) {
        return false;
    }
    if (profile == ToneProfile::NONE) {
        profile = ToneProfile::FR_FR;
    }
    if (lookupPattern(profile, event, out_pattern)) {
        return true;
    }
    if (lookupPattern(ToneProfile::ETSI_EU, event, out_pattern)) {
        return true;
    }
    return false;
}
