#ifndef TELEPHONY_SERVICE_H
#define TELEPHONY_SERVICE_H

#include <functional>

#include "audio/AudioEngine.h"
#include "core/PlatformProfile.h"
#include "slic/SlicController.h"
#include "telephony/DtmfDecoder.h"

enum class TelephonyState : uint8_t {
    IDLE = 0,
    RINGING,
    PLAYING_MESSAGE,
    OFF_HOOK
};

enum class DialRouteMatch : uint8_t {
    NONE = 0,
    PREFIX,
    EXACT,
    EXACT_AND_PREFIX,
};

enum class DialMatchState : uint8_t {
    NONE = 0,
    PREFIX,
    EXACT_PENDING,
    TRIGGERED,
};

const char* telephonyStateToString(TelephonyState state);
const char* dialMatchStateToString(DialMatchState state);

class TelephonyService {
public:
    using DialCallback = std::function<bool(const String&, bool from_pulse)>;
    using DialMatchCallback = std::function<DialRouteMatch(const String&)>;
    using AnswerCallback = std::function<bool()>;

    TelephonyService();
    bool begin(BoardProfile profile, SlicController& slic, AudioEngine& audio);
    void setDialCallback(DialCallback cb);
    void setDialMatchCallback(DialMatchCallback cb);
    void setAnswerCallback(AnswerCallback cb);
    void triggerIncomingRing();
    void setIncomingRing(bool active);
    void forceTelephonyPower(bool enabled);
    void tick();
    TelephonyState state() const;
    bool isTelephonyPowered() const;
    bool isPowerProbeActive() const;
    void suppressDialToneForMs(uint32_t duration_ms);
    void clearDialToneSuppression();
    bool isDialToneSuppressed(uint32_t now_ms) const;
    const String& dialBuffer() const;
    const char* dialSource() const;
    DialMatchState dialMatchState() const;
    bool dialingStarted() const;

private:
    void setTelephonyPower(bool enabled);
    void applyPowerPolicyPreTick(uint32_t now);
    void applyPowerPolicyPostTick(bool hook_off, uint32_t now);
    void onDialDigit(char digit, bool from_pulse);
    void updatePulseDecode(bool hook_off, uint32_t now);
    void evaluateDialBuffer(uint32_t now, const char* reason);
    void commitDialBuffer(const char* reason);
    void clearDialSession();

    BoardProfile profile_;
    FeatureMatrix features_;
    SlicController* slic_;
    AudioEngine* audio_;
    DialCallback dial_callback_;
    DialMatchCallback dial_match_callback_;
    AnswerCallback answer_callback_;
    DtmfDecoder dtmf_;
    TelephonyState state_;
    bool incoming_ring_;
    bool ring_phase_on_;
    uint32_t ring_cycle_start_ms_;
    bool telephony_powered_;
    bool power_probe_active_;
    uint32_t idle_since_ms_;
    uint32_t next_power_probe_ms_;
    uint32_t power_probe_end_ms_;
    bool capture_active_;
    bool pulse_hook_initialized_;
    bool pulse_last_hook_off_;
    bool pulse_collecting_;
    uint8_t pulse_count_;
    uint32_t last_hook_edge_ms_;
    uint32_t pulse_break_start_ms_;
    uint32_t pulse_make_start_ms_;
    uint32_t idle_hook_off_since_ms_;
    uint32_t last_pulse_ms_;
    uint32_t dtmf_capture_start_ms_;
    uint32_t next_dtmf_read_ms_;
    uint32_t off_hook_enter_ms_;
    uint32_t last_pulse_edge_ms_;
    bool suppress_dial_tone_;
    uint32_t dial_tone_suppressed_until_ms_ = 0U;
    bool dialing_started_;
    bool dial_lock_until_on_hook_;
    uint8_t dial_source_;
    DialMatchState dial_match_state_;
    String dial_buffer_;
    uint32_t last_digit_ms_;
    uint32_t dial_exact_pending_since_ms_;
    String last_dial_error_;
    const char* message_path_;
};

#endif  // TELEPHONY_SERVICE_H
