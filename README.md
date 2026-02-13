# Le Mystère du Professeur Zacus

Escape game / enquête modulaire assisté par IA (9–11 ans, 6–14 enfants, 60–90 min).

## Démarrage rapide
- Quickstart: `docs/QUICKSTART.md`
- Canon scénario: `game/scenarios/zacus_v1.yaml`
- Kit MJ prêt à jouer: `kit-maitre-du-jeu/`

## Structure
- `game/` : source de vérité scénario + prompts IA
- `kit-maitre-du-jeu/` : animation minute-par-minute, solution, anti-chaos
- `printables/` : prompts et exports imprimables
- `audio/` : manifest audio + pipeline local
- `hardware/` : firmware existant + intégration scénario
- `tools/` : validateurs scénario/audio

## Validation
- `python3 tools/scenario/validate_scenario.py`
- `python3 tools/audio/validate_manifest.py`

## Licence
- **Code**: MIT (`LICENSES/MIT.txt`)
- **Contenus créatifs/docs/printables**: CC BY-NC 4.0 (`LICENSES/CC-BY-NC-4.0.txt`)
- Historique licences: `LICENSES/legacy/`
