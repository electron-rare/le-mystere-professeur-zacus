# Glossaire des identifiants

## Scénario
- `zacus_v2` : scénario canon servant de single source of truth pour les acts, stations, puzzles et étapes runtime (voir `game/scenarios/zacus_v2.yaml`).
- `steps_narrative` : séquence ordonnée des étapes firmware (ex. `STEP_LA_DETECTOR`, `STEP_QR_DETECTOR`).

## Audio
- `intro`, `incident`, `accusation`, `solution` : identifiants des pistes dans `audio/manifests/zacus_v2_audio.yaml` qui pointent vers `game/prompts/audio/*.md`.

## Printables
Chaque `id` du manifeste `printables/manifests/zacus_v2_printables.yaml` a un prompt associé dans `printables/src/prompts/` :
- Invitations A6 : `invitation_a6_recto`, `invitation_a6_verso` → `invitation_recto.md`, `invitation_verso.md`.
- Cartes A6 : `carte_personnage_a6`, `carte_lieu_a6`, `carte_objet_a6` → `card_personnage.md`, `card_lieu.md`, `card_objet.md`.
- Feuille d’enquête : `fiche_enquete_a4` → `fiche_enquete.md`.
- Badge détective : `badge_detective_a6` → `badge_detective.md`.
- Règles A4 : `regles_a4` → `regles.md`.
- Hotline et affiches zones : `hotline_a4`, `affiches_zone_a4_z1` … `affiches_zone_a4_z6` → `hotline.md`, `zone_affiche.md`.

> Note : le `tools/printables/validate_manifest.py` vérifie que chaque `prompt` existe et reste aligné avec le YAML.
