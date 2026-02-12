#include "screen_link.h"

ScreenLink::ScreenLink(HardwareSerial& serial,
                       uint8_t txPin,
                       uint32_t baud,
                       uint16_t updatePeriodMs)
    : serial_(serial),
      txPin_(txPin),
      baud_(baud),
      updatePeriodMs_(updatePeriodMs) {}

void ScreenLink::begin() {
  serial_.begin(baud_, SERIAL_8N1, -1, txPin_);
}

void ScreenLink::update(bool laDetected,
                        bool mp3Playing,
                        bool sdReady,
                        bool mp3Mode,
                        bool uLockMode,
                        bool uLockListening,
                        bool uSonFunctional,
                        uint8_t key,
                        uint16_t track,
                        uint16_t trackCount,
                        uint8_t volumePercent,
                        uint8_t micLevelPercent,
                        int8_t tuningOffset,
                        uint8_t tuningConfidence,
                        bool micScopeEnabled,
                        uint8_t unlockHoldPercent,
                        uint32_t nowMs) {
  const bool changed = !hasState_ || laDetected != lastLa_ || mp3Playing != lastMp3_ ||
                       sdReady != lastSd_ || mp3Mode != lastMp3Mode_ || key != lastKey_ ||
                       track != lastTrack_ || trackCount != lastTrackCount_ ||
                       volumePercent != lastVolumePercent_ ||
                       micLevelPercent != lastMicLevelPercent_ ||
                       uLockMode != lastULockMode_ || uLockListening != lastULockListening_ ||
                       uSonFunctional != lastUSonFunctional_ ||
                       tuningOffset != lastTuningOffset_ ||
                       tuningConfidence != lastTuningConfidence_ ||
                       micScopeEnabled != lastMicScopeEnabled_ ||
                       unlockHoldPercent != lastUnlockHoldPercent_;
  const bool due = (nowMs - lastTxMs_) >= updatePeriodMs_;
  if (!changed && !due) {
    return;
  }

  serial_.printf("STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
                 laDetected ? 1U : 0U,
                 mp3Playing ? 1U : 0U,
                 sdReady ? 1U : 0U,
                 static_cast<unsigned long>(nowMs),
                 static_cast<unsigned int>(key),
                 mp3Mode ? 1U : 0U,
                 static_cast<unsigned int>(track),
                 static_cast<unsigned int>(trackCount),
                 static_cast<unsigned int>(volumePercent),
                 uLockMode ? 1U : 0U,
                 uSonFunctional ? 1U : 0U,
                 static_cast<int>(tuningOffset),
                 static_cast<unsigned int>(tuningConfidence),
                 uLockListening ? 1U : 0U,
                 static_cast<unsigned int>(micLevelPercent),
                 micScopeEnabled ? 1U : 0U,
                 static_cast<unsigned int>(unlockHoldPercent));

  hasState_ = true;
  lastLa_ = laDetected;
  lastMp3_ = mp3Playing;
  lastSd_ = sdReady;
  lastMp3Mode_ = mp3Mode;
  lastULockMode_ = uLockMode;
  lastULockListening_ = uLockListening;
  lastUSonFunctional_ = uSonFunctional;
  lastKey_ = key;
  lastTrack_ = track;
  lastTrackCount_ = trackCount;
  lastVolumePercent_ = volumePercent;
  lastMicLevelPercent_ = micLevelPercent;
  lastTuningOffset_ = tuningOffset;
  lastTuningConfidence_ = tuningConfidence;
  lastMicScopeEnabled_ = micScopeEnabled;
  lastUnlockHoldPercent_ = unlockHoldPercent;
  lastTxMs_ = nowMs;
}
