# Architecture d'intégration du conversation bundle — baseline (G1)

## Entrée / sortie
- **Entrée**: bundle conversationnel `scenario-ai-coherence/zacus_conversation_bundle_v3/*`.
- **Sortie**: ajout des fichiers dans ce sous-dossier + commit atomique + evidence.

## Interface d'intégration
- Source de vérité gameplay inchangée: `game/scenarios/*.yaml`.
- Bundle stocké comme matériau de travail IA/scénario, isolé dans `scenario-ai-coherence/`.
- Aucun couplage runtime ajouté dans ce lot.

## Budget / impact
- Impact code exécutable: nul (docs/data only).
- Impact CI: validateurs scénario/audio/printables exécutés pour non-régression.
- Bootstrap dépendances validateurs: `tools/setup/install_validators.sh`.

## Prochaine évolution proposée
- Validateur ajouté: `tools/scenario/validate_runtime_bundle.py` (cohérence entre `scenario_runtime.json`, `scenario_canonical.yaml` et `scenario_promptable_template.yaml`).
- Prochaine itération: intégration CI de ce contrôle dans la gate G3.

## Statut Gates
- G1 Arch Freeze : **satisfait** pour ce lot d'import documentaire/data.
