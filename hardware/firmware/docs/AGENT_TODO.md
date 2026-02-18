### Procédure smoke test Freenove

1. Vérifier la configuration du port série (résolution dynamique via cockpit.sh ports).
2. Flasher la cible Freenove avec cockpit.sh flash (ou build_all.sh).
3. Lancer le smoke test avec tools/dev/run_matrix_and_smoke.sh.
4. Vérifier le verdict UI_LINK_STATUS connected==1 (fail strict si absent).
5. Vérifier l’absence de panic/reboot dans les logs.
6. Vérifier la santé des WebSockets (logs, auto-recover).
7. Consigner les logs et artefacts produits (logs/rc_live/freenove_esp32s3_YYYYMMDD.log, artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html).
8. Documenter toute anomalie ou fail dans AGENT_TODO.md.

## TODO Freenove ESP32-S3 (contrat agent)

### [2026-02-17] Log détaillé des étapes réalisées – Freenove ESP32-S3

- Audit mapping hardware exécuté (artifacts/audit/firmware_20260217/validate_mapping_report.txt)
- Suppression des macros UART TX/RX pour UI Freenove all-in-one (platformio.ini, ui_freenove_config.h)
- Documentation détaillée des périphériques I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
- Synchronisation des macros techniques SPI/Serial/Driver dans ui_freenove_config.h (SPI_FREQUENCY, UI_LCD_SPI_HOST, etc.)
- Ajout d’une section multiplexage dans RC_FINAL_BOARD.md (partage BL/BTN3, CS TFT/BTN4)
- Ajout d’un commentaire explicite multiplexage dans ui_freenove_config.h
- Evidence synchronisée : tous les artefacts et logs sont tracés (artifacts/audit/firmware_20260217, logs/rc_live/freenove_esp32s3_YYYYMMDD.log)
- Traçabilité complète : chaque étape, correction, mapping, doc et evidence sont référencés dans AGENT_TODO.md

### [2026-02-17] Rapport d’audit mapping hardware Freenove ESP32-S3

**Correspondances parfaites** :
  - TFT SPI : SCK=18, MOSI=23, MISO=19, CS=5, DC=16, RESET=17, BL=4
  - Touch SPI : CS=21, IRQ=22
  - Boutons : 2, 3, 4, 5
  - Audio I2S : WS=25, BCK=26, DOUT=27
  - LCD : WIDTH=480, HEIGHT=320, ROTATION=1

**Écarts ou incohérences** :
  - UART : platformio.ini (TX=43, RX=44), ui_freenove_config.h (TX=1, RX=3), RC_FINAL_BOARD.md (non documenté)
  - I2C, LED, Buzzer, DHT11, MPU6050 : présents dans ui_freenove_config.h, absents ou non alignés ailleurs
  - SPI Host, Serial Baud, Driver : macros techniques non synchronisées
  - Multiplexage : BL/Bouton3 et CS TFT/Bouton4 partagent la broche, à clarifier

**Recommandations** :
  - Aligner UART TX/RX sur une valeur unique et documenter
  - Documenter I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
  - Synchroniser macros techniques dans ui_freenove_config.h et platformio.ini
  - Ajouter une note sur le multiplexage dans RC_FINAL_BOARD.md et ui_freenove_config.h
  - Reporter toute évolution dans AGENT_TODO.md

**Evidence** :
  - Rapport complet : artifacts/audit/firmware_20260217/validate_mapping_report.txt
  [2026-02-17] Synthèse technique – Détection dynamique des ports USB
    - Tous les ports USB série sont scannés dynamiquement (glob /dev/cu.*).
    - Attribution des rôles (esp32, esp8266, rp2040) selon mapping, fingerprint, VID:PID, fallback CP2102.
    - Esp32-S3 : peut apparaître en SLAB_USBtoUART (CP2102) ou usbmodem (mode bootloader, DFU, flash initial).
    - RP2040 : typiquement usbmodem.
    - Si un seul CP2102 détecté, fallback mono-port (esp32+esp8266 sur le même port).
    - Si plusieurs ports, mapping par location, fingerprint, ou VID:PID.
    - Aucun port n’est hardcodé : la détection s’adapte à chaque run (reset, flash, bootloader).
    - Les scripts cockpit.sh, resolve_ports.py, run_matrix_and_smoke.sh exploitent cette logique.
    - Traçabilité : logs/ports_debug.json, logs/ports_resolve.json, artifacts/rc_live/...
    - Documentation onboarding mise à jour pour refléter la procédure.
    - Recommandation : toujours utiliser la détection dynamique, ne jamais forcer un port sauf cas extrême (override manuel).

  [2026-02-17] Validation RC Live Freenove ESP32-S3
    - Port USB corrigé : /dev/cu.SLAB_USBtoUART
    - Flash ciblé relancé : pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.SLAB_USBtoUART
    - Evidence synchronisée : summary.md, ui_link.log, logs/rc_live/20260217-211606
    - Résultat : UI link FAIL (connected=1 mais status unavailable)
    - Artefacts : artifacts/rc_live/20260217-211606/summary.md, ui_link.log
    - Log détaillé ajouté dans AGENT_TODO.md

- [ ] Vérifier et compléter le mapping hardware dans platformio.ini ([env:freenove_esp32s3])
  - Pins, UART, SPI, I2C, etc. : vérifier cohérence avec la doc.
  - Exemple : GPIO22=I2S_WS, GPIO23=I2S_DATA, UART1=TX/RX.
- [ ] Aligner ui_freenove_config.h avec platformio.ini et la documentation RC_FINAL_BOARD.md
  - Vérifier que chaque pin, bus, périphérique est documenté et codé.
  - Ajouter commentaires explicites pour chaque mapping.
- [ ] Mettre à jour docs/RC_FINAL_BOARD.md
  - Décrire le mapping complet, schéma, photo, table des pins.
  - Ajouter procédure de flash/test spécifique Freenove.
  - Mentionner les différences avec ESP32Dev.
- [ ] Adapter build_all.sh, cockpit.sh, run_matrix_and_smoke.sh, etc.
  - Ajouter/valider la cible freenove_esp32s3 dans les scripts.
  - Vérifier que le build, flash, smoke, logs fonctionnent sur Freenove.
  - Exemple : ./build_all.sh doit inclure freenove_esp32s3.
- [ ] Vérifier la production de logs et artefacts lors des tests sur Freenove
  - Chemin : logs/rc_live/freenove_esp32s3_YYYYMMDD.log
  - Artefacts : artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html
  - Référencer leur existence (chemin, timestamp, verdict) dans docs/AGENT_TODO.md.
- [ ] Mettre à jour l’onboarding (docs/QUICKSTART.md, docs/AGENTS_COPILOT_PLAYBOOK.md)
  - Ajouter section « Flash Freenove », « Test Freenove ».
  - Préciser les ports série, baud, procédure de résolution dynamique.
- [ ] Vérifier que les gates smoke et stress test sont compatibles et valident strictement la cible Freenove
  - Fail sur panic/reboot, verdict UI_LINK_STATUS connected==1.
  - Tester WebSocket health, stress I2S, scénario LittleFS.
- [ ] Documenter toute évolution ou correction dans docs/AGENT_TODO.md
  - Détailler les étapes réalisées, artefacts produits, impasses matérielles.
  - Exemple : « Test flash Freenove OK, logs produits dans logs/rc_live/freenove_esp32s3_20260217.log ».
## [2026-02-17] ESP32 pin remap test kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-185336_wip.patch` and `/tmp/zacus_checkpoint/20260217-185336_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link handshake still FAIL (`connected=0`) on latest cross-monitor evidence (`artifacts/ui_link_diag/20260217-174822/`), LittleFS fallback status unchanged, I2S stress status unchanged (last 30 min PASS).

## [2026-02-17] ESP32 UI pin remap execution (Codex)

- Applied ESP32 UI UART remap in firmware config: `TX=GPIO23`, `RX=GPIO18` (OLED side stays `D4/D5`).
- Rebuilt and reflashed ESP32 (`pio run -e esp32dev`, then `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9`).
- Cross-monitor rerun: `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-175606/`.
- Result moved from FAIL to WARN: ESP32 now receives HELLO frames (`esp32 tx/rx=48/20`, `UI_LINK_STATUS connected=1` seen), but ESP8266 still receives nothing from ESP32 (`esp8266 tx/rx=19/0`), so handshake remains incomplete.

## [2026-02-17] UI link diagnostic patch kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-181730_wip.patch` and `/tmp/zacus_checkpoint/20260217-181730_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link strict gate still FAIL (`connected=0`), LittleFS scenario load now mitigated by V2 fallback, I2S stress status currently PASS on last 30 min run.

## [2026-02-17] Cross-monitor boot capture (Codex)

- Built and flashed diagnostics on both boards: `pio run -e esp32dev -e esp8266_oled`, then upload on `/dev/cu.SLAB_USBtoUART9` (ESP32) and `/dev/cu.SLAB_USBtoUART` (ESP8266).
- Captured synchronized boot monitors with `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-173005/` (`esp32.log`, `esp8266.log`, `merged.log`, `summary.md`, `ports_resolve.json`, `meta.json`).
- Verdict remains FAIL: no discriminant RX observed on either side (`ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`), indicating traffic still not seen on D4/D5 at runtime.

## [2026-02-17] Cross-monitor rerun after pin inversion (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after manual pin inversion test.
- Evidence: `artifacts/ui_link_diag/20260217-174330/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] Cross-monitor rerun after inverted wiring confirmation (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after updated wiring check.
- Evidence: `artifacts/ui_link_diag/20260217-174822/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] D4/D5 handshake + strict firmware_tests rerun (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-173556_wip.patch` and `/tmp/zacus_checkpoint/20260217-173556_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Build gate rerun: `./build_all.sh` PASS (5/5 envs) after firmware updates.
- Reflash completed: `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9` and `pio run -e esp8266_oled -t upload --upload-port /dev/cu.SLAB_USBtoUART`.
- `firmware_tooling` rerun PASS (`plan-only` + full run); all `--help` paths stay non-destructive.
- `firmware_tests` strict rerun (`ZACUS_REQUIRE_HW=1`) still FAIL at gate 1: `artifacts/rc_live/20260217-164154/summary.md` (`UI_LINK_STATUS connected=0`, esp8266 monitor FAIL).
- Strict smoke rerun uses repo-root resolver (`tools/test/resolve_ports.py`) and strict ESP32+ESP8266 mapping; gate FAIL remains UI link only (`artifacts/smoke_tests/20260217-164237/smoke_tests.log`), while `STORY_LOAD_SCENARIO DEFAULT` now succeeds through fallback (`STORY_LOAD_SCENARIO_FALLBACK V2 DEFAULT` + `STORY_LOAD_SCENARIO_OK`).
- `audit_coherence.py` rerun PASS (`artifacts/audit/20260217-164243/summary.md`).
- Content validators rerun PASS from repo root: scenario validate/export + audio manifest + printables manifest.
- Stress rerun completed PASS (`python3 tools/dev/run_stress_tests.py --hours 0.5`): `artifacts/stress_test/20260217-164248/summary.md` (`87` iterations, success rate `100.0%`, no panic/reboot markers in log).
- Runtime status after reruns: UI Link = FAIL (`connected=0`), LittleFS default scenario = mitigated by V2 fallback in serial path, I2S stability = PASS over 30 min stress run (no panic/reboot markers).

## [2026-02-17] Fix handshake/UI smoke strict kickoff (Codex)

- Safety checkpoint executed via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-155753_wip.patch` and `/tmp/zacus_checkpoint/20260217-155753_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Runtime status at kickoff: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S stability = FAIL/intermittent panic in stress recovery path.

## [2026-02-17] Copilot sequence checkpoint (firmware_tooling -> firmware_tests)

- Safety checkpoint executed before edits via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-162509_wip.patch` and `/tmp/zacus_checkpoint/20260217-162509_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Kickoff runtime status: UI Link still failing in prior smoke evidence, default LittleFS scenario still missing in prior QA notes, I2S panic already known in stress path.

## [2026-02-17] Copilot sequence execution + alignment fixes

- Plan/runner alignment applied: root `tools/dev/plan_runner.sh` now delegates to `hardware/firmware/tools/dev/plan_runner.sh`; firmware runner now executes from repo root and resolves active agent IDs recursively under `.github/agents/` (excluding `archive/`).
- `run_matrix_and_smoke.sh` now supports `--help`/`-h` without side effects and returns explicit non-zero on unknown args (`--bad-arg` -> rc=2).
- Agent briefs synchronized in root + firmware copies (`domains/firmware-tooling.md`, `domains/firmware-tests.md`) with repo-root commands and venv-aware PATH (`PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH`).
- `firmware_tooling` sequence: PASS (`bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling`).
- `firmware_tests` strict sequence (`ZACUS_REQUIRE_HW=1`): blocked at step 1 because `run_matrix_and_smoke` reports UI link failure; evidence `artifacts/rc_live/20260217-153129/summary.md` (`UI_LINK_STATUS connected=0`).
- Remaining test gates executed manually after the blocked step:
  - `run_smoke_tests.sh` strict: FAIL (port resolution), evidence `artifacts/smoke_tests/20260217-153214/summary.md`.
  - `run_stress_tests.py --hours 0.5`: FAIL, scenario does not complete (`DEFAULT`), evidence `artifacts/stress_test/20260217-153220/summary.md`; earlier run also captured I2S panic evidence `artifacts/stress_test/20260217-153037/stress_test.log`.
  - `audit_coherence.py`: initially FAIL on missing runbook refs, then PASS after `cockpit_commands.yaml` runbook fixes + regenerated commands doc; evidence `artifacts/audit/20260217-153246/summary.md` (latest PASS).
- RP2040 docs/config audit: no env naming drift detected (`ui_rp2040_ili9488`, `ui_rp2040_ili9486`) across `platformio.ini`, `build_all.sh`, `run_matrix_and_smoke.sh`, `docs/QUICKSTART.md`, and `docs/TEST_SCRIPT_COORDINATOR.md`.
- Runtime status after this pass: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S panic = intermittent (observed in stress evidence above).

## [2026-02-17] Audit kickoff checkpoint (Codex)

- Safety checkpoint run from `hardware/firmware`: branch `story-V2`, working tree dirty (pre-existing changes), `git diff --stat` captured before edits.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-141814_wip.patch` and `/tmp/zacus_checkpoint/20260217-141814_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths, so no untrack action was needed.
- Runtime status at kickoff: UI Link `connected=0` in previous smoke evidence, LittleFS default scenario still flagged as missing by prior QA notes, I2S panic previously reported in stress recovery path.

## [2026-02-17] Audit + coherence pass (Codex)

- Tooling/scripts corrected: `tools/dev/run_matrix_and_smoke.sh` (broken preamble/functions restored, non-interactive USB path now resolves ports, accepted `learned-map` reasons, story screen skip evidence), `tools/dev/serial_smoke.py` (ports map loading re-enabled, dead duplicate role code removed), `tools/dev/cockpit.sh` (`rc` no longer forces `ZACUS_REQUIRE_HW=1`, debug banner removed), `tools/dev/plan_runner.sh` (portable paths/date and safer arg parsing), UI OLED HELLO/PONG now built with protocol CRC via `uiLinkBuildLine`.
- Command registry/doc sync fixed: `tools/dev/cockpit_commands.yaml` normalized under one `commands:` key (including `wifi-debug`) and regenerated `docs/_generated/COCKPIT_COMMANDS.md` now lists full evidence columns without blank rows.
- Protocol/runtime check: `protocol/ui_link_v2.md` expects CRC frames; OLED runtime now emits CRC on HELLO/PONG. Remaining runtime discrepancy logged: Story V2 controller still boots from generated scenario IDs (`DEFAULT`) rather than directly from `game/scenarios/zacus_v1.yaml`; root YAML remains validated/exported as canonical content source.
- Build gate result: `./build_all.sh` PASS (5/5 envs) with log `logs/run_matrix_and_smoke_20260217-143000.log` and build artifacts in `.pio/build/`.
- Smoke gate result: `./tools/dev/run_matrix_and_smoke.sh` FAIL (`artifacts/rc_live/20260217-143000/summary.json`) — ports resolved, then `smoke_esp8266_usb` and `ui_link` failed (`UI_LINK_STATUS missing`, ESP32 log in bootloader/download mode). Evidence: `artifacts/rc_live/20260217-143000/ui_link.log`, `artifacts/rc_live/20260217-143000/smoke_esp8266_usb.log`.
- Content gates result (run from repo root): scenario validate/export PASS (`game/scenarios/zacus_v1.yaml`), audio manifest PASS (`audio/manifests/zacus_v1_audio.yaml`), printables manifest PASS (`printables/manifests/zacus_v1_printables.yaml`).
- Runtime status after this pass: UI Link = FAIL on latest smoke evidence (`connected` unavailable), LittleFS default scenario = not revalidated in this run (known previous blocker remains), I2S panic = not retested in this pass (stress gate pending).

## [2026-02-17] Rapport d’erreur automatisé – Story V2

- Correction du bug shell (heredoc Python → script temporaire).
- Échec de la vérification finale : build FAIL, ports USB OK, smoke/UI/artefacts SKIPPED.
- Rapport d’erreur généré : voir artifacts/rapport_erreur_story_v2_20260217.md
- Recommandation : analyser le log de build, corriger, relancer la vérification.

# Agent TODO & governance

## 1. Structural sweep & merge
- [ ] Commit the pending cleanup described in `docs/SPRINT_RECOMMENDATIONS.md:18-80` (structure/tree fixes + PR #86 merge/tag) so the repo is back on main.

## 2. Build/test gates
- [x] Re-run `./build_all.sh` (`build_all.sh:6`); artifacts landed under `artifacts/build/` and logs live in `logs/run_matrix_and_smoke_*.log` if rerun again via the smoke gate.
- [x] Re-launch `./tools/dev/run_matrix_and_smoke.sh` (`tools/dev/run_matrix_and_smoke.sh:9-200`) – run completed 2026-02-16 14:35 (artifact `artifacts/rc_live/20260216-143539/`), smoke scripts and serial monitors succeeded but UI link still reports `connected=0` (no UI handshake). Need to plug in/validate UI firmware before closing gate.
- [x] Capture evidence for HTTP API, WebSocket, and WiFi/Health steps noted as blocked or TODO in `docs/TEST_SCRIPT_COORDINATOR.md:13-20` – `tools/dev/healthcheck_wifi.sh` created `artifacts/rc_live/healthcheck_20260216-154709.log` (ping+HTTP fail) and the HTTP API script logged connection failures under `artifacts/http_ws/20260216-154945/http_api.log` (ESP_URL=127.0.0.1:9). WebSocket skipped (wscat missing). All failures logged to share evidence.

## 3. QA + automation hygiene
- [x] Execute the manual serial smoke path (`python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`) – passed on /dev/cu.SLAB_USBtoUART + /dev/cu.SLAB_USBtoUART9, reporting UI link still down (same failure as matrix run).
- [ ] Run the story QA suite (`tools/dev/run_smoke_tests.sh`, `python3 tools/dev/run_stress_tests.py ...`, `make fast-*` loops) documented in `esp32_audio/TESTING.md:36-138` and capture logs (smoke_tests failed: DEFAULT scenario missing `/story/scenarios/DEFAULT.json`; run_stress_tests failed with I2S panic during recovery; `make fast-esp32` / `fast-ui-oled` built & flashed but monitor commands quit in non-interactive mode, `fast-ui-tft` not run because no RP2040 board connected). Need scenario files/UI recovery to unblock.
- [x] Ensure any generated artifacts remain untracked per agent contract (no logs/artifacts added to git).

## 4. Documentation & agent contracts
- [ ] Update `AGENTS.md` and `tools/dev/AGENTS.md` whenever scripts/gates change, per their own instructions (`AGENTS.md`, `tools/dev/AGENTS.md`).
- [ ] Keep `tools/dev/cockpit_commands.yaml` in sync with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` after edits, and confirm the command registry is reflected in `docs/TEST_SCRIPT_COORDINATOR.md` guidance.
- [ ] Review `docs/INDEX.md`, `docs/ARCHITECTURE_UML.md`, and `docs/QUICKSTART.md` after significant changes so the onboarding picture matches the agent constraints.

## 5. Reporting & evidence
- [ ] When publishing smoke/baseline runs, include the required artifacts (`meta.json`, `commands.txt`, `summary.md`, per-step logs) under `artifacts/…` as demanded by `docs/TEST_SCRIPT_COORDINATOR.md:160-199`.
- [ ] Document any pipeline/test regressions in `docs/RC_AUTOFIX_CICD.md` or similar briefing docs and flag them for the Test & Script Coordinator.

## Traçabilité build/smoke 17/02/2026

- Succès : esp32dev, esp32_release, esp8266_oled
- Échec : ui_rp2040_ili9488, ui_rp2040_ili9486
- Evidence : logs et artefacts dans hardware/firmware/artifacts/build, logs/
- Actions tentées : correction du filtre build_src_filter, création de placeholder, relance build/smoke
- Problème persistant : échec RP2040 (sources/configuration à investiguer)
- Prochaine étape : escalade à un agent expert RP2040 ou hand-off

## [2026-02-18] Story portable + V3 serial migration

- [x] Added host-side story generation library `lib/zacus_story_gen_ai` (Yamale + Jinja2) with CLI:
  - `story-gen validate`
  - `story-gen generate-cpp`
  - `story-gen generate-bundle`
  - `story-gen all`
- [x] Replaced legacy generator entrypoint `hardware/libs/story/tools/story_gen/story_gen.py` with compatibility wrapper delegating to `zacus_story_gen_ai`.
- [x] Migrated portable runtime internals to tinyfsm-style state handling in `lib/zacus_story_portable/src/story_portable_runtime.cpp` while keeping `StoryPortableRuntime` facade.
- [x] Introduced Story serial V3 JSON-lines handlers (`story.status`, `story.list`, `story.load`, `story.step`, `story.validate`, `story.event`) and removed `STORY_V2_*` routing from canonical command list.
- [x] Updated run matrix/log naming to include environment label:
  - log: `logs/rc_live/<env_label>_<ts>.log`
  - artifacts: `artifacts/rc_live/<env_label>_<ts>/summary.md`
- [x] `run_matrix_and_smoke.sh` now supports single-board mode (`ZACUS_ENV="freenove_esp32s3"`): ESP8266/UI-link/story-screen checks are emitted as `SKIP` with detail `not needed for combined board`.
- [x] `tools/test/resolve_ports.py` now honors `--need-esp32` and `--need-esp8266` explicitly via required roles, while still emitting both `esp32` and `esp8266` fields in JSON output.
[20260218-020041] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041/summary.md
[20260218-021042] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042/summary.md
