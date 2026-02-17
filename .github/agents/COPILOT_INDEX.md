# Copilot Agent Index

Every `.github/agents/*.md` file is a focused briefing for Copilot/GUIs; chaque fiche inclut une section `## Plan d’action` basée sur `.github/agents/PLAN_TEMPLATE.md`. Open the one matching your domain before editing. The table below summarizes each card.

| Agent file | Résumé rapide |
|---|---|
| `global.md` | Règles repo-wide (checkpoint, story points, PlatformIO/scénarios/audios/printables gate list). |
| `hardware.md` | Enclosure/BOM/wiring + validation via `hardware/firmware` builds and smoke gate scripts. |
| `audio.md` | Manifestes audio : validator + scenario export, pas de régénération binaire. |
| `game.md` | YAML scénarios/prompts; valider/exporter avec les scripts `tools/scenario`. |
| `printables.md` | Manifestes printables : validator PB, naming déterministe. |
| `tools.md` | Scripts/tools : `--help`, flags/timeout configurables, pas de paths machine. |
| `docs.md` | Docs/onboarding : updates concises, liens valides, structure préservée. |
| `kit.md` | GM kit station/export, lien jeu/printables, pas de renommage sans synchro. |
| `ci.md` | Workflows/templates : edits minimaux, CI impact reporté. |
| `firmware_core.md` | Firmware tree complet : PlatformIO builds, `docs/AGENT_TODO.md`, logs/artifacts hors git. |
| `firmware_tooling.md` | `tools/dev/` helpers : `--help`, grep-friendly logs, ports/timeouts configurables. |
| `firmware_copilot.md` | Copilot-specific firmware duties (UI Link, LittleFS, I2S, artefact tracking, sprint/runbook refs). |
| `firmware_tests.md` | Smoke/stress scripts + artifact metadata (`meta.json`, `commands.txt`, `summary.md`) et reporting. |
| `firmware_docs.md` | Firmware docs/onboarding updates, command index regen, runbook mentions. |
| `ALIGNMENT_COMPLETE.md` | Checklist de pré-lancement : AGENT contracts, runbooks, onboarding, artefacts. |
| `PHASE_LAUNCH_PLAN.md` | Checkpoint de lancement de phase : gates, artefacts, reporting RC/AGENT_TODO. |

Pour chaque fiche, consulte la section “References” pour pointer vers le contrat `AGENTS*.md` correspondant et les runbooks à mettre à jour.
