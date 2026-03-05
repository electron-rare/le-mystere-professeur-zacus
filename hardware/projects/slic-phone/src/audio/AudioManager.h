#ifndef AUDIO_AUDIO_MANAGER_H
#define AUDIO_AUDIO_MANAGER_H

#include "audio/AudioEngine.h"

class AudioManager {
public:
    AudioManager();
    bool begin(const AudioConfig& config);
    bool playFile(const char* path);
    bool startCapture();
    size_t readCaptureFrame(int16_t* dst, size_t samples);
    void stopCapture();
    bool supportsFullDuplex() const;
    bool isPlaying() const;
    AudioRuntimeMetrics metrics() const;
    void resetMetrics();
    void tick();

private:
    AudioEngine engine_;
    bool initialized_;
};

#endif  // AUDIO_AUDIO_MANAGER_H
