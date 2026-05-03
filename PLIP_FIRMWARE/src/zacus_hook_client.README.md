# `zacus_hook_client`

Reports handset hook-switch transitions from the PLIP firmware to the
Zacus master ESP32 over HTTP.

| Item | Value |
|------|-------|
| Framework | PlatformIO + Arduino (ESP32) |
| Master endpoint | `POST /voice/hook` (slice 10) |
| Default master URL | `http://zacus-master.local` (slice 12 mDNS) |
| Override (compile-time) | `-DZACUS_MASTER_URL=\"http://192.168.0.42\"` |
| Worker task | `zacus-hook`, prio 5, 8 KB stack |
| Queue depth | 4 events (oldest dropped if full) |
| HTTP timeout | 3000 ms, retry once at +250 ms |

The module never blocks the caller: `zacus_hook_client_report()` only
enqueues an event. A dedicated FreeRTOS worker drains the queue and runs
the actual `HTTPClient::POST`.

## API

```c
bool zacus_hook_client_init(const char *master_url);
bool zacus_hook_client_report(const char *state, const char *reason);
```

- `state` must be `"off"` (handset lifted, pickup) or `"on"` (handset
  cradled, hangup).
- `reason` is a free-form short tag (`"pickup"`, `"hangup"`, `"boot"`,
  `"manual"`).

JSON body sent to the master:

```json
{ "state": "off", "reason": "pickup" }
```

## Hook switch wiring

Active-low with the ESP32 internal pull-up. The pin is set in
`platformio.ini` build flags as `OFF_HOOK_GPIO` (default `4`, BOOT/KEY1
on the AI-Thinker ESP32-A1S dev kit).

```
            +3V3
              |
              R (internal pull-up, INPUT_PULLUP)
              |
GPIO4 -------+------ HOOK_SWITCH ------ GND
              |
        (ISR on CHANGE)
```

- LOW  -> handset off-hook (pickup)
- HIGH -> handset on-hook  (hangup, idle)

On the Si3210 PCB, replace the mechanical switch with the SLIC `INT` line
(already hardware-debounced). The default `OFF_HOOK_GPIO=4` build flag
just needs to be repointed at the chosen routing pin.

## Override the master URL

```ini
; platformio.ini
build_flags =
  ...
  -DZACUS_MASTER_URL="\"http://192.168.0.42\""
```

Note the doubled escaping: PlatformIO removes one layer, the C preprocessor
needs the inner `"` to keep the literal a string.

## Test without a PLIP

Simulate any pickup/hangup with `curl` against the master:

```bash
# Pickup
curl -X POST http://zacus-master.local/voice/hook \
  -H 'Content-Type: application/json' \
  -d '{"state":"off","reason":"manual"}'

# Hangup
curl -X POST http://zacus-master.local/voice/hook \
  -H 'Content-Type: application/json' \
  -d '{"state":"on","reason":"manual"}'
```

If `zacus-master.local` does not resolve from your laptop yet (slice 12
mDNS not deployed), use the master's static LAN IP.

## Logs

```
[zacus-hook] worker ready, target=http://zacus-master.local/voice/hook
[zacus-hook] POST http://zacus-master.local/voice/hook -> 200 (state=off reason=pickup)
[zacus-hook] Wi-Fi down, dropping event (state=on reason=hangup)
[zacus-hook] queue full, dropping (state=off reason=pickup)
```

## Failure policy

| Outcome | Behaviour |
|---------|-----------|
| Wi-Fi down (waited 1.5 s) | Event dropped, logged. No retry — handset state will be re-synced on the next real edge. |
| HTTP non-2xx / transport error | Single retry at +250 ms, then drop. |
| Queue full | Oldest non-served event keeps its slot, new event dropped. |

The master is expected to be idempotent on `/voice/hook` so duplicate
events from a flaky link are safe.
