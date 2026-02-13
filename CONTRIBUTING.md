# Contribuer

Merci de contribuer à Zacus.

## Principes
- Jouable 9–11 ans, 6–14 enfants, 60–90 min.
- Lisibilité impression N&B prioritaire.
- Aucune contradiction avec le canon (`game/scenarios/zacus_v1.yaml`).

## Workflow recommandé
1. Créer une branche (`feat/...` ou `fix/...`).
2. Modifier les sources (pas uniquement les exports).
3. Lancer les validateurs:
   - `python3 tools/scenario/validate_scenario.py`
   - `python3 tools/audio/validate_manifest.py`
4. Mettre à jour `CHANGELOG.md`.

## Licence des contributions
- **Code**: MIT.
- **Contenus créatifs/docs/printables**: CC BY-NC 4.0.

En contribuant, vous acceptez que votre apport soit publié sous ce schéma dual.
