// phone_task — ring control + off-hook GPIO interrupt.
//
// On the ESP32-A1S dev kit the SLIC is replaced by an ES8388 audio path
// and a button (BOOT/KEY1) acting as a stand-in for the off-hook signal.
// On the Si3210 PCB the same task drives ring control over SPI and
// listens to Si3210 INT for off-hook / on-hook transitions.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

extern QueueHandle_t audio_command_queue();  // audio_task.cpp

namespace {

volatile bool g_off_hook_pending = false;

void IRAM_ATTR on_off_hook_isr() {
  g_off_hook_pending = true;
}

void phone_task(void*) {
  pinMode(OFF_HOOK_GPIO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(OFF_HOOK_GPIO), on_off_hook_isr, FALLING);
  Serial.println(F("[phone] task ready, watching off-hook"));

  for (;;) {
    if (g_off_hook_pending) {
      g_off_hook_pending = false;
      Serial.println(F("[phone] off-hook detected"));
      // TODO(bringup): debounce in software; SLIC INT will already debounce
      //                in hardware but the dev kit button needs ~30 ms.
      // TODO(bringup): notify network_task so the Zacus master sees the event.
      // TODO(bringup): tell audio_task to start the appropriate audio cue.
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void start_phone_task() {
  xTaskCreatePinnedToCore(phone_task, "phone", 4096, nullptr, 5, nullptr, 1);
}
