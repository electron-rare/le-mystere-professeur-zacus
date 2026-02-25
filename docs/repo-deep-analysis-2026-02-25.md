# Analyse approfondie du dépôt — 2026-02-25

## Objectif
Aller plus loin que le lot conversation bundle en évaluant l'état global du repo (docs, workflows, contenu gameplay, outillage validation, CI).

## Méthode
Commandes de revue utilisées (échantillon):
- `rg --files`
- `rg -n "validate_|workflow|scenario" .github/workflows tools docs game`
- `python3 tools/scenario/validate_runtime_bundle.py`
- `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml`
- `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Constat global

### 1) Source de vérité gameplay
- ✅ `game/scenarios/zacus_v1.yaml` est bien utilisé comme source canon.
- ✅ Les exports MJ/docs générés sont cohérents avec le canon courant.
- ⚠️ Le bundle `scenario-ai-coherence/` reste parallèle: utile pour itérer, mais il faut éviter toute dérive silencieuse.

### 2) Gates G3 / validation
- ✅ Validateurs scénario/audio/printables présents et opérationnels.
- ✅ `validate_runtime_bundle.py` existe et est branché dans CI (`.github/workflows/validate.yml`).
- ⚠️ Le diff fonctionnel gameplay est scripté localement, mais pas encore exécuté automatiquement en CI.

### 3) Documentation et guidage équipe
- ✅ `docs/WORKFLOWS.md` décrit l'enchaînement validation.
- ✅ Spec/arch conversation bundle documentent le périmètre.
- ⚠️ Il manque un guide "release gate" unique qui enchaîne G0→G5 en une seule checklist opérationnelle.

### 4) Risques principaux
- Divergence entre bundle conversationnel et canon gameplay si la promotion n'est pas rituelle.
- G4/G5 encore partiels (pas de lot release/compliance structuré dans ce cycle).
- Dépendance implicite à PyYAML: bootstrap présent, mais doit rester obligatoire en CI locale/README.

## Plan de progression recommandé (prochaines PRs)

### PR-A (QA/Test)
- Ajouter un job CI dédié qui exécute aussi `tools/scenario/diff_gameplay_scenarios.py`.
- Publier l'artifact markdown de diff en sortie CI (upload artifact GitHub Actions).

### PR-B (Doc/Release)
- Créer `docs/RELEASE_GATES.md` avec checklist G0→G5 exécutable.
- Standardiser le template de PR pour exiger la section "Gates satisfaites" + "limitations".

### PR-C (Compliance)
- Ajouter un inventaire SBOM/licences minimal pour G5 (au moins dépendances Python validateurs + manifest repo).
- Centraliser les preuves dans `evidence/` avec index daté.

## Décision proposée maintenant
- Garder le canon gameplay tel quel.
- Passer sur PR-A en priorité pour fermer l'écart CI sur le diff fonctionnel.
