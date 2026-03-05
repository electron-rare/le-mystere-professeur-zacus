# Checklist scenario

## Configurer

- Scenario : Le mystère du Professeur Zacus — Version Réelle (U-SON, piano LEFOU, QR WIN)
- Fichier source : game/scenarios/zacus_v2.yaml
- Participants : 6-14
- Duree : 105-105 min
- Stations prevues : 3
- Puzzles : 3
- Preuves listees : 3

## Materiel recommande

- Enveloppes numerotees
- Badges detective + fiches d'enquete
- Audio : audio/manifests/zacus_v2_audio.yaml
- Printables : suivre printables/manifests/zacus_v2_printables.yaml

## Verifications

- python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v2.yaml
- python3 tools/audio/validate_manifest.py audio/manifests/zacus_v2_audio.yaml
- python3 tools/printables/validate_manifest.py printables/manifests/zacus_v2_printables.yaml
