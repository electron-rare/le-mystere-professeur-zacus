#include "audio/AudioManager.h"
#include "core/AgentSupervisor.h"
#include <Arduino.h>

AudioManager::AudioManager() : initialized_(false) {}

void notifyAudio(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("audio", status);
}

bool AudioManager::begin(const AudioConfig& config) {
    initialized_ = engine_.begin(config);
    notifyAudio(initialized_ ? "initialized" : "init_failed", initialized_ ? "" : "init error");
    return initialized_;
}

bool AudioManager::playFile(const char* path) {
    bool ok = initialized_ && engine_.playFile(path);
    notifyAudio(ok ? "playing" : "play_failed", ok ? "" : "play error");
    return ok;
}

bool AudioManager::startCapture() {
    bool ok = initialized_ && engine_.startCapture();
    notifyAudio(ok ? "capture" : "capture_failed", ok ? "" : "capture error");
    return ok;
}

size_t AudioManager::readCaptureFrame(int16_t* dst, size_t samples) {
    if (!initialized_) {
        return 0;
    }
    return engine_.readCaptureFrame(dst, samples);
}

void AudioManager::stopCapture() {
    if (!initialized_) {
        return;
    }
    engine_.stopCapture();
    notifyAudio("stopped");
}

bool AudioManager::supportsFullDuplex() const {
    return initialized_ && engine_.supportsFullDuplex();
}

bool AudioManager::isPlaying() const {
    return initialized_ && engine_.isPlaying();
}

AudioRuntimeMetrics AudioManager::metrics() const {
    return engine_.metrics();
}

void AudioManager::resetMetrics() {
    engine_.resetMetrics();
}

void AudioManager::tick() {
    if (!initialized_) {
        return;
    }
    engine_.tick();
}
