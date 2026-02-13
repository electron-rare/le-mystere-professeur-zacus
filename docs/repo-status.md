# État rapide du dépôt (audit automatique)

Date audit: 2026-02-13

## 1) Inventaire et TODO détectés
- Dossiers principaux présents: `kit-maitre-du-jeu/`, `printables/`, `hardware/`, `docs/`, `examples/`.
- Plusieurs fichiers “À compléter” ont été trouvés dans le kit MJ (script, solution, checklist, anti-chaos, etc.) puis remplis dans cette itération.

## 2) Problèmes détectés
- **Fichiers incomplets**: 6 fichiers clés du kit MJ étaient des placeholders (`> À compléter`).
- **Incohérence licences**:
  - `README.md` et `CONTRIBUTING.md` mélangeaient deux modèles (CC BY-SA/GPL et CC BY-NC/MIT).
  - `LICENSE.md` annonçait CC BY-SA + GPL.
- **Noms non portables**:
  - Dossier `include humain:IA/` (caractère `:`).
  - Plusieurs fichiers avec accents et espaces (tolérés localement, mais fragiles pour certains scripts CI multi-OS).
- **Chemins/lisibilité**:
  - README avec sections dupliquées.
  - Liens de navigation “quickstart/canon” absents de `docs/index.md`.

## 3) Correctifs appliqués
- Mise en place d’un **canon scénario**: `game/scenarios/zacus_v1.yaml`.
- Création d’une pipeline IA-friendly: prompts printables/audio + manifests + validateurs Python.
- Harmonisation licence:
  - **Créatif/docs/printables**: CC BY-NC 4.0.
  - **Code/scripts/firmware**: MIT.
  - Anciennes licences déplacées en `LICENSES/legacy/`.
- Ajout docs d’accès rapide: `docs/QUICKSTART.md`, `docs/STYLEGUIDE.md`, `docs/GLOSSARY.md`.

## 4) Risques restants (hors scope immédiat)
- Le dossier historique `include humain:IA/` reste non renommé pour éviter une migration destructive.
- Vérification exhaustive de tous les liens binaires/non-Markdown non réalisée (audit orienté docs/scénario).
