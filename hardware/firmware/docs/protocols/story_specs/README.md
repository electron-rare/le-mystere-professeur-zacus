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
