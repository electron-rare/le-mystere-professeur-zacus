#ifndef LECTURE_AUDIO_MANAGER_H
#define LECTURE_AUDIO_MANAGER_H

#include "audio/AudioManager.h"
#include "core/PlatformProfile.h"

class LectureAudioManager {
public:
    LectureAudioManager();
    bool begin(BoardProfile profile);
    bool playFile(const char* filename);
    void controlPlayback();
    bool isPlaying() const;

private:
    AudioManager audio_;
    bool initialized_;
};

#endif  // LECTURE_AUDIO_MANAGER_H
