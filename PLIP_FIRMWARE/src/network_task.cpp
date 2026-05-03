// network_task — WiFi station + REST endpoint.
//
// REST contract (consumed by the Zacus master ESP32):
//   POST /ring     { "duration_ms": 4000 }    -> trigger SLIC ring (or beep on dev kit)
//   POST /play     { "source": "sd:/intro.mp3" | "http://tower:8001/..." }
//   POST /stop                                  -> stop current playback
//   GET  /status                                -> { "off_hook": bool, "playing": bool }
//
// WiFi credentials come from NVS (provisioned via the desktop NvsConfigurator).
// The dev kit can fall back to compile-time defaults via build_flags for
// quick bringup; remove them once the desktop NVS flow is wired.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

void network_task(void*) {
  Serial.println(F("[net] task ready"));
  // TODO(bringup): WiFi.begin(ssid, password) using NVS-provisioned creds.
  // TODO(bringup): wait for IP, advertise via mDNS as plip.local.
  // TODO(bringup): start ESPAsyncWebServer with the 4 handlers above.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

}  // namespace

void start_network_task() {
  xTaskCreatePinnedToCore(network_task, "net", 8192, nullptr, 3, nullptr, 0);
}
