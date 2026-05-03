// phone_task — ring control + off-hook GPIO interrupt.
//
// On the ESP32-A1S dev kit the SLIC is replaced by an ES8388 audio path
// and a button (BOOT/KEY1) acting as a stand-in for the off-hook signal.
// On the Si3210 PCB the same task drives ring control over SPI and
// listens to Si3210 INT for off-hook / on-hook transitions.
//
// Wire convention (active-low with INPUT_PULLUP):
//   level LOW  = handset off-hook (pickup)
//   level HIGH = handset on-hook  (hangup / idle)
// Each transition is forwarded to the Zacus master via zacus_hook_client.
// The HTTP POST runs in its own worker task so this loop and the ISR stay
// fast.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "zacus_hook_client.h"

extern QueueHandle_t audio_command_queue();  // audio_task.cpp

namespace {

// Software debounce window — the ESP32-A1S BOOT button is mechanical and
// bounces ~10–20 ms. The Si3210 INT is hardware-debounced, so we keep a
// modest window that covers both.
constexpr uint32_t kDebounceMs = 30;

volatile bool g_edge_pending = false;

void IRAM_ATTR on_hook_change_isr() {
  g_edge_pending = true;
}

void phone_task(void*) {
  pinMode(OFF_HOOK_GPIO, INPUT_PULLUP);
  // CHANGE so we observe both pickup (FALLING) and hangup (RISING).
  attachInterrupt(digitalPinToInterrupt(OFF_HOOK_GPIO), on_hook_change_isr, CHANGE);

  // Read initial level so the first transition is a real edge, not a
  // boot-time spurious event. Report it to the master so its state machine
  // is in sync from the start.
  int last_level = digitalRead(OFF_HOOK_GPIO);
  zacus_hook_client_report(last_level == LOW ? "off" : "on", "boot");
  Serial.printf("[phone] task ready, watching off-hook (level=%d)\n", last_level);

  for (;;) {
    if (g_edge_pending) {
      g_edge_pending = false;
      vTaskDelay(pdMS_TO_TICKS(kDebounceMs));
      int level = digitalRead(OFF_HOOK_GPIO);
      if (level != last_level) {
        last_level = level;
        if (level == LOW) {
          Serial.println(F("[phone] off-hook (pickup) detected"));
          zacus_hook_client_report("off", "pickup");
          // TODO(bringup): tell audio_task to start the appropriate audio cue.
        } else {
          Serial.println(F("[phone] on-hook (hangup) detected"));
          zacus_hook_client_report("on", "hangup");
          // TODO(bringup): tell audio_task to stop any active playback.
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void start_phone_task() {
  xTaskCreatePinnedToCore(phone_task, "phone", 4096, nullptr, 5, nullptr, 1);
}
