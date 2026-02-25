# HAL_VERIFCATOR_STATUS

## Objectif
Verifier que les couches hardware (camera, micro, amp) sont bien activees ou desactivees selon la scene et le besoin runtime.

## Verifications
- `SCENE_GOTO` doit retourner `ACK ... ok=1` pour chaque scene testee.
- `CAM_STATUS rec_scene` doit correspondre a l'etat attendu (`cam=0|1`).
- `AMP_STATUS scene` doit correspondre a l'etat attendu (`amp=0|1`).
- `RESOURCE_STATUS mic_should_run` doit correspondre a l'etat attendu (`mic=0|1`).
- Aucun marqueur fatal (`PANIC`, `ASSERT`, `ABORT`, `REBOOT`, `rst:`).

## Script global associe
- `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh`

## Usage
```bash
~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200 \
  "SCENE_READY:cam=0,amp=0,mic=0;SCENE_LA_DETECTOR:cam=0,amp=0,mic=1;SCENE_MP3_PLAYER:cam=0,amp=1,mic=0;SCENE_CAMERA_SCAN:cam=1,amp=0,mic=0"
```
