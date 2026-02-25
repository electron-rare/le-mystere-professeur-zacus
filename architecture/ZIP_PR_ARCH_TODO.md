# Architecture d'intégration ZIP — implémentation minimale (G1)

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

## Statut Gates
- G1 Arch Freeze : **satisfait** pour ce lot d'import documentaire/data.
