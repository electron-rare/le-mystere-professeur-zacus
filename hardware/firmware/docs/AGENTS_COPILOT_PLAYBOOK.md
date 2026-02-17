# Copilot Agent Playbook

## Per-spec agents
- `.github/agents/global.md` – repo-wide governance, checkpoints, story-point sourcing, et PlatformIO/scénario/audio/printables gates.
- `.github/agents/hardware.md` – enclosure/BOM/wiring + firmware integration builds/smoke, ports/baud monitoring.
- `.github/agents/audio.md` – audio manifest + export validation (`tools/audio/validate_manifest.py`, scenario export).
- `.github/agents/game.md` – scenarios/prompts (YAML source of truth) avec les validators `tools/scenario/validate_scenario.py` et `export_md`.
- `.github/agents/printables.md` – printable manifest discipline et naming déterministe.
- `.github/agents/tools.md` – scripts tooling CLI (`--help`, timeout/port flags, non interactif).
- `.github/agents/docs.md` – docs et onboarding : updates ciblées, liens et structure préservés.
- `.github/agents/kit.md` – GM kit station/export, liens game/printables, aucun renommage sans synchronisation.
- `.github/agents/ci.md` – workflows/templates : minimal changes + CI impact reporting.
- `.github/agents/firmware_core.md` – firmware full tree : PlatformIO builds, `docs/AGENT_TODO.md`, logs/artifacts hors git.
- `.github/agents/firmware_tooling.md` – `tools/dev/` helpers (logs [step]/[ok]/[fail], `--help`, port/timeout flags).
- `.github/agents/firmware_copilot.md` – Copilot-specific firmware duties (UI Link, LittleFS, I2S, artefacts, sprint/runbook refs).
- `.github/agents/firmware_tests.md` – smoke/stress runners + artifact metadata (`meta.json`, `commands.txt`, `summary.md`) et reporting.
- `.github/agents/firmware_docs.md` – firmware docs/onboarding updates, command index regen, runbook mentions.
- `.github/agents/ALIGNMENT_COMPLETE.md` – pré-lancement : cohérence AGENT contracts, runbooks, onboarding, artefacts.
- `.github/agents/PHASE_LAUNCH_PLAN.md` – lancement de phase : liste des gates, artefacts/artefacts metadata, reporting RC.
## Skills for Copilot (see `agents/openai.yaml`)
1. **firmware** – PlatformIO matrix, serial smoke, UI link verdicts; run `./build_all.sh` and `tools/dev/serial_smoke.py` helpers before reporting.
2. **tooling** – CLI helpers; keep `--help`, non-interactive defaults, and `[step]` logging.
3. **docs** – docs updates; stay focused, verify links, preserve indexes.
4. **printables** – manifest hygiene; update sources before derived assets.
5. **repo_hygiene** – checkpoints, artifact untracking, portable naming; follow `/tmp/zacus_checkpoint` snapshot flow.

## Key observations to keep Copilot aligned
- `docs/AGENTS_INDEX.md` is the canonical map of gates/quick commands for every agent.
- `.github/agents/AGENT_BRIEFINGS.md` raconte les règles git/agents et liste chaque fiche métier.
- `.github/agents/COPILOT_INDEX.md` aide à choisir rapidement la fiche front-end/GitOps.
- `hardware/firmware/docs/AGENT_TODO.md` is the evidence tracker for builds, smoke/stress runs, UI Link/LittleFS/I2S, and WebSocket/HTTP health—log each artifact path when reporting.
- Build artifacts/logs live outside git; reference their directories/timestamps in reports instead of committing them.
- Skills and AGENT contracts work hand-in-hand: trigger the right skill, then obey the contract file(s) tied to that business spec.
