# Printables

Organisation :
- `src/` = sources éditables
- `export/pdf/` = prêts à imprimer
- `export/png/` = previews versionnés
  - `general/` = affiches, fiches et cartes hors sous-thèmes
  - `fiche-enquete/` = actes de la fiche d'enquête
  - `personnages/` = cartes personnages
  - `zones/` = visuels des zones

Convention de nommage recommandée :
- `a6` / `a5` / `a4`
- `recto` / `verso`
- `v1`, `v2`, etc.

Exemples :
- `invite-a6-recto-v1.pdf`
- `carte-indice-a6-v2.pdf`
- `regles-1-page-a4-v1.pdf`

Ordre d’impression : voir `ordre-dimpression.md`.

## État actuel
- Structure alignée : `src/`, `export/pdf/`, `export/png/`.
- Préviews PNG regroupées dans `export/png/` par sous-dossiers thématiques.
- Invitations initialisées : `invitations/src/` et `invitations/export/{pdf,png}/`.
