#include "lecture_audio/LectureAudioManager.h"

LectureAudioManager::LectureAudioManager() : initialized_(false) {}

bool LectureAudioManager::begin(BoardProfile profile) {
    initialized_ = audio_.begin(defaultAudioConfigForProfile(profile));
    return initialized_;
}

bool LectureAudioManager::playFile(const char* filename) {
    if (!initialized_) {
        return false;
    }
    return audio_.playFile(filename);
}

void LectureAudioManager::controlPlayback() {
    if (!initialized_) {
        return;
    }
    audio_.tick();
}

bool LectureAudioManager::isPlaying() const {
    if (!initialized_) {
        return false;
    }
    return audio_.isPlaying();
}
