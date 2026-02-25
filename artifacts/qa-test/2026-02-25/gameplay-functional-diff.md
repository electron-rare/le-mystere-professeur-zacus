# Rapport de diff fonctionnel — scénarios Zacus

- Date: 2026-02-25
- Base canon: `scenario-ai-coherence/zacus_conversation_bundle_v3/zacus_v2.yaml`
- Cible gameplay: `game/scenarios/zacus_v1.yaml`

## Identité
- id: `zacus_v2` -> `zacus_v1`
- version: `3` -> `2`
- title: `Le mystère du Professeur Zacus — Version Réelle (U-SON, piano LEFOU, QR WIN)` -> `Le mystère du Professeur Zacus — U-SON (promotion gameplay)`

## Cadre de session
- players: `6-14` -> `6-14`
- duration_minutes: `None-None` -> `60-90`

## Structure narrative
- stations: `3` -> `3`
- puzzles: `3` -> `3`
- solution_unique: `None` -> `True`

## Changements clés détectés
- Stations cibles: Atelier des ondes, Zone 4 — Studio de Résonance, Salle des archives
- IDs puzzles base: PUZZLE_LA_440, PUZZLE_PIANO_ALPHABET_5, PUZZLE_QR_WIN
- IDs puzzles cible: la-440, piano-lefou, qr-win
- ✅ Les stations ont été réalignées avec le gameplay U-SON / Zone 4 / Archives.
- ✅ Les puzzles ont été remplacés pour refléter LA 440 → LEFOU → QR WIN.

## Impact
- Source de vérité gameplay maintenue dans `game/scenarios/*.yaml`.
- Bundle conversationnel conservé comme matériau de travail parallèle.
- Validation G3 requise via validateurs scénario/audio/printables + runtime bundle.
