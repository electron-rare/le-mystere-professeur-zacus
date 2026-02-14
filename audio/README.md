# Pipeline audio Zacus v1

Objectif: pipeline local IA-friendly sans dépendance cloud obligatoire.

## Outils cibles
- TTS: `espeak`, `mbrola`, ou `piper`
- Conversion: `ffmpeg` (MP3 mono faible débit)

## Convention export
- Mono, 22.05 kHz ou 44.1 kHz
- 48–96 kbps
- Nommage depuis `audio/manifests/zacus_v1_audio.yaml`

## Étapes
1. Générer voix (hotline + messages U-SON).
2. Générer textures/musiques via prompts (voir `game/prompts/audio/`).
3. Normaliser volume.
4. Valider présence des fichiers avec `tools/audio/validate_manifest.py`.

## Génération locale rapide (placeholder)
Si vous voulez débloquer rapidement des tests sans TTS externe:
- `python3 tools/audio/generate_local_assets.py`

Ce script crée `audio/generated/*` en réutilisant des MP3 présents dans le repo (placeholders techniques).
