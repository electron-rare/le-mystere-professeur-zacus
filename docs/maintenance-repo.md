# Plan de mise à jour et nettoyage du dépôt (hors firmware)

Ce plan se concentre sur la **qualité documentaire et éditoriale** du projet,
sans modifier le code firmware.

## Objectifs

- Rendre le dépôt plus lisible pour un nouveau contributeur en moins de 5 minutes.
- Aligner la documentation avec le contenu réellement versionné.
- Installer une routine légère de maintenance (changelog, structure, revue).

## Priorités recommandées

### 1) Clarifier l'état des sous-projets

- Conserver une vue d'ensemble au `README.md`.
- Indiquer explicitement ce qui est:
  - prêt à l'usage,
  - en cours de structuration,
  - à compléter.

### 2) Assainir la partie printables

- Créer une arborescence cible claire et versionnée:
  - `printables/src/`
  - `printables/export/pdf/`
  - `printables/export/png/`
- Ajouter des `README.md` courts dans ces dossiers avec:
  - format attendu,
  - convention de nommage,
  - checklist pré-impression.

### 3) Normaliser l'historique

- Garder `CHANGELOG.md` à jour à chaque PR (même doc-only).
- Utiliser une structure stable:
  - `Ajouté`, `Modifié`, `Corrigé`.
- Éviter les sections vagues dans `Unreleased` (préférer des items actionnables).

### 4) Renforcer la contribution éditoriale

- Ajouter une mini-checklist PR dans `CONTRIBUTING.md`:
  - cohérence narrative (indices/cartes/solution),
  - lisibilité impression N&B,
  - mise à jour changelog,
  - aperçu PNG si document visuel.

### 5) Préparer la publication

- Ajouter un jalon de release documentaire:
  - version,
  - lot de fichiers figés,
  - date de validation.
- Maintenir `docs/index.md` comme point d'entrée simple vers les sections stables.

## Routine de maintenance proposée (mensuelle)

1. Vérifier les liens internes cassés (`README`, `docs`, sous-dossiers).
2. Vérifier la cohérence des conventions de nommage printables.
3. Regrouper les petits correctifs dans une seule PR doc.
4. Mettre à jour `CHANGELOG.md` avant merge.

## Checklist PR "doc-only"

- [ ] Aucun changement firmware.
- [ ] Références de fichiers/dossiers exactes.
- [ ] Cohérence de ton et vocabulaire (FR).
- [ ] `CHANGELOG.md` mis à jour.
- [ ] Diff relu pour supprimer le bruit (formatage inutile, doublons).
