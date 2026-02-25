# SCENE_VERIFICATOR

## Objectif
Valider l'enchainement des scenes story sur Freenove avec des verdicts stricts et reproductibles, y compris les triggers de test qui doivent provoquer des passages de scene.

## Verifications
- `SCENE_GOTO` doit retourner `ACK ... ok=1` pour chaque scene cible.
- Les triggers de test (`BTN_*`, `story.validate`, `STORY_*`) doivent etre emis et verifies.
- Chaque trigger doit montrer une progression (changement `SCREEN_SYNC` ou changement scene/status).
- `SCREEN_SYNC` doit montrer au moins une transition de sequence.
- `story.status` doit rester coherent avec scene/step attendus.
- Aucun marqueur fatal (`PANIC`, `ASSERT`, `ABORT`, `REBOOT`, `rst:`).

## Script global associe
- `~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh`

## Usage
```bash
~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh /dev/cu.usbmodem5AB90753301 115200 \
  "SCENE_LOCKED,SCENE_LA_DETECTOR,SCENE_WIN_ETAPE,SCENE_READY" \
  "BTN_NEXT@SCENE_LOCKED,STORY_FORCE_ETAPE2@SCENE_LA_DETECTOR,story.validate@SCENE_WIN_ETAPE"
```
