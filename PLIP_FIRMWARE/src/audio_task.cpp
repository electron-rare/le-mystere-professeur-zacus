// audio_task — drains an audio command queue and routes the source to
// ES8388 (dev kit) or Si3210 PCM (PCB).
//
// Sources:
//   - SD MP3 via ESP32-audioI2S (file://...)
//   - HTTP stream from Tower:8001 Piper TTS (http://... .wav/.mp3)
//   - Future: A2DP sink (BT classic) — out of P3=b MVP scope.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

QueueHandle_t g_queue = nullptr;

}  // namespace

QueueHandle_t audio_command_queue() {
  return g_queue;
}

namespace {

struct AudioCommand {
  enum Kind { Stop, PlaySdMp3, PlayHttpStream };
  Kind kind;
  char path[192];
};

void audio_task(void*) {
  Serial.println(F("[audio] task ready, awaiting commands"));
  AudioCommand cmd;
  for (;;) {
    if (xQueueReceive(g_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      switch (cmd.kind) {
        case AudioCommand::Stop:
          Serial.println(F("[audio] stop"));
          // TODO(bringup): audio.stopSong();
          break;
        case AudioCommand::PlaySdMp3:
          Serial.printf("[audio] sd: %s\n", cmd.path);
          // TODO(bringup): audio.connecttoSD(cmd.path);
          break;
        case AudioCommand::PlayHttpStream:
          Serial.printf("[audio] http: %s\n", cmd.path);
          // TODO(bringup): audio.connecttohost(cmd.path);
          break;
      }
    }
    // TODO(bringup): audio.loop();
  }
}

}  // namespace

void start_audio_task() {
  g_queue = xQueueCreate(8, sizeof(AudioCommand));
  xTaskCreatePinnedToCore(audio_task, "audio", 8192, nullptr, 4, nullptr, 0);
}
