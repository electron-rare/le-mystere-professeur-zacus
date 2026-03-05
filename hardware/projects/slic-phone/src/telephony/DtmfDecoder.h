#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

class DtmfDecoder {
public:
    using DigitCallback = std::function<void(char)>;
    DtmfDecoder();
    explicit DtmfDecoder(uint16_t sampleRateHz, size_t windowSize = 160);
    void feedAudioSamples(const int16_t* samples, size_t count);
    void setDigitCallback(DigitCallback cb);

private:
    char detectDigit(const int16_t* samples, size_t count) const;
    DigitCallback onDigit;
    uint16_t sampleRateHz_;
    size_t windowSize_;
    char lastCandidate_;
    uint8_t stableCount_;
    char latchedDigit_;
};
