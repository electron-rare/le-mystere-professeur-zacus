# Prompt Story — SPECTRE_RADIO_LAB

## Intent
Creer un scenario court, lisible et rejouable pour une ambiance "laboratoire radio" avec progression audio claire:
- detection LA (unlock)
- phase sonar de recherche
- indice en morse
- recompense WIN
- ouverture gate MP3

## Contraintes techniques
- format StorySpec V1 (YAML)
- transitions uniquement non bloquantes (on_event)
- reutiliser les apps existantes (APP_LA, APP_AUDIO, APP_SCREEN, APP_GATE)
- conserver compatibilite serie STORY_V2_*
- pas de changement du scenario par defaut

## Config LA (optionnel)
- `hold_ms`: 100..60000 (defaut 3000)
- `unlock_event`: nom d'event (defaut `UNLOCK`)
- `require_listening`: true/false (defaut true)

## Ressources visees
- scene recherche mutualisee: SCENE_SEARCH
- packs audio dedies:
  - PACK_SONAR_HINT
  - PACK_MORSE_HINT
  - PACK_WIN (existant)

## Flux attendu
1. STEP_WAIT_UNLOCK
2. STEP_SONAR_SEARCH
3. STEP_MORSE_CLUE
4. STEP_WIN
5. STEP_DONE
