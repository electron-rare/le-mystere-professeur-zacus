# ZIP intake — statut après réception bundle (G0 Spec Freeze)

## Contexte
Le bundle annoncé par l'utilisateur est désormais disponible sous :
`scenario-ai-coherence/zacus_conversation_bundle_v3/`.

## Critères d'acceptation (AC)
- Les fichiers du bundle sont présents localement dans le même chemin.
- Le contenu est importé tel quel (pas de transformation fonctionnelle).
- La traçabilité est mise à jour (`artifacts/...` + `evidence/manifest.json`).
- Les validateurs scénario/audio/printables existants passent après import.

## Non-goals
- Aucune promotion automatique de `zacus_v2.yaml` vers `game/scenarios/*.yaml`.
- Aucune modification firmware/hardware.
- Aucune refonte des manifests audio/printables.

## Risques
- Le bundle peut diverger du scénario canonique `game/scenarios/zacus_v1.yaml`.
- Les formats `scenario_runtime.json`/templates peuvent nécessiter une validation dédiée ultérieure.

## Statut Gates
- G0 Spec Freeze : **satisfait** pour l'import brut du bundle.
