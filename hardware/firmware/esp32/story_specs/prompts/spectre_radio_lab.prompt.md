# Prompt Story — SPECTRE_RADIO_LAB

## Intent
Créer un scénario court, lisible et rejouable pour une ambiance "laboratoire radio" avec progression audio claire:
- détection LA (unlock)
- phase sonar de recherche
- indice en morse
- récompense WIN
- ouverture gate MP3

## Contraintes techniques
- format `StorySpec V1` (YAML)
- transitions uniquement non bloquantes (`on_event`)
- réutiliser les apps existantes (`APP_LA`, `APP_AUDIO`, `APP_SCREEN`, `APP_GATE`)
- conserver compatibilité série `STORY_V2_*`
- pas de changement du scénario par défaut

## Ressources visées
- scène recherche mutualisée: `SCENE_SEARCH`
- packs audio dédiés:
  - `PACK_SONAR_HINT`
  - `PACK_MORSE_HINT`
  - `PACK_WIN` (existant)

## Flux attendu
1. `STEP_WAIT_UNLOCK`
2. `STEP_SONAR_SEARCH`
3. `STEP_MORSE_CLUE`
4. `STEP_WIN`
5. `STEP_DONE`
