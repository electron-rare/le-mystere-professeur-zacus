# Conversation bundle intake — statut courant (G0 Spec Freeze)

## Contexte
Le bundle conversationnel est maintenu dans :
`scenario-ai-coherence/zacus_conversation_bundle_v3/`.

## Critères d'acceptation (AC)
- Les fichiers du bundle sont présents localement dans le même chemin.
- Le contenu est importé tel quel (pas de transformation fonctionnelle).
- La traçabilité est mise à jour (`artifacts/...` + `evidence/manifest.json`).
- Les validateurs scénario/audio/printables existants passent après import.
- L'installation des validateurs est reproductible par script versionné.

## Non-goals
- Aucune promotion automatique de `zacus_v2.yaml` vers `game/scenarios/*.yaml` (promotion manuelle validée dans ce lot).
- Aucune modification firmware/hardware.
- Aucune refonte des manifests audio/printables.

## Risques
- Le bundle peut diverger du scénario canonique `game/scenarios/zacus_v1.yaml`.
- Les formats `scenario_runtime.json`/templates peuvent nécessiter une validation dédiée ultérieure.

## Prochaine étape proposée
- Validateur dédié implémenté: `tools/scenario/validate_runtime_bundle.py` (cohérence runtime/canonical/template).
- Diff fonctionnel implémenté: `tools/scenario/diff_gameplay_scenarios.py` (rapport markdown QA).
- Étape suivante: brancher le validateur runtime-bundle dans l'automatisation CI (gate G3 systématique).

## Statut Gates
- G0 Spec Freeze : **satisfait** pour l'import brut du bundle.
