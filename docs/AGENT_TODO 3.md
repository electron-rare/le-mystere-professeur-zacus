
## [2026-02-26] In progress - bascule textes scene vers backend LovyanGFX

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226_110721_wip.patch`
  - `/tmp/zacus_checkpoint/20260226_110721_status.txt`
- Demande: afficher tous les textes de scene via LovyanGFX (et non labels LVGL) pour eviter regressions d'affichage.
- Plan d'execution:
  1) auditer le chemin rendu texte actuel (LVGL labels vs direct FX/LGFX)
  2) implementer un overlay texte LGFX pour toutes les scenes runtime
  3) desactiver/neutraliser les labels LVGL titre/sous-titre/symbole
  4) build + upload Freenove + verification serie
