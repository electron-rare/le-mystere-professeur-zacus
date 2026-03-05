#include "DtmfDecoder.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array<double, 4> kLowFreq = {{697.0, 770.0, 852.0, 941.0}};
constexpr std::array<double, 4> kHighFreq = {{1209.0, 1336.0, 1477.0, 1633.0}};
constexpr char kDigitMap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};
constexpr double kPi = 3.14159265358979323846;
constexpr double kDominanceRatio = 1.8;

double goertzelPower(const int16_t* samples, size_t count, double freqHz, uint16_t sampleRateHz) {
    if (samples == nullptr || count == 0 || sampleRateHz == 0U) {
        return 0.0;
    }

    const double omega = 2.0 * kPi * freqHz / static_cast<double>(sampleRateHz);
    const double coeff = 2.0 * std::cos(omega);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;

    for (size_t i = 0; i < count; ++i) {
        q0 = coeff * q1 - q2 + static_cast<double>(samples[i]);
        q2 = q1;
        q1 = q0;
    }
    return q1 * q1 + q2 * q2 - coeff * q1 * q2;
}

template <size_t N>
size_t indexOfMax(const std::array<double, N>& values) {
    size_t idx = 0;
    for (size_t i = 1; i < N; ++i) {
        if (values[i] > values[idx]) {
            idx = i;
        }
    }
    return idx;
}

template <size_t N>
double secondBest(const std::array<double, N>& values, size_t bestIndex) {
    double second = 0.0;
    for (size_t i = 0; i < N; ++i) {
        if (i == bestIndex) {
            continue;
        }
        second = std::max(second, values[i]);
    }
    return second;
}
}  // namespace

DtmfDecoder::DtmfDecoder()
    : DtmfDecoder(8000U, 160U) {}

DtmfDecoder::DtmfDecoder(uint16_t sampleRateHz, size_t windowSize)
    : onDigit(nullptr),
      sampleRateHz_(sampleRateHz == 0U ? 8000U : sampleRateHz),
      windowSize_(windowSize < 80U ? 80U : windowSize),
      lastCandidate_('\0'),
      stableCount_(0U),
      latchedDigit_('\0') {}

void DtmfDecoder::setDigitCallback(DigitCallback cb) {
    onDigit = cb;
}

char DtmfDecoder::detectDigit(const int16_t* samples, size_t count) const {
    if (samples == nullptr || count < (windowSize_ / 2U)) {
        return '\0';
    }

    std::array<double, 4> lowPower = {{0.0, 0.0, 0.0, 0.0}};
    std::array<double, 4> highPower = {{0.0, 0.0, 0.0, 0.0}};
    for (size_t i = 0; i < 4; ++i) {
        lowPower[i] = goertzelPower(samples, count, kLowFreq[i], sampleRateHz_);
        highPower[i] = goertzelPower(samples, count, kHighFreq[i], sampleRateHz_);
    }

    const size_t lowIdx = indexOfMax(lowPower);
    const size_t highIdx = indexOfMax(highPower);
    const double lowBest = lowPower[lowIdx];
    const double highBest = highPower[highIdx];
    const double lowSecond = secondBest(lowPower, lowIdx);
    const double highSecond = secondBest(highPower, highIdx);
    const double lowSum = lowPower[0] + lowPower[1] + lowPower[2] + lowPower[3];
    const double highSum = highPower[0] + highPower[1] + highPower[2] + highPower[3];

    if (lowBest <= 0.0 || highBest <= 0.0) {
        return '\0';
    }
    if (lowSecond > 0.0 && (lowBest / lowSecond) < kDominanceRatio) {
        return '\0';
    }
    if (highSecond > 0.0 && (highBest / highSecond) < kDominanceRatio) {
        return '\0';
    }
    if ((lowBest / (lowSum + 1.0)) < 0.55 || (highBest / (highSum + 1.0)) < 0.55) {
        return '\0';
    }

    return kDigitMap[lowIdx][highIdx];
}

void DtmfDecoder::feedAudioSamples(const int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0U) {
        return;
    }

    for (size_t offset = 0; offset < count; offset += windowSize_) {
        const size_t frameSize = std::min(windowSize_, count - offset);
        if (frameSize < (windowSize_ / 2U)) {
            continue;
        }

        const char candidate = detectDigit(samples + offset, frameSize);
        if (candidate == '\0') {
            lastCandidate_ = '\0';
            stableCount_ = 0U;
            latchedDigit_ = '\0';
            continue;
        }

        if (candidate == lastCandidate_) {
            if (stableCount_ < 255U) {
                ++stableCount_;
            }
        } else {
            lastCandidate_ = candidate;
            stableCount_ = 1U;
        }

        if (stableCount_ >= 2U && candidate != latchedDigit_) {
            latchedDigit_ = candidate;
            if (onDigit) {
                onDigit(candidate);
            }
        }
    }
}
