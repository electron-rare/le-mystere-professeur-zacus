# Agent Contract – Firmware Copilot

## Scope
Firmware agents cover `esp32_audio/`, `ui/esp8266_oled/`, `ui/rp2040_tft/`, and shared `protocol/` code that drives the MCU binaries.

## Doit
- Lire puis mettre à jour `docs/AGENT_TODO.md` avant d’agir (le tracker unique garde la trace de l’état de chaque gate).
- Respecter le contrat global (`AGENTS.md`) et les règles d’outillage de `tools/dev/AGENTS.md` (gates/scripts centralisés, artefacts hors git).
- S’assurer que tout build se passe via `platformio.ini`/`build_all.sh` ou `Makefile fast-*`, et documenter les artefacts dans `docs/AGENT_TODO.md`.
- Pour les modifications de structure, ajouter un commentaire dans `docs/AGENT_TODO.md` + mentionner le commit dans `docs/TEST_SCRIPT_COORDINATOR.md`.
- Toujours signaler l’état des UI Link / LittleFS / I2S dans `docs/AGENT_TODO.md:6-14` lorsqu’on manipule ces composants.

## Reporting
- Chaque run doit produire des artefacts `artifacts/<phase>/<timestamp>` ; les chemins doivent figurer dans la case correspondante du TODO.
- Données hardware (logs, ports) restent hors git : référencer les fichiers dans `docs/AGENT_TODO.md` ou le rapport final.

## References
- Branche principale : `docs/SPRINT_RECOMMENDATIONS.md`, `README.md`, `protocol/ui_link_v2.md`
- Evidence tracker : `docs/TEST_SCRIPT_COORDINATOR.md`, `docs/TEST_COHERENCE_AUDIT_RUNBOOK.md`
