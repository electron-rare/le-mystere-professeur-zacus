# Firmware Phase 2 — `/game/scenario` hot-load + ESP-NOW relay

**Status:** specified 2026-05-24, **not yet implemented**.
**Consumer:** `zacus-hub` SwiftUI app via `tools/zacus-gateway` `POST /v1/flash/{board}`.
**Targets:** `idf_zacus` (master), `box3_voice`, `plip_firmware`, `puzzles/*`.

The gateway's flash endpoint already attempts `POST http://<board>/game/scenario`
in hot strategy mode. It receives `404` today because the route doesn't exist
in any firmware. This document is the exhaustive plan to wire it.

## Goal

A game-master can iterate on a scenario in the Blockly Studio, click **Flasher**,
and have the **running** Zacus boards reload the new Runtime 3 IR in ~1 second —
no rebuild, no reboot, no serial cable, no OTA partition swap.

The master can additionally forward the IR to peer boards over ESP-NOW so satellite
modules (BOX-3, PLIP, p7_coffre) get the update even when not on WiFi.

## Surface (firmware)

### `POST /game/scenario` (every board)

Add to `idf_zacus/components/game_endpoint/game_endpoint.c` (already hosts
`/game/group_profile` on the same `esp_http_server`).

```http
POST /game/scenario HTTP/1.1
Content-Type: application/json
Content-Length: ≤ 64 KiB

{ "schema_version": "zacus.runtime3.v1", "scenario": {...}, "steps": [...], ... }
```

Behaviour:
1. Body cap **64 KiB** (`GAME_ENDPOINT_MAX_SCENARIO_BYTES`). Reject 413 otherwise.
2. Parse with cJSON. Reject 400 on bad JSON / missing `schema_version`.
3. Validate `schema_version == "zacus.runtime3.v1"` and `steps` non-empty.
4. Atomic write to LittleFS partition `storage` at `/storage/scenario.json`
   (the partition is already declared in `partitions.csv` for this purpose).
5. Call `scenario_engine_reload(target)` — a new symbol that:
   - Saves current step + state to NVS for graceful interrupt recovery
   - Closes any pending NPC TTS / voice session
   - Re-parses `/storage/scenario.json` into the in-RAM `scenario_t`
   - Jumps to the new `entry_step_id`
6. Respond `200 { "status": "ok", "steps_count": N, "entry_step_id": "..." }`.
7. If reload throws, restore the previous scenario from a `.bak` copy and
   respond `500 { "status": "rollback", "error": "..." }`.

Failure modes that must be handled gracefully:
- LittleFS write failure → respond `500 { "status": "storage_error" }`, don't reload.
- New scenario references missing audio assets → log warnings but accept the load
  (assets resolve at action dispatch time).

### `POST /game/scenario/relay` (master only)

Optional add-on, only on the master (`idf_zacus`). Accepts the same payload plus
a `peers` array:

```json
{ "peers": ["box3", "plip", "p7_coffre"], "ir": { … } }
```

Behaviour:
1. For each peer alias:
   - Resolve to MAC via the existing ESP-NOW peer registry (extend with
     `mac_for_alias()`).
   - Chunk the IR JSON into ≤ 240-byte ESP-NOW frames (ESP-NOW max payload).
     Use a 4-byte frame header `{ seq: u16, total: u16 }`.
   - Send all frames sequentially with `esp_now_send`, await per-frame ack.
   - On reception side (existing `espnow_recv_cb`), accumulate frames keyed by
     a sender+sequence-zero token, until `total` received, then concatenate and
     invoke the same internal `_scenario_apply()` used by the HTTP handler.
2. Respond `200 { "relayed": ["box3","plip"], "skipped": [{"name":"p7_coffre","reason":"timeout"}] }`.

A failure on one peer does not abort the others.

## Receiver side on peer boards

Each non-master board (`box3_voice`, `plip_firmware`, `puzzles/p7_coffre`) must
gain an ESP-NOW receive callback that recognises the IR frame protocol header
and writes the reassembled JSON to its local LittleFS, then calls
`scenario_engine_reload(target)`.

If a peer is also reachable on WiFi (some boards may be), the direct HTTP
`POST /game/scenario` continues to work — the ESP-NOW path is the fallback for
boards in WiFi-disabled mode (battery saving, RF noise).

## Implementation tasks

| # | Task | Component | Loc estimate |
|---|------|-----------|--------------|
| 1 | `scenario_engine_reload(const char *path)` | `npc_engine` or new `scenario_loader` | ~120 LOC |
| 2 | Atomic-write helper `littlefs_atomic_replace()` | new `storage_util` | ~40 LOC |
| 3 | `POST /game/scenario` handler in `game_endpoint` | `game_endpoint` | ~80 LOC |
| 4 | ESP-NOW frame protocol + reassembly | new `scenario_mesh` | ~180 LOC |
| 5 | `POST /game/scenario/relay` handler (master) | `game_endpoint` | ~60 LOC |
| 6 | Receiver side on each peer | each firmware | ~60 LOC each |
| 7 | Tests: golden IR, partial frame, large payload | unit + on-target | ~150 LOC |

Total ~750 LOC across components. Estimated **2-3 days dev** + 1 day on-target
debugging + ESP-NOW frame loss tuning.

## Migration & rollout

1. Implement on `idf_zacus` master first (single-board target). Validate hot-load
   end-to-end with the gateway's `strategy: hot` against the master IP.
2. Once green, add receiver on `box3_voice`, then `plip_firmware`, then puzzles.
3. Add `/game/scenario/relay` last — by then both sides know the frame protocol.
4. After all boards are flashed with the new firmware once via cold-flash, every
   subsequent scenario change is a 1-second hot-push from the app.

## Why not full OTA every time?

Full OTA via the existing `ota_server` works but takes ~30s build + 30s push +
30s post-boot warmup. It also burns flash write cycles unnecessarily — the
LittleFS scenario partition can take millions of writes.

The hot-load path stays within the same firmware image; only the LittleFS file
changes. Combined with the dual-bank OTA for firmware itself, the system supports
two orthogonal update lanes:
- **Slow lane** (OTA, weeks): firmware code, drivers, NPC engine fixes.
- **Fast lane** (hot-load, minutes): scenarios, story tweaks, hint copy.

## Open questions

- **Auth**: should `/game/scenario` require a bearer like the gateway, or is
  LAN-only sufficient? Decision deferred until first deployment.
- **Schema evolution**: when `zacus.runtime3.v1` becomes `.v2`, do we reject or
  attempt migration on the board? Probably reject — gateway is the migration point.
- **ESP-NOW encryption**: peers currently use unencrypted ESP-NOW. Scenario
  payloads contain dialogue but no secrets; if that changes we move to PMK+LMK.
