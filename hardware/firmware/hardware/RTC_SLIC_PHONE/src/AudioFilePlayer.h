#ifndef AUDIO_FILE_PLAYER_H
#define AUDIO_FILE_PLAYER_H

#include <Arduino.h>
#include <SD.h>

class AudioFilePlayer {
public:
    AudioFilePlayer();
    bool begin();
    bool play(const char* filename);
    void loop();
    void stop();
    bool isPlaying() const;

private:
    bool sd_ready_;
    bool playing_;
    uint32_t play_until_ms_;
    String current_file_;
};

#endif  // AUDIO_FILE_PLAYER_H
