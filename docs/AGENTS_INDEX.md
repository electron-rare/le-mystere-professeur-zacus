# AGENTS Index

## Subproject Map
- Root contract: [`AGENTS.md`](../AGENTS.md)
- Firmware: [`hardware/firmware/AGENTS.md`](../hardware/firmware/AGENTS.md)
- Firmware tooling: [`hardware/firmware/tools/dev/AGENTS.md`](../hardware/firmware/tools/dev/AGENTS.md)
- Docs: [`docs/AGENTS.md`](AGENTS.md)
- Game content: [`game/AGENTS.md`](../game/AGENTS.md)
- Printables: [`printables/AGENTS.md`](../printables/AGENTS.md)
- Repo meta/CI: [`.github/AGENTS.md`](../.github/AGENTS.md)

# Exécution planifiée
- Utiliser `tools/dev/plan_runner.sh --agent <name>` pour dérouler automatiquement la section `## Plan d’action` (ajoute `--dry-run` ou `--plan-only` pour prévisualiser).
- Depuis Copilot/VS Code, exécuter `hardware/firmware/tools/dev/codex_prompts/trigger_firmware_core_plan.prompt.md` pour lancer `tools/dev/plan_runner.sh --agent firmware_core`.

# Briefings personnalisés
- `.github/agents/AGENT_BRIEFINGS.md` – récit complet des politiques de git/agents + liste des fiches métier.
- `.github/agents/COPILOT_INDEX.md` – table d’aide rapide pour sélectionner le bon agent Copilot dans l’interface.

## Entry Commands
- Firmware build matrix: `cd hardware/firmware && ./build_all.sh`
- Firmware smoke (local hardware): `bash hardware/firmware/tools/test/hw_now.sh`
- Scenario validate/export: `python3 tools/scenario/validate_scenario.py <scenario>` then `python3 tools/scenario/export_md.py <scenario>`
- Audio manifest validate: `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- Printables manifest validate: `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Primary Gates
- `pio run -e esp32dev`
- `pio run -e esp32_release`
- `pio run -e esp8266_oled`
- `pio run -e ui_rp2040_ili9488`
- `pio run -e ui_rp2040_ili9486`

## Skills
- Firmware skill: [`skills/firmware/SKILL.md`](../skills/firmware/SKILL.md)
- Tooling skill: [`skills/tooling/SKILL.md`](../skills/tooling/SKILL.md)
- Docs skill: [`skills/docs/SKILL.md`](../skills/docs/SKILL.md)
- Printables skill: [`skills/printables/SKILL.md`](../skills/printables/SKILL.md)
- Repo hygiene skill: [`skills/repo_hygiene/SKILL.md`](../skills/repo_hygiene/SKILL.md)
- Optional registry: [`agents/openai.yaml`](../agents/openai.yaml)
