# Story specs (source de verite)

Ce dossier centralise schema, templates, prompts et scenarios pour Story V2.

Structure:
- schema/story_spec_v1.yaml
- templates/scenario.template.yaml
- prompts/*.prompt.md
- scenarios/*.yaml

Les prompts Story sont destines a l'autoring (pas aux ops), mais peuvent etre utilises par les outils Codex si besoin.

Generation (depuis hardware/firmware/esp32_audio):
- make story-validate
- make story-gen
- make qa-story-v2

Runtime event matching note:
- `event_type: button` with `event_name: ANY` is treated as a button wildcard.
- It matches button events such as `ANY`, `BTN1_SHORT`, `BTN3_LONG`, and other non-empty button names.
- Wildcard behavior is limited to `button`; other event types keep strict name matching.
