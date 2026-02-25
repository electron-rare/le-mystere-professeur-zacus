# HAL_VERIFCATOR_STATUS

## Objectif
Verifier que les couches hardware (camera, micro, amp) et les 4 LED WS2812 Freenove sont bien activees/desactivees selon la scene et le besoin runtime.

## Verifications
- `SCENE_GOTO` doit retourner `ACK ... ok=1` pour chaque scene testee.
- `CAM_STATUS rec_scene` doit correspondre a l'etat attendu (`cam=0|1`).
- `AMP_STATUS scene` doit correspondre a l'etat attendu (`amp=0|1`).
- `RESOURCE_STATUS mic_should_run` doit correspondre a l'etat attendu (`mic=0|1`).
- `HW_STATUS ws2812` doit correspondre a l'etat attendu (`ws2812=0|1`).
- `HW_STATUS auto` doit correspondre a la politique LED attendue (`led_auto=0|1`).
- `HW_STATUS led=R,G,B` peut etre verifie en couleur exacte (`led=R/G/B` ou `led=R,G,B`).
- Grammaire LED alignee avec `scene-verificator` et `media-manager`.
- Les cles `cam`, `amp`, `mic`, `ws2812`, `led_auto`, `led` sont optionnelles par scene.
- Aucun marqueur fatal (`PANIC`, `ASSERT`, `ABORT`, `REBOOT`, `rst:`).

## Script global associe
- `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh`

## Usage
```bash
~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200 \
  "SCENE_READY:cam=0,amp=0,mic=0,ws2812=1,led_auto=1,led=18/45/95;SCENE_LA_DETECTOR:cam=0,amp=0,mic=1,ws2812=1,led_auto=1,led=0/0/0;SCENE_MP3_PLAYER:cam=0,amp=1,mic=1,ws2812=1,led_auto=1,led=18/45/95;SCENE_QR_DETECTOR:cam=0,amp=0,mic=0,ws2812=1,led_auto=1,led=18/45/95;SCENE_FINAL_WIN:cam=0,amp=0,mic=0,ws2812=1,led_auto=1,led=252/212/92;SCENE_MEDIA_MANAGER:cam=0,amp=0,mic=0,ws2812=1,led_auto=1,led=18/45/95"

# Variante policy-only (pas de couleur exacte)
~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200 \
  "SCENE_READY:cam=0,amp=0,mic=0,ws2812=1,led_auto=1;SCENE_MP3_PLAYER:cam=0,amp=1,ws2812=1,led_auto=1"
```
