# MEDIA_MANAGER

## Objectif
Verifier le flux QR final, la persistance NVS du boot mode `MEDIA_MANAGER`, et la disponibilite des entrees Photo/MP3/Story avec rollback explicite.

## Verifications
- `BOOT_MODE_STATUS` doit exposer un etat coherent (`mode`, `media_validated`).
- Le script force `SCENE_GOTO SCENE_CAMERA_SCAN` avant `QR_SIM <payload>` pour un test deterministe.
- `QR_SIM` doit produire une evidence `UI_EVENT` (`SERIAL:QR_OK` ou `QR_OK`) et/ou un routage `SCENE_MEDIA_MANAGER`.
- Scope ESP-NOW actif: `SC_EVENT espnow QR_OK` doit aussi permettre le passage `STEP_DONE -> STEP_MEDIA_MANAGER`.
- La persistence doit rester active apres `RESET` (retour `SCENE_MEDIA_MANAGER`).
- La verification post-reset repose sur `HW_STATUS scene=...` (pas de dependance `story.status`).
- Les scenes hub doivent etre atteignables: `SCENE_PHOTO_MANAGER`, `SCENE_MP3_PLAYER`, story (`SC_LOAD DEFAULT` + `SCENE_READY`).
- Validation WS2812 stricte sur scenes critiques: `ws2812`, `led_auto`, `led=R/G/B`.
- Rollback obligatoire via `BOOT_MODE_SET STORY` puis `BOOT_MODE_CLEAR`.
- Echec strict si `PANIC|ASSERT|ABORT|REBOOT|rst:`.

## Script global associe
- `~/.codex/skills/media-manager/scripts/run_media_manager_verification.sh`

## Usage
```bash
~/.codex/skills/media-manager/scripts/run_media_manager_verification.sh /dev/cu.usbmodem5AB90753301 115200 ZACUS:MEDIA_MANAGER \
  "SCENE_CAMERA_SCAN:ws2812=1,led_auto=1,led=18/45/95;SCENE_MEDIA_MANAGER:ws2812=1,led_auto=1,led=18/45/95;SCENE_MP3_PLAYER:ws2812=1,led_auto=1,led=18/45/95;SCENE_READY:ws2812=1,led_auto=1,led=18/45/95"
```
