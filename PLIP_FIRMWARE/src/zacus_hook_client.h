// zacus_hook_client — POST hook switch state changes to the Zacus master.
//
// The Zacus master (slice 10) exposes:
//   POST /voice/hook   { "state": "off" | "on", "reason": "<short>" }
//
// "off" = handset off-hook (pickup), "on" = handset on-hook (hangup).
// The master URL defaults to http://zacus-master.local (slice 12 mDNS) and
// is overridable at compile time via -DZACUS_MASTER_URL=\"http://1.2.3.4\".
//
// Thread model: a single worker FreeRTOS task drains an event queue. Calls
// from any context (ISR-safe via xQueueSendFromISR is NOT used here — call
// from a regular task, not the ISR itself; the existing phone_task already
// debounces in software) never block the caller.

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialise the client (spawns the worker task). Safe to call once from
// setup(). `master_url` may be nullptr to use the compile-time default.
// Returns true on success.
bool zacus_hook_client_init(const char *master_url);

// Enqueue a state report. Non-blocking: drops the event (with a Serial
// warning) if the queue is full. `state` must be "off" or "on"; `reason`
// is a short free-form tag ("pickup", "hangup", "boot", "manual", ...).
// Returns true if the event was enqueued, false otherwise.
bool zacus_hook_client_report(const char *state, const char *reason);

#ifdef __cplusplus
}
#endif
