// audio_manager.cpp - Gestion audio pour firmware all-in-one
#include "audio_manager.h"


#include <Arduino.h>
#include <LittleFS.h>

static bool playing = false;

void AudioManager::begin() {
  // Ex: initialiser I2S ou PWM (placeholder)
  Serial.println("[AUDIO] Init audio (I2S/PWM)");
}

void AudioManager::play(const char* filename) {
  if (!LittleFS.exists(filename)) {
    Serial.print("[AUDIO] Fichier absent: "); Serial.println(filename);
    playing = false;
    return;
  }
  Serial.print("[AUDIO] Lecture: "); Serial.println(filename);
  // TODO: lecture réelle (I2S, PWM, etc.)
  playing = true;
}

void AudioManager::stop() {
  Serial.println("[AUDIO] Stop");
  playing = false;
}

bool AudioManager::isPlaying() const {
  return playing;
}
