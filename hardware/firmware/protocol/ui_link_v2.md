# UI Link v2 UART protocol

This document defines the canonical UART link between ESP32 audio firmware and interchangeable UI firmware (ESP8266 OLED, RP2040 TFT).

## 1. Transport

- Physical: UART 3.3V TTL, full duplex.
- Default UART pins on ESP32:
  - TX: GPIO22
  - RX: GPIO19
  - GND: common ground required
- Default baud: 19200 (configurable, both sides must match).
- Frame format (ASCII, line based):

```
<TYPE>,k=v,k=v*CC\n
```

Where:
- `<TYPE>` is an uppercase token (`HELLO`, `STAT`, etc.).
- `k=v` fields are comma separated key/value pairs.
- `CC` is CRC8 in uppercase hex, computed over bytes before `*`.
- CRC polynomial: `0x07`, init `0x00`, no reflection, no xorout.

## 2. Versioning and compatibility

- `proto=2` is mandatory in `HELLO`.
- Policy is additive-only:
  - Adding optional fields/messages is allowed.
  - Removing/renaming existing fields requires a deprecation cycle.
- Unknown fields must be ignored.
- Unknown message types must be ignored.

## 3. Connection lifecycle (hot-swap)

1. UI boots or is hot-plugged, sends `HELLO`.
2. ESP32 validates `proto=2` and replies `ACK`.
3. ESP32 sends `KEYFRAME` immediately after `ACK`.
4. ESP32 continues periodic `STAT` updates.
5. ESP32 sends `PING` every 1000 ms.
6. UI must answer `PONG`.
7. If no valid frame from UI for 1500 ms, ESP32 marks UI disconnected (headless mode) and waits for next `HELLO`.

Any new `HELLO` from UI at runtime must force immediate `ACK` + `KEYFRAME` resync.

## 4. Message catalog

## 4.1 UI -> ESP32

### HELLO

Required fields:
- `proto=2`

Recommended fields:
- `ui_type=OLED|TFT`
- `ui_id=<stable-id>`
- `fw=<firmware-version>`
- `caps=<capability-string>`

Example:

```
HELLO,proto=2,ui_type=TFT,ui_id=rp2040-01,fw=1.0.0,caps=btn:1;touch:1;display:tft*7A
```

### CAPS (optional refresh)

Used to refresh capabilities after boot.

### BTN

- `id=NEXT|PREV|OK|BACK|VOL_UP|VOL_DOWN|MODE`
- `action=down|up|click|long`
- `ts=<ui timestamp ms>`

### TOUCH (optional raw touch)

- `x=<px>`
- `y=<px>`
- `action=down|move|up`
- `ts=<ui timestamp ms>`

### CMD (optional)

- `op=request_keyframe|set_page|...`
- optional `arg=<value>`

### PONG

- optional `ms=<ui timestamp ms>`

## 4.2 ESP32 -> UI

### ACK

- `proto=2`
- `session=<counter>`
- optional `caps=<esp-caps>`

### KEYFRAME

Complete UI state snapshot. Must include all currently relevant fields so UI can render from scratch after reconnect.

### STAT

Compact periodic telemetry update (delta-friendly). Must include sequence/time and mode fields.

### PING

- `ms=<esp uptime ms>`

## 5. State field conventions

These fields are shared by `STAT` and `KEYFRAME` (subset allowed in `STAT`, full set required in `KEYFRAME`):

- `seq` frame sequence
- `ms` esp uptime
- `mode` `SIGNAL|MP3|U_LOCK|STORY`
- `la` 0/1
- `mp3` 0/1
- `sd` 0/1
- `key` last logical key (0..6)
- `track`, `track_total`
- `vol` 0..100
- `tune_off` -8..8
- `tune_conf` 0..100
- `u_lock_listen` 0/1
- `hold` 0..100
- `startup` 0..1 (0=inactive, 1=boot_validation)
- `app` 0..3 (0=ulock_wait, 1=ulock_listen, 2=uson, 3=mp3)
- `ui_page`
- `ui_cursor`, `ui_offset`, `ui_count`
- `queue`
- `repeat`
- `fx`
- `backend`
- `scan`
- `err`

Implementations may add optional fields.

## 6. Timing defaults

- ESP32 `STAT` period: 250 ms (or on meaningful change)
- ESP32 `KEYFRAME` period: 1000 ms and on (re)connect
- ESP32 `PING`: every 1000 ms
- UI disconnect timeout at ESP32: 1500 ms
- UI reconnect request timeout for downstream state: 1000..1500 ms recommended

## 7. Error handling

- Invalid CRC: drop frame.
- Malformed frame: drop frame.
- Missing required `HELLO` fields: ignore until a valid `HELLO`.
- BTN/TOUCH outside context: ignore and keep link alive.

## 8. Reference implementation

Portable helpers (CRC, parse, build, field lookup) are defined in:

- `protocol/ui_link_v2.h`
