# RTOS + WiFi Health (Story V2)

## Objectif

Ce document decrit les checks de stabilite WiFi, la telemetrie RTOS minimale, et la procedure de recovery pour l'ESP32 Story V2.

## Implementation Audit (2026-02-16)

**Summary**
- RTOS runtime tasks are wired into boot and controlled by config.
- `/api/rtos` exposes per-task snapshots when runtime is available.
- Serial `SYS_RTOS_STATUS` prints global snapshot plus per-task data.
- Watchdog support is enabled for RTOS tasks (configurable).

**Current RTOS runtime**
- Runtime class: `RadioRuntime` (FreeRTOS tasks pinned per core).
- Tasks: `TaskAudioEngine`, `TaskStreamNet`, `TaskStorageScan`, `TaskWebControl`, `TaskUiOrchestrator`.
- Update flow:
   - RTOS mode: tasks tick and call WiFi/web updates.
   - Cooperative mode: updates are done from the main loop via `RadioRuntime::updateCooperative`.

**Health telemetry**
- Global snapshot: heap, task count, current task stack watermark.
- Per-task snapshot: stack watermark, ticks, last tick time, core id.
- HTTP: `/api/status` (rtos block) and `/api/rtos` (detailed).
- Serial: `SYS_RTOS_STATUS`.

**Watchdog**
- RTOS tasks register to the ESP task watchdog when enabled.
- Each RTOS task resets the watchdog in its loop.
- Config:
   - `kEnableRadioRuntimeWdt`
   - `kRadioRuntimeWdtTimeoutSec`

**Gaps / Follow-ups**
- Run RTOS/WiFi health script to generate artifact (`artifacts/rtos_wifi_health_<timestamp>.log`).
- Validate WiFi reconnect + WebSocket recovery on hardware.
- Confirm task stack margins under real load.

## Indicateurs exposes

### WiFi

- `disconnect_reason` + `disconnect_label` (dernier motif de deconnexion)
- `disconnect_count` (compteur total)
- `last_disconnect_ms` (timestamp relatif du dernier event)

Endpoints:
- `GET /api/status` (bloc `wifi`)
- `GET /api/wifi`

### RTOS

- `tasks` (nombre de taches FreeRTOS)
- `heap_free`, `heap_min`, `heap_size`
- `stack_min_words`, `stack_min_bytes` (watermark de la tache courante)
- `runtime_enabled` (runtime RTOS actif)
- `task_list[]` (par tache: `name`, `core`, `stack_min_words`, `stack_min_bytes`, `ticks`, `last_tick_ms`)

Endpoints:
- `GET /api/status` (bloc `rtos`)
- `GET /api/rtos`

Commande serie:
- `SYS_RTOS_STATUS`

Note:
- le watermark stack reflete la tache courante (loop ou handler web) et sert d'indicateur rapide.

## Check rapide (HTTP)

```
ESP_URL=http://<ip-esp32>:8080 ./tools/dev/rtos_wifi_health.sh
```

L'artefact est ecrit dans:
- `artifacts/rtos_wifi_health_<timestamp>.log`

## Debug WiFi (serial)

Pour capturer les logs WiFi via serial (filtre regex + log brut):

```bash
./tools/dev/rtos_wifi_health.sh --serial-debug --serial-debug-seconds 600
```

Options utiles:
- `--serial-debug-regex "wifi|disconnect|reconnect|dhcp|got ip"`
- `ZACUS_PORT_ESP32=/dev/cu.SLAB_USBtoUART` (override port)
- `ZACUS_WIFI_SERIAL_DEBUG=1` (enable via env)

Artefacts:
- `artifacts/rtos_wifi_health_<timestamp>/wifi_serial_debug.log`

## Check WiFi reconnect (manuel)

1. Lancer `ESP_URL=http://<ip-esp32>:8080 ./tools/dev/rtos_wifi_health.sh`.
2. Couper le WiFi de l'AP pendant 10-15s.
3. Remettre l'AP et verifier:
   - `disconnect_count` augmente
   - `disconnect_label` correspond au motif
   - `connected=true` dans `GET /api/status` en < 30s

Optionnel WebSocket:
- Connecter un client sur `ws://<ip-esp32>:8080/api/story/stream`
- Verifier que le stream recupere apres reconnexion

## Crash markers (serial)

Les markers ci-dessous doivent etre consideres comme FAIL:
- `Guru Meditation Error`
- `Core panic`
- `rst:0x...`
- `abort()`

## Recovery checklist (operateur)

1. Sauver les logs (`logs/` ou monitor serial).
2. Verifier `SYS_RTOS_STATUS` (heap/stack min).
3. Rebooter l'ESP32 si un panic est detecte.
4. Si reboot recurrent: reflasher `esp32dev`, verifier alim 5V stable.
5. Relancer le smoke (`./tools/dev/run_matrix_and_smoke.sh`) si la panne persiste.
