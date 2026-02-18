// audio_manager.h - Interface gestion audio
#pragma once

class AudioManager {
 public:
  void begin();
  void play(const char* filename);
  void stop();
  bool isPlaying() const;
};
