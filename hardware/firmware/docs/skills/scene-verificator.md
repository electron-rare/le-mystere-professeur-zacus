# SCENE_VERIFICATOR

## Objectif
Valider l'enchainement des scenes story sur Freenove avec des verdicts stricts et reproductibles, y compris les triggers de test qui doivent provoquer des passages de scene.

## Verifications
- `SCENE_GOTO` doit retourner `ACK ... ok=1` pour chaque scene cible.
- Les triggers de test (`BTN_*`, `story.validate`, `STORY_*`) doivent etre emis et verifies.
- Chaque trigger doit montrer une progression (changement `SCREEN_SYNC` ou changement scene/status).
- Les alias de trigger sont geres: `BTN_*` (fallback `SC_EVENT serial ...`) et noms pointes (fallback `SC_EVENT_RAW ...`).
- `SCREEN_SYNC` doit montrer une transition de sequence, ou a defaut un changement de scene/status observable.
- `story.status` doit rester coherent avec scene/step attendus.
- Validation WS2812 stricte supportee par scene: `ws2812`, `led_auto`, `led=R/G/B`.
- Pour `CMD@SCENE->TARGET`, la LED de la scene cible est aussi verifiee si configuree.
- Aucun marqueur fatal (`PANIC`, `ASSERT`, `ABORT`, `REBOOT`, `rst:`).

## Script global associe
- `~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh`

## Usage
```bash
~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh /dev/cu.usbmodem5AB90753301 115200 \
  "SCENE_LOCKED,SCENE_LA_DETECTOR,SCENE_WIN_ETAPE,SCENE_READY" \
  "BTN_NEXT@SCENE_LOCKED,STORY_FORCE_ETAPE2@SCENE_LA_DETECTOR" \
  "SCENE_LA_DETECTOR:ws2812=1,led_auto=1,led=0/0/0;SCENE_READY:ws2812=1,led_auto=1,led=18/45/95"
```
