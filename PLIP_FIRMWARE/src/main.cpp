// PLIP firmware — bringup skeleton on ESP32-A1S Audio Kit (ES8388 codec).
//
// Three FreeRTOS tasks share state via a queue:
//   - phone_task:    drives the ring + handles off-hook GPIO interrupt
//   - audio_task:    streams MP3 from SD or PCM from network to ES8388
//   - network_task:  WiFi + REST server (POST /ring, POST /play, /status)
//
// State transitions live in src/phone_state.cpp; they fire commands onto
// the audio queue. Network commands are translated to phone-state events.
//
// Pin assignments come from platformio.ini build flags so we can swap the
// dev kit for the Si3210 PCB without touching source.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern void start_phone_task();
extern void start_audio_task();
extern void start_network_task();

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("[PLIP] boot — bringup skeleton (ES8388 dev kit)"));

  // TODO(bringup): I2C init + ES8388 codec init before audio task starts.
  // TODO(bringup): SD.begin() before audio task.
  // TODO(bringup): WiFi.begin() inside network_task.

  start_phone_task();
  start_audio_task();
  start_network_task();
}

void loop() {
  // All work runs in FreeRTOS tasks. Idle loop is intentionally empty.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
