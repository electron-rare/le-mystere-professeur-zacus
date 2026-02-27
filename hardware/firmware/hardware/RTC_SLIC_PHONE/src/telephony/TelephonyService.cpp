#include "telephony/TelephonyService.h"

namespace {
constexpr uint16_t kDtmfFrameSamples = 160U;
constexpr uint32_t kHookHangupMs = 300U;
constexpr uint32_t kHookStabilizeMs = 40U;
constexpr uint32_t kPulseInterDigitGapMs = 700U;
constexpr uint32_t kPulseEdgeDebounceMs = 22U;
constexpr uint32_t kPulseBreakMinMs = 28U;
constexpr uint32_t kPulseBreakMaxMs = 220U;
constexpr uint32_t kPulseMakeMinMs = 28U;
constexpr uint32_t kPulseDtmfGuardMs = 900U;
// Keep this short so the first rotary digit is not lost when users dial
// immediately after lifting the handset.
constexpr uint32_t kIdleHookOffEnterDebounceMs = 80U;
constexpr size_t kDialMaxDigits = 20U;
constexpr uint32_t kDialExactPendingCommitMs = 1200U;
constexpr uint32_t kDtmfCaptureStartDelayMs = 0U;
constexpr uint32_t kDtmfReadPeriodMs = 12U;
constexpr uint32_t kTelephonyIdlePowerDownDelayMs = 2500U;
constexpr uint32_t kTelephonyPowerProbeIntervalMs = 1200U;
constexpr uint32_t kTelephonyPowerProbeWindowMs = 180U;
constexpr uint8_t kDialSourceNone = 0U;
constexpr uint8_t kDialSourceDtmf = 1U;
constexpr uint8_t kDialSourcePulse = 2U;
}

const char* telephonyStateToString(TelephonyState state) {
    switch (state) {
        case TelephonyState::IDLE:
            return "IDLE";
        case TelephonyState::RINGING:
            return "RINGING";
        case TelephonyState::PLAYING_MESSAGE:
            return "PLAYING_MESSAGE";
        case TelephonyState::OFF_HOOK:
            return "OFF_HOOK";
        default:
            return "UNKNOWN";
    }
}

const char* dialMatchStateToString(DialMatchState state) {
    switch (state) {
        case DialMatchState::PREFIX:
            return "PREFIX";
        case DialMatchState::EXACT_PENDING:
            return "EXACT_PENDING";
        case DialMatchState::TRIGGERED:
            return "TRIGGERED";
        case DialMatchState::NONE:
        default:
            return "NONE";
    }
}

TelephonyService::TelephonyService()
    : profile_(BoardProfile::ESP32_A252),
      features_(getFeatureMatrix(BoardProfile::ESP32_A252)),
      slic_(nullptr),
      audio_(nullptr),
      dial_callback_(nullptr),
      dial_match_callback_(nullptr),
      answer_callback_(nullptr),
      dtmf_(8000U, kDtmfFrameSamples),
      state_(TelephonyState::IDLE),
      incoming_ring_(false),
      ring_phase_on_(false),
      ring_cycle_start_ms_(0),
      telephony_powered_(true),
      power_probe_active_(false),
      idle_since_ms_(0),
      next_power_probe_ms_(0),
      power_probe_end_ms_(0),
      capture_active_(false),
      pulse_hook_initialized_(false),
      pulse_last_hook_off_(false),
      pulse_collecting_(false),
      pulse_count_(0),
      last_hook_edge_ms_(0),
      pulse_break_start_ms_(0),
      pulse_make_start_ms_(0),
      idle_hook_off_since_ms_(0),
      last_pulse_ms_(0),
      dtmf_capture_start_ms_(0),
      next_dtmf_read_ms_(0),
      off_hook_enter_ms_(0),
      last_pulse_edge_ms_(0),
      suppress_dial_tone_(false),
      dialing_started_(false),
      dial_lock_until_on_hook_(false),
      dial_source_(kDialSourceNone),
      dial_match_state_(DialMatchState::NONE),
      dial_buffer_(""),
      last_digit_ms_(0),
      dial_exact_pending_since_ms_(0),
      last_dial_error_(""),
      message_path_("/welcome.wav") {}

bool TelephonyService::begin(BoardProfile profile, SlicController& slic, AudioEngine& audio) {
    profile_ = profile;
    features_ = getFeatureMatrix(profile);
    slic_ = &slic;
    audio_ = &audio;
    state_ = TelephonyState::IDLE;
    incoming_ring_ = false;
    ring_phase_on_ = false;
    ring_cycle_start_ms_ = millis();
    telephony_powered_ = true;
    power_probe_active_ = false;
    idle_since_ms_ = ring_cycle_start_ms_;
    next_power_probe_ms_ = ring_cycle_start_ms_ + kTelephonyPowerProbeIntervalMs;
    power_probe_end_ms_ = 0;
    capture_active_ = false;
    pulse_hook_initialized_ = false;
    pulse_last_hook_off_ = false;
    pulse_collecting_ = false;
    pulse_count_ = 0;
    last_hook_edge_ms_ = 0;
    pulse_break_start_ms_ = 0;
    pulse_make_start_ms_ = 0;
    idle_hook_off_since_ms_ = 0;
    last_pulse_ms_ = 0;
    dtmf_capture_start_ms_ = 0;
    next_dtmf_read_ms_ = 0;
    off_hook_enter_ms_ = 0;
    last_pulse_edge_ms_ = 0;
    suppress_dial_tone_ = false;
    dial_tone_suppressed_until_ms_ = 0U;
    dialing_started_ = false;
    dial_lock_until_on_hook_ = false;
    dial_source_ = kDialSourceNone;
    dial_match_state_ = DialMatchState::NONE;
    dial_buffer_ = "";
    last_digit_ms_ = 0;
    dial_exact_pending_since_ms_ = 0;
    last_dial_error_ = "";

    dtmf_.setDigitCallback([this](char digit) {
        onDialDigit(digit, false);
    });

    slic_->setRing(false);
    setTelephonyPower(true);
    setTelephonyPower(false);
    return true;
}

void TelephonyService::setDialCallback(DialCallback cb) {
    dial_callback_ = cb;
}

void TelephonyService::setDialMatchCallback(DialMatchCallback cb) {
    dial_match_callback_ = cb;
}

void TelephonyService::setAnswerCallback(AnswerCallback cb) {
    answer_callback_ = cb;
}

void TelephonyService::triggerIncomingRing() {
    incoming_ring_ = true;
    setTelephonyPower(true);
    power_probe_active_ = false;
    idle_since_ms_ = 0;
}

void TelephonyService::setIncomingRing(bool active) {
    incoming_ring_ = active;
    if (active) {
        setTelephonyPower(true);
        power_probe_active_ = false;
        idle_since_ms_ = 0;
    }
}

void TelephonyService::forceTelephonyPower(bool enabled) {
    setTelephonyPower(enabled);
    power_probe_active_ = false;
    if (enabled) {
        idle_since_ms_ = 0;
    } else {
        idle_since_ms_ = millis();
        next_power_probe_ms_ = idle_since_ms_ + kTelephonyPowerProbeIntervalMs;
    }
}

void TelephonyService::setTelephonyPower(bool enabled) {
    if (slic_ == nullptr) {
        return;
    }
    if (telephony_powered_ == enabled) {
        return;
    }

    if (enabled) {
        slic_->setPowerDown(false);
        slic_->setLineEnabled(true);
    } else {
        if (ring_phase_on_) {
            ring_phase_on_ = false;
            slic_->setRing(false);
        }
        slic_->setLineEnabled(false);
        slic_->setPowerDown(true);
    }

    telephony_powered_ = enabled;
    Serial.printf("[Telephony] slic_power=%s\n", enabled ? "on" : "off");
}

void TelephonyService::applyPowerPolicyPreTick(uint32_t now) {
    if (slic_ == nullptr) {
        return;
    }

    const bool keep_power_for_audio =
        (audio_ != nullptr) && (audio_->isToneRenderingActive() || audio_->isPlaying());
    if (keep_power_for_audio) {
        setTelephonyPower(true);
        power_probe_active_ = false;
        idle_since_ms_ = 0;
        return;
    }

    if (state_ != TelephonyState::IDLE || incoming_ring_) {
        setTelephonyPower(true);
        power_probe_active_ = false;
        idle_since_ms_ = 0;
        return;
    }

    if (telephony_powered_) {
        if (idle_since_ms_ == 0U) {
            idle_since_ms_ = now;
        }
        if (!power_probe_active_ && (now - idle_since_ms_) >= kTelephonyIdlePowerDownDelayMs) {
            setTelephonyPower(false);
            next_power_probe_ms_ = now + kTelephonyPowerProbeIntervalMs;
            power_probe_end_ms_ = 0U;
        }
        return;
    }

    if (now >= next_power_probe_ms_) {
        setTelephonyPower(true);
        power_probe_active_ = true;
        power_probe_end_ms_ = now + kTelephonyPowerProbeWindowMs;
    }
}

void TelephonyService::applyPowerPolicyPostTick(bool hook_off, uint32_t now) {
    if (slic_ == nullptr) {
        return;
    }

    const bool keep_power_for_audio =
        (audio_ != nullptr) && (audio_->isToneRenderingActive() || audio_->isPlaying());
    if (keep_power_for_audio) {
        setTelephonyPower(true);
        power_probe_active_ = false;
        idle_since_ms_ = 0;
        return;
    }

    if (state_ != TelephonyState::IDLE || incoming_ring_ || hook_off) {
        setTelephonyPower(true);
        power_probe_active_ = false;
        idle_since_ms_ = 0;
        return;
    }

    if (telephony_powered_ && idle_since_ms_ == 0U) {
        idle_since_ms_ = now;
    }

    if (power_probe_active_ && telephony_powered_ && now >= power_probe_end_ms_) {
        setTelephonyPower(false);
        power_probe_active_ = false;
        next_power_probe_ms_ = now + kTelephonyPowerProbeIntervalMs;
    }
}

void TelephonyService::onDialDigit(char digit, bool from_pulse) {
    if (digit < '0' || digit > '9') {
        return;
    }
    if (dial_lock_until_on_hook_) {
        return;
    }

    const uint32_t now = millis();
    if (!from_pulse) {
        // Rotary pulse has priority: suppress DTMF captures while pulse edges are active/recent.
        const bool pulse_recent =
            pulse_collecting_ || pulse_count_ > 0U ||
            (last_pulse_edge_ms_ != 0U && (now - last_pulse_edge_ms_) < kPulseDtmfGuardMs);
        if (pulse_recent) {
            return;
        }
    }

    const uint8_t source = from_pulse ? kDialSourcePulse : kDialSourceDtmf;
    if (dial_source_ == kDialSourceNone) {
        dial_source_ = source;
    } else if (dial_source_ != source) {
        // Allow pulse to override an early DTMF false-start (typically tone bleed).
        if (from_pulse && dial_source_ == kDialSourceDtmf && dial_buffer_.length() <= 1U) {
            dial_buffer_ = "";
            last_digit_ms_ = 0;
            dial_source_ = source;
        } else {
            // Keep strict ordering by ignoring mixed-source digits in the same session.
            return;
        }
    }

    if (audio_ != nullptr && dial_buffer_.isEmpty() && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    dialing_started_ = true;
    if (dial_buffer_.length() >= kDialMaxDigits) {
        dial_buffer_ = "";
        dial_match_state_ = DialMatchState::NONE;
        dial_exact_pending_since_ms_ = 0U;
        dial_source_ = kDialSourceNone;
    }

    dial_buffer_ += digit;
    last_digit_ms_ = now;
    Serial.printf("[Telephony] digit=%c source=%s buffer=%s\n",
                  digit,
                  from_pulse ? "pulse" : "dtmf",
                  dial_buffer_.c_str());
    evaluateDialBuffer(now, from_pulse ? "digit_pulse" : "digit_dtmf");
}

void TelephonyService::updatePulseDecode(bool hook_off, uint32_t now) {
    if (!pulse_hook_initialized_) {
        pulse_hook_initialized_ = true;
        pulse_last_hook_off_ = hook_off;
        last_hook_edge_ms_ = now;
        pulse_break_start_ms_ = 0U;
        pulse_make_start_ms_ = now;
        return;
    }

    if (hook_off == pulse_last_hook_off_) {
        return;
    }

    if ((now - last_pulse_edge_ms_) < kPulseEdgeDebounceMs) {
        return;
    }
    last_pulse_edge_ms_ = now;

    // Any valid hook edge during OFF_HOOK indicates dialing activity start.
    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    dialing_started_ = true;

    if (pulse_last_hook_off_ && !hook_off) {
        // Make -> Break
        const uint32_t make_ms = (pulse_make_start_ms_ == 0U) ? 0U : (now - pulse_make_start_ms_);
        if (make_ms >= kPulseMakeMinMs) {
            if (!pulse_collecting_) {
                pulse_collecting_ = true;
                pulse_count_ = 0;
                // Stop dial tone as soon as rotary dialing starts (first pulse edge),
                // not only after the first full decoded digit.
                if (audio_ != nullptr && audio_->isToneRenderingActive()) {
                    audio_->stopTone();
                }
            }
            pulse_break_start_ms_ = now;
        }
    } else if (!pulse_last_hook_off_ && hook_off) {
        // Break -> Make
        pulse_make_start_ms_ = now;
        const uint32_t break_ms = (pulse_break_start_ms_ == 0U) ? 0U : (now - pulse_break_start_ms_);
        if (pulse_collecting_ && pulse_count_ < 20U && break_ms >= kPulseBreakMinMs && break_ms <= kPulseBreakMaxMs) {
            ++pulse_count_;
            last_pulse_ms_ = now;
            Serial.printf("[Telephony] pulse_count=%u break_ms=%u\n", pulse_count_, break_ms);
        }
    }

    pulse_last_hook_off_ = hook_off;
    last_hook_edge_ms_ = now;
}

void TelephonyService::commitDialBuffer(const char* reason) {
    if (dial_buffer_.isEmpty()) {
        return;
    }

    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }

    const String number = dial_buffer_;
    const bool from_pulse = (dial_source_ == kDialSourcePulse);
    const bool ok = dial_callback_ ? dial_callback_(number, from_pulse) : false;
    if (ok) {
        // Freeze dialing once a hotline route is launched; unlock only on hangup.
        dial_lock_until_on_hook_ = true;
    }
    last_dial_error_ = ok ? "" : "dial_failed";
    dial_match_state_ = DialMatchState::TRIGGERED;
    Serial.printf("[Telephony] dial_trigger reason=%s number=%s ok=%s\n",
                  reason != nullptr ? reason : "unknown",
                  number.c_str(),
                  ok ? "true" : "false");

    dial_buffer_ = "";
    last_digit_ms_ = 0;
    dial_exact_pending_since_ms_ = 0U;
    dial_source_ = kDialSourceNone;
}

void TelephonyService::evaluateDialBuffer(uint32_t now, const char* reason) {
    if (dial_buffer_.isEmpty()) {
        dial_match_state_ = DialMatchState::NONE;
        dial_exact_pending_since_ms_ = 0U;
        return;
    }

    if (!dial_match_callback_) {
        dial_match_state_ = DialMatchState::PREFIX;
        if (dial_buffer_.length() >= 10U) {
            commitDialBuffer(reason != nullptr ? reason : "legacy_len10");
        }
        return;
    }

    const DialRouteMatch match = dial_match_callback_(dial_buffer_);
    switch (match) {
        case DialRouteMatch::NONE:
            Serial.printf("[Telephony] dial_no_match buffer=%s reset\n", dial_buffer_.c_str());
            dial_buffer_ = "";
            last_digit_ms_ = 0U;
            dial_exact_pending_since_ms_ = 0U;
            dial_match_state_ = DialMatchState::NONE;
            dial_source_ = kDialSourceNone;
            dialing_started_ = false;
            return;
        case DialRouteMatch::PREFIX:
            dial_match_state_ = DialMatchState::PREFIX;
            dial_exact_pending_since_ms_ = 0U;
            return;
        case DialRouteMatch::EXACT:
            commitDialBuffer(reason != nullptr ? reason : "exact");
            return;
        case DialRouteMatch::EXACT_AND_PREFIX:
            dial_match_state_ = DialMatchState::EXACT_PENDING;
            if (dial_exact_pending_since_ms_ == 0U) {
                dial_exact_pending_since_ms_ = now;
            }
            return;
        default:
            return;
    }
}

void TelephonyService::clearDialSession() {
    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    if (audio_ != nullptr && capture_active_) {
        audio_->releaseCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
    }
    capture_active_ = false;
    dtmf_capture_start_ms_ = 0;
    next_dtmf_read_ms_ = 0;
    off_hook_enter_ms_ = 0;
    pulse_hook_initialized_ = false;
    pulse_collecting_ = false;
    pulse_count_ = 0;
    last_hook_edge_ms_ = 0;
    pulse_break_start_ms_ = 0;
    pulse_make_start_ms_ = 0;
    last_pulse_ms_ = 0;
    last_pulse_edge_ms_ = 0;
    dial_source_ = kDialSourceNone;
    dial_match_state_ = DialMatchState::NONE;
    dialing_started_ = false;
    dial_lock_until_on_hook_ = false;
    suppress_dial_tone_ = false;
    dial_tone_suppressed_until_ms_ = 0U;
    dial_buffer_ = "";
    last_digit_ms_ = 0;
    dial_exact_pending_since_ms_ = 0U;
}

void TelephonyService::suppressDialToneForMs(uint32_t duration_ms) {
    if (duration_ms == 0U) {
        dial_tone_suppressed_until_ms_ = 0U;
        return;
    }
    dial_tone_suppressed_until_ms_ = millis() + duration_ms;
}

void TelephonyService::clearDialToneSuppression() {
    dial_tone_suppressed_until_ms_ = 0U;
}

bool TelephonyService::isDialToneSuppressed(uint32_t now_ms) const {
    return dial_tone_suppressed_until_ms_ != 0U && now_ms < dial_tone_suppressed_until_ms_;
}

void TelephonyService::tick() {
    if (slic_ == nullptr || audio_ == nullptr) {
        return;
    }

    const uint32_t now = millis();
    applyPowerPolicyPreTick(now);
    slic_->tick();

    const bool hook_off = telephony_powered_ ? slic_->isHookOff() : false;
    const bool tone_suppressed = suppress_dial_tone_ || isDialToneSuppressed(now);
    const TelephonyState prev_state = state_;

    switch (state_) {
        case TelephonyState::IDLE:
            if (incoming_ring_ && !hook_off) {
                ring_cycle_start_ms_ = millis();
                ring_phase_on_ = true;
                slic_->setRing(true);
                state_ = TelephonyState::RINGING;
                idle_hook_off_since_ms_ = 0;
            } else if (hook_off) {
                if (idle_hook_off_since_ms_ == 0U) {
                    idle_hook_off_since_ms_ = now;
                } else if ((now - idle_hook_off_since_ms_) >= kIdleHookOffEnterDebounceMs) {
                    state_ = TelephonyState::OFF_HOOK;
                    idle_hook_off_since_ms_ = 0;
                }
            } else {
                idle_hook_off_since_ms_ = 0;
            }
            break;

        case TelephonyState::RINGING: {
            if (hook_off) {
                incoming_ring_ = false;
                ring_phase_on_ = false;
                slic_->setRing(false);
                const bool answered = answer_callback_ ? answer_callback_() : false;
                // Keep dial tone muted while transitioning from incoming ring to call answer.
                suppress_dial_tone_ = true;
                suppressDialToneForMs(3000U);
                last_dial_error_ = answered ? "" : "answer_failed";
                state_ = TelephonyState::OFF_HOOK;
                break;
            }

            if (!incoming_ring_) {
                ring_phase_on_ = false;
                slic_->setRing(false);
                state_ = TelephonyState::IDLE;
                break;
            }

            const uint32_t elapsed = (millis() - ring_cycle_start_ms_) % 5000U;
            const bool should_ring = elapsed < 1000U;
            if (should_ring != ring_phase_on_) {
                ring_phase_on_ = should_ring;
                slic_->setRing(ring_phase_on_);
            }
            break;
        }

        case TelephonyState::PLAYING_MESSAGE:
            if (!audio_->isPlaying()) {
                state_ = hook_off ? TelephonyState::OFF_HOOK : TelephonyState::IDLE;
            }
            break;

        case TelephonyState::OFF_HOOK:
            // While dial is locked, pulse decoder is disabled; keep hangup edge timing
            // in sync without touching normal pulse decoding flow.
            if (dial_lock_until_on_hook_ && hook_off != pulse_last_hook_off_) {
                last_hook_edge_ms_ = now;
                pulse_last_hook_off_ = hook_off;
            }

            if (!dial_lock_until_on_hook_ && (now - off_hook_enter_ms_) >= kHookStabilizeMs) {
                updatePulseDecode(hook_off, now);
            }

            if (!hook_off) {
                const bool hangup_confirmed = (now - last_hook_edge_ms_) >= kHookHangupMs;
                if (hangup_confirmed) {
                    if (audio_ != nullptr && audio_->isToneRenderingActive()) {
                        audio_->stopTone();
                    }
                    if (audio_ != nullptr && audio_->isPlaying()) {
                        audio_->stopPlayback();
                    }
                    if (audio_ != nullptr && capture_active_) {
                        audio_->releaseCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
                        capture_active_ = false;
                    }
                    incoming_ring_ = false;
                    state_ = TelephonyState::IDLE;
                }
                break;
            }

            if (dial_lock_until_on_hook_) {
                if (audio_ != nullptr && capture_active_) {
                    audio_->releaseCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
                    capture_active_ = false;
                }
                if (!dial_buffer_.isEmpty() || dial_source_ != kDialSourceNone || dial_match_state_ != DialMatchState::NONE) {
                    dial_buffer_ = "";
                    last_digit_ms_ = 0U;
                    dial_source_ = kDialSourceNone;
                    dial_match_state_ = DialMatchState::NONE;
                    dial_exact_pending_since_ms_ = 0U;
                }
                break;
            }

            if (pulse_collecting_ && pulse_count_ > 0U && (now - last_pulse_ms_) >= kPulseInterDigitGapMs) {
                const uint8_t count = pulse_count_;
                pulse_collecting_ = false;
                pulse_count_ = 0;
                const char digit = (count == 10U) ? '0' : ((count >= 1U && count <= 9U) ? static_cast<char>('0' + count)
                                                                                           : '\0');
                if (digit != '\0') {
                    onDialDigit(digit, true);
                }
            }

            if (dial_match_state_ == DialMatchState::EXACT_PENDING &&
                dial_exact_pending_since_ms_ != 0U &&
                (now - last_digit_ms_) >= kDialExactPendingCommitMs) {
                commitDialBuffer("exact_pending_timeout");
            }

            if (!capture_active_ && now >= dtmf_capture_start_ms_) {
                capture_active_ = audio_->requestCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
            }
            if (capture_active_ && now >= next_dtmf_read_ms_) {
                int16_t frame[kDtmfFrameSamples] = {0};
                const size_t samples_read = audio_->readCaptureFrameNonBlocking(frame, kDtmfFrameSamples);
                if (samples_read > 0U) {
                    dtmf_.feedAudioSamples(frame, samples_read);
                }
                next_dtmf_read_ms_ = now + kDtmfReadPeriodMs;
            }

            if (suppress_dial_tone_ && audio_->isDialToneActive()) {
                audio_->stopDialTone();
            }

            const bool pulse_dial_in_progress =
                pulse_collecting_ || pulse_count_ > 0U ||
                (last_pulse_edge_ms_ != 0U && (now - last_pulse_edge_ms_) < kPulseInterDigitGapMs);
            if (!tone_suppressed && !dialing_started_ && dial_buffer_.isEmpty() && !audio_->isDialToneActive() &&
                !pulse_dial_in_progress) {
                audio_->startDialTone();
            }

            if (!dial_buffer_.isEmpty() && (now - last_digit_ms_) >= 10000U) {
                // Drop stale partial numbers instead of dialing an incomplete value.
                dial_buffer_ = "";
                last_digit_ms_ = 0;
                dial_match_state_ = DialMatchState::NONE;
                dial_exact_pending_since_ms_ = 0U;
                dial_source_ = kDialSourceNone;
            }
            break;
    }

    if (prev_state != state_) {
        if (state_ == TelephonyState::OFF_HOOK) {
            off_hook_enter_ms_ = now;
            pulse_hook_initialized_ = false;
            pulse_collecting_ = false;
            pulse_count_ = 0;
            last_hook_edge_ms_ = now;
            pulse_last_hook_off_ = hook_off;
            pulse_break_start_ms_ = 0U;
            pulse_make_start_ms_ = now;
            last_pulse_ms_ = 0;
            last_pulse_edge_ms_ = 0;
            dial_source_ = kDialSourceNone;
            dial_match_state_ = DialMatchState::NONE;
            dialing_started_ = false;
            dial_lock_until_on_hook_ = false;
            dial_buffer_ = "";
            last_digit_ms_ = 0;
            dial_exact_pending_since_ms_ = 0U;
            dtmf_capture_start_ms_ = now + kDtmfCaptureStartDelayMs;
            next_dtmf_read_ms_ = now;
            if (audio_ != nullptr && !tone_suppressed) {
                audio_->startDialTone();
            }
        }

        if (prev_state == TelephonyState::OFF_HOOK && state_ != TelephonyState::OFF_HOOK) {
            clearDialSession();
        }
    }

    applyPowerPolicyPostTick(hook_off, now);
}

TelephonyState TelephonyService::state() const {
    return state_;
}

bool TelephonyService::isTelephonyPowered() const {
    return telephony_powered_;
}

bool TelephonyService::isPowerProbeActive() const {
    return power_probe_active_;
}

const String& TelephonyService::dialBuffer() const {
    return dial_buffer_;
}

const char* TelephonyService::dialSource() const {
    switch (dial_source_) {
        case kDialSourceDtmf:
            return "DTMF";
        case kDialSourcePulse:
            return "PULSE";
        case kDialSourceNone:
        default:
            return "NONE";
    }
}

DialMatchState TelephonyService::dialMatchState() const {
    return dial_match_state_;
}

bool TelephonyService::dialingStarted() const {
    return dialing_started_;
}
