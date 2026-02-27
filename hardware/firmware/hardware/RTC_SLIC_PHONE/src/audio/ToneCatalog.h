#ifndef AUDIO_TONE_CATALOG_H
#define AUDIO_TONE_CATALOG_H

#include <Arduino.h>
#include <stdint.h>

#include "media/MediaRouting.h"

struct ToneStep {
    uint16_t freq_a_hz = 0;
    uint16_t freq_b_hz = 0;
    uint16_t duration_ms = 0;
    bool silence = true;

    constexpr ToneStep() = default;
    constexpr ToneStep(uint16_t freq_a, uint16_t freq_b, uint16_t duration, bool is_silence)
        : freq_a_hz(freq_a), freq_b_hz(freq_b), duration_ms(duration), silence(is_silence) {}
};

struct TonePattern {
    const ToneStep* steps = nullptr;
    uint8_t step_count = 0;
    bool loop = false;
    uint8_t loop_start_index = 0;
};

class ToneCatalog {
public:
    static bool resolve(ToneProfile profile, ToneEvent event, TonePattern& out_pattern);
};

#endif  // AUDIO_TONE_CATALOG_H
