# Skill: ChatGPT File Exemple Intake

Use this skill when you drop pre-patch examples in `hardware/ChatGPT_file_exemple/**`.

## Purpose
- Treat example files as reference only.
- Integrate useful deltas into canonical files under `hardware/firmware/**`.
- Avoid blind copy of nested paths and duplicate compile units.

## Workflow
1. Inventory files in `hardware/ChatGPT_file_exemple/**`.
2. Map each example file to a canonical target in `hardware/firmware/**`.
3. Extract only required changes with focused diffs.
4. Patch canonical files minimally.
5. Validate with requested build/upload/smoke gates.

## Guardrails
- Do not compile files from `hardware/ChatGPT_file_exemple/**`.
- Do not commit machine-specific absolute paths.
- Report integrated vs skipped examples explicitly.

## Skill location
- `~/.codex/skills/chatgpt-file-exemple-intake/`
  - `SKILL.md`
  - `agents/openai.yaml`
  - `references/checklist.md`
  - `scripts/scan_example_candidates.sh`
