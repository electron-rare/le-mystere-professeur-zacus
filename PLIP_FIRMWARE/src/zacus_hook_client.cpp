// zacus_hook_client — implementation.
//
// Posts handset state changes to the Zacus master REST endpoint without
// ever blocking the caller (phone task / hook ISR). One worker task drains
// a small FreeRTOS queue and performs the HTTP request with esp_http_client
// semantics via Arduino's HTTPClient.
//
// Failure policy:
//   - timeout 3 s
//   - one retry at +250 ms on non-2xx / transport error
//   - second failure logs and drops; caller already moved on
//
// Wi-Fi: we DO NOT initiate the WiFi connection here. network_task owns
// it. We just wait (with a short timeout) for WiFi.status() == WL_CONNECTED
// before each POST and skip the request if still down.

#include "zacus_hook_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#ifndef ZACUS_MASTER_URL
#define ZACUS_MASTER_URL "http://zacus-master.local"
#endif

#ifndef ZACUS_HOOK_PATH
#define ZACUS_HOOK_PATH "/voice/hook"
#endif

#ifndef ZACUS_HOOK_TIMEOUT_MS
#define ZACUS_HOOK_TIMEOUT_MS 3000
#endif

#ifndef ZACUS_HOOK_RETRY_DELAY_MS
#define ZACUS_HOOK_RETRY_DELAY_MS 250
#endif

#ifndef ZACUS_HOOK_QUEUE_DEPTH
#define ZACUS_HOOK_QUEUE_DEPTH 4
#endif

#ifndef ZACUS_HOOK_WIFI_WAIT_MS
#define ZACUS_HOOK_WIFI_WAIT_MS 1500
#endif

namespace {

struct HookEvent {
  char state[8];   // "off" | "on"
  char reason[32]; // free-form short tag
};

QueueHandle_t g_queue = nullptr;
char g_url[160] = {0};

bool wifi_ready_within(uint32_t budget_ms) {
  if (WiFi.status() == WL_CONNECTED) return true;
  const uint32_t step = 50;
  uint32_t waited = 0;
  while (waited < budget_ms) {
    vTaskDelay(pdMS_TO_TICKS(step));
    waited += step;
    if (WiFi.status() == WL_CONNECTED) return true;
  }
  return false;
}

bool post_once(const HookEvent &ev) {
  HTTPClient http;
  http.setTimeout(ZACUS_HOOK_TIMEOUT_MS);
  http.setConnectTimeout(ZACUS_HOOK_TIMEOUT_MS);
  if (!http.begin(g_url)) {
    Serial.printf("[zacus-hook] http.begin failed for %s\n", g_url);
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  // Hand-rolled JSON: ArduinoJson is overkill for two short strings and
  // we want zero allocation cost on the hot path.
  char body[96];
  snprintf(body, sizeof(body),
           "{\"state\":\"%s\",\"reason\":\"%s\"}",
           ev.state, ev.reason);

  // HTTPClient::POST takes a non-const uint8_t*; body is a local stack
  // buffer so the const_cast is safe (no shared mutable aliasing).
  int code = http.POST(reinterpret_cast<uint8_t *>(body), strlen(body));
  http.end();
  if (code >= 200 && code < 300) {
    Serial.printf("[zacus-hook] POST %s -> %d (state=%s reason=%s)\n",
                  g_url, code, ev.state, ev.reason);
    return true;
  }
  Serial.printf("[zacus-hook] POST %s failed: code=%d (state=%s reason=%s)\n",
                g_url, code, ev.state, ev.reason);
  return false;
}

void worker_task(void *) {
  Serial.printf("[zacus-hook] worker ready, target=%s%s\n", g_url, ZACUS_HOOK_PATH);
  HookEvent ev;
  for (;;) {
    if (xQueueReceive(g_queue, &ev, portMAX_DELAY) != pdTRUE) continue;

    if (!wifi_ready_within(ZACUS_HOOK_WIFI_WAIT_MS)) {
      Serial.printf("[zacus-hook] Wi-Fi down, dropping event (state=%s reason=%s)\n",
                    ev.state, ev.reason);
      continue;
    }

    if (post_once(ev)) continue;

    // One retry at +250 ms.
    vTaskDelay(pdMS_TO_TICKS(ZACUS_HOOK_RETRY_DELAY_MS));
    if (!post_once(ev)) {
      Serial.printf("[zacus-hook] giving up after retry (state=%s reason=%s)\n",
                    ev.state, ev.reason);
    }
  }
}

}  // namespace

bool zacus_hook_client_init(const char *master_url) {
  if (g_queue != nullptr) return true;  // idempotent

  const char *base = (master_url && *master_url) ? master_url : ZACUS_MASTER_URL;
  // Compose full URL once: <base><path>. Strip trailing slash on base to
  // avoid "http://host//voice/hook".
  size_t blen = strnlen(base, sizeof(g_url) - sizeof(ZACUS_HOOK_PATH) - 1);
  if (blen == 0 || blen >= sizeof(g_url) - sizeof(ZACUS_HOOK_PATH) - 1) {
    Serial.println(F("[zacus-hook] invalid master_url length"));
    return false;
  }
  memcpy(g_url, base, blen);
  if (g_url[blen - 1] == '/') blen--;
  g_url[blen] = 0;
  strncat(g_url, ZACUS_HOOK_PATH, sizeof(g_url) - strlen(g_url) - 1);

  g_queue = xQueueCreate(ZACUS_HOOK_QUEUE_DEPTH, sizeof(HookEvent));
  if (g_queue == nullptr) {
    Serial.println(F("[zacus-hook] xQueueCreate failed"));
    return false;
  }

  BaseType_t ok = xTaskCreate(worker_task, "zacus-hook", 8192, nullptr, 5, nullptr);
  if (ok != pdPASS) {
    Serial.println(F("[zacus-hook] xTaskCreate failed"));
    vQueueDelete(g_queue);
    g_queue = nullptr;
    return false;
  }
  return true;
}

bool zacus_hook_client_report(const char *state, const char *reason) {
  if (g_queue == nullptr) {
    Serial.println(F("[zacus-hook] report() before init(), dropping"));
    return false;
  }
  if (state == nullptr) state = "";
  if (reason == nullptr) reason = "";

  HookEvent ev;
  strncpy(ev.state, state, sizeof(ev.state) - 1);
  ev.state[sizeof(ev.state) - 1] = 0;
  strncpy(ev.reason, reason, sizeof(ev.reason) - 1);
  ev.reason[sizeof(ev.reason) - 1] = 0;

  // Non-blocking enqueue: never wait, never block ISR / hot path.
  if (xQueueSend(g_queue, &ev, 0) != pdTRUE) {
    Serial.printf("[zacus-hook] queue full, dropping (state=%s reason=%s)\n",
                  ev.state, ev.reason);
    return false;
  }
  return true;
}
