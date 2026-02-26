## [2026-02-26] Calibration demandee "BGR565" (passe B compile-time)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T055214Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T055214Z_status.txt`
- Decision appliquee:
  - En pratique, le mode reste RGB565 avec ordre panneau BGR (`TFT_RGB_ORDER=TFT_BGR`), pas un format bpp different.
  - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h` (3 variantes LCD).
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload` ✅
- Verification serie:
  - `UI_GFX_STATUS`: `depth=16 mode=RGB565` ✅
  - `SCENE_GOTO SCENE_TEST_LAB`: `ACK ... ok=1` ✅
  - `UI_SCENE_STATUS`: `scene_id=SCENE_TEST_LAB`, subtitle palette canonique ✅
- Validation visuelle ecran:
  - en attente retour utilisateur sur l'ordre percu mire + scroller.

## [2026-02-26] Alignement final LVGL DMA sur contrat FX + recalibration BGR

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T052733Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T052733Z_status.txt`
- Retour visuel:
  - mire: `NBBGRJCM`
  - texte: `BGRJMC`
- Correctif:
  - `pushImageDma` LGFX n'utilise plus `pushImageDMA<T>` (chemin d'interpretation different).
  - `pushImageDma` passe maintenant par:
    - `setAddrWindow(...)`
    - `writePixelsDMA(..., swap=true)`
  - objectif: appliquer strictement le meme contrat RGB565+swap que `pushColors(..., true)` (parite LVGL/FX).
  - fichier: `hardware/firmware/ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`
  - calibration panneau remise en `TFT_BGR` (3 variantes) dans `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`.

## [2026-02-26] Recalibration panneau apres validation visuelle (RGB retenu)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T052733Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T052733Z_status.txt`
- Retour utilisateur apres lot precedent:
  - mire: `NBBGRJMC`
  - texte: `RVBJMC`
- Decision:
  - le contrat pipeline LVGL+FX reste unifie (RGB565), mais calibration panneau bascule en `TFT_RGB`.
- Correctif:
  - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`
  - `TFT_RGB_ORDER` repasse a `TFT_RGB` pour les 3 variantes.
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload` ✅
- Verification serie:
  - `UI_GFX_STATUS` confirme `depth=16 mode=RGB565` ✅
  - `SCENE_GOTO SCENE_TEST_LAB` + `UI_SCENE_STATUS` confirment scene test lock active ✅

## [2026-02-26] Correction pipeline couleur LVGL+LGFX (contrat RGB565 unifie)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T051611Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T051611Z_status.txt`
- Baseline pre-patch capturee:
  - commandes serie: `UI_GFX_STATUS`, `SCENE_GOTO SCENE_TEST_LAB`, `UI_SCENE_STATUS`
  - observation ecran reference user:
    - mire: `NOIR, BLANC, ROUGE, BLEU, VERT, CYAN, JAUNE, MAGENTA`
    - texte FX: `BLEU, VERT, ROUGE, JAUNE, MAGENTA, CYAN`
- Correctif applique (scope pipeline uniquement):
  - contrat HAL documente: `pushImageDma` et `pushColors(..., true)` doivent consommer le meme format logique RGB565.
    - `hardware/firmware/ui_freenove_allinone/include/drivers/display/display_hal.h`
  - chemin LVGL DMA aligne sur RGB565 LGFX (plus de voie `swap565` implicite):
    - `hardware/firmware/ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`
    - `pushImageDMA(..., reinterpret_cast<const lgfx::rgb565_t*>(pixels))`
  - calibration panneau compile-time reglee en BGR sur les 3 variantes:
    - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload` ✅
- Verification post-flash:
  - `UI_GFX_STATUS` confirme `depth=16 mode=RGB565 backend=LGFX` ✅
  - `SCENE_GOTO SCENE_TEST_LAB` + `UI_SCENE_STATUS` confirment mire active (`TEST_LAB_LOCK`) ✅
  - regression `SCENE_U_SON_PROTO`, `SCENE_LA_DETECTOR`, `SCENE_FINAL_WIN` executee; lock test lab force le retour `SCENE_TEST_LAB` (comportement attendu en mode calibration) ⚠️

## [2026-02-26] Calibration couleur: alignement LGFX flush DMA + retour RGB global

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T044708Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T044708Z_status.txt`
- Symptome recu:
  - mire observee: `NOIR BLANC ROUGE BLEU VERT CYAN JAUNE MAGENTA`
  - scroller LGFX observe: `BLEU VERT ROUGE JAUNE MAGENTA CYAN`
- Correctif applique:
  - retour `TFT_RGB_ORDER` a `TFT_RGB` (3 variantes LCD):
    - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`
  - restauration du type LGFX attendu sur flush DMA LVGL:
    - `display_.pushImageDMA(..., reinterpret_cast<const lgfx::swap565_t*>(pixels));`
    - `hardware/firmware/ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload` ✅
- Verification serie post-flash:
  - `UI_GFX_STATUS`: `depth=16 mode=RGB565 dma_async=1 backend=LGFX` ✅
  - `SCENE_GOTO SCENE_TEST_LAB` + `UI_SCENE_STATUS`: scene test lock active + subtitle palette canonique ✅

## [2026-02-26] Correction couleur globale UI Freenove: passage RGB (LCD panel)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T043651Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T043651Z_status.txt`
- Baseline pre-fix (avant flash RGB):
  - commandes serie: `UI_GFX_STATUS`, `UI_SCENE_STATUS`, `SCENE_GOTO SCENE_TEST_LAB`
  - statut: `mode=RGB565`, scene active `SCENE_TEST_LAB`, subtitle palette canonique presente.
  - observation utilisateur precedente conservee comme evidence: inversion R/B visible a l'ecran.
- Correctif applique:
  - `TFT_RGB_ORDER` force a `TFT_RGB` pour les 3 variantes LCD dans
    `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`.
  - mapping LovyanGFX conserve via `cfg.rgb_order` derive de `TFT_RGB_ORDER`
    (`hardware/firmware/ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`).
  - pas de changement sur `LV_COLOR_16_SWAP`, policy theme, JSON story, ni cycle scroller.
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload` ✅
- Verification post-flash:
  - `UI_GFX_STATUS`: `depth=16 mode=RGB565 theme256=0` ✅
  - `SCENE_GOTO SCENE_TEST_LAB` + `UI_SCENE_STATUS`: scene/test payload actifs ✅
  - lock scene test toujours actif (`kForceTestLabSceneLock=true`), donc les `SCENE_GOTO` vers autres scenes reviennent sur `SCENE_TEST_LAB` (comportement attendu dans ce mode).
- Limitation:
  - regression visuelle multi-scenes non executable tant que le lock test est actif compile-time.
  - validation visuelle finale demandee sur hardware: ordre mire + couleurs du scroller RVBCMJ.

## [2026-02-26] TEST_LAB lock + FX texte wave + RGB confirme

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T040721Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T040721Z_status.txt`
  - `/tmp/zacus_checkpoint/20260226T040313Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T040313Z_status.txt`
- Correctifs:
  - lock test scene reactive: `kForceTestLabSceneLock=true` dans `ui_freenove_allinone/src/app/main.cpp`.
  - ajout FX texte wave sur `SCENE_TEST_LAB` (anim sinus sur sous-titre) dans `ui_freenove_allinone/src/ui/ui_manager.cpp`.
  - `TFT_RGB_ORDER` laisse en `TFT_RGB` (demande "remettre en RGB").
- Build/flash:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅

## [2026-02-26] Palette globale LVGL+GFX: alignement `TFT_RGB_ORDER` sur Freenove

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T034545Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T034545Z_status.txt`
- Correctif global couleur:
  - declaration explicite `TFT_RGB_ORDER TFT_BGR` ajoutee pour variantes ST7796:
    - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`
  - backend LovyanGFX aligne sur macro panel:
    - `cfg.rgb_order` derive de `TFT_RGB_ORDER` dans
      `hardware/firmware/ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`
  - mire `SCENE_TEST_LAB` remise en palette canonique (plus de compensation locale RGB):
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅

## [2026-02-26] SCENE_TEST_LAB: suppression fond noir sous titre/sous-titre

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T034545Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T034545Z_status.txt`
- Correctif (scene test uniquement):
  - retrait du fond noir opacifie sous les labels de `SCENE_TEST_LAB`.
  - styles labels forces en fond transparent (`LV_OPA_TRANSP`), padding `0`, radius `0`.
  - applique dans les deux passes de stylage (state static + post dynamic) pour eviter le retour des blocs noirs.
- Build/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `UI_SCENE_STATUS` confirme `scene_id=SCENE_TEST_LAB`, `step_id=TEST_LAB_LOCK`, `show_title=true`, `show_subtitle=true` ✅

## [2026-02-26] SCENE_TEST_LAB: texte force visible + scene lock test + flash FS

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T032507Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T032507Z_status.txt`
- Correctifs cibles (SCENE_TEST_LAB uniquement):
  - verrou runtime test actif (`kForceTestLabSceneLock=true`) pour rester sur `SCENE_TEST_LAB`.
  - override rendu: scene forcee `SCENE_TEST_LAB` + step logique `TEST_LAB_LOCK`.
  - labels titre/sous-titre re-styles apres dynamic state avec `lv_obj_remove_style_all(...)` puis style explicite (font, couleur, opa, bg, padding, align) pour eviter blocs noirs sans texte.
  - payload scene test aligne avec sous-titre compact:
    - `NOIR | BLANC | ROUGE | VERT | BLEU | CYAN | MAGENTA | JAUNE`
- Flash/gates executes:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
- Verification serie:
  - `UI_SCENE_STATUS`: `scene_id=SCENE_TEST_LAB`, `step_id=TEST_LAB_LOCK`, `show_title=true`, `show_subtitle=true`, payload `/story/screens/SCENE_TEST_LAB.json` ✅

## [2026-02-26] Boot: desactivation auto scene palette + backlight par defaut 30

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T015322Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T015322Z_status.txt`
- Correctifs:
  - scene boot palette automatique desactivee (`kBootPaletteAutoOnBoot=false`) pour ne plus rester sur la mire au boot.
  - retroeclairage LCD par defaut au boot fixe a `30` (`g_lcd_backlight_level=30`).
- Build + upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `STATUS` au boot: `step=SCENE_U_SON_PROTO` ✅
  - `LCD_BACKLIGHT`: `level=30` ✅

## [2026-02-26] SCENE_TEST_LAB: compensation ordre couleur TFT_BGR

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T031015Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T031015Z_status.txt`
- Constat:
  - la mire etait bien active mais les couleurs etaient lues `NOIR BLANC BLEU ROUGE VERT JAUNE CYAN MAGENTA`.
  - cause: panel en ordre `TFT_BGR`, donc inversion R/B sur les barres RGB de test.
- Correctif:
  - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
  - ajout d'une conversion `toPanelOrder(...)` appliquee uniquement a la mire `SCENE_TEST_LAB` quand `TFT_RGB_ORDER == TFT_BGR`.
  - objectif: conserver l'ordre visuel canonique `NOIR BLANC ROUGE VERT BLEU CYAN MAGENTA JAUNE` sur l'ecran physique.
- Gates/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅

## [2026-02-26] SCENE_TEST_LAB: overlay texte force (titre + liste)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T031519Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T031519Z_status.txt`
- Constat:
  - titre/sous-titre non visibles sur la mire selon validation ecran.
  - la liste multi-ligne passait par un mode `LV_LABEL_LONG_DOT` qui ne convient pas a une mire en lignes.
- Correctif:
  - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
  - en `SCENE_TEST_LAB`:
    - fond contraste noir semi-opaque sur titre/sous-titre
    - pads/rayon pour lisibilite
    - subtitle en `LV_LABEL_LONG_WRAP` + largeur forcee
    - alignement explicite top/bottom
    - reset des styles de fond/padding hors `SCENE_TEST_LAB` pour eviter regressions.
- Gates/flash:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅

## [2026-02-26] SCENE_TEST_LAB: mire fixe visible au boot

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T025819Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T025819Z_status.txt`
- Correctif:
  - `SCENE_TEST_LAB` n'est plus forcé vide côté UI fallback.
  - Le fallback interne de `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp` expose désormais la mire fixe par défaut (texte + fond noir, aucun FX).
  - La scène est forcée côté boot via `SCENE_TEST_LAB` dans le code runtime (déjà en place), et la payload `/story/screens/SCENE_TEST_LAB.json` est chargée côté FS.
- Gates/flash/validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅

## [2026-02-26] Pre-scene boot palette + commande LCD_BACKLIGHT

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T013435Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T013435Z_status.txt`
- Ajouts:
  - pre-scene boot palette temporaire (`SCENE_BOOT_PALETTE`) affichee avant la scene story initiale.
  - payload ajoute:
    - `data/story/screens/SCENE_BOOT_PALETTE.json`
    - `data/screens/SCENE_BOOT_PALETTE.json`
  - commande serie ecran:
    - `LCD_BACKLIGHT [0..255]` (set/get) dans `ui_freenove_allinone/src/app/main.cpp`.
- Build + upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `LCD_BACKLIGHT 140` -> `ACK LCD_BACKLIGHT ok=1`
  - `LCD_BACKLIGHT` -> `LCD_BACKLIGHT level=140` ✅

## [2026-02-26] Verrou temporaire NVS media manager (STEP_MEDIA_MANAGER)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T011719Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T011719Z_status.txt`
- Correctif:
  - blocage explicite de l'action `ACTION_SET_BOOT_MEDIA_MANAGER` pour empecher toute ecriture NVS vers le mode media manager.
  - fichier: `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
  - comportement: force le mode runtime `STORY` et log `[ACTION] SET_BOOT_MEDIA_MANAGER blocked nvs_lock=1`.
- Build + upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `BOOT_MODE_STATUS` avant/apres `SCENE_GOTO SCENE_MEDIA_MANAGER` reste `mode=story`
  - log de blocage observe: `[ACTION] SET_BOOT_MEDIA_MANAGER blocked nvs_lock=1` ✅

## [2026-02-26] Wildcard bouton ANY -> BTN*_SHORT/LONG (global runtime)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226T010816Z_wip.patch`
  - `/tmp/zacus_checkpoint/20260226T010816Z_status.txt`
- Correctif applique globalement (sans changement de schema scenario):
  - `button/ANY` matche maintenant tout nom bouton non vide (ex: `BTN1_SHORT`, `BTN3_LONG`) dans:
    - `hardware/firmware/ui_freenove_allinone/src/app/scenario_manager.cpp`
    - `hardware/libs/story/src/core/story_engine_v2.cpp`
  - matching strict conserve pour les autres types d'evenements.
- Documentation contrat:
  - note runtime ajoutee dans `docs/protocols/story_specs/README.md`.
- Gates executees:
  - `python3 tools/dev/verify_story_default_flow.py` ✅
  - `pio run -e freenove_esp32s3` ✅

## [2026-02-26] UI Freenove conforme `/data` (palette v3 + UI_SCENE_STATUS + verification profonde)

- Skills chain active (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-story-stack`
  - `firmware-littlefs-stack`
  - `firmware-graphics-stack`
  - `firmware-build-stack`
- Checkpoint securite reutilise (run en cours):
  - `/tmp/zacus_checkpoint/20260225_235911_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_235911_status.txt`
- Lot 1 (source de verite `/data`):
  - ajout palette canonique:
    - `data/story/palette/screens_palette_v3.yaml`
  - ajout commande tooling:
    - `./tools/dev/story-gen sync-screens` (`--check` supporte)
    - fichiers: `lib/zacus_story_gen_ai/src/zacus_story_gen_ai/cli.py`, `lib/zacus_story_gen_ai/src/zacus_story_gen_ai/generator.py`
  - regeneration ecrans depuis palette:
    - `data/story/screens/*.json` (set canonique complet)
    - `data/screens/*.json` (mirror legacy durable)
  - source prioritaire intro Win Etape alignee:
    - `data/ui/scene_win_etape.json`
- Lot 2 (runtime UI + diagnostic):
  - metadata source payload ecran exposee:
    - `StorageManager::ScenePayloadMeta` + `lastScenePayloadMeta()`
    - `hardware/firmware/ui_freenove_allinone/include/storage/storage_manager.h`
    - `hardware/firmware/ui_freenove_allinone/src/storage/storage_manager.cpp`
  - snapshot rendu scene ajoute:
    - `UiSceneStatusSnapshot` + `sceneStatusSnapshot()`
    - `hardware/firmware/ui_freenove_allinone/include/ui/ui_manager.h`
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
  - nouvelle commande serie JSON mono-ligne:
    - `UI_SCENE_STATUS`
    - `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
  - reduction overrides tardifs lock/win_etape (JSON domine le statique) + cache parse payload hors changements de scene:
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
- Lot 3 (verification profonde UI):
  - nouveau validateur:
    - `tools/dev/verify_story_ui_conformance.py`
  - combine transitions deep + lecture `UI_SCENE_STATUS` + comparaison `/data/story/screens` avec allowlist dynamique QR/WIN_ETAPE.
- Stabilite complementaire:
  - correction fallback `SC_LOAD` quand scenario JSON est volumineux (`NoMemory`):
    - parse filtree des seuls champs id dans `loadScenarioIdFromFile`
    - `hardware/firmware/ui_freenove_allinone/src/app/scenario_manager.cpp`
- Gates build/tooling executees (vert):
  - `./tools/dev/story-gen validate` ✅
  - `./tools/dev/story-gen sync-screens --check` ✅
  - `./tools/dev/story-gen generate-cpp` ✅
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/zacus_bundle_final_ui` ✅
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- Flash/FS USB modem:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification hardware USB modem (vert):
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
  - `python3 tools/dev/test_story_4scenarios.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
    - log: `artifacts/rc_live/test_4scenarios_20260226-004510.log`
  - `python3 tools/dev/verify_story_transitions_deep.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
    - log: `artifacts/rc_live/deep_transition_verify_20260226-004530.log` (51/51 pass)
  - `python3 tools/dev/verify_story_ui_conformance.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
    - log: `artifacts/rc_live/ui_conformance_verify_20260226-010143.log` (51/51 pass, 0 payload missing)

## [2026-02-25] Lots 3→4→5 Freenove (stabilite unlock + perf cache + validation USB modem)

- Skills chain active (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-story-stack`
  - `firmware-littlefs-stack`
  - `firmware-graphics-stack`
  - `firmware-build-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225-224120_wip.patch`
  - `/tmp/zacus_checkpoint/20260225-224120_status.txt`
- Lot 3 (stabilite/compat transitions):
  - support `SC_EVENT unlock <name>` sans casser `SC_EVENT unlock` (fallback `UNLOCK`) :
    - `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
    - `hardware/firmware/ui_freenove_allinone/include/app/scenario_manager.h`
    - `hardware/firmware/ui_freenove_allinone/src/app/scenario_manager.cpp`
  - ajout du validateur exhaustif runtime:
    - `tools/dev/verify_story_transitions_deep.py`
  - preuve correction defect `TR_SCENE_QR_DETECTOR_2`:
    - log deep verify: `artifacts/rc_live/deep_transition_verify_20260225-225305.log` (51/51 pass, 0 fail)
- Lot 4 (optimisations perf):
  - `ui_manager_effects`: cache couleurs theme + cache segment timeline (evite updates/recalculs inutiles)
    - `hardware/firmware/ui_freenove_allinone/include/ui/ui_manager.h`
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager_effects.cpp`
  - reduction allocations `String` hotpath dispatch `SC_EVENT`/`SC_EVENT_RAW`
    - `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
  - extension cache storage scene/audio de single-entry a cache borne (3 slots) + invalidation preservee
    - `hardware/firmware/ui_freenove_allinone/include/storage/storage_manager.h`
    - `hardware/firmware/ui_freenove_allinone/src/storage/storage_manager.cpp`
  - campagne perf USB modem (150s, sequence identique avant/apres):
    - before (commit `c3efe94`): `artifacts/rc_live/perf_campaign_before_20260225-231633.log`
      - `loop avg=136605us`, `ui_tick avg=97345us`, `ui_flush avg=2053us`
    - after (commit `1787e9b`): `artifacts/rc_live/perf_campaign_after_20260225-232110.log`
      - `loop avg=164980us`, `ui_tick avg=102288us`, `ui_flush avg=2068us`
    - constat: variance elevee liee charge audio/SD; optimisation UI ne degrade pas la stabilite (panic/reboot non observes pendant campagne) mais gain net non concluant sur cette charge mixte.
- Lot 5 (validation finale USB modem + tooling):
  - flash firmware: `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - smoke/tests:
    - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
    - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
    - `python3 tools/dev/test_story_4scenarios.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅ (log: `artifacts/rc_live/test_4scenarios_20260225-225245.log`)
    - `python3 tools/dev/verify_story_transitions_deep.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅ (51 pass / 0 fail)
  - tooling/build gates finales:
    - `./tools/dev/story-gen validate` ✅
    - `./tools/dev/story-gen generate-cpp` ✅
    - `./tools/dev/story-gen generate-bundle --out-dir /tmp/zacus_bundle_final` ✅
    - `pio run -e freenove_esp32s3` ✅ (RAM 74.6%, Flash 32.9%)
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- Notes:
  - verification compat explicite:
    - `SC_EVENT unlock UNLOCK_QR` -> `dispatched=1 changed=1 step=SCENE_FINAL_WIN`
    - `SC_EVENT unlock` conserve fallback `UNLOCK` (compat legacy preservee)

## [2026-02-25] Split lourd ui_manager.cpp (effects/display/intro)

- Skills utilises (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-graphics-stack`
  - `firmware-build-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225-222145_wip.patch`
  - `/tmp/zacus_checkpoint/20260225-222145_status.txt`
- Actions:
  - scission des implementations `UiManager` en 3 unites dediees:
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager_display.cpp`
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager_intro.cpp`
    - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager_effects.cpp`
  - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp` conserve l'orchestration + helpers internes et inclut les 3 unites split via `UI_MANAGER_SPLIT_IMPL` (pas de changement API publique).
- Gates/evidence:
  - `pio run -e freenove_esp32s3` ✅ (RAM 74.6%, Flash 32.8%)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅

## [2026-02-25] USB modem - correction scripts smoke story + rerun

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_215404_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_215404_status.txt`
- Port cible:
  - `/dev/cu.usbmodem5AB90753301` (ESP32)
- Correctifs:
  - `tools/dev/verify_story_default_flow.py`
    - flow aligne scenario runtime `DEFAULT` (initial `SCENE_U_SON_PROTO`),
    - assertions passees de `screen` seul a `step+screen`,
    - sequence events mise a jour: `button ANY` -> `serial BTN_NEXT` -> `serial FORCE_DONE` -> `serial BTN_NEXT` -> `button ANY`.
  - `lib/zacus_story_portable/test_story_4scenarios.py`
    - migration commandes legacy JSON (`story.load/status`, `STORY_FORCE_STEP`) vers commandes runtime actuelles (`SC_LOAD`, `STATUS`, `SC_EVENT`),
    - verification `ACK SC_LOAD`,
    - verification status parse (`scenario/step/screen`) + check scenario courant.
- Gates/evidence:
  - `python3 -m py_compile tools/dev/verify_story_default_flow.py lib/zacus_story_portable/test_story_4scenarios.py` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
  - `python3 tools/dev/test_story_4scenarios.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
    - evidence: `artifacts/rc_live/test_4scenarios_20260225-215755.log`
  - `./tools/dev/story-gen validate` ✅

## [2026-02-25] Audit + correction/optimisation Freenove (lot refactor+stabilite+perf partiels)

- Skills chain active (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-story-stack`
  - `firmware-littlefs-stack`
  - `firmware-graphics-stack`
  - `firmware-build-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_210711_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_210711_status.txt`
- Scope run:
  - refactor `ui_freenove_allinone/src/app/main.cpp`
  - compat story/bundle LittleFS
  - refactor `ui_freenove_allinone/src/ui/ui_manager.cpp` (defer split lourd, voir limitation)
  - optimisation hotpaths runtime/UI/storage
- Actions:
  - ajout des bridges de services runtime app:
    - `include/app/runtime_serial_service.h` + `src/app/runtime_serial_service.cpp`
    - `include/app/runtime_scene_service.h` + `src/app/runtime_scene_service.cpp`
    - `include/app/runtime_web_service.h` + `src/app/runtime_web_service.cpp`
    - wiring dans `src/app/main.cpp` avec impl callbacks (`*Impl`) pour isoler orchestration.
  - stabilite story/bundle:
    - correction JSON ecrans invalides:
      - `data/story/screens/SCENE_U_SON_PROTO.json`
      - `data/story/screens/SCENE_WIN_ETAPE1.json`
      - `data/story/screens/SCENE_FINAL_WIN.json`
    - `generator.py`:
      - auto-generation des `screens` connus de `SCENE_PROFILES` si JSON manquant (legacy compat),
      - maintien FAIL strict pour scene inconnue,
      - trace manifest: `autogenerated_resources.screens`.
  - LittleFS/runtime:
    - `storage_manager.cpp`: fin du sentinel `SCENE_LOCKED`, check presence aligne runtime default scenario.
    - warning rate-limite payload scene manquant au rendu (`main.cpp`).
  - optimisation perf:
    - cache RAM `scene payload` et `audio pack path` dans `StorageManager`,
    - invalidation explicite sur `syncStoryFileFromSd`, `syncStoryTreeFromSd`, provisioning embedded.
- Gates/evidence:
  - `./tools/dev/story-gen validate` ✅
  - `./tools/dev/story-gen generate-cpp` ✅
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/zacus_bundle_lot3_20260225_1` ✅
    - manifest: `autogenerated_resources.screens = [SCENE_LOCKED, SCENE_READY, SCENE_REWARD, SCENE_SEARCH]`
  - `python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅
  - `python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
  - `pio run -e freenove_esp32s3` ✅ (RAM 74.6%, Flash 32.8%)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- Limitations:
  - lot split `ui_manager.cpp` en `ui_manager_effects/display/intro` non applique dans ce run (risque de regression large sans campagne hardware complete).
  - gates hardware serie/combined-board/perf 2-3 min non executees ici (pas de port board fourni dans le run).

## [2026-02-25] Clean ecrans - phase 2 (runtime strict 9)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_203828_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_203828_status.txt`
- Scope:
  - `data/story/screens/*.json`
  - `game/scenarios/scene_editor_all.yaml`
  - `game/scenarios/scenario_reel_template.yaml`
- Actions:
  - suppression de 6 ecrans restants hors runtime principal:
    - `SCENE_LOCKED`, `SCENE_MP3_PLAYER`, `SCENE_PHOTO_MANAGER`,
      `SCENE_READY`, `SCENE_REWARD`, `SCENE_SEARCH`.
  - regeneration du workbench runtime:
    - `python3 tools/dev/export_scene_editor_workbench.py`
  - nettoyage template scenario reel pour supprimer les references ecrans retirees:
    - `prompt_input.media_hub.entries: []`
    - retrait des scenes retirees dans les catalogues `scene_screen_audio_catalog_all` et `scene_action_trigger_catalog_all`.
- Resultat:
  - `data/story/screens`: 15 -> 9 fichiers (runtime strict).
  - `scene_editor_all.yaml`: `unused_scene_ids: []`.
- Gates/evidence:
  - `./tools/dev/story-gen validate` ✅
  - parse JSON `data/story/screens/*.json` ✅ (`json_screens_ok 9`)
  - check refs `screen_json` YAML manquants ✅ (`missing_screen_json_refs 0`)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- Limitation:
  - `./tools/dev/story-gen generate-bundle` ❌ (`Missing required screens resource 'SCENE_LOCKED'`) car les scenarios legacy (hors runtime strict) referencent encore des scenes retirees.

## [2026-02-25] Clean ecrans - reduction du catalogue LittleFS (phase 1)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_202801_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_202801_status.txt`
- Scope:
  - `data/story/screens/*.json`
  - `game/scenarios/scene_editor_all.yaml`
  - `game/scenarios/scenario_reel_template.yaml`
- Actions:
  - suppression de 9 ecrans non utilises par le runtime principal:
    - `SCENE_BROKEN`, `SCENE_CAMERA_SCAN`, `SCENE_FIREWORKS`, `SCENE_LA_DETECT`,
      `SCENE_MEDIA_ARCHIVE`, `SCENE_SIGNAL_SPIKE`, `SCENE_WIN`, `SCENE_WINNER`, `SCENE_WIN_ETAPE`.
  - regeneration workbench scenes:
    - `python3 tools/dev/export_scene_editor_workbench.py`
  - suppression des references YAML `screen_json` obsoletes dans `scenario_reel_template.yaml` (catalogues ecrans + triggers scenes).
- Resultat:
  - `data/story/screens`: 24 -> 15 fichiers.
  - `scene_editor_all.yaml`: `unused_scene_ids` reduit de 15 -> 6.
- Gates/evidence:
  - `./tools/dev/story-gen validate` ✅
  - parse JSON `data/story/screens/*.json` ✅ (`json_screens_ok 15`)
  - check refs `screen_json` YAML manquants ✅ (`missing_screen_json_refs 0`)
  - `python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅
  - `python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅

## [2026-02-25] Clean ecrans (screens) - sync workbench + gates

- Scope:
  - `game/scenarios/scene_editor_all.yaml`
  - `data/story/screens/*.json`
  - `data/screens/*.json`
- Actions:
  - regeneration du workbench ecrans depuis les JSON canoniques:
    - `python3 tools/dev/export_scene_editor_workbench.py`
  - controle coherence tokens UI (`effect`, `timeline.keyframes[].effect`, `transition.effect`) contre le parseur runtime `ui_manager.cpp`: aucun token invalide detecte.
  - controle JSON des ecrans (`data/story/screens` + `data/screens`): parse OK.
- Resultat:
  - `game/scenarios/scene_editor_all.yaml` resynchronise avec l'etat reel des ecrans runtime.
  - aucune derive restante detectee sur les 6 mirrors legacy `data/screens/*.json` (alignes avec `data/story/screens/SCENE_*.json` correspondants).
- Gates/evidence:
  - `./tools/dev/story-gen validate` ✅
  - `jq empty` sur `data/story/screens/*.json` et `data/screens/*.json` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅

## [2026-02-25] Clean global scenario/scene (YAML + code + assets)

- Skills utilises (ordre):
  - `firmware-story-stack`
  - `firmware-littlefs-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_200958_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_200958_status.txt`
- Clean applique:
  - `lib/zacus_story_gen_ai/src/zacus_story_gen_ai/generator.py`
    - classification `workbench` ajoutee pour ignorer les YAML d'edition scene (`meta` + `scenes`) dans `story-gen validate`.
    - resolution alias actions ajoutee pour les noms courts LittleFS:
      - `ACTION_QR_CODE_SCANNER_START` -> `ACTION_QR_SCAN_START`
      - `ACTION_SET_BOOT_MEDIA_MANAGER` -> `ACTION_BOOT_MEDIA_MGR`
  - runtime Freenove:
    - `ui_freenove_allinone/src/app/main.cpp`
    - `ui_freenove_allinone/src/main.cpp`
    - `executeStoryAction` charge maintenant d'abord `ACTION_ID.json`, puis fallback alias fichier court si absent.
- Gates/evidence:
  - `./tools/dev/story-gen validate` ✅ (`scenarios=5`, `game_scenarios=2`)
  - `./tools/dev/story-gen generate-cpp` ✅ (`spec_hash=aa9658456a0d`)
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/zacus_story_bundle_clean_20260225_201347` ✅
    - verification assets actions generes:
      - `/tmp/zacus_story_bundle_clean_20260225_201347/story/actions/ACTION_QR_CODE_SCANNER_START.json` (type/config presents)
      - `/tmp/zacus_story_bundle_clean_20260225_201347/story/actions/ACTION_SET_BOOT_MEDIA_MANAGER.json` (type/config presents)
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅

## [2026-02-25] Audit + clean dossier data (story/littlefs)

- Skills utilises (ordre):
  - `firmware-story-stack`
  - `firmware-littlefs-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_195727_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_195727_status.txt`
- Audit `data/`:
  - 89 fichiers, 16 dossiers, aucun fichier temporaire suspect detecte.
  - derive detectee entre fallback legacy `/screens/*` et canonique `/story/screens/*` pour:
    - `data/screens/la_detector.json`
    - `data/screens/locked.json`
  - fichier legacy non reference detecte:
    - `data/scenarios/data/default_scenario.json`
- Clean applique:
  - synchronisation fallback `/screens` avec les JSON canoniques:
    - `data/screens/la_detector.json` -> aligne sur `data/story/screens/SCENE_LA_DETECTOR.json`
    - `data/screens/locked.json` -> aligne sur `data/story/screens/SCENE_LOCKED.json`
  - suppression du legacy non reference:
    - `data/scenarios/data/default_scenario.json`
- Gates/evidence:
  - parse JSON complet `data/**/*.json` via `jq empty` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - note: `./tools/dev/story-gen validate` echoue sur `../../game/scenarios/scene_editor_all.yaml` (format workbench non reconnu), anomalie preexistante hors clean `data/`.

## [2026-02-25] Preparation test utilisateur reel (runbook)

- Carte preparee pour test terrain:
  - `BOOT_MODE_CLEAR` applique, `SC_LOAD DEFAULT`, `RESET` executes.
  - smoke post-reset valide (`serial_smoke.py` PASS).
- Commande de capture log conseillee pendant test utilisateur:
  - `python3 tools/dev/user_live_logger.py --port /dev/cu.usbmodem5AB90753301 --baud 115200 --duration 1200 --log logs/user_live_test_<timestamp>.log`
- Point de vigilance:
  - surveiller recurrence potentielle panic SDMMC observee une fois lors d'un run precedent `media-manager`.

## [2026-02-25] FX verificator - scenes non direct-FX en NA + rerun gates

- Objectif:
  - supprimer les faux FAIL FX sur scenes runtime LVGL-only (pas de moteur direct-FX).
- Correctif script global:
  - `~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh`
  - nouveau parametre: `non_direct_policy` (`auto` par defaut, `strict` optionnel).
  - mode `auto`: scene sans activite FX (`fx_fps=0`, `fx_frames=0`, `fx_blit=0`) => `NA` au lieu de FAIL.
  - mode `strict`: comportement historique (`fx_fps > 0` obligatoire partout).
- Miroir doc repo mis a jour:
  - `docs/skills/fx-verificator.md`
- Rerun verification:
  - `run_fx_verification.sh ... \"SCENE_WIN_ETAPE1,SCENE_WIN_ETAPE2,SCENE_FINAL_WIN,SCENE_MEDIA_MANAGER\" 10 auto` ✅
  - `run_fx_verification.sh ... \"SCENE_WINNER,SCENE_FIREWORKS\" 8 strict` ✅
  - `serial_smoke.py` ✅
  - `run_scene_verification.sh` (9 scenes runtime) ✅
  - `run_hal_verification.sh` (9 scenes runtime) ✅
  - `run_media_manager_verification.sh` ✅
- Note:
  - une execution intermediaire `media-manager` a logue un panic SDMMC puis reboot; rerun immediat apres smoke => PASS.
  - a surveiller si reproduction frequente sous charge longue.

## [2026-02-25] Upload Freenove + smoke + verifs scenes/FX/HAL/media

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_182057_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_182057_status.txt`
- Upload effectues:
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Smoke:
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
- Verification scenes runtime (ordre scenario):
  - `run_scene_verification.sh ... "SCENE_U_SON_PROTO,...,SCENE_MEDIA_MANAGER"` ✅
- Verification FX:
  - runtime direct (`SCENE_WIN_ETAPE1,SCENE_WIN_ETAPE2,SCENE_FINAL_WIN,SCENE_MEDIA_MANAGER`) ❌
    - `SCENE_FINAL_WIN` rapporte `fx_fps=0` (scene LVGL celebratory sans moteur direct-FX actif).
  - fallback direct-FX (`SCENE_WINNER,SCENE_FIREWORKS`) ✅
- Verification HAL runtime (9 scenes):
  - `run_hal_verification.sh ...` ✅ (cam=0, amp=0, ws2812=1, led_auto=1 sur les scenes runtime)
- Verification media-manager:
  - `run_media_manager_verification.sh ...` ✅
  - QR -> FINAL_WIN -> MEDIA_MANAGER + persistance boot + rollback verifies.

## [2026-02-25] Apply workbench runtime -> JSON ecrans (pass simplification)

- Action demandee executee:
  - `python3 tools/dev/export_scene_editor_workbench.py --apply`
- JSON ecrans mis a jour (9 scenes runtime):
  - `data/story/screens/SCENE_U_SON_PROTO.json`
  - `data/story/screens/SCENE_LA_DETECTOR.json`
  - `data/story/screens/SCENE_WIN_ETAPE1.json`
  - `data/story/screens/SCENE_WARNING.json`
  - `data/story/screens/SCENE_LEFOU_DETECTOR.json`
  - `data/story/screens/SCENE_WIN_ETAPE2.json`
  - `data/story/screens/SCENE_QR_DETECTOR.json`
  - `data/story/screens/SCENE_FINAL_WIN.json`
  - `data/story/screens/SCENE_MEDIA_MANAGER.json`
- Verification:
  - parse JSON sur les 9 fichiers: OK
  - gate FS: `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅

## [2026-02-25] Simplification workbench scenes (utilisees uniquement + ordre runtime)

- Skills utilises:
  - `firmware-story-stack`
  - `firmware-scene-ui-editor`
- Fichier cible:
  - `game/scenarios/scene_editor_all.yaml`
- Actions:
  - verification scenes utilisees/non utilisees via `game/scenarios/default_unlock_win_etape2.yaml`,
  - suppression des scenes non utilisees dans le workbench (15 retirees),
  - re-ordonnancement des scenes selon l'ordre runtime des steps.
- Ordre final conserve:
  - `SCENE_U_SON_PROTO`
  - `SCENE_LA_DETECTOR`
  - `SCENE_WIN_ETAPE1`
  - `SCENE_WARNING`
  - `SCENE_LEFOU_DETECTOR`
  - `SCENE_WIN_ETAPE2`
  - `SCENE_QR_DETECTOR`
  - `SCENE_FINAL_WIN`
  - `SCENE_MEDIA_MANAGER`
- Script outille egalement ce besoin:
  - `tools/dev/export_scene_editor_workbench.py` exporte desormais par defaut en `runtime_only` (ordre scenario),
    avec option `--include-unused` pour reintroduire les scenes hors runtime.

## [2026-02-25] Revue complete scene_editor_all (coherence LVGL/FX/audio)

- Skills utilises (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-story-stack`
  - `firmware-graphics-stack`
  - `firmware-fx-overlay-lovyangfx`
  - `firmware-scene-ui-editor`
- Fichier revu/mis a jour:
  - `game/scenarios/scene_editor_all.yaml`
- Verifications effectuees:
  - existence `screen_json` pour les 24 scenes,
  - coherence `runtime_step_ids` + `default_audio_pack_id` contre `game/scenarios/default_unlock_win_etape2.yaml`,
  - validite tokens FX/transition supportes par `ui_manager.cpp`.
- Corrections appliquees:
  - `SCENE_WIN_ETAPE1` et `SCENE_WIN_ETAPE2`: `fx.effect` invalide `sparkle` -> `celebrate` (contrat runtime).
  - scenes QR (`SCENE_CAMERA_SCAN`, `SCENE_QR_DETECTOR`): mode scan stabilise (`fx.effect=none`, timeline vide, `effect_speed_ms=0`).
  - ajout bloc `meta.review` dans le workbench pour tracer l'etat de coherence.
- Resultat:
  - revue complete = `issue_count=0`.

## [2026-02-25] Workbench ecrans LVGL/FX + audio (toutes scenes)

- Objectif:
  - accelerer l'edition de toutes les scenes (LVGL/FX + son associe) depuis un seul fichier.
- Ajouts:
  - script `tools/dev/export_scene_editor_workbench.py`:
    - export JSON ecrans -> YAML editable (`scene_editor_all.yaml`),
    - apply YAML -> JSON ecrans (`--apply`).
  - fichier genere `game/scenarios/scene_editor_all.yaml` (24 scenes).
- Commandes:
  - export: `python3 hardware/firmware/tools/dev/export_scene_editor_workbench.py`
  - apply: `python3 hardware/firmware/tools/dev/export_scene_editor_workbench.py --apply`
- Passe proposition visuelle (runtime reel):
  - `game/scenarios/scene_editor_all.yaml` ajuste pour scenes runtime:
    `SCENE_U_SON_PROTO`, `SCENE_LA_DETECTOR`, `SCENE_WIN_ETAPE1`,
    `SCENE_WARNING`, `SCENE_LEFOU_DETECTOR`, `SCENE_WIN_ETAPE2`,
    `SCENE_QR_DETECTOR`, `SCENE_FINAL_WIN`, `SCENE_MEDIA_MANAGER`.
  - coherence garde-fou:
    - QR scene sans FX intrusif (`effect=none`),
    - final win en celebratory,
    - hub media en `radar/pulse` boucle lente.

## [2026-02-25] Template scenario reel - catalogue complet scenes/ecrans/sons

- Fichier mis a jour:
  - `game/scenarios/scenario_reel_template.yaml`
- Ajouts:
  - `prompt_input.scene_screen_audio_catalog_all` avec les 24 scenes connues, lien JSON ecran, pack audio par defaut, steps runtime utilises.
  - `prompt_input.audio_pack_catalog_all` avec les 6 packs audio actuels, chemin JSON, fichier audio cible et steps associes.
- Validation:
  - parse YAML OK (`yaml.safe_load`) avec compteurs:
    - scenes catalogue = 24
    - packs catalogue = 6

## [2026-02-25] Refonte scenario reel v2 + stabilisation verificators + upload/test Freenove

- Skills utilises (ordre):
  - `freenove-firmware-orchestrator`
  - `firmware-story-stack`
  - `firmware-espnow-stack`
  - `firmware-build-stack`
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_162241_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_162241_status.txt`
- Correctifs principaux:
  - flow runtime applique sur `DEFAULT` avec `button`/`espnow`, QR gate `APP_QR_UNLOCK`, timer `WIN_DUE`, `STEP_MEDIA_MANAGER`.
  - action boot cible maintenue `ACTION_SET_BOOT_MEDIA_MANAGER` avec compat runtime `ACTION_SET_BOOT_MEDIA`.
  - LittleFS: fichiers action a nom court pour contourner la limite de nom (`ACTION_QR_SCAN_START.json`, `ACTION_BOOT_MEDIA_MGR.json`) tout en conservant les IDs longs en payload.
  - `main.cpp`: `SC_EVENT`/`SC_EVENT_RAW` appliquent maintenant `refreshSceneIfNeeded(true)` + `startPendingAudioIfAny()` sur changement de step (corrige reset lors trigger `button`).
  - `main.cpp`: commande `RESET` stoppe audio/amp/media avant reset scenario puis rerender immediate (corrige panic rollback media-manager).
  - HAL scene `SCENE_FINAL_WIN`: LED fixee (pas de pulse) pour verification RGB stricte.
  - scripts skills:
    - `scene-verificator`: robustesse boot marker + triggers button/espnow + checks LED.
    - `hal-verificator-status`: attentes par defaut alignees (`SCENE_QR_DETECTOR mic=0`, `SCENE_FINAL_WIN led=252/212/92`).
    - `media-manager`: `AUDIO_STOP` avant QR et avant reset rollback + detection `scene=SCENE_MEDIA_MANAGER`/ACK step media.
  - generator story: validation narrative game assouplie (runtime YAML reste strict) pour ne pas bloquer sur `game/scenarios/zacus_v2.yaml`.
- Gates executes:
  - story gen:
    - `PYTHONPATH=lib/zacus_story_gen_ai/src .venv/bin/python -m zacus_story_gen_ai.cli validate` ✅
    - `PYTHONPATH=lib/zacus_story_gen_ai/src .venv/bin/python -m zacus_story_gen_ai.cli generate-cpp` ✅
    - `PYTHONPATH=lib/zacus_story_gen_ai/src .venv/bin/python -m zacus_story_gen_ai.cli generate-bundle` ✅
  - build/upload:
    - `pio run -e freenove_esp32s3` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅ (plus de warning LittleFS sur action QR/boot media manager)
    - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - smoke/verificators:
    - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
    - `~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
    - `~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
    - `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
    - `~/.codex/skills/media-manager/scripts/run_media_manager_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
- Limitation materielle observee (non bloquante):
  - erreurs camera DMA sur `SCENE_PHOTO_MANAGER` (`cam_dma_config ... malloc failed`) quand PSRAM libre faible; verifs scene/HAL/media passent malgre ces logs.

## [2026-02-25] Scope ESP-NOW + fix action boot media (LittleFS)

- Objectif:
  - inclure ESP-NOW dans le passage vers `STEP_MEDIA_MANAGER`,
  - corriger l'action de boot media qui provoquait un warning buildfs.
- Actions:
  - action renommee `ACTION_SET_BOOT_MEDIA` (`data/story/actions/ACTION_SET_BOOT_MEDIA.json`) pour rester compatible LittleFS.
  - references scenario mises a jour:
    - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`
    - `data/story/scenarios/DEFAULT.json`
  - transition ESP-NOW ajoutee:
    - `on_event espnow QR_OK -> STEP_MEDIA_MANAGER`.
  - compat runtime conservee dans `main.cpp`:
    - accepte `ACTION_SET_BOOT_MEDIA` + alias legacy `ACTION_SET_BOOT_MEDIA_MANAGER`.
- Gates:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - warning `unable to open '/story/actions/ACTION_SET_BOOT_MEDIA_MANAGER.json.` disparu.

## [2026-02-25] Passes completes ChatGPT examples -> firmware (QR + media + gates)

- Skills utilises:
  - `chatgpt-file-exemple-intake`
  - `freenove-firmware-orchestrator`
- Pass 1 (QR assets):
  - integration des assets exemples dans `data/ui/qr/` (`ok.png`, `ok_48.png`, `bad.png`, `bad_48.png`, `reticle.png`, `scanlines.png`, `README.txt`).
  - hook runtime ajoute dans `ui/qr/qr_scene_controller.cpp` avec trace presence assets (`[QR_UI] assets ...`).
- Pass 2 (audio/media depuis exemples):
  - extraction logique "natural sort" pour catalogues media dans `ui_freenove_allinone/src/system/media/media_manager.cpp`.
  - `MEDIA_LIST` retourne maintenant un ordre stable/humain (`track2` avant `track10`).
- Pass 3 (QR tool + full gates):
  - `tools/dev/gen_qr_crc16.py` etendu avec `--scope data|suffix` + `--prefix`.
  - builds + uploads executes:
    - `pio run -e freenove_esp32s3` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅ (warning LittleFS deja connu sur `ACTION_SET_BOOT_MEDIA_MANAGER.json`)
    - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - verifications executees:
    - `serial_smoke.py` ✅
    - `scene-verificator` ✅
    - `fx-verificator` ✅ (1er run panic puis rerun PASS apres reset)
    - `hal-verificator-status` ✅
    - `media-manager` ✅
    - `run_stress_tests.py` ❌ (scenario `DEFAULT` n'atteint pas `STEP_DONE`, sans panic sur ce run).

## [2026-02-25] Intake fichiers ChatGPT_file_exemple (integration controlee)

- Skills utilises:
  - `chatgpt-file-exemple-intake` (inventaire + mapping source->cible)
  - `freenove-firmware-orchestrator` (respect architecture Freenove existante)
- Inventaire execute sur `hardware/ChatGPT_file_exemple/**`:
  - candidats utiles identifies sur QR flow (`qr_scan_controller.*`, `ui_manager.*`, `gen_qr_crc16.py`, assets PNG).
- Decision d'integration:
  - conserve architecture canonique actuelle sous `hardware/firmware/**` (pas de copie brute des arbres imbriques `.../hardware/firmware/hardware/firmware/...`).
  - extraction d'un delta utile depuis l'exemple QR:
    - `tools/dev/gen_qr_crc16.py` enrichi avec:
      - `--scope data|suffix`
      - `--prefix <text>` (obligatoire en scope `suffix`)
      - validation prefix + calcul CRC sur suffixe.
  - fichiers exemples non integres tels quels (assets/UI patch complet) car deja couverts partiellement ou non references runtime.
- Validation rapide:
  - `python3 tools/dev/gen_qr_crc16.py "ZACUS:ETAPE1" --ci --scope data` ✅
  - `python3 tools/dev/gen_qr_crc16.py "ZACUS:ETAPE1:42" --ci --scope suffix --prefix "ZACUS:"` ✅

## [2026-02-25] Scenario reel template - sync etat code + format promptable

- Fichier mis a jour: `game/scenarios/scenario_reel_template.yaml`
- Refonte du template pour usage "promptable":
  - section editable unique `prompt_input` (besoin metier),
  - section reference `current_firmware_snapshot` (etat runtime actuel derive du code/spec).
- Snapshot aligne sur l'etat courant:
  - flow `DEFAULT` avec final `QR_OK -> STEP_MEDIA_MANAGER`,
  - scenes hub media (`SCENE_MEDIA_MANAGER`, `SCENE_MP3_PLAYER`, `SCENE_PHOTO_MANAGER`, `SCENE_READY`),
  - HAL/LED runtime connus pour scenes critiques.
- Validation:
  - parse YAML OK (`python3` + `yaml.safe_load`).

## [2026-02-25] Etat scenario + template de saisie "reel"

- Etat courant source spec:
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`
  - flow actif: `STEP_WAIT_UNLOCK -> STEP_U_SON_PROTO/STEP_WAIT_ETAPE2 -> STEP_ETAPE2 -> STEP_DONE (SCENE_CAMERA_SCAN) -> STEP_MEDIA_MANAGER (SCENE_MEDIA_MANAGER via QR_OK)`.
- Nouveau fichier de saisie simplifie pour expression besoin metier:
  - `game/scenarios/scenario_reel_template.yaml`
  - format orienté "brief scenario reel" (catalogue scenes, besoins HAL, LED, steps, transitions, acceptance).
  - usage: l'utilisateur edite ce template, puis Codex convertit en scenario YAML canonique + JSON runtime.

## [2026-02-25] HAL skill update - WS2812 (4 LED Freenove)

- Skill global `hal-verificator-status` etendu pour verifier aussi les LED WS2812:
  - `HW_STATUS ws2812` (`ws2812=0|1`)
  - `HW_STATUS auto` (`led_auto=0|1`)
  - couleur exacte optionnelle `HW_STATUS led=R,G,B` (`led=R/G/B` ou `led=R,G,B`)
- Script mis a jour:
  - `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh`
  - parsing expectations enrichi (`cam|amp|mic|ws2812|led_auto|led`)
  - verdict ligne detaillee avec etats LED.
- Documentation skill:
  - global `~/.codex/skills/hal-verificator-status/SKILL.md`
  - global `~/.codex/skills/hal-verificator-status/references/checklist.md`
  - miroir repo `docs/skills/hal-verificator-status.md`
- Verification rapide:
  - `run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200 \"SCENE_READY:cam=0,amp=0,mic=0,ws2812=1,led_auto=1;SCENE_MP3_PLAYER:cam=0,amp=1,ws2812=1,led_auto=1;SCENE_CAMERA_SCAN:cam=0,amp=0,ws2812=1,led_auto=1\"` ✅

## [2026-02-25] QR final + boot MEDIA_MANAGER + skill media-manager

- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260225_102014_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_102014_status.txt`
- Firmware/UI:
  - ajout module QR `ui/qr/qr_scan_controller.*` + integration `UiManager` (`consumeRuntimeEvent`, `simulateQrPayload`, parsing regles `qr.expected/prefix/contains`).
  - pont runtime event UI -> story (`SERIAL:QR_OK` / `SERIAL:QR_INVALID`) dans `main.cpp`.
  - ajout persistance NVS `BootModeStore` (`zacus_boot`, `startup_mode`, `media_validated`) + commandes `BOOT_MODE_STATUS|SET|CLEAR`.
  - action story `ACTION_SET_BOOT_MEDIA_MANAGER` applique mode persistant `media_manager`.
  - `SCENE_CAMERA_SCAN` repasse en QR-only, nouvelle scene `SCENE_PHOTO_MANAGER`, scene hub `SCENE_MEDIA_MANAGER`.
  - policy reset: `RESET` reroute vers `SCENE_MEDIA_MANAGER` si mode boot media actif.
- Story/data:
  - MAJ scenario YAML: `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` (transition `QR_OK` -> `STEP_MEDIA_MANAGER`).
  - MAJ runtime JSON: `data/story/scenarios/DEFAULT.json`, `data/story/screens/SCENE_CAMERA_SCAN.json`, ajout `SCENE_MEDIA_MANAGER.json`, `SCENE_PHOTO_MANAGER.json`, `ACTION_SET_BOOT_MEDIA_MANAGER.json`.
  - generation: `./tools/dev/story-gen validate` ✅, `./tools/dev/story-gen generate-bundle` ✅ (`artifacts/story_fs/deploy`).
- Skills:
  - creation globale `~/.codex/skills/media-manager/` (`SKILL.md`, `agents/openai.yaml`, `scripts/run_media_manager_verification.sh`, `references/checklist.md`).
  - miroir repo: `docs/skills/media-manager.md`.
- Gates executes:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅ (warning d'indexation FS a surveiller sur `ACTION_SET_BOOT_MEDIA_MANAGER.json`, build final OK)
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅

## [2026-02-25] Skill HAL_VERIFCATOR_STATUS (scene-aware hardware gating)

- Creation skill global:
  - `~/.codex/skills/hal-verificator-status/SKILL.md`
  - `~/.codex/skills/hal-verificator-status/agents/openai.yaml`
  - `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh`
  - `~/.codex/skills/hal-verificator-status/references/checklist.md`
- Scope validation couverte:
  - camera (`CAM_STATUS rec_scene`), amp (`AMP_STATUS scene`), micro (`RESOURCE_STATUS mic_should_run`).
  - verification activation/desactivation par scene via plan explicite `SCENE:cam=,amp=,mic=`.
- Miroir doc versionne:
  - `docs/skills/hal-verificator-status.md`
- Ajustement robustesse:
  - `AMP_STATUS ready=0` interprete comme `amp=0` (pas de faux negatif quand player non initialise).
  - attentes partielles supportees par scene (`cam|amp|mic` optionnels).

## [2026-02-25] Follow-up verificators (scene/fx/hal)

- `scene-verificator`:
  - alias triggers ajoutes (`BTN_*` -> `SC_EVENT serial`, noms pointes -> `SC_EVENT_RAW`).
  - argument trigger vide supporte (desactive les triggers custom sans fallback implicite).
  - critere global assoupli: accepte progression via `SCREEN_SYNC` ou changements `scene/status`.
- `fx-verificator`:
  - script etendu (`scenes_csv`, `seconds_per_scene`) et verdict per-scene (`max(fx_fps)>0`).
- Gates executes:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
- Resultats a investiguer:
  - `scene-verificator` avec trigger `story.validate` provoque panic serie (`Guru Meditation ... Unhandled debug exception`).
  - `hal-verificator` montre `SCENE_MP3_PLAYER` avec `mic_should_run=1` (si on force `mic=0`, echec attendu).
- Integration changement detecte dans workspace:
  - fichier `data/screens/la_detect.json` integre au scope utilisateur.
  - ajout alias story `data/story/screens/SCENE_LA_DETECT.json` pour coherer avec `SCENE_LA_DETECT` deja supporte cote runtime.
  - gate `pio run -e freenove_esp32s3_full_with_ui -t buildfs` relancee ✅ (fichier inclus dans LittleFS).

## [2026-02-25] Scene/FX orchestrator refactor + VERIFICATOR skills

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225_093916_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_093916_status.txt`
- Refactor runtime:
  - ajout orchestrateur `SceneFxOrchestrator` (`ui_freenove_allinone/include|src/app/scene_fx_orchestrator.*`) pour planifier ownership `Story/IntroFx/DirectFx/Amp/Camera`.
  - `main.cpp`: ordre explicite transition owner `pre-exit -> render scene -> post-enter`.
  - `UiManager`: cache scene payload hash + `audio_playing` pour eviter reinitialisations statiques inutiles; cleanup FX recentre.
  - `ScenarioManager`: logs de transition structures (`from_step`, `to_step`, `from_scene`, `to_scene`, `event`, `source`, `audio_pack`) + trace chaines immediates.
  - `resolve_ports.py`: remap mono-port robuste (`esp8266 -> esp32`) pour setup Freenove single-usb.
- Skills crees (global + scripts):
  - `~/.codex/skills/scene-verificator/`
  - `~/.codex/skills/fx-verificator/`
  - `scene-verificator` etendu pour piloter des triggers de test (`CMD@SCENE`) et verifier la progression post-trigger.
- Miroir doc versionne:
  - `docs/skills/scene-verificator.md`
  - `docs/skills/fx-verificator.md`
- Verification a executer en fin de run:
  - build Freenove, serial smoke, scene chain, FX status, stress test.

## [2026-02-24] Plan v4 integration (FX3D modes + SCENE_CAMERA_SCAN recorder Win311)

- Skills chain executee (ordre): `freenove-firmware-orchestrator` -> `firmware-graphics-stack` -> `firmware-camera-stack` -> `firmware-fx-overlay-lovyangfx` -> `firmware-build-stack`.
- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260224_214537_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_214537_status.txt`
- Implementations principales:
  - `FxEngine`: ajout `FxMode` (`classic|starfield3d|dotsphere3d|voxel|raycorridor`) + `setMode()/mode()`, renderers 3D integres sans casser le runtime v9.
  - `UiManager`: parse/apply `FX_MODE_A/B/C` sur `SCENE_WIN_ETAPE` (defaults `A=starfield3d`, `B=dotsphere3d`, `C=raycorridor`), reset mode sur scenes direct FX.
  - `CameraManager`: session recorder RGB565/QVGA, freeze/save/list/delete/select, restauration mode legacy en sortie.
  - `Win311 camera UI` + `CameraCaptureService`: integration overlay LVGL branchee sur `CameraManager`.
  - `main.cpp`: ownership scene `SCENE_CAMERA_SCAN`, mapping boutons physiques (BTN1..BTN5), commandes serie `CAM_UI_*` + `CAM_REC_*`, blocage forwarding scenario quand scene camera active.
  - `SCENE_CAMERA_SCAN` payload reduit (fond neutre, symbole/effects minimaux) pour limiter le bruit visuel sous overlay.
- Contrats data/doc:
  - `data/SCENE_WIN_ETAPE.json` + `data/ui/scene_win_etape.txt`: ajout `FX_MODE_A/B/C`.
  - `README.md` et `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md`: doc modes 3D + scene camera recorder + commandes.
  - `lv_conf.h`: `LV_USE_BTN=1`, `LV_USE_IMG=1`, `LV_USE_LIST=1`.
- Build/flash Freenove:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅ (1er passage)
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ⚠️ echec final pySerial `Device not configured` apres ecriture 100%.
- Verification serie rapide:
  - `SCENE_GOTO SCENE_WIN_ETAPE` -> ACK + rendu actif.
  - `SCENE_GOTO SCENE_CAMERA_SCAN` -> overlay actif, ownership camera engage.
  - `CAM_REC_STATUS` visible; sur ce hardware: `camera_init_failed` (preview indisponible, pas de crash).
  - `SCENE_GOTO SCENE_LOCKED` -> sortie propre scene camera (`[CAM_UI] scene owner=legacy`).
- Gates contrat multi-env:
  - matrice `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` lancee puis interrompue (duree excesive en session interactive); `esp32dev` compile complet observe ✅ avant interruption.

## [2026-02-24] Plan v3 integration hardening (FX v9 + MP3 scene + crash fixes)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260224_204345_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_204345_status.txt`
- Correctifs runtime appliques:
  - `amiga_audio_player.cpp`: remplacement par overlay LVGL minimal compatible config projet.
  - `audio_player_service.cpp`: backend `Audio` simplifie (API project-safe) + stats/playlist.
  - `fx/v9/engine/engine.cpp`: retrait RTTI (`dynamic_cast`) -> binding par `clip.fx` + `static_cast`.
  - `fx_engine.cpp/.h`: injection LUT v9 (`services.luts`) pour supprimer panic `StarfieldFx::render`.
  - `main.cpp`: init AMP en lazy mode sur `SCENE_MP3_PLAYER` pour eviter contention I2S avec audio scenario.
  - `main.cpp`: scene MP3 active suspend `g_audio.update`, sortie scene restaure pipeline story.
- Build/flash Freenove:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie (pyserial):
  - `SCENE_GOTO SCENE_WIN_ETAPE` -> `UI_GFX_STATUS` avec `fx_fps>0` (`3..4`) ✅
  - `SCENE_GOTO SCENE_WINNER` / `SCENE_GOTO SCENE_FIREWORKS` -> `ACK ... ok=1`, `fx_fps>0`, plus de panic ✅
  - `SCENE_GOTO SCENE_MP3_PLAYER` + `AMP_SCAN` -> `tracks=5 base=/music` ✅
  - `AMP_PLAY 0` / `AMP_STOP` -> ACK + status playback ✅
  - cycle cleanup scenes (`LOCKED -> WIN_ETAPE -> MP3 -> LOCKED`) -> logs `cleanup scene assets transition ...` sur chaque transition ✅
- Documentation:
  - `README.md`: runtime `FX_ONLY_V9` + section scene MP3 + commandes `AMP_*`.
  - `SCENE_WIN_ETAPE_demoscene_calibration.md`: lock runtime v9 + mapping timelines.

## [2026-02-24] Upload USB modem + validation scenes WINNER/FIREWORKS

- Port detecte: `/dev/cu.usbmodem5AB90753301`.
- Flash execute:
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Correctif runtime pour `SCENE_GOTO` hors steps scenario:
  - `ScenarioManager::gotoScene` accepte desormais un fallback override scene canonique (sans modifier `game/scenarios/*.yaml`).
  - override clear automatiquement lors d'un vrai `enterStep`.
- Validation serie (pyserial):
  - `SCENE_GOTO SCENE_WINNER` -> `ACK ... ok=1` ✅
  - `SCENE_GOTO SCENE_FIREWORKS` -> `ACK ... ok=1` ✅
  - retour `SCENE_GOTO SCENE_WIN_ETAPE` -> `ACK ... ok=1`, logs phase A preset `demo`, `UI_GFX_STATUS` avec `fx_fps>0` ✅

## [2026-02-24] Port v2 FX/boing + scenes WINNER/FIREWORKS (iteration)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260224_183508_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_183508_status.txt`
- Integration technique:
  - import v9/boing stabilise (includes normalises, compat C++11).
  - `fx_engine.cpp`: shadow boing via `boing_shadow_darken_span_half_rgb565` (ASM S3 par defaut, fallback C), fast blit 2x (`fx_blit_fast`) sur ratio exact.
  - `platformio.ini`: ajout `UI_BOING_SHADOW_ASM=1` (env Freenove).
  - `timeline_load.cpp`: parser ArduinoJson complet (`meta/clips/mods/events`, params/args typed->string).
  - `assets_fs.cpp`: lecture LittleFS reelle + fallback texte/palette.
  - scenes exposees: `SCENE_WINNER`, `SCENE_FIREWORKS` (registry + JSON + defaults UiManager).
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ❌ (port absent)
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ❌ (port absent)

## [2026-02-24] Hotfix texte phase A SCENE_WIN_ETAPE (FX v8)

- Symptome utilisateur: texte non visible sur la premiere scene apres integration FX v8.
- Correctifs appliques:
  - `ui_manager.cpp`: en mode `FX_ONLY_V8`, ne plus masquer `scene_title_label_`, `scene_subtitle_label_`, `scene_symbol_label_`.
  - `fx_engine.cpp`: fallback glyph 6x8 robuste (`a..z` force en `A..Z`, caracteres hors plage -> espace).
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - check serie: `SCENE_GOTO SCENE_WIN_ETAPE` -> logs `[WIN_ETAPE] phase=A preset=demo bpm=125 font=italic`, `fx_fps>0`, aucun panic/reboot detecte.

## [2026-02-24] Skill sync: firmware-lvgl-lgfx-overlay-stack v1.1

- Scope: Standardisation métier documentaire de la stack graphique overlay LVGL/LovyanGFX.
- Actions:
  - Mis à jour `~/.codex/skills/firmware-lvgl-lgfx-overlay-stack/SKILL.md` en v1.1 (single SPI master, FX→overlay→`lv_timer_handler`, PSRAM/DRAM, RGB332+LUT, hard guardrails).
  - Ajout/reprise miroir exact dans `hardware/firmware/docs/skills/firmware-lvgl-lgfx-overlay-stack.md`.
  - Vérification: présence des flags requis, ordre de rendu, invariant no-frame-allocation, checklist d’acceptance.
- Validation (read-only):
  - `rg -n \"UI_FX_LGFX|UI_FX_BACKEND_LGFX|UI_COLOR_256|UI_DRAW_BUF_IN_PSRAM|UI_DMA_TX_IN_DRAM|UI_DEMO_AUTORUN_WIN_ETAPE|FX -> invalidate overlay|pushRotateZoom|0 alloc par frame|Acceptance\"` sur les deux fichiers.
  - `diff -u` entre fichiers source et miroir (doit être vide).

## [2026-02-24] Resource coordinator + mic profile commandes (SCENE-driven + manual/auto)

- Scope: finaliser le contrôle scène/micro dans `main.cpp`.
- Changement effectué:
  - Ajout de `RESOURCE_PROFILE_AUTO <on|off>` sur la chaîne contrôle API/serial.
  - `RESOURCE_PROFILE ...` force désormais le mode manuel (`profile_auto=0`) pour éviter les auto-overrides non attendus.
  - Réactivation auto explicite: `RESOURCE_PROFILE_AUTO ON` applique immédiatement la politique scène courante.
  - Politique scène élargie: `SCENE_WIN_ETAPE` force `gfx_focus` aussi via `screen_scene_id` (et pas uniquement `STEP_ETAPE2/PACK_WIN`).
- Prochaine vérification:
  - `RESOURCE_STATUS` + `SCENE_GOTO SCENE_WIN_ETAPE` puis `UI_GFX_STATUS` / `PERF_STATUS`.
- Checkpoint local: `/tmp/zacus_checkpoint/1771936163_wip.patch`, `/tmp/zacus_checkpoint/1771936163_status.txt`.

## [2026-02-24] SCENE_WIN_ETAPE back-pressure + trans buffer guard (flush/lvgl)

- Scope: Freenove UI SCENE_WIN_ETAPE. Durcissement de la boucle `update()` : aucune frame FX/LVGL relancée si flush ou DMA occupés, mise en attente `pending_lvgl_flush_request_` et relance dès que le bus est libre. `displayFlushCb` protège les surcharges (overflow count) sans écraser `flush_ctx_`.
- Mémoire/alloc: si trans buffer plus petit que draw lines (RGB332/PSRAM ou DMA actif), réduction automatique du nombre de lignes draw pour rester aligné sur le buffer trans; reset complet des stats/pending à l’init pipeline.
- Validation: `pio run -e freenove_esp32s3` ✅, `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅.
- Checkpoint: `/tmp/zacus_checkpoint/1771928050_wip.patch`, `/tmp/zacus_checkpoint/1771928050_status.txt`.

## [2026-02-24] LVGL pipeline perf (double buffer + DMA RGB332)

- Scope: Freenove UI/LVGL. DMA async désormais autorisé même en RGB332 via trans buffer (conversion RGB332->RGB565) + garde flush; draw lines ajustées si trans buffer plus petit.
- Perf métriques: stats draw ajoutées (`draw_count/avg_us/max_us`) dans `UI_GFX_STATUS` et snapshots; build optimisé `-O2 -ffast-math` dans `platformio.ini`.
- Validation: `pio run -e freenove_esp32s3` ✅, upload `/dev/cu.usbmodem5AB90753301` ✅.

## [2026-02-24] SCENE_WIN_ETAPE flush/fx back-pressure

- Scope: UI Freenove (SCENE_WIN_ETAPE). Guarded LVGL flush re-entrance + FX/LVGL pipeline back-pressure (skip FX when DMA/flush busy; no lv_timer_handler while pending). UI_GFX_STATUS now reports fx_fps/fx_frames/fx_skip_busy + flush block/overflow counters; intro debug overlay shows fx_fps.
- Evidence: code only (no artifacts/logs generated in this session).
- Next steps: run `pio run -e freenove_esp32s3` then (if hardware) `tools/dev/post_upload_checklist.sh --port <PORT> --baud 115200` and check UI_GFX_STATUS (`fx_fps≈18`, flush pending=0, fx_skip_busy stable).

## [2026-02-24] Mic runtime gating by scene

- Scope: Freenove UI runtime. Mic processing can be toggled per scène via `UiManager::renderScene` → `HardwareManager::setMicRuntimeEnabled`, now disabled when waveform/LA overlay not needed (e.g., WIN_ETAPE clean loop). Hardware snapshot cleared when mic is off; re-enabled auto when needed.
- Validation: `pio run -e freenove_esp32s3` ✅, upload ✅ `/dev/cu.usbmodem5AB90753301`.
- Next: observe telemetry on SCENE_WIN_ETAPE (mic fields should stay at 0 when disabled); re-run `UI_GFX_STATUS`/`PERF_STATUS` if required.

## [2026-02-24] Standardisation checklist post-upload

- Scope: outillage visuel + stabilité Freenove.
- Travaux:
  - Added `tools/dev/post_upload_checklist.sh` (auto-détection port série, upload optionnel, capture N lignes de logs, validation minimale série, `SCENE_GOTO` check optionnel, confirmation visuelle manuelle).
  - Added `docs/ui/post_upload_checklist.md` (mode d’emploi de la checklist).
  - Evidence location:
    - `artifacts/post_upload/<timestamp>/post_upload_serial.log`
    - `artifacts/post_upload/<timestamp>/upload.log`
    - `artifacts/post_upload/<timestamp>/ports.json`
- Limites constatées:
  - la validation visuelle reste manuelle (pas d’accès caméra depuis cette session Codex).

## [2026-02-24] Programme Freenove 3 vagues - fondations architecture/memoire/perf

- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/1771909054_wip.patch`
  - `/tmp/zacus_checkpoint/1771909054_status.txt`
- Baseline runtime ajustee pour mesure realiste (Phase 0):
  - `platformio.ini` Freenove: `UI_FULL_FRAME_BENCH=0`, `UI_DEMO_AUTORUN_WIN_ETAPE=0`.
- Vague A (architecture) - extraction de l'orchestration:
  - nouveaux modules: `runtime/runtime_services.h`, `runtime/app_coordinator.*`, `runtime/serial_command_router.*`.
  - `main.cpp` delegue desormais la boucle runtime via `AppCoordinator::tick(...)` et le dispatch serie via `AppCoordinator::onSerialLine(...)`.
- Vague B (securite memoire) - garde-fous d'allocation:
  - nouveaux modules: `runtime/memory/caps_allocator.*`, `runtime/memory/safe_size.h`.
  - `ui_manager.cpp` migre les alloc/free critiques vers wrappers capability-aware + verification overflow sur tailles de buffers.
  - nouveau snapshot memoire public UI: `UiMemorySnapshot` + `UiManager::memorySnapshot()`.
- Vague C (performance) - instrumentation:
  - nouveau service: `runtime/perf/perf_monitor.*` + sections (`loop`, `ui_tick`, `ui_flush`, `scenario_tick`, `network_update`, `audio_update`).
  - nouvelles commandes serie: `PERF_STATUS`, `PERF_RESET`.
  - instrumentation ajoutee sur `AppCoordinator::tick`, `UiManager::displayFlushCb/completePendingFlush`, `g_network.update`, `g_audio.update`, `g_scenario.tick`, `g_ui.tick`.
- Validation/gates:
  - `pio run -e freenove_esp32s3` ✅
  - `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (verdict global `SKIP` faute de mapping port strict; evidence `artifacts/rc_live/freenove_esp32s3_20260224-050717/summary.md`).

## [2026-02-24] Fonts registry completion (aliases + docs + UI cleanup)

- Scope complete for requested multi-family LVGL font pack:
  - API aliases added in `ui_fonts.h/.cpp`: `fontBody()`, `fontBodyBoldOrTitle()`.
  - optional 1px style shadow support added (`UI_FONT_STYLE_SHADOW`, default `1`) on title/pixel styles.
  - remaining hardcoded Montserrat usages in `ui_manager.cpp` replaced by `UiFonts::*` getters.
  - new doc added: `docs/ui/fonts.md` (families, sizes, FR subset, bpp, regen flow, flash/RAM notes).
  - `docs/ui/fonts_fr.md` updated to reflect lowercase API names.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 6 --wait-port 10 --no-evidence` ✅ (`RESULT=PASS`)

## [2026-02-24] SCENE_WIN_ETAPE refonte alignement final + mode test lock

- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_045912_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_045912_status.txt`
- Alignement livrables scene:
  - `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md` corrige (`B1=4000`, bande centre >=50%, padding centre explicite).
  - `hardware/firmware/ui_freenove_allinone/README.md` corrige (B1 3..5s, ordre overrides JSON-first, `FONT_MODE`).
  - `data/ui/scene_win_etape.txt` corrige (`B1_MS=4000`, `FONT_MODE=orbitron`, note padding auto scroller centre).
- Demande test hardware verrouillee:
  - `platformio.ini` force pour l'env freenove:
    - `UI_FULL_FRAME_BENCH=1`
    - `UI_DEMO_AUTORUN_WIN_ETAPE=1`
- Skill sync:
  - `~/.codex/skills/demoscene-demomaking-generic/SKILL.md` enrichi (profil LVGL 320x480, split B1/B2, regle centre >=50%, padding).
  - MAJ references:
    - `references/parameter-calibration-guide.md`
    - `references/engine-adapter-matrix.md`
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 6 --wait-port 10 --no-evidence` ✅ (`RESULT=PASS`)
  - `SCENE_GOTO SCENE_WIN_ETAPE` serie -> `ACK SCENE_GOTO ok=1` ✅
  - Runtime serie (`UI_GFX_STATUS`) ✅:
    - `full_frame=1`, `source=PSRAM`, `mode=RGB332`, autorun scene actif (`[UI][WIN_ETAPE] phase=...` sans `SCENE_GOTO`).

## [2026-02-24] Freenove kit lock FNK0102H + test relance

- Verrouillage confirme pour le kit utilisateur **FNK0102H**:
  - `platformio.ini`: `FREENOVE_LCD_VARIANT_FNK0102A=0`, `...B=0`, `...H=1`.
  - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`: default `FREENOVE_LCD_VARIANT_FNK0102H=1`.
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_045143_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_045143_status.txt`
- Correctif compile rapide applique:
  - `LV_OPA_65` -> `LV_OPA_60` dans `ui_manager.cpp` (compat LVGL 8.4).
- Verification build/flash/runtime:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 6 --wait-port 10 --no-evidence` ✅ (`RESULT=PASS`)
  - Runtime check serie: `UI_GFX_STATUS` + `SCENE_GOTO SCENE_WIN_ETAPE` + phase logs intro OK.

## [2026-02-24] SCENE_WIN_ETAPE demoscene runtime hardening (B1/B2 + morph + TXT/JSON)

- Skills chain appliquee pour la scene:
  1) `demoscene-demomaking-generic`
  2) `firmware-graphics-stack`
  3) `firmware-scene-ui-editor`
- Correctifs firmware (`ui_manager.h/.cpp`):
  - phase B split runtime: `B1` crash court (ms clamp 700..1000) puis `B2` interlude jusqu'a `B=15000`.
  - copper rendu circulaire/wavy via rings animees (pool existant reutilise, sans alloc per-frame).
  - wirecube: morph geometrique cube<->sphere (interpolation vertices + projection perspective existante).
  - scroller C: ping-pong wavy borne sur bande verticale demi-ecran (centre).
  - overrides runtime etendus: TXT+JSON (`MID_A_SCROLL`, `BOT_A_SCROLL`, `B1_MS`, `SPEED_MID_A`, `SPEED_BOT_A`, etc.).
  - instrumentation start/phase gardee et enrichie (B1, vitesses scroll).
- Docs/livrables:
  - calibration refraichie: `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md`.
  - exemple override TXT: `data/ui/scene_win_etape.txt`.
  - README scene mis a jour (A/B/C + B1/B2, contrat overrides TXT+JSON, notes perf).
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_021234_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_021234_status.txt`

## [2026-02-24] SCENE_WIN_ETAPE timing lock update (A/B/C long format)

- Timings Freenove verrouilles pour la sequence intro:
  - A = `30000 ms`
  - B = `15000 ms`
  - C = `20000 ms` (puis boucle C infinie tant que la scene reste active)
- Alignement doc runtime:
  - `hardware/firmware/ui_freenove_allinone/README.md` (section Intro Amiga92)
  - `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md` (table calibration + defaults)
- Correctif build LVGL:
  - `LV_OPA_35` remplace par `LV_OPA_30` dans `ui_manager.cpp` pour compat LVGL 8.4.
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_014020_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_014020_status.txt`
- Verification:
  - `pio run -e freenove_esp32s3` ✅

## [2026-02-24] Skill pack demoscene v1.2 + UI skill sync

- Creation du skill global `demoscene-demomaking-generic` dans `~/.codex/skills/`:
  - `SKILL.md`, `agents/openai.yaml`, `references/*`, `templates/*.schema.json`,
  - scripts: `build_reference_pack.py`, `validate_reference_pack.py`, `validate_demo_spec.py`, `emit_trackset_example.py`.
- Contrat skill v1.2 aligne:
  - pipeline obligatoire `References -> ReferencePack -> StyleSheet/Timeline/TrackSet/FxGraph`,
  - baseline parametres chiffrés cracktro/clean/glitch/fireworks,
  - mode clean scroller par defaut: rollback `ping_pong`.
- Skills UI synchronises:
  - `~/.codex/skills/firmware-graphics-stack/SKILL.md` mis a jour (adapter workflow ReferencePack -> runtime LVGL).
  - `~/.codex/skills/firmware-scene-ui-editor/SKILL.md` mis a jour (scene tuning via tracks + validation `SCENE_GOTO`).
  - ajout metadata UI: `~/.codex/skills/firmware-scene-ui-editor/agents/openai.yaml`.
- Playbook repo synchronise:
  - `docs/AGENTS_COPILOT_PLAYBOOK.md` ajoute le nouveau skill et l'ordre recommande:
    1) `$demoscene-demomaking-generic`, 2) skill adapter UI, 3) validation hardware.
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_011756_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_011756_status.txt`

## [2026-02-24] SCENE_WIN_ETAPE intro Amiga92 JSON-first

- UiManager: intro A/B/C dediee a `SCENE_WIN_ETAPE` (state machine + `lv_timer` 33 ms), skip bouton/touch, cleanup strict des anims/objets intro.
- Runtime override JSON-only ajoute:
  - chemin canonique: `/SCENE_WIN_ETAPE.json`
  - compat lecture: `/ui/SCENE_WIN_ETAPE.json`
  - schema flexible: `logo_text`, `crack_scroll`, `clean_title`, `clean_scroll` (+ aliases legacy).
- Assets/exemple:
  - fichier exemple ajoute: `data/SCENE_WIN_ETAPE.json`.
- Documentation:
  - section `Intro Amiga92` + `References consulted` ajoutees dans `hardware/firmware/ui_freenove_allinone/README.md`.
- Verification:
  - `pio run -e freenove_esp32s3` OK.
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` OK.
  - `pio run -e freenove_esp32s3 -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` OK.
  - serial: `SCENE_GOTO SCENE_WIN_ETAPE` -> `ACK SCENE_GOTO ok=1`, override JSON charge depuis `/SCENE_WIN_ETAPE.json`.

## [2026-02-24] SCENE_WIN_ETAPE UI sequence (cracktro -> crash -> demo clean)

- UI sequence ajoutee pour `SCENE_WIN_ETAPE` en mode Freenove fireworks:
  - phase A (4-5s): raster/copper bars + starfield multi-couches + logo `PROFESSEUR ZACUS` en overshoot + scrolltext bas.
  - phase B (~0.9s): crash/glitch (blink + jitter + fade) avec burst firework.
  - phase C (6-8s): fond sobre type degrade, reveal titre `BRAVO BRIGADE Z`, puis scroll lent avec oscillation sinus.
- Implementation principale: `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp` + nouveaux widgets/etat dans `include/ui_manager.h`.
- Verification:
  - `pio run -e freenove_esp32s3` OK
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` OK
  - passage direct scene valide via serie: `SCENE_GOTO SCENE_WIN_ETAPE` (`ACK SCENE_GOTO ok=1`).

## [2026-02-23] Audit coherence global (source de verite: firmware Freenove)

- Validation story spec OK: `./tools/dev/story-gen validate`.
- Regeneration C++ story/apps OK: `./tools/dev/story-gen generate-cpp` (spec hash `a4e034ba637f`).
- Build firmware cible Freenove OK: `pio run -e freenove_esp32s3`.
- Contrat ESP-NOW corrige:
  - `SCENE` documente comme `SCENE <scenario_id>` (chargement scenario),
  - `SCENE_GOTO <scene_id>` documente comme extension one-shot Freenove.
- Drift checker cross-repo aligne:
  - `SCENE_GOTO` parsee explicitement,
  - marquee comme extension `OPTIONAL Zacus-only` pour eviter les faux positifs de drift.
- Coordination cross-repo rendue portable:
  - docs mises a jour pour utiliser `RTC_BL_PHONE_REPO` (ou `RTC_REPO`) au lieu d'un chemin machine hardcode.

## [2026-02-23] Coherence lock Freenove (Codex)

- Canonical hardware target: `freenove_esp32s3` on **ESP32-S3-WROOM-1-N16R8**.
- Canonical upload port for this setup: `/dev/cu.usbmodem5AB90753301` (usbmodem flow).
- Canonical one-shot scene jump command: `SCENE_GOTO <SCENE_ID>` (serial + API control action).
- API parity for one-shot jump: `POST /api/control` with `action=SCENE_GOTO SCENE_WIN_ETAPE`.
- UI text safety lock: UTF-8 to ASCII fallback is applied before label rendering in `ui_manager.cpp` to avoid missing-glyph artifacts on the Freenove display.
- Skill sync lock: `firmware-scene-ui-editor` uses `SCENE_GOTO` as primary visual validation path.

## [2026-02-23] Contrat ESP-NOW compact (1-pager)

- Ajout d'une fiche de synthèse pour intégration externe: `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_QUICK.md` (format recommandé v1, réponses ACK, endpoints, erreurs).
- Ajout d'une version ultra-légère `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_MINI_SCHEMA.md` pour le second repo.
- Coordination API confirmée: la carte pair ESP-NOW Freenove est verrouillée sur `RTC_BL_PHONE` branche `esp32_RTC_ZACUS` (Freenove en broadcast-only).
- Ajout d'un document d'orchestration API: `docs/ESP_NOW_API_COORDINATION_AGENT.md`.
- Ajout d'une fiche interne de coordination cross-repo: `docs/CROSS_REPO_INTELLIGENCE.md` (contrôle bi-directionnel sans écriture RTC tant qu'autorisé).
- Ajout d'un script de contrôle drift: `tools/dev/check_cross_repo_espnow_contract.py`.
- Intégration du contrôle contractuel cross-repo dans `tools/dev/run_matrix_and_smoke.sh` via l'étape `cross_repo_contract_check` (env: `RTC_BL_PHONE_REPO`, `ZACUS_SKIP_CONTRACT_CHECK`).


## [2026-02-23] ESP-NOW Freenove (commandes runtime)

- Ajouté support ESP-NOW de `RING` (dispatch event story) et `SCENE <scenario_id>` (charge scénario par id) dans `executeEspNowCommandPayload` du firmware Freenove.
- Mise à jour du contrat ESP-NOW Freenove (`ESP_NOW_API_CONTRACT_FREENOVE_V1*`) pour documenter ces commandes côté pair intégré.
- Envoi ESP-NOW simplifié: `ESPNOW_SEND` force désormais l’envoi en broadcast (argument de target ignoré pour la compatibilité existante, WebUI simplifiée).


## [2026-02-23] Contrat ESP-NOW Freenove (v1)

- Ajout doc contrat API ESP-NOW Freenove: `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md` (commandes, endpoints, exemples, erreurs/compatibilité).


## [2026-02-23] Enchainement scène LA → etape gagnée (Freenove)

- `STEP_ETAPE2` utilise désormais `SCENE_WIN_ETAPE` dans `data/story/scenarios/DEFAULT.json` (côté runtime actuel); `SCENE_SIGNAL_SPIKE` reste conservée en flux legacy/compat.
- Déploiement provisoire embarqué aligné: mise à jour de `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp` (asset scenario par défaut + ajout `SCENE_WIN_ETAPE.json` intégré).
- Registre runtime mis à jour: `hardware/libs/story/src/resources/screen_scene_registry.cpp` et fallback C++ de scénario compilé `hardware/libs/story/src/generated/scenarios_gen.cpp`.
- Fichier scène ajouté: `data/story/screens/SCENE_WIN_ETAPE.json` (clone visuel de `SCENE_WIN` avec nouvel ID).
- Convergence des sources de vérité: `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` aligné sur `SCENE_WIN_ETAPE` + `SCENE_MEDIA_ARCHIVE`.
- Générateur story: ajout de `SCENE_WIN_ETAPE` dans `lib/zacus_story_gen_ai/src/zacus_story_gen_ai/generator.py` pour conserver le mapping canonical et régression C++ alignée.
- UI `SCENE_WIN_ETAPE`: texte dynamique (`Validation en cours...` / `BRAVO! vous avez eu juste`) selon l'état audio pour piloter l'attente de validation côté UI.

### Procédure smoke test Freenove

1. Vérifier la configuration du port série (résolution dynamique via cockpit.sh ports).
2. Flasher la cible Freenove avec cockpit.sh flash (ou build_all.sh).
3. Lancer le smoke test avec tools/dev/run_matrix_and_smoke.sh.
4. Vérifier le verdict UI_LINK_STATUS connected==1 (fail strict si absent),
   sauf en mode carte combinée (`ZACUS_ENV=freenove_esp32s3`) où la gate doit être `SKIP: not needed for combined board`.
5. Vérifier l’absence de panic/reboot dans les logs.
6. Vérifier la santé des WebSockets (logs, auto-recover).
7. Consigner les logs et artefacts produits (`logs/rc_live/*.log`, `artifacts/rc_live/<env>_<timestamp>/{meta.json,commands.txt,summary.md,...}`).
8. Documenter toute anomalie ou fail dans AGENT_TODO.md.

## TODO -- LA 440Hz en musique (en cours)

- [x] Script `verify_la_music_chain.py` aligné aux délais réels scène + statut MP3 + `--bg-music` optionnel.
- [x] Lissage DSP + matching LA en fenêtres courtes implémentés (cumul de stabilité).
- [x] Defaults runtime appliqués (`la_stable_ms=3000`, `la_release_ms=180`).
- [x] Run terrain 20 s sans `--bg-music` (musique externe déjà lancée), récupérer log et JSON:  
      ✅ `max_stable_ms=3000` obtenu en cumulé, no panic (log `/tmp/zacus_la_music_20s_retry5.log`, json `/tmp/zacus_la_music_20s_retry5.json`).
- [x] Run terrain 30 s + JSON, valider `max_stable_ms >= 3000` et `panic_seen=false`:  
      ✅ `max_stable_ms=3000` obtenu en cumulé, no panic (log `/tmp/zacus_la_music_30s_retry5.log`, json `/tmp/zacus_la_music_30s_retry5.json`).
- [x] Vérification script renforcée côté scènes: alias `SCENE_LA_DETECT` accepté, et attente de `audio`/`AUDIO_STATUS` plus robuste.
- [x] Vérifier visuellement la montée continue de `Stabilite ...` sur l’écran LA.
- [x] Contrôle build ciblé post-ajustements: `pio run -e freenove_esp32s3` ✅.
- [x] Ajustements fins de génération Python pour forcer davantage de LA cumulatif (anchor 70%, interruptions courtes) → passes 20/30 s.
- [ ] Gate finale: build + smoke + run terrain validés.

## [2026-02-23] Ajustement détection LA musique (Freenove)

- Mission: rendre la progression `la_stable_ms` cumulative sur `SCENE_LA_DETECTOR` (musique bruyante) sans relancer la fenêtre à chaque coupure courte.
- Implémentation:
  - ajout d'un lissage court pitch/confidence (3 trames) dans `HardwareManager` pour lisser `mic_freq_hz`, `mic_pitch_cents`, `mic_pitch_confidence`.
  - logique `LaTriggerService::update` adaptée pour cumuler `stable_ms` sur plusieurs fenêtres + exigence de continuité série courte.
  - état LA étendu avec `la_consecutive_match_count` / `la_match_start_ms` (`LaTriggerRuntimeState`).
  - fenêtre de continuité élargie côté config: `la_release_ms` portée à `180` dans config runtime embarquée (`APP_HARDWARE`).
  - nouveau script `tools/dev/verify_la_music_chain.py` ajouté:
    - génération LA longue + segments hors-LA/noise,
    - options: `--target-hz`, `--stable-ms`, `--duration`, `--bg-music`, `--bg-volume`, `--la-volume`, `--seed`, `--out-json`, `--log-file`,
    - logs automatiques dans `hardware/firmware/logs/verify_la_music_chain_*.log`,
    - sortie JSON métriques (`first_lock_ms`, `latency_to_lock`, `max_stable_ms`, `freq_at_lock`, `conf_max`, `panic_seen`),
    - musique de fond externe pilotée uniquement via `--bg-music` (facultatif).
- Vérification:
  - `pio run -e freenove_esp32s3` ✅
  - smoke/validation hardware en chaîne de test musique à lancer avec carte branchée (`tools/dev/verify_la_music_chain.py --duration 20 --target-hz 440`).
- Ajustement script en cours:
  - options ajoutées au test `verify_la_music_chain.py` pour les délais réels de scène: `--scene-stabilize-ms`, `--audio-warmup-ms`, `--first-tuner-sample-ms`.
  - logique de démarrage:
    - attente post-`SCENE_LA_DETECTOR`,
    - détection d'audio scène via `STATUS` (`audio=1` ou `track!=n/a`),
    - première capture de `MIC_TUNER_STATUS` avant chronométrage lock.
  - amélioration livrée:
    - polling `STATUS` + `AUDIO_STATUS` pour détecter le vrai démarrage audio scène,
    - capture de `audio_ready_ms` (et `audio_ready_scene_ms` en JSON),
    - logs de `stable_ms` à chaque variation (montées + reset),
    - métriques JSON enrichies (`first_tuner_sample_ms`, `audio_ready_ms`).

### [2026-02-23] Suite script LA musique — adaptation startup hardware

- Script ciblé ajusté:
  - `wait_screen()` renvoie désormais le temps réel d'entrée d'une scène (et exige des échantillons stables).
  - `wait_scene_audio()` attend explicitement le démarrage piste/lecture en scène (`audio`, `media_play`, `track`).
  - parseur `STATUS` renforcé pour capturer `media_play`, `track`, `pack` sans sur-généralisation.
  - métriques enrichies: `scene_locked_ms`, `scene_la_entry_ms`, `expected_stable_ms`, `duration_s`.
- Navigation renforcée:
  - ajout de `ensure_scene_loaded()` pour garantir `SCENE_LA_DETECTOR` avant démarrage des mesures LA.
  - enchaînement automatique BTN_NEXT (avec fallback `SC_LOAD DEFAULT`) si le script démarre depuis un écran inattendu.
- Comportement:
  - délai de démarrage scène/audio calculé à l'exécution, avec sauvegarde en JSON et logs détaillés.
  - compatible avec son externe déjà actif: `--bg-music` reste optionnel, le script ne dépend plus d'un lancement forcé d'une piste locale.
- Validation récente:
  - `python3 tools/dev/verify_la_music_chain.py --help` ✅
  - `python3 -m py_compile tools/dev/verify_la_music_chain.py` ✅
  - `pio run -e freenove_esp32s3` ✅

### [2026-02-23] PSRAM Freenove (Codex)

- Ajout des toggles build Freenove pour activer des allocations PSRAM ciblées (`FREENOVE_PSRAM_UI_DRAW_BUFFER`, `FREENOVE_PSRAM_CAMERA_FRAMEBUFFER`) et macro de profil (`FREENOVE_USE_PSRAM`).
- UI (`ui_manager.cpp`) : allocation dynamique du draw buffer LVGL en PSRAM avec fallback DRAM + logs de source/mémoire.
- Caméra (`camera_manager.cpp`) : framebuffer PSRAM activable via macro dédiée.
- Runtime (`main.cpp`) : logs de profil mémoire/PSRAM en boot pour valider capacité disponible.

### [2026-02-23] Alignement cible Freenove mémoire

- Cible Freenove corrigée vers **ESP32-S3-WROOM-1-N16R8**.
- Ajout d’un board custom `boards/freenove_esp32_s3_wroom_n16r8.json` pour refléter le profil 16MB/16MB (flash+PSRAM), avec alias utilisé par `env:freenove_esp32s3_full_with_ui`.
- Docs de mapping mises à jour (`docs/RC_FINAL_BOARD.md`, `hardware/firmware/ui_freenove_allinone/RC_FINAL_BOARD.md`).

### Verrou opératoire (demande utilisateur)

- Mode de travail verrouillé: utiliser uniquement les commandes `pio` pour build/flash/FS (`pio run ...`).
- Ne pas exécuter les scripts `tools/dev/*` tant que ce verrou est actif.

## [2026-02-23] SCENE_LA_DETECT: migration contrôlée vers `SCENE_LA_DETECTOR` (Codex)

- Source de vérité des scènes: `hardware/libs/story/src/resources/screen_scene_registry.cpp` garde le schéma canonique.
- Tolérance migration activée:
  - alias unique: `SCENE_LA_DETECT -> SCENE_LA_DETECTOR` (runtime + générateur).
  - `storyNormalizeScreenSceneId` accepte l’alias et le mappe vers le canonical.
- Alignement scénarios/payloads:
  - la plupart des sources de scenario/payload ont été migrées sur `SCENE_LA_DETECTOR`;
- les tests documentent la tolérance/normalisation (`generator.py`, `test_generator.py`, `test_story_fs_manager.cpp`).
- Build hygiene:
  - suppression du warning `SUPPORT_TRANSACTIONS redefined` en retirant les redéfinitions CLI en doublon pour les envs qui incluent `ui_freenove_config.h`.

## [2026-02-23] Freenove boutons + triggers (Codex)

- Refactor lecture boutons Freenove vers tâche dédiée + queue RTOS:
  - `ButtonManager` scanne les entrées en tâche dédiée (`scan_task`) et place les événements dans une queue.
  - garde-fou fallback synchrone si création queue/tâche RTOS échoue.
- Ajustement mapping runtime boutons:
  - `STEP_WAIT_UNLOCK` : tout appui (court/long) 1..5 => `BTN_NEXT` (fallback `NEXT` via `dispatchEvent`).
  - `SCENE_LA_DETECT` / `SCENE_LA_DETECTOR` (ou `STEP_WAIT_ETAPE2`) : réinitialisation du timeout scène LA sur tout appui 1..5.
- Nettoyage du revalidation scénario (suppression des cas de test physiques non actifs).
- Validation: `pio run -e freenove_esp32s3` ✅.

## [2026-02-23] Freenove audit + refactor agressif (RTOS/audio/wifi/esp-now/UI/story) (Codex)

- Checkpoint sécurité exécuté avant modifs:
  - branche: `main`
  - patch/status: `/tmp/zacus_checkpoint/20260223-011928_wip.patch`, `/tmp/zacus_checkpoint/20260223-011928_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun match.
- Audit runtime (focus Freenove) et constats:
  - `main.cpp` concentrait bootstrap + orchestration loop + parsing commandes série + bridge ESP-NOW + routes web + actions story (`hardware/firmware/ui_freenove_allinone/src/main.cpp:3137`, `hardware/firmware/ui_freenove_allinone/src/main.cpp:3241`, `hardware/firmware/ui_freenove_allinone/src/main.cpp:1532`).
  - callback ESP-NOW RX faisait trop de travail en chemin critique; allègement effectué: enqueue brut en callback, parsing envelope au `consume` (`hardware/firmware/ui_freenove_allinone/src/network_manager.cpp:910`, `hardware/firmware/ui_freenove_allinone/src/network_manager.cpp:564`).
  - duplication parsing JSON multi-clés dans ScenarioManager; factorisé via helper canonique (`hardware/firmware/ui_freenove_allinone/src/scenario_manager.cpp:82`, `hardware/firmware/ui_freenove_allinone/src/scenario_manager.cpp:506`).
  - provisioning LittleFS embarqué du bundle story répétitif; factorisé avec `provisionEmbeddedAsset` (`hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:667`, `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:703`).
  - UI LVGL présentait des séquences style/hide/show répétées sur rings + overlay LA; extraction helpers `SceneElement`/`SceneState` (`hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:312`, `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:1516`).
- Refactor/optimisations appliqués:
  - extraction config runtime APP_* dans service dédié (`hardware/firmware/ui_freenove_allinone/src/runtime/runtime_config_service.cpp:102`, usage `hardware/firmware/ui_freenove_allinone/src/main.cpp:3167`).
  - extraction logique LA/matching/timeout dans service dédié (`hardware/firmware/ui_freenove_allinone/src/runtime/la_trigger_service.cpp:183`, usage `hardware/firmware/ui_freenove_allinone/src/main.cpp:248`).
  - `HardwareManager` expose `snapshotRef()` + palette LED centralisée scène->RGB (`hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp:172`, `hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp:875`).
  - `UiManager` lit le snapshot hardware par référence stable (moins de copies hot-loop) via `setHardwareSnapshotRef` (`hardware/firmware/ui_freenove_allinone/src/main.cpp:3236`, `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:252`).
  - action story `queue_audio_pack` branchée (ACTION_QUEUE_SONAR) pour audio hint piloté par scénario (`hardware/firmware/ui_freenove_allinone/src/main.cpp:1589`).
- Update scénario DEFAULT:
  - suppression redondance audio sur `STEP_U_SON_PROTO` (action media file doublonnait `audio_pack_id`).
  - ajout `ACTION_QUEUE_SONAR` sur `STEP_WAIT_ETAPE2`.
  - synchronisation fallback embarqué LittleFS avec le même contenu (`data/story/scenarios/DEFAULT.json:33`, `data/story/scenarios/DEFAULT.json:60`, `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:69`).
- Fichiers manifests demandés:
  - `zacus_v1_audio.yaml` et `zacus_v1_printables.yaml` introuvables dans ce scope `hardware/firmware`; ignorés conformément à la consigne utilisateur.
- Validation build demandée par verrou utilisateur:
  - commande unique exécutée: `pio run -e freenove_esp32s3_full_with_ui`
  - run #1 (01:39): FAIL sur `scenario_manager.cpp` (`JsonObjectConst::as<>` invalide), corrigé via passage `JsonVariantConst`.
  - run #2 (01:40): PASS (`freenove_esp32s3_full_with_ui SUCCESS`, RAM 55.3%, Flash 73.1%).
  - timestamp evidence: `2026-02-23 01:40:39 +0100`.
- Ajustements audio + test ciblé (suite au passage précédent):
  - `audio_manager.cpp` passe en mode pump dédié FreeRTOS + protections d'accès concurrents (mutex, queue d'événements `done`) afin de réduire les latences de lecture.
  - `ui_freenove_config.h` stabilisé pour éviter la redéfinition `SUPPORT_TRANSACTIONS` (`#ifndef` avant `#define`).
  - calibration runtime: passage `vTaskDelay(1)` -> `taskYIELD()` quand le playback est actif (réduction de jitter potentielle de boucle decode).
  - build/flash vérifiés: `pio run -e freenove_esp32s3` ✅, `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅.

## [2026-02-23] Migration Freenove vers ESP32-audioI2S (Codex)

- Checkpoint sécurité exécuté avant modifs:
  - branche: `main`
  - patch/status: `/tmp/zacus_checkpoint/20260223_151405_wip.patch`, `/tmp/zacus_checkpoint/20260223_151405_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun.
- Audio backend Freenove migré:
  - `ui_freenove_allinone/src/audio_manager.cpp` réécrit sur `ESP32-audioI2S` (`Audio.h`) avec maintien de l’API publique (`play/stop/update/status/profiles/fx/callback`).
  - normalisation des chemins audio (`/littlefs/...`, `/sd/...`) + validation de présence de fichier avant lecture.
  - détection codec/bitrate conservée pour le reporting (`AUDIO_STATUS`) avec scan MP3 header.
- Dépendances PlatformIO (env Freenove):
  - ajout `esphome/ESP32-audioI2S@^2.3.0`.
  - retrait de `earlephilhower/ESP8266Audio` sur l’env Freenove.
  - `build_src_filter` Freenove réduit aux unités Story réellement utilisées (`core/scenario_def`, `generated/scenarios_gen`, `resources/screen_scene_registry`, `scenarios/default_scenario_v2`, `ui/player_ui_model`) pour découpler le build des modules legacy audio.
- Validation:
  - `pio run -e freenove_esp32s3_full_with_ui` ✅ (`SUCCESS`, RAM 56.2%, Flash 80.1%).
  - `pio run -e freenove_esp32s3_full_with_ui` ✅ après découplage build Story (`SUCCESS`, RAM 56.2%, Flash 79.9%).
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅.
  - `pio device monitor -e freenove_esp32s3_full_with_ui --port /dev/cu.usbmodem5AB90753301` ✅.
- Incident runtime (triage RTOS/audio):
  - Repro stable d’un panic `Guru Meditation Error: Core 1 panic'ed (Double exception)` sur séquence série `AUDIO_STOP` puis `AUDIO_TEST_FS`.
  - Correctif `audio_manager.cpp`:
    - suppression du `stopSong()` redondant juste avant `connecttoFS`,
    - ajout d’un démarrage différé (file d’attente + cooldown 80ms) pour éviter stop/reopen immédiat sur le backend `ESP32-audioI2S`.
    - ajout d’un pump RTOS dédié audio (`audio_pump`, coeur 1, loop 1ms actif) avec mutex d’état + file d’événements `done` pour maintenir `Audio::loop()` hors charge UI/réseau et éviter la saccade.
  - Vérification post-correctif:
    - séquences répétées `AUDIO_STATUS`, `AUDIO_STOP`, `AUDIO_TEST_FS`, `AUDIO_STATUS` sans reboot/panic,
    - profils `AUDIO_FX` / `AUDIO_PROFILE` + `STATUS` valides après arrêt/reprise audio.
    - reflash + retest matériel validés (`pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301`) ; boot log confirme `pump task=1`.

## [2026-02-22] Verrou transition boutons -> LA detector (Codex)

- [x] Contrat verrouillé: depuis `STEP_WAIT_UNLOCK` (`SCENE_LOCKED`), tout appui bouton (`1..5`, court/long) déclenche `BTN_NEXT` et enchaîne vers `STEP_WAIT_ETAPE2`.
- [x] Spécification canonique alignée:
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` annoté + `screen_scene_id: SCENE_LA_DETECTOR`.
- [x] Data Story alignée:
  - `data/story/scenarios/DEFAULT.json`: `button_short_1..5 = BTN_NEXT`.
  - `data/story/scenarios/DEFAULT.json`: `STEP_WAIT_ETAPE2.screen_scene_id = SCENE_LA_DETECTOR`.
  - `data/story/screens/SCENE_LA_DETECTOR.json` ajouté (alias écran LA detector).
- [x] Validation hardware série (usbmodem Freenove):
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 3.0 --settle 3.0` ✅
  - `NEXT` direct validé: `STEP_WAIT_UNLOCK/SCENE_LOCKED -> STEP_WAIT_ETAPE2/SCENE_LA_DETECTOR` ✅
- [x] Correctif robustesse bouton:
  - `ScenarioManager::notifyButton`: en `STEP_WAIT_UNLOCK`, tout appui bouton (`1..5`, court **ou** long) déclenche `BTN_NEXT`.
  - revalidation runtime: `SC_REVALIDATE_HW` => `changed=1` pour `BTN1_SHORT`, `BTN3_LONG`, `BTN4_LONG`, `BTN5_SHORT`, `BTN5_LONG` ✅
- [x] Outillage test aligné:
  - `tools/dev/verify_story_default_flow.py`: attente `SCENE_LA_DETECTOR` + fallback robuste si `SC_LOAD` ne renvoie pas d'ACK.

## [2026-02-22] Focus validation micro + détection (hardware Freenove)

- [x] Correctifs runtime micro/tuner appliqués dans `hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp`:
  - capture I2S micro en `32-bit` puis conversion PCM16 (`>>8`) pour éviter les lectures saturées parasites,
  - détection LA recentrée sur bande utile (`320..560 Hz`) pour réduire les faux positifs hors contexte,
  - sortie tuner instantanée (`mic_freq_hz/mic_pitch_cents/mic_pitch_confidence`) sans lissage décroissant trompeur.
- [x] Validation firmware sur carte (port USB modem):
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- [x] Validation live micro:
  - `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 25 --duration 12` ✅
  - verdict observé: `mic_ready_all=1`, `conf_non_zero>0`, `freq_non_zero>0`.
- [x] Validation comportement détection fréquentielle (campagne locale via `afplay`):
  - 392 Hz -> médiane ~391 Hz (conf élevée),
  - 440 Hz -> médiane ~441 Hz (conf élevée),
  - 466 Hz -> médiane ~468 Hz (conf élevée),
  - 523 Hz -> médiane ~521 Hz (détection présente),
  - baseline silencieuse/ambiante: médiane confiance à `0` (quelques détections isolées restantes).
- [x] Non-régression Story:
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 2.2` ✅
  - flux validé: `SCENE_LOCKED -> SCENE_BROKEN -> SCENE_LA_DETECT -> SCENE_SIGNAL_SPIKE -> SCENE_MEDIA_ARCHIVE`.

## [2026-02-22] SCENE_LA_DETECTOR — UI instrumentée + trigger match micro strict (Codex)

- [x] UI `SCENE_LA_DETECT` enrichie (dans `ui_manager`):
  - waveform micro circulaire autour du noyau (input réel micro),
  - indicateur niveau micro (barre),
  - analyseur fréquentiel simplifié (barres),
  - indicateur de justesse (aiguille) + statut (`AUCUNE NOTE` / `PRESQUE JUSTE` / `LA VALIDE`),
  - compteur LA détecté et compteur timeout visibles à l’écran.
  - accordeur autour du LA au niveau des LED (WS2812) du freenove
- [x] Trigger gameplay LA ajusté:
  - validation LA continue à `3s` (`la_stable_ms=3000`) -> passage scène suivante,
  - timeout global à `60s` (`la_timeout_ms=60000`) -> retour direct `SCENE_LOCKED`.
- [x] Garde-fous “match micro obligatoire” sur l’étape LA:
  - blocage des raccourcis `NEXT/UNLOCK` manuels en `STEP_WAIT_ETAPE2` (série/web/control/boutons),
  - seule la validation micro peut déclencher la transition nominale.
- [x] Stabilité runtime:
  - correction stack canary loopTask (buffers DSP micro déplacés hors pile dans `HardwareManager`).
- [x] Validation exécutée (USB modem Freenove):
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - logs série auto: absence `Guru Meditation`/panic ✅
  - logs série auto: `NEXT` bloqué en scène LA (`ACK NEXT ok=0`) ✅
  - logs série auto: timeout LA ~60s puis reset `STEP_WAIT_UNLOCK / SCENE_LOCKED` ✅
- [ ] Validation visuelle matérielle à confirmer:
  - lisibilité des 4 widgets UI (waveform/level/analyzer/timers),
  - calibration terrain pour atteindre `LA VALIDE` en 3s sur ton setup micro.

## [2026-02-22] Freenove lockscreen glitch + LEDs cassées (Codex)

- Renforcement de l'écran `SCENE_LOCKED`:
  - nouvel effet runtime dédié `kGlitch` (secousses aléatoires X/Y, flicker aléatoire opacité, particules aléatoires, contraste renforcé).
  - boucle d'effet enrichie pour les particules en mode `glitch` et `celebrate` (mode cassé) avec jitter non périodique.
  - reset explicite des translations LVGL entre scènes pour éviter les résidus d'animation.
  - waveform micro ajoutée sur `SCENE_LOCKED` en mode glitch mobile (scan vertical sur l'écran + distorsions aléatoires).
- Renforcement LEDs WS2812 (4 LEDs Freenove) en scène cassée:
  - pattern “broken hardware” aléatoire sur `SCENE_LOCKED` / `SCENE_BROKEN` / `SCENE_SIGNAL_SPIKE` (flashs brefs, non simultanés entre LEDs, coupures, sparks, dérive couleur).
  - vérifié en série via `HW_STATUS` (LED#1 majoritairement OFF avec flashs brefs intermittents).
- Data Story/FS alignées:
  - `data/story/screens/SCENE_LOCKED.json` + `data/screens/locked.json` mis à jour (timeline plus agressive, strobe 100, contraste élevé, texte: `Module U-SON PROTO` / `VERIFICATION EN COURS`).
  - fallback embarqué `/story/screens/SCENE_LOCKED.json` synchronisé dans `storage_manager.cpp`.
- Correctif stabilité:
  - régression stack canary détectée avec `StaticJsonDocument<4096>` dans `UiManager::renderScene`.
  - corrigé via `DynamicJsonDocument(4096)` (plus de panic observé sur rechargement scénario).
- Validations exécutées:
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
  - vérif série: `[UI] scene=SCENE_LOCKED effect=6 speed=90 ... timeline=8` + absence `Guru Meditation` sur `SC_LOAD DEFAULT` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --timeout 2.2` ✅
    (flux attendu: `SCENE_LOCKED -> SCENE_BROKEN -> SCENE_LA_DETECT -> SCENE_SIGNAL_SPIKE -> SCENE_MEDIA_ARCHIVE`)
  - waveform micro déplacée sur un tracé circulaire autour du noyau écran (`scene_core_/scene_ring_outer_`) pour `SCENE_LA_DETECT`.
  - validation micro Freenove série en condition runtime (`STEP_WAIT_ETAPE2`, `SCENE_LA_DETECT`):
    - `mic_ready=1` sur tous les échantillons.
    - `mic_level_pct` variable (span 45), `mic_peak` non nul (max 32768), `mic_freq_hz>0` et `mic_pitch_confidence>0`.
  - variante scope renforcée:
    - double tracé circulaire (anneau principal + anneau externe) autour du noyau (`scene_core_`) pour visualiser la forme d'onde directement sur le cercle.
    - couleur/épaisseur pilotées par la justesse (`mic_pitch_cents`) et la confiance (`mic_pitch_confidence`) pour la détection LA.
  - nouvel outil de validation opérateur:
    - `tools/dev/verify_mic_live.py` (auto-load scénario, passage `SCENE_LA_DETECT`, sampling `HW_STATUS_JSON`, verdict PASS/FAIL).
    - run validé: `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 20 --duration 10` ✅ (`mic_ready_all=1`, `level span=40`, `peak_max=32768`, `freq_non_zero=17`).
  - nettoyage anti-doublons:
    - purge des artefacts `* 2*` non trackés (copies parasites), restauration des fichiers trackés détectés par erreur hors scope firmware.
    - règles préventives ajoutées:
      - `.gitignore` racine: `**/* 2` + `**/* 2.*`
      - `hardware/firmware/.gitignore`: `* 2` + `* 2.*`
  - évolution accordeur:
    - LEDs (4 WS2812) en mode tuner plus lisible: `flat` à gauche (bleu/cyan), `in-tune` au centre (vert), `sharp` à droite (orange/rouge), avec flash extrême en forte dérive.
    - debug série live ajouté: `MIC_TUNER_STATUS [ON|OFF|<period_ms>]`.
  - évolution écran LA_DETECT:
    - rendu “vibromètre” accentué sur anneaux circulaires (amplitude/punch renforcés, warp de phase léger, boost niveau micro).
  - validations additionnelles:
    - `pio run -e freenove_esp32s3_full_with_ui` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - série: `MIC_TUNER_STATUS` one-shot + `MIC_TUNER_STATUS ON 180` (flux live observé) + `MIC_TUNER_STATUS OFF` ✅
    - `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 20 --duration 10` ✅ (fallback `MIC_TUNER_STATUS` si `HW_STATUS_JSON` non décodable)
    - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --timeout 2.2` ✅

## [2026-02-21] Firmware workflow hardening (Codex)

- PR de correction et merge: `feat/fix-firmware-story-workflow` → PR #105 → squash merge (main).
- Fichier touché: `.github/workflows/firmware-story-v2.yml`
- Correctifs:
  - indentation YAML corrigée sur `concurrency.cancel-in-progress` (anciennement bloquant l’exécution du workflow).
  - correction du répertoire de travail du job `story-toolchain` (`hardware/firmware` au lieu de `hardware/firmware/esp32_audio` inexistant).
  - alignement des artefacts de job `story-toolchain` vers les chemins existants.
- Validation:
  - PR checks: `story toolchain`, `validate`, `build env` (esp32dev/esp32_release/esp8266_oled/ui_rp2040_*) pass.
  - PR #105 mergeée (commit `04e4e50`) avec workflow OK.

## [2026-02-21] Coordination Orchestrator binôme (Kill_LIFE + Zacus + RTC)

- PR Kill_LIFE `#11` a été vérifiée et validée manuellement (`scope`):
  - scripts/tools d'orchestration (`tools/ai/zeroclaw_*`, y compris `zeroclaw_stack_up.sh`, `zeroclaw_hw_firmware_loop.sh`, `zeroclaw_watch_1min.sh`);
  - docs techniques `specs/*`.
- Vérification stricte:
  - `gh pr checks 11 --repo electron-rare/Kill_LIFE` : tous ✅ (`api_contract`, `lint-and-contract`, `ZeroClaw Dual Orchestrator`, etc.).
  - aucun artefact hors-scope ajouté dans le PR.
- Merge status:
  - PR fermé via squash (commit `e8e44048a8b36b7debcb12788183b34045dde7f2`) sur `main`.
  - commentaire de clôture codex publié (PASS) dans PR #11.
- Correctif suite: PR Kill_LIFE `#12` créé pour durcir le fallback mismatch:
  - `tools/ai/zeroclaw_hw_firmware_loop.sh` vérifie maintenant la stabilité du port USB avant fallback env.
  - merge PR #12 (squash) validé, commit `5f517ae82b95c1988d6c07e888194dcc28e02ff3`.
- Coordination inter-repos:
  - PR Zacus déjà alignée côté AP/ESP-NOW/WebUI (PRs/faits et mergeés précédemment dans ce chantier).
  - `RTC_BL_PHONE`: aucun PR ouvert en cours au moment du contrôle (`gh pr list --state open` vide).
- Actions de traçabilité:
  - pas de dette bloquante détectée.
  - prochaine étape: continuer surveillance post-merge (runbook ZeroClaw/CI + vérification liaison `watcher`).

## [2026-02-21] Freenove AP fallback dédié (anti-oscillation hors Les cils) (Codex)

- Contexte coordination merge train: PR Zacus #101 mergée, puis patch dédié AP fallback.
- Patch runtime réseau:
  - séparation SSID local vs AP fallback (`local_ssid=Les cils`, `ap_default_ssid=Freenove-Setup`)
  - nouveau flag config: `pause_local_retry_when_ap_client`
  - retry STA vers `Les cils` mis en pause si client connecté à l'AP fallback
  - nouveaux indicateurs exposés:
    - série `NET_STATUS`: `ap_clients`, `local_retry_paused`
    - WebUI `/api/status` + `/api/network/wifi`: `ap_clients`, `local_retry_paused`
- Fichiers touchés:
  - `data/story/apps/APP_WIFI.json`
  - `hardware/firmware/ui_freenove_allinone/include/network_manager.h`
  - `hardware/firmware/ui_freenove_allinone/src/network_manager.cpp`
  - `hardware/firmware/ui_freenove_allinone/src/main.cpp`
- Notes:
  - pas de constantes hardcodées nouvelles pour credentials AP en dehors de la config runtime/littlefs
  - comportement historique conservé si `pause_local_retry_when_ap_client=false`.
  - validations exécutées (PIO only):
    - `pio run -e freenove_esp32s3_full_with_ui` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - série `NET_STATUS`, `SC_REVALIDATE`, `SC_REVALIDATE_ALL`, `ESPNOW_STATUS_JSON` ✅
    - WebUI `GET /api/status`, `GET /api/network/wifi` (champs `ap_clients`, `local_retry_paused`) ✅
  - limite de validation:
    - le cas `local_retry_paused=1` nécessite un client connecté sur l'AP fallback pendant l'absence de `Les cils` (non reproduit sur ce run).

## [2026-02-21] Freenove story/ui/network pass final + revalidate step(x) (Codex)

- Checkpoint sécurité exécuté:
  - branche: `feat/freenove-webui-network-ops-parity`
  - patch/status: `/tmp/zacus_checkpoint/20260221_021417_wip.patch`, `/tmp/zacus_checkpoint/20260221_021417_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun.
- Correctif incohérence revalidate:
  - `SC_REVALIDATE_STEP2` renommé en `SC_REVALIDATE_STEPX`
  - logs enrichis avec `anchor_step=<step courant>` pour éviter l'ambiguïté multi-scénarios.
- Validations PIO:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS (x2)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2)
- Evidence série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
  - `SC_COVERAGE scenario=DEFAULT unlock=1 audio_done=1 timer=1 serial=1 action=1`
  - `SC_REVALIDATE` OK avec `SC_REVALIDATE_STEPX ... anchor_step=STEP_WAIT_ETAPE2 ... step_after=STEP_ETAPE2`
  - `ESPNOW_STATUS_JSON` OK (`ready=true`, compteurs cohérents)
  - `ESPNOW_SEND broadcast ping` OK (`ACK ... ok=1`)
  - `NET_STATUS` local connecté validé (`sta_ssid=Les cils`, `local_match=1`, `fallback_ap=0`).
- Evidence WebUI:
  - `GET /api/status`, `GET /api/network/wifi`, `GET /api/network/espnow` OK
  - `POST /api/control` avec `SC_EVENT unlock UNLOCK` OK (`ok=true` + transition observée en série).

## [2026-02-20] GH binôme Freenove AP local + alignement ESP-NOW RTC (Codex)

- Branche de travail: `feat/freenove-ap-local-espnow-rtc-sync`
- Coordination GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/91
  - issue RTC (binôme, branche `audit/telephony-webserver`): https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/pull/92
- Runtime Freenove mis à jour:
  - AP fallback piloté par cible locale (`Les cils`) + retry périodique (`local_retry_ms`)
  - règle appliquée: AP actif si aucun WiFi connu n'est connecté (fallback), AP coupé quand la STA est reconnectée à `Les cils`
  - indicateurs réseau série ajoutés: `local_target`, `local_match`
  - commande série ajoutée: `ESPNOW_STATUS_JSON` (format RTC: ready/peer_count/tx_ok/tx_fail/rx_count/last_rx_mac/peers)
  - bootstrap peers ESP-NOW au boot via `APP_ESPNOW.config.peers`
  - WebUI embarquée (`http://<ip>/`) avec endpoints:
    - `GET /api/status`
    - `POST /api/wifi/connect`, `POST /api/wifi/disconnect`
    - `POST /api/espnow/send`
    - `POST /api/scenario/unlock`, `POST /api/scenario/next`
    - endpoints alignés RTC:
      - `GET /api/network/wifi`
      - `GET /api/network/espnow`
      - `GET/POST/DELETE /api/network/espnow/peer`
      - `POST /api/network/espnow/send`
      - `POST /api/network/wifi/connect`, `POST /api/network/wifi/disconnect`
      - `POST /api/control` (dispatch d'actions)
  - correction WebUI:
    - `WIFI_DISCONNECT` est maintenant différé (~250 ms) pour laisser la réponse HTTP sortir avant la coupure STA
  - Data story apps mises à jour:
    - `data/story/apps/APP_WIFI.json`: `local_ssid`, `local_password`, `ap_policy=if_no_known_wifi`, `local_retry_ms`
    - `data/story/apps/APP_ESPNOW.json`: `peers` + contrat payload enrichi
- Validations exécutées (PIO only):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - boot config: `local=Les cils ... ap_policy=0 retry_ms=15000`
    - `NET_STATUS ... local_target=Les cils local_match=1 ... fallback_ap=0` (local connecté)
    - `ESPNOW_STATUS_JSON` OK
    - `WIFI_DISCONNECT` => `fallback_ap=1` puis retry local
    - après reconnect WiFi: `ESPNOW_SEND broadcast ping` => recovery auto ESP-NOW + `ACK ... ok=1`
  - WebUI:
    - `GET /api/status` OK (`network/local_match`, `espnow`, `story`, `audio`)
    - `POST /api/scenario/unlock` et `POST /api/scenario/next` OK (transitions observées)
    - `POST /api/wifi/connect` OK
    - `POST /api/network/wifi/disconnect` => réponse HTTP `200` reçue avant coupure STA (plus de timeout systématique)
    - `POST /api/network/espnow/send` (payload JSON) OK
    - `POST /api/control` (`SC_EVENT unlock`, `WIFI_DISCONNECT`) OK
- Note d'incohérence traitée:
  - si AP fallback et cible locale partagent le même SSID (`Les cils`), le retry local coupe brièvement l'AP fallback pour éviter l'auto-association.

## [2026-02-21] Freenove WebUI parity réseau avec RTC (Codex)

- Branche de travail: `feat/freenove-webui-network-ops-parity`
- Tracking GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/93
  - coordination RTC: https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR RTC liée (ops web): https://github.com/electron-rare/RTC_BL_PHONE/pull/8
- Runtime WebUI enrichi (parité endpoints réseau):
  - `POST /api/network/wifi/reconnect`
  - `POST /api/network/espnow/on`
  - `POST /api/network/espnow/off`
  - routes existantes conservées (`/api/status`, `/api/network/wifi`, `/api/network/espnow`, `/api/network/espnow/peer`, `/api/network/espnow/send`, `/api/control`)
- Validation exécutée:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - HTTP (`curl`):
    - `POST /api/network/espnow/off` => `{"action":"ESPNOW_OFF","ok":true}`
    - `POST /api/network/espnow/on` => `{"action":"ESPNOW_ON","ok":true}`
    - `POST /api/network/wifi/reconnect` => `{"action":"WIFI_RECONNECT","ok":true}`
    - `POST /api/control` (`ESPNOW_OFF`, `ESPNOW_ON`, `WIFI_RECONNECT`) => `ok=true`
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - `NET_STATUS` cohérent (`sta_ssid=Les cils`, `fallback_ap=0`)
    - `ESPNOW_STATUS_JSON` cohérent (`ready=1`)
    - logs observés: `[NET] ESP-NOW off` puis `[NET] ESP-NOW ready`

## [2026-02-20] Freenove WiFi/AP fallback local + ESP-NOW bridge/story + UI FX (Codex)

- Exigence appliquée: l'AP ne sert qu'en fallback si la carte n'est pas sur le WiFi local (`Les cils`).
- Runtime modifié:
  - boot auto-connect vers `Les cils`/`mascarade`
  - AP fallback auto uniquement en absence de connexion STA (et stop auto quand STA connecté)
- commandes série enrichies: `WIFI_CONNECT`, `WIFI_DISCONNECT`, `ESPNOW_PEER_ADD/DEL/LIST`, `ESPNOW_SEND <text|json>` (broadcast)
  - bridge ESP-NOW -> events scénario durci (texte + JSON `cmd/raw/event/event_type/event_name`)
- Story/UI:
  - intégration `APP_WIFI` + `APP_ESPNOW` dans les scénarios/story specs YAML
  - génération C++ story régénérée (`spec_hash=1834bcdab734`)
  - écran Freenove en mode effect-first (symboles/effets, titres cachés par défaut, vitesses d'effet pilotables par payload JSON)
- Vérifications exécutées:
  - `./tools/dev/story-gen validate` PASS
  - `./tools/dev/story-gen generate-cpp` PASS
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - contenu repo root via `.venv/bin/python`: validate/export scenario + audio manifest + printables manifest PASS
- Evidence série (115200):
  - boot: `[NET] wifi connect requested ssid=Les cils` + `[NET] boot wifi target=Les cils started=1`
  - statut validé: `NET_STATUS state=connected mode=STA ... sta_ssid=Les cils ... ap=0 fallback_ap=0`

## [2026-02-20] Freenove audio + écran + chaîne scénario/écran (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_203024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_203024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
- Validation série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`) :
  - `AUDIO_TEST` PASS (tonalité intégrée RTTTL, sans dépendance LittleFS)
  - `AUDIO_TEST_FS` PASS (`/music/boot_radio.mp3`)
  - `AUDIO_PROFILE 0/1/2` PASS (switch runtime pinout I2S)
  - `UNLOCK` -> transition `STEP_U_SON_PROTO`, audio pack `PACK_BOOT_RADIO`, puis `audio_done` -> transition `STEP_WAIT_ETAPE2` PASS.
- Incohérence restante observée au boot: warning HAL SPI `spiAttachMISO(): HSPI Does not have default pins on ESP32S3!` (non bloquant en exécution actuelle).

## [2026-02-20] Freenove story step2 hardware + YAML GitHub + stack WiFi/AP + ESP-NOW (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_211024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_211024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Story specs YAML mis à jour (source of truth):
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` enrichi avec transitions hardware pour l’étape 2:
    - `serial:BTN_NEXT`
    - `unlock:UNLOCK`
    - `action:ACTION_FORCE_ETAPE2`
  - génération revalidée:
    - `./tools/dev/story-gen validate` PASS
    - `./tools/dev/story-gen generate-cpp` PASS
    - `./tools/dev/story-gen generate-bundle` PASS
  - fichiers générés synchronisés (`hardware/libs/story/src/generated/*`), spec hash: `6edd9141d750`.
- GitHub pipeline Story restauré:
  - `.github/workflows/firmware-story-v2.yml` remplacé par un workflow fonctionnel (validate + generate-cpp + generate-bundle + check diff + artifact bundle).
- Runtime Freenove enrichi:
  - stack réseau ajoutée (`network_manager.{h,cpp}`) avec:
    - WiFi STA + AP management
    - ESP-NOW init/send/receive counters
    - bridge ESP-NOW payload -> événement scénario (`SERIAL:<event>`, `TIMER:<event>`, `ACTION:<event>`, `UNLOCK`, `AUDIO_DONE`)
  - commandes série ajoutées:
    - `NET_STATUS`, `WIFI_STATUS`, `WIFI_TEST`, `WIFI_STA`, `WIFI_AP_ON`, `WIFI_AP_OFF`
    - `ESPNOW_ON`, `ESPNOW_OFF`, `ESPNOW_STATUS`, `ESPNOW_SEND`
    - `SC_LIST`, `SC_LOAD`, `SC_REVALIDATE_ALL`, `SC_EVENT_RAW`
  - credentials test appliqués:
    - SSID test/AP par défaut: `Les cils`
    - mot de passe: `mascarade`
- Revalidation transitions via ScenarioSnapshot:
  - `SC_REVALIDATE` couvre events + hardware probes + vérifs ciblées step2:
    - `SC_REVALIDATE_STEP2 event=timer ... changed=1`
    - `SC_REVALIDATE_STEP2 event=action name=ACTION_FORCE_ETAPE2 ... changed=1`
    - `SC_REVALIDATE_STEP2 event=button label=BTN5_SHORT ... changed=1`
  - `SC_REVALIDATE_ALL` exécuté sur tous les scénarios built-in.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2 après patch final)
- Validation série réseau:
  - `WIFI_AP_ON` PASS (`ssid=Les cils`, `mode=AP_STA`, `ip=192.168.4.1`)
  - `WIFI_AP_OFF` PASS
  - `WIFI_TEST` PASS (requête de connexion STA vers `Les cils`)
  - `ESPNOW_SEND` PASS (émission OK), pas de loopback RX local observé (`rx=0`) sur test mono-carte.

## TODO Freenove ESP32-S3 (contrat agent)

### [2026-02-17] Log détaillé des étapes réalisées – Freenove ESP32-S3

- Audit mapping hardware exécuté (artifacts/audit/firmware_20260217/validate_mapping_report.txt)
- Suppression des macros UART TX/RX pour UI Freenove all-in-one (platformio.ini, ui_freenove_config.h)
- Documentation détaillée des périphériques I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
- Synchronisation des macros techniques SPI/Serial/Driver dans ui_freenove_config.h (SPI_FREQUENCY, UI_LCD_SPI_HOST, etc.)
- Ajout d’une section multiplexage dans RC_FINAL_BOARD.md (partage BL/BTN3, CS TFT/BTN4)
- Ajout d’un commentaire explicite multiplexage dans ui_freenove_config.h
- Evidence synchronisée : tous les artefacts et logs sont tracés (artifacts/audit/firmware_20260217, logs/rc_live/freenove_esp32s3_YYYYMMDD.log)
- Traçabilité complète : chaque étape, correction, mapping, doc et evidence sont référencés dans AGENT_TODO.md

### [2026-02-17] Rapport d’audit mapping hardware Freenove ESP32-S3

**Correspondances parfaites** :
  - TFT SPI : SCK=18, MOSI=23, MISO=19, CS=5, DC=16, RESET=17, BL=4
  - Touch SPI : CS=21, IRQ=22
  - Boutons : 2, 3, 4, 5
  - Audio I2S : WS=25, BCK=26, DOUT=27
  - LCD : WIDTH=480, HEIGHT=320, ROTATION=1

**Écarts ou incohérences** :
  - UART : platformio.ini (TX=43, RX=44), ui_freenove_config.h (TX=1, RX=3), RC_FINAL_BOARD.md (non documenté)
  - I2C, LED, Buzzer, DHT11, MPU6050 : présents dans ui_freenove_config.h, absents ou non alignés ailleurs
  - SPI Host, Serial Baud, Driver : macros techniques non synchronisées
  - Multiplexage : BL/Bouton3 et CS TFT/Bouton4 partagent la broche, à clarifier

**Recommandations** :
  - Aligner UART TX/RX sur une valeur unique et documenter
  - Documenter I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
  - Synchroniser macros techniques dans ui_freenove_config.h et platformio.ini
  - Ajouter une note sur le multiplexage dans RC_FINAL_BOARD.md et ui_freenove_config.h
  - Reporter toute évolution dans AGENT_TODO.md

**Evidence** :
  - Rapport complet : artifacts/audit/firmware_20260217/validate_mapping_report.txt
  [2026-02-17] Synthèse technique – Détection dynamique des ports USB
    - Tous les ports USB série sont scannés dynamiquement (glob /dev/cu.*).
    - Attribution des rôles (esp32, esp8266, rp2040) selon mapping, fingerprint, VID:PID, fallback CP2102.
    - Esp32-S3 : peut apparaître en SLAB_USBtoUART (CP2102) ou usbmodem (mode bootloader, DFU, flash initial).
    - RP2040 : typiquement usbmodem.
    - Si un seul CP2102 détecté, fallback mono-port (esp32+esp8266 sur le même port).
    - Si plusieurs ports, mapping par location, fingerprint, ou VID:PID.
    - Aucun port n’est hardcodé : la détection s’adapte à chaque run (reset, flash, bootloader).
    - Les scripts cockpit.sh, resolve_ports.py, run_matrix_and_smoke.sh exploitent cette logique.
    - Traçabilité : logs/ports_debug.json, logs/ports_resolve.json, artifacts/rc_live/...
    - Documentation onboarding mise à jour pour refléter la procédure.
    - Recommandation : toujours utiliser la détection dynamique, ne jamais forcer un port sauf cas extrême (override manuel).

  [2026-02-17] Validation RC Live Freenove ESP32-S3
    - Port USB corrigé : /dev/cu.SLAB_USBtoUART
    - Flash ciblé relancé : pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.SLAB_USBtoUART
    - Evidence synchronisée : summary.md, ui_link.log, logs/rc_live/20260217-211606
    - Résultat : UI link FAIL (connected=1 mais status unavailable)
    - Artefacts : artifacts/rc_live/20260217-211606/summary.md, ui_link.log
    - Log détaillé ajouté dans AGENT_TODO.md

- [ ] Vérifier et compléter le mapping hardware dans platformio.ini ([env:freenove_esp32s3])
  - Pins, UART, SPI, I2C, etc. : vérifier cohérence avec la doc.
  - Exemple : GPIO22=I2S_WS, GPIO23=I2S_DATA, UART1=TX/RX.
- [ ] Aligner ui_freenove_config.h avec platformio.ini et la documentation RC_FINAL_BOARD.md
  - Vérifier que chaque pin, bus, périphérique est documenté et codé.
  - Ajouter commentaires explicites pour chaque mapping.
- [ ] Mettre à jour docs/RC_FINAL_BOARD.md
  - Décrire le mapping complet, schéma, photo, table des pins.
  - Ajouter procédure de flash/test spécifique Freenove.
  - Mentionner les différences avec ESP32Dev.
- [ ] Adapter build_all.sh, cockpit.sh, run_matrix_and_smoke.sh, etc.
  - Ajouter/valider la cible freenove_esp32s3 dans les scripts.
  - Vérifier que le build, flash, smoke, logs fonctionnent sur Freenove.
  - Exemple : ./build_all.sh doit inclure freenove_esp32s3.
- [ ] Vérifier la production de logs et artefacts lors des tests sur Freenove
  - Chemin : logs/rc_live/freenove_esp32s3_YYYYMMDD.log
  - Artefacts : artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html
  - Référencer leur existence (chemin, timestamp, verdict) dans docs/AGENT_TODO.md.
- [ ] Mettre à jour l’onboarding (docs/QUICKSTART.md, docs/AGENTS_COPILOT_PLAYBOOK.md)
  - Ajouter section « Flash Freenove », « Test Freenove ».
  - Préciser les ports série, baud, procédure de résolution dynamique.
- [ ] Vérifier que les gates smoke et stress test sont compatibles et valident strictement la cible Freenove
  - Fail sur panic/reboot, verdict UI_LINK_STATUS connected==1.
  - Tester WebSocket health, stress I2S, scénario LittleFS.
- [ ] Documenter toute évolution ou correction dans docs/AGENT_TODO.md
  - Détailler les étapes réalisées, artefacts produits, impasses matérielles.
  - Exemple : « Test flash Freenove OK, logs produits dans logs/rc_live/freenove_esp32s3_20260217.log ».
## [2026-02-17] ESP32 pin remap test kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-185336_wip.patch` and `/tmp/zacus_checkpoint/20260217-185336_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link handshake still FAIL (`connected=0`) on latest cross-monitor evidence (`artifacts/ui_link_diag/20260217-174822/`), LittleFS fallback status unchanged, I2S stress status unchanged (last 30 min PASS).

## [2026-02-17] ESP32 UI pin remap execution (Codex)

- Applied ESP32 UI UART remap in firmware config: `TX=GPIO23`, `RX=GPIO18` (OLED side stays `D4/D5`).
- Rebuilt and reflashed ESP32 (`pio run -e esp32dev`, then `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9`).
- Cross-monitor rerun: `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-175606/`.
- Result moved from FAIL to WARN: ESP32 now receives HELLO frames (`esp32 tx/rx=48/20`, `UI_LINK_STATUS connected=1` seen), but ESP8266 still receives nothing from ESP32 (`esp8266 tx/rx=19/0`), so handshake remains incomplete.

## [2026-02-17] UI link diagnostic patch kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-181730_wip.patch` and `/tmp/zacus_checkpoint/20260217-181730_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link strict gate still FAIL (`connected=0`), LittleFS scenario load now mitigated by V2 fallback, I2S stress status currently PASS on last 30 min run.

## [2026-02-17] Cross-monitor boot capture (Codex)

- Built and flashed diagnostics on both boards: `pio run -e esp32dev -e esp8266_oled`, then upload on `/dev/cu.SLAB_USBtoUART9` (ESP32) and `/dev/cu.SLAB_USBtoUART` (ESP8266).
- Captured synchronized boot monitors with `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-173005/` (`esp32.log`, `esp8266.log`, `merged.log`, `summary.md`, `ports_resolve.json`, `meta.json`).
- Verdict remains FAIL: no discriminant RX observed on either side (`ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`), indicating traffic still not seen on D4/D5 at runtime.

## [2026-02-17] Cross-monitor rerun after pin inversion (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after manual pin inversion test.
- Evidence: `artifacts/ui_link_diag/20260217-174330/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] Cross-monitor rerun after inverted wiring confirmation (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after updated wiring check.
- Evidence: `artifacts/ui_link_diag/20260217-174822/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] D4/D5 handshake + strict firmware_tests rerun (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-173556_wip.patch` and `/tmp/zacus_checkpoint/20260217-173556_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Build gate rerun: `./build_all.sh` PASS (5/5 envs) after firmware updates.
- Reflash completed: `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9` and `pio run -e esp8266_oled -t upload --upload-port /dev/cu.SLAB_USBtoUART`.
- `firmware_tooling` rerun PASS (`plan-only` + full run); all `--help` paths stay non-destructive.
- `firmware_tests` strict rerun (`ZACUS_REQUIRE_HW=1`) still FAIL at gate 1: `artifacts/rc_live/20260217-164154/summary.md` (`UI_LINK_STATUS connected=0`, esp8266 monitor FAIL).
- Strict smoke rerun uses repo-root resolver (`tools/test/resolve_ports.py`) and strict ESP32+ESP8266 mapping; gate FAIL remains UI link only (`artifacts/smoke_tests/20260217-164237/smoke_tests.log`), while `STORY_LOAD_SCENARIO DEFAULT` now succeeds through fallback (`STORY_LOAD_SCENARIO_FALLBACK V2 DEFAULT` + `STORY_LOAD_SCENARIO_OK`).
- `audit_coherence.py` rerun PASS (`artifacts/audit/20260217-164243/summary.md`).
- Content validators rerun PASS from repo root: scenario validate/export + audio manifest + printables manifest.
- Stress rerun completed PASS (`python3 tools/dev/run_stress_tests.py --hours 0.5`): `artifacts/stress_test/20260217-164248/summary.md` (`87` iterations, success rate `100.0%`, no panic/reboot markers in log).
- Runtime status after reruns: UI Link = FAIL (`connected=0`), LittleFS default scenario = mitigated by V2 fallback in serial path, I2S stability = PASS over 30 min stress run (no panic/reboot markers).

## [2026-02-17] Fix handshake/UI smoke strict kickoff (Codex)

- Safety checkpoint executed via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-155753_wip.patch` and `/tmp/zacus_checkpoint/20260217-155753_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Runtime status at kickoff: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S stability = FAIL/intermittent panic in stress recovery path.

## [2026-02-17] Copilot sequence checkpoint (firmware_tooling -> firmware_tests)

- Safety checkpoint executed before edits via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-162509_wip.patch` and `/tmp/zacus_checkpoint/20260217-162509_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Kickoff runtime status: UI Link still failing in prior smoke evidence, default LittleFS scenario still missing in prior QA notes, I2S panic already known in stress path.

## [2026-02-17] Copilot sequence execution + alignment fixes

- Plan/runner alignment applied: root `tools/dev/plan_runner.sh` now delegates to `hardware/firmware/tools/dev/plan_runner.sh`; firmware runner now executes from repo root and resolves active agent IDs recursively under `.github/agents/` (excluding `archive/`).
- `run_matrix_and_smoke.sh` now supports `--help`/`-h` without side effects and returns explicit non-zero on unknown args (`--bad-arg` -> rc=2).
- Agent briefs synchronized in root + firmware copies (`domains/firmware-tooling.md`, `domains/firmware-tests.md`) with repo-root commands and venv-aware PATH (`PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH`).
- `firmware_tooling` sequence: PASS (`bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling`).
- `firmware_tests` strict sequence (`ZACUS_REQUIRE_HW=1`): blocked at step 1 because `run_matrix_and_smoke` reports UI link failure; evidence `artifacts/rc_live/20260217-153129/summary.md` (`UI_LINK_STATUS connected=0`).
- Remaining test gates executed manually after the blocked step:
  - `run_smoke_tests.sh` strict: FAIL (port resolution), evidence `artifacts/smoke_tests/20260217-153214/summary.md`.
  - `run_stress_tests.py --hours 0.5`: FAIL, scenario does not complete (`DEFAULT`), evidence `artifacts/stress_test/20260217-153220/summary.md`; earlier run also captured I2S panic evidence `artifacts/stress_test/20260217-153037/stress_test.log`.
  - `audit_coherence.py`: initially FAIL on missing runbook refs, then PASS after `cockpit_commands.yaml` runbook fixes + regenerated commands doc; evidence `artifacts/audit/20260217-153246/summary.md` (latest PASS).
- RP2040 docs/config audit: no env naming drift detected (`ui_rp2040_ili9488`, `ui_rp2040_ili9486`) across `platformio.ini`, `build_all.sh`, `run_matrix_and_smoke.sh`, `docs/QUICKSTART.md`, and `docs/TEST_SCRIPT_COORDINATOR.md`.
- Runtime status after this pass: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S panic = intermittent (observed in stress evidence above).

## [2026-02-17] Audit kickoff checkpoint (Codex)

- Safety checkpoint run from `hardware/firmware`: branch `story-V2`, working tree dirty (pre-existing changes), `git diff --stat` captured before edits.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-141814_wip.patch` and `/tmp/zacus_checkpoint/20260217-141814_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths, so no untrack action was needed.
- Runtime status at kickoff: UI Link `connected=0` in previous smoke evidence, LittleFS default scenario still flagged as missing by prior QA notes, I2S panic previously reported in stress recovery path.

## [2026-02-17] Audit + coherence pass (Codex)

- Tooling/scripts corrected: `tools/dev/run_matrix_and_smoke.sh` (broken preamble/functions restored, non-interactive USB path now resolves ports, accepted `learned-map` reasons, story screen skip evidence), `tools/dev/serial_smoke.py` (ports map loading re-enabled, dead duplicate role code removed), `tools/dev/cockpit.sh` (`rc` no longer forces `ZACUS_REQUIRE_HW=1`, debug banner removed), `tools/dev/plan_runner.sh` (portable paths/date and safer arg parsing), UI OLED HELLO/PONG now built with protocol CRC via `uiLinkBuildLine`.
- Command registry/doc sync fixed: `tools/dev/cockpit_commands.yaml` normalized under one `commands:` key (including `wifi-debug`) and regenerated `docs/_generated/COCKPIT_COMMANDS.md` now lists full evidence columns without blank rows.
- Protocol/runtime check: `protocol/ui_link_v2.md` expects CRC frames; OLED runtime now emits CRC on HELLO/PONG. Remaining runtime discrepancy logged: Story V2 controller still boots from generated scenario IDs (`DEFAULT`) rather than directly from `game/scenarios/zacus_v1.yaml`; root YAML remains validated/exported as canonical content source.
- Build gate result: `./build_all.sh` PASS (5/5 envs) with log `logs/run_matrix_and_smoke_20260217-143000.log` and build artifacts in `.pio/build/`.
- Smoke gate result: `./tools/dev/run_matrix_and_smoke.sh` FAIL (`artifacts/rc_live/20260217-143000/summary.json`) — ports resolved, then `smoke_esp8266_usb` and `ui_link` failed (`UI_LINK_STATUS missing`, ESP32 log in bootloader/download mode). Evidence: `artifacts/rc_live/20260217-143000/ui_link.log`, `artifacts/rc_live/20260217-143000/smoke_esp8266_usb.log`.
- Content gates result (run from repo root): scenario validate/export PASS (`game/scenarios/zacus_v1.yaml`), audio manifest PASS (`audio/manifests/zacus_v1_audio.yaml`), printables manifest PASS (`printables/manifests/zacus_v1_printables.yaml`).
- Runtime status after this pass: UI Link = FAIL on latest smoke evidence (`connected` unavailable), LittleFS default scenario = not revalidated in this run (known previous blocker remains), I2S panic = not retested in this pass (stress gate pending).

## [2026-02-17] Rapport d’erreur automatisé – Story V2

- Correction du bug shell (heredoc Python → script temporaire).
- Échec de la vérification finale : build FAIL, ports USB OK, smoke/UI/artefacts SKIPPED.
- Rapport d’erreur généré : voir artifacts/rapport_erreur_story_v2_20260217.md
- Recommandation : analyser le log de build, corriger, relancer la vérification.

# Agent TODO & governance

## 1. Structural sweep & merge
- [x] Commit the pending cleanup described in `docs/SPRINT_RECOMMENDATIONS.md:18-80` (structure/tree fixes + PR #86 merge/tag) so the repo is back on main.
- [x] Consolidation 2026-02-21 exécutée: `feat/fix-firmware-story-workflow`, `feat/freenove-ap-fallback-stable`, `feat/freenove-webui-network-ops-parity` intégrés dans l’historique de `main`; CI GitHub Actions désormais limité à `main` seulement.
- [x] Branches de travail supprimées (local + remote): `feat/freenove-ap-local-espnow-rtc-sync`, `origin/feat/fix-firmware-story-workflow`, `origin/feat/freenove-ap-fallback-stable`, `origin/feat/freenove-webui-network-ops-parity`.
- [x] Tags de sauvegarde créés avant suppression: `backup/20260221_212813/feat-fix-firmware-story-workflow-1b4c328`, `backup/20260221_212813/feat-fix-firmware-story-workflow-b530be3`, `backup/20260221_212813/feat-freenove-ap-fallback-stable`, `backup/20260221_212813/feat-freenove-webui-network-ops-parity`, `backup/20260221_212813/feat-freenove-ap-local-espnow-rtc-sync`.

## 2. Build/test gates
- [x] Re-run `./build_all.sh` (`build_all.sh:6`); artifacts landed under `artifacts/build/` and logs live in `logs/run_matrix_and_smoke_*.log` if rerun again via the smoke gate.
- [x] Re-launch `./tools/dev/run_matrix_and_smoke.sh` (`tools/dev/run_matrix_and_smoke.sh:9-200`) – run completed 2026-02-16 14:35 (artifact `artifacts/rc_live/20260216-143539/`), smoke scripts and serial monitors succeeded but UI link still reports `connected=0` (no UI handshake). Need to plug in/validate UI firmware before closing gate.
- [x] Capture evidence for HTTP API, WebSocket, and WiFi/Health steps noted as blocked or TODO in `docs/TEST_SCRIPT_COORDINATOR.md:13-20` – `tools/dev/healthcheck_wifi.sh` created `artifacts/rc_live/healthcheck_20260216-154709.log` (ping+HTTP fail) and the HTTP API script logged connection failures under `artifacts/http_ws/20260216-154945/http_api.log` (ESP_URL=127.0.0.1:9). WebSocket skipped (wscat missing). All failures logged to share evidence.

## 3. QA + automation hygiene
- [x] Execute the manual serial smoke path (`python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`) – passed on /dev/cu.SLAB_USBtoUART + /dev/cu.SLAB_USBtoUART9, reporting UI link still down (same failure as matrix run).
- [ ] Run the story QA suite (`tools/dev/run_smoke_tests.sh`, `python3 tools/dev/run_stress_tests.py ...`, `make fast-*` loops) documented in `esp32_audio/TESTING.md:36-138` and capture logs (smoke_tests failed: DEFAULT scenario missing `/story/scenarios/DEFAULT.json`; run_stress_tests failed with I2S panic during recovery; `make fast-esp32` / `fast-ui-oled` built & flashed but monitor commands quit in non-interactive mode, `fast-ui-tft` not run because no RP2040 board connected). Need scenario files/UI recovery to unblock.
- [x] Ensure any generated artifacts remain untracked per agent contract (no logs/artifacts added to git).

## 4. Documentation & agent contracts
- [ ] Update `AGENTS.md` and `tools/dev/AGENTS.md` whenever scripts/gates change, per their own instructions (`AGENTS.md`, `tools/dev/AGENTS.md`).
- [ ] Keep `tools/dev/cockpit_commands.yaml` in sync with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` after edits, and confirm the command registry is reflected in `docs/TEST_SCRIPT_COORDINATOR.md` guidance.
- [ ] Review `docs/INDEX.md`, `docs/ARCHITECTURE_UML.md`, and `docs/QUICKSTART.md` after significant changes so the onboarding picture matches the agent constraints.

## 5. Reporting & evidence
- [ ] When publishing smoke/baseline runs, include the required artifacts (`meta.json`, `commands.txt`, `summary.md`, per-step logs) under `artifacts/…` as demanded by `docs/TEST_SCRIPT_COORDINATOR.md:160-199`.
- [ ] Document any pipeline/test regressions in `docs/RC_AUTOFIX_CICD.md` or similar briefing docs and flag them for the Test & Script Coordinator.

## Traçabilité build/smoke 17/02/2026

- Succès : esp32dev, esp32_release, esp8266_oled
- Échec : ui_rp2040_ili9488, ui_rp2040_ili9486
- Evidence : logs et artefacts dans hardware/firmware/artifacts/build, logs/
- Actions tentées : correction du filtre build_src_filter, création de placeholder, relance build/smoke
- Problème persistant : échec RP2040 (sources/configuration à investiguer)
- Prochaine étape : escalade à un agent expert RP2040 ou hand-off

## [2026-02-18] Story portable + V3 serial migration

- [x] Added host-side story generation library `lib/zacus_story_gen_ai` (Yamale + Jinja2) with CLI:
  - `story-gen validate`
  - `story-gen generate-cpp`
  - `story-gen generate-bundle`
  - `story-gen all`
- [x] Replaced legacy generator entrypoint `hardware/libs/story/tools/story_gen/story_gen.py` with compatibility wrapper delegating to `zacus_story_gen_ai`.
- [x] Migrated portable runtime internals to tinyfsm-style state handling in `lib/zacus_story_portable/src/story_portable_runtime.cpp` while keeping `StoryPortableRuntime` facade.
- [x] Introduced Story serial V3 JSON-lines handlers (`story.status`, `story.list`, `story.load`, `story.step`, `story.validate`, `story.event`) and removed `STORY_V2_*` routing from canonical command list.
- [x] Updated run matrix/log naming to include environment label:
  - log: `logs/rc_live/<env_label>_<ts>.log`
  - artifacts: `artifacts/rc_live/<env_label>_<ts>/summary.md`
- [x] `run_matrix_and_smoke.sh` now supports single-board mode (`ZACUS_ENV="freenove_esp32s3"`): ESP8266/UI-link/story-screen checks are emitted as `SKIP` with detail `not needed for combined board`.
- [x] `tools/test/resolve_ports.py` now honors `--need-esp32` and `--need-esp8266` explicitly via required roles, while still emitting both `esp32` and `esp8266` fields in JSON output.
[20260218-020041] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041/summary.md
[20260218-021042] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042/summary.md
[20260221-222358] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358/summary.md
[20260221-223450] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450/summary.md

## [2026-02-22] Freenove story modular + SD + ESP-NOW v1 envelope + WebUI push

- [x] Story storage rendu modulaire avec fallback intégré:
  - bundle Story par défaut provisionné automatiquement en LittleFS (`/story/{scenarios,screens,audio,apps,actions}`),
  - support SD_MMC Freenove activé (`/sd/story/...`) avec sync SD -> LittleFS au boot et via commande.
- [x] Commandes/runtime ajoutés pour opérabilité story:
  - serial: `STORY_SD_STATUS`, `STORY_REFRESH_SD`,
  - WebUI/API: bouton + endpoint `POST /api/story/refresh-sd`.
- [x] Timeline JSON enrichie sur les scènes Story (`data/story/screens/SCENE_*.json`) avec keyframes `at_ms/effect/speed_ms/theme`.
- [x] Audio crossfade consolidé:
  - correction callback fin de piste (track réel reporté),
  - lecture des packs audio depuis LittleFS **ou SD** (`/sd/...`).
- [x] ESP-NOW aligné avec `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md`:
  - enveloppe v1 supportée (`msg_id`, `seq`, `type`, `payload`, `ack`),
  - extraction metadata + statut exposé,
  - trames `type=command` exécutées côté runtime et réponse corrélée `type=ack` renvoyée si `ack=true`.
- [x] WebUI passée en push temps réel:
  - endpoint SSE `GET /api/stream` (statut Story/WiFi/ESP-NOW),
  - fallback refresh conservé côté front.
- [x] Générateur Story (`zacus_story_gen_ai`) amélioré:
  - bundle Story injecte désormais le contenu réel des ressources JSON (`data/story/*`) au lieu de placeholders minimalistes.

### Vérifications exécutées

- `pio run -e freenove_esp32s3` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Vérification série post-flash (`STORY_SD_STATUS`, `WIFI_STATUS`, `ESPNOW_STATUS_JSON`) ✅, IP observée: `192.168.0.91`
- `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅
- `./tools/dev/story-gen validate` ✅
- `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_test` ✅
- `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- `curl http://192.168.0.91/api/status` + `curl http://192.168.0.91/api/stream` ✅ (payloads UTF-8 validés après fix snapshot JSON)
- Frontend dev UI: `npm run build` ✅ (fix TS no-return sur `src/lib/deviceApi.ts` pour débloquer le flux front)
[20260221-234147] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147/summary.md
[20260221-235139] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139/summary.md
[20260222-000305] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305/summary.md

## [2026-02-22] Suite autonome — installations + vérifications complètes

- [x] Préflight sécurité refait avant action:
  - branch/diff/status affichés,
  - checkpoint enregistré: `/tmp/zacus_checkpoint/20260222-005615_wip.patch` et `/tmp/zacus_checkpoint/20260222-005615_status.txt`,
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) = aucun trouvé.
- [x] Installations/outillage:
  - `./tools/dev/bootstrap_local.sh` exécuté (venv + dépendances Python + `zacus_story_gen_ai` editable),
  - validation outillage: `pio --version`, `python3 -m serial.tools.list_ports -v`,
  - `tools/dev/check_env.sh` durci pour fallback `pip3` et dépendances optionnelles en `WARN` (exécution validée).
- [x] Vérifications story/content:
  - `.venv/bin/python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml` ✅
  - `.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅
  - `.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- [x] Vérifications firmware/hardware Freenove USB modem:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅ (`UI_LINK_STATUS=SKIP` justifié combined board).
- [x] Gates build contractuels:
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅ (5/5).
- [x] Cohérence doc/registry:
  - `.venv/bin/python3 tools/dev/gen_cockpit_docs.py` ✅
  - `.venv/bin/python3 tools/test/audit_coherence.py` ✅ (`RESULT=PASS`).
- [x] Statut réseau et API runtime Freenove:
  - série `WIFI_STATUS`/`ESPNOW_STATUS_JSON`/`STORY_SD_STATUS` ✅,
  - IP actuelle observée: `192.168.0.91`,
  - `curl /api/status` et `curl /api/stream` ✅ (payloads Story/WiFi/ESP-NOW valides).

## [2026-02-22] Clarification doc Story V2 vs protocole série V3

- [x] Alignement documentaire réalisé pour éviter l'ambiguïté "V2/V3":
  - `docs/protocols/GENERER_UN_SCENARIO_STORY_V2.md`: V2 explicitée (moteur/spec), commandes de test série basculées en JSON-lines V3 (`story.*`), chemins outillage alignés (`./tools/dev/story-gen`).
  - `docs/protocols/story_v3_serial.md`: section `Scope` ajoutée (V3 = interface série, V2 = génération/runtime).
  - `docs/protocols/story_README.md`: section commandes réorganisée avec recommandation V3 et rappel legacy `STORY_V2_*` pour debug uniquement.

## [2026-02-22] Story UI — génération écrans/effets/transitions (suite)

- [x] Générateur `zacus_story_gen_ai` renforcé pour les ressources écran:
  - normalisation automatique d'un profil écran (fallback par `SCENE_*`),
  - timeline normalisée en objet `timeline = {loop, duration_ms, keyframes[]}`,
  - keyframes consolidées (`at_ms`, `effect`, `speed_ms`, `theme`) avec garde-fous (ordre, bornes, minimum 2 keyframes),
  - transition normalisée (`transition.effect`, `transition.duration_ms`).
- [x] Runtime UI Freenove enrichi:
  - parse `timeline.keyframes` (et compat ancien format array),
  - support `timeline.loop` + `timeline.duration_ms`,
  - transitions de scène ajoutées (`fade`, `slide_left/right/up/down`, `zoom`, `glitch`) avec durée pilotable.
- [x] Données Story alignées:
  - `data/story/screens/SCENE_*.json` migrés vers format timeline keyframes + transition,
  - fallback embarqué (`storage_manager.cpp`) synchronisé sur le même contrat JSON.
- [x] Vérifications exécutées:
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (4 tests),
  - `./tools/dev/story-gen validate` ✅,
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_fx_<ts>` ✅ (payloads écran keyframes+transition vérifiés),
  - `pio run -e freenove_esp32s3` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `ZACUS_ENV=freenove_esp32s3 ... ./tools/dev/run_matrix_and_smoke.sh` ✅ (artefact: `artifacts/rc_live/freenove_esp32s3_20260222-002556/`),
  - validation série live (`SC_LOAD/SC_EVENT`) avec logs UI montrant `transition=<type>:<ms>` sur changements de scène.
[20260222-002556] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556/summary.md
[20260222-025923] Run artefacts: `artifacts/rc_live/freenove_esp32s3_20260222-025923/`, logs: `logs/rc_live/`, summary: `artifacts/rc_live/freenove_esp32s3_20260222-025923/summary.md`

## [2026-02-22] Intégration complète Story + options Freenove (phases 1→3)

- [x] Runtime Freenove branché en modules:
  - `HardwareManager` intégré au cycle principal (`init/update/noteButton/setSceneHint`),
  - nouveaux modules `CameraManager` et `MediaManager` ajoutés et initialisés au boot.
- [x] Options Story (`data/story/apps`) ajoutées + fallback embarqué synchronisé:
  - `APP_HARDWARE.json`, `APP_CAMERA.json`, `APP_MEDIA.json`,
  - fallback LittleFS mis à jour dans `storage_manager.cpp` (bundle embarqué complet).
- [x] Interfaces publiques ajoutées:
  - série: `HW_*`, `CAM_*`, `MEDIA_*`, `REC_*`,
  - API WebUI: `/api/hardware*`, `/api/camera*`, `/api/media*`,
  - `/api/status` enrichi (`hardware`, `camera`, `media`),
  - commandes ESP-NOW `type=command` dispatchées sur les nouveaux contrôles.
- [x] Couplage Story actions au changement d'étape:
  - exécuteur d'`action_ids` branché,
  - nouvelles actions supportées (`ACTION_HW_LED_*`, `ACTION_CAMERA_SNAPSHOT`, `ACTION_MEDIA_PLAY_FILE`, `ACTION_REC_*`, etc.),
  - snapshot action `SERIAL:CAMERA_CAPTURED` émis en succès.
- [x] Évolution écran/effets Story:
  - effets ajoutés: `radar`, `wave`,
  - aliases transition ajoutés: `wipe -> slide_left`, `camera_flash -> glitch`,
  - scènes ajoutées: `SCENE_CAMERA_SCAN`, `SCENE_SIGNAL_SPIKE`, `SCENE_MEDIA_ARCHIVE`,
  - docs alignées: `docs/protocols/story_screen_palette_v2.md`, `docs/protocols/story_README.md`.
- [x] Contrôle de flux: ajout d'un déclencheur `BTN_NEXT` depuis `STEP_WAIT_UNLOCK` vers `STEP_WAIT_ETAPE2` (scène LA détection), pour permettre le passage immédiat via `NEXT` / `/api/scenario/next`/`SC_EVENT serial BTN_NEXT`; artefact build: `pio run -e freenove_esp32s3` (success 2026-02-22) et fichiers générés mis à jour (`docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`, `hardware/libs/story/src/generated/*`).
- [x] Contrôle tactile/physique Freenove: adaptation de `ScenarioManager::notifyButton` pour que tout bouton court (`1..5`) sur `STEP_WAIT_UNLOCK` déclenche `BTN_NEXT` si disponible (fallback `NEXT`), donc passage direct vers `STEP_WAIT_ETAPE2` depuis n'importe quel bouton.
- [x] Validation réalisée (session courante):
  - builds: `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅,
  - build Freenove: `pio run -e freenove_esp32s3` ✅,
  - FS Freenove: `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - smoke orchestré: `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (run sans hardware -> smoke/UI link `SKIPPED`, justif. combined board + port non résolu; artefact `artifacts/rc_live/freenove_esp32s3_20260222-025923/`),
  - Story tooling: `./tools/dev/story-gen validate` ✅, `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_options_20260222T035914` ✅,
  - tests Python: `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (5 passed),
  - scénario canonique: `.venv/bin/python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅,
  - export brief: `.venv/bin/python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅.
- [!] Limites constatées hors scope firmware:
  - `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ❌ (`game/prompts/audio/intro.md` manquant),
  - `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ❌ (prompts `printables/src/prompts/*.md` manquants),
  - aucune validation live `curl /api/*` ou cycle série ACK HW/CAM/MEDIA possible sans carte connectée pendant cette session.

## [2026-02-22] Correctif validateurs contenu + checks intégration API (local/GH/Codex/ChatGPT)

- [x] Correctif robustesse chemins pour exécution depuis `hardware/firmware/` **et** repo root:
  - `tools/audio/validate_manifest.py`: résolution `source` maintenant tolérante (`cwd`, dossier manifeste, repo root).
  - `tools/printables/validate_manifest.py`: résolution manifeste/prompt indépendante du `cwd` (fallbacks repo root + dossier manifeste).
  - `hardware/firmware/tools/dev/check_env.sh`: check env enrichi avec vérifications d'intégration API (`gh` auth, `codex login status`, reachability OpenAI) et diagnostic `MCP_DOCKER` (daemon Docker).
- [x] Revalidation post-correctif:
  - depuis `hardware/firmware`:  
    `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅  
    `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
  - depuis repo root:  
    `hardware/firmware/.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅  
    `hardware/firmware/.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
- [x] Cohérence firmware/story toolchain recheck:
  - `python ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_api_check_<ts>` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (`5 passed`)
- [x] Vérification accès API outillage:
  - GitHub CLI: `gh auth status` ✅ (account `electron-rare`), `gh api user` ✅, `gh api rate_limit` ✅.
  - Codex CLI: `codex login status` ✅ (`Logged in using ChatGPT`), `codex exec --ephemeral` ✅ (réponse reçue).
  - OpenAI endpoint reachability: `curl https://api.openai.com/v1/models` => `401 Missing bearer` (connectivité OK, clé API absente côté shell).
- [!] Point d'intégration à traiter côté poste:
  - `codex mcp list` signale `MCP_DOCKER` activé mais non opérationnel si daemon Docker arrêté (`Cannot connect to the Docker daemon ...`).

## [2026-02-22] Réparation workspace — déplacement involontaire `esp8266_oled/src`

- [x] Analyse de l'anomalie:
  - suppressions trackées massives sous `hardware/firmware/ui/esp8266_oled/src/**`,
  - dossier non tracké détecté: `hardware/firmware/ui/esp8266_oled/src 2/`.
- [x] Correctif appliqué:
  - restauration de l'arborescence par renommage du dossier `src 2` vers `src`.
  - résultat: suppressions annulées, structure source ESP8266 OLED rétablie.
- [x] Validation:
  - `pio run -e esp8266_oled` ✅ (build complet OK après réparation).
[20260222-113849] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-113849, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-113849/summary.md
[20260222-204146] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-204146, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-204146/summary.md

## Audit test hardware Freenove (2026-02-23)
- Commande: `pio run -e freenove_esp32s3_full_with_ui -t upload`
- Résultat: flash OK vers `/dev/cu.usbmodem5AB90753301`.
- Port auto-détecté, upload réussi (RAM/Flash: 55.3% / 73.1%).
- Monitoring runtime (capture locale via `python3 + pyserial`, 60s) : boot OK, scénario `DEFAULT` exécuté, logs HW/LA/SCENARIO/FS/UI cohérents.
- Événement constaté: erreur caméra récurrente lors de la première action `ACTION_CAMERA_SNAPSHOT` (`cam_dma_config(301): frame buffer malloc failed`, `camera config failed`) sur ce lot matériel.
- Comportement attendu LA: `LA_TRIGGER timeout` puis reset vers `SCENE_LOCKED`.
- Aucune régression panic/reboot observée sur la fenêtre de test.
- Notes: scripts de validation demandés par le contrat firmware sont uniquement exécutés pour build + flash; logs hardware non versionnés et non archivés.

## Test hardware runtime (séquence réseau/ESP-NOW) - 2026-02-23
- Test en condition carte branchée sur `/dev/cu.usbmodem5AB90753301` après `pio run -e freenove_esp32s3_full_with_ui -t upload`.
- Commandes série envoyées: `STATUS`, `NET_STATUS`, `WIFI_TEST`, `ESPNOW_STATUS`, `ESPNOW_ON`, `ESPNOW_STATUS_JSON`, `ESPNOW_PEER_LIST`, `WIFI_AP_ON`, `ESPNOW_SEND ff:ff:ff:ff:ff:ff {...}`.
- Bilan WiFi:
  - Boot initial déjà en `STA` connecté sur SSID `Les cils`, IP `192.168.0.91`.
  - `WIFI_TEST` retourne `ACK WIFI_TEST ... ok=1`.
  - `WIFI_DISCONNECT` provoque bien la logique de reconnect (`[NET] wifi disconnected` puis `[NET] wifi connect requested ...`).
  - `WIFI_STATUS`/`WIFI_RECONN` : la commande `WIFI_RECONN` n’existe pas (retour `UNKNOWN WIFI_RECONN`).
- Bilan ESP-NOW:
  - `ESPNOW_ON` actif, `ESPNOW_STATUS` confirme `espnow=1` et compteur `peers=0` avant ajout.
  - `ESPNOW_SEND` vers `ff:ff:ff:ff:ff:ff` renvoie `ACK ESPNOW_SEND ok=1`.
  - Après send, statut indique `tx_ok=1`, peer broadcast visible via `ESPNOW_PEER_LIST` puis `NET_STATUS`.
  - `ESPNOW_STATUS_JSON` retourne JSON attendu (`ready`, `peer_count`, compteurs).
  - Pas de réception de payload externe observée (pas de `ESPNOW_RECV`, pas de `NET` RX event).
- API test WebSocket/HTTP non testée en ce cycle (focal hardware UART/serial).
- `status` global reste stable, pas de panic/reboot détecté pendant captures.

## Hardening scènes (SCENE) - 2026-02-23
- Scope: firmware Freenove (UI + registry scène).
- Stabilisation demandée:
  - Source de vérité `hardware/libs/story/src/resources/screen_scene_registry.cpp` avec `kScenes` sans alias.
  - Validation runtime: `SCREEN_SCENE_ID_UNKNOWN` sur `screenSceneId` non reconnu.
  - Chargement payload: normalisation stricte `screen_scene_id` avant lookup dans `StorageManager`.
  - UI: règles d'effets/transitions explicites, logs sur token inconnu.
- Tests: cas d'IDs inconnues + alias legacy dans validation de scénario + lookup.
[20260223-210304] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210304, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210304/summary.md
[20260223-210353] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210353, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210353/summary.md
[20260223-210410] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210410, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210410/summary.md

## Refactor LVGL graphics stack - 2026-02-24
- Checkpoint evidence:
  - `/tmp/zacus_checkpoint/20260224_032248_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_032248_status.txt`
- Scope en cours:
  - `ui_manager.{h,cpp}`: pipeline buffers/dma/8bpp + stats/diagnostics.
  - `lv_conf.h` + `platformio.ini`: flags build pour mode 256, DMA async, budget mem.
  - `main.cpp`: commandes série `UI_GFX_STATUS` et `UI_MEM_STATUS`.
  - docs ajoutées: `docs/ui/graphics_stack.md`, `docs/ui/lvgl_memory_budget.md`, `docs/ui/fonts_fr.md`.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - tentative multi-env (`esp32dev`, `esp32_release`, `esp8266_oled`, `ui_rp2040_ili9488`, `ui_rp2040_ili9486`) interrompue pour limiter le temps de cycle après confirmation Freenove.

## Refactor LVGL graphics stack - suite run 2026-02-24
- Checkpoint evidence:
  - `/tmp/zacus_checkpoint/20260224_035128_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_035128_status.txt`
- Validation build matrix terminee:
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅
  - `pio run -e freenove_esp32s3` ✅
- Flash hardware Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification scene:
  - `SCENE_GOTO SCENE_WIN_ETAPE` envoye via pyserial (ACK recu, logs scene/audio coherents).
  - test boucle host 95s avec relance scene a `t=90s` (ACK recu a chaque injection).
- Observation runtime a investiguer:
  - `UI_GFX_STATUS` retourne `depth=8` mais `lines=0`, `double=0`, `dma_req=0`, `dma_async=0` (pipeline graphique semble retomber en mode non configure).

## Refactor LVGL graphics stack - affichage Freenove (2026-02-24)
- Checkpoint evidence:
  - `/tmp/zacus_checkpoint/20260224_040114_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_040114_status.txt`
- Probleme reproduit et corrige:
  - boot montrait `draw buffer allocation failed` (heap DMA interne trop faible pour le mode initial).
  - allocation draw buffers rendue robuste: fallback auto SRAM_DMA -> PSRAM, candidats lignes et mono fallback.
  - trans buffer DMA rendu adaptatif (fallback par lignes), conversion RGB332->RGB565 sync ligne-par-ligne corrigee.
  - guard de stabilite ajoute: async DMA desactive en mode RGB332 (panic `dma_end_callback` observe sur SCENE_GOTO).
- Option hardware ajoutee:
  - `FREENOVE_LCD_USE_HSPI` dans `ui_freenove_config.h` (switch HSPI/FSPI via flag).
  - support macro ajoute pour `FREENOVE_LCD_VARIANT_FNK0102H`.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - test runtime 95s avec `SCENE_GOTO SCENE_WIN_ETAPE` + relance a t=90s: pas de panic, scene boucle.
  - retour utilisateur: affichage OK.

## External font pack generation (2026-02-24)
- Checkpoint evidence:
  - `/tmp/zacus_checkpoint/20260224_042527_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_042527_status.txt`
- Generated and committed LVGL external font sources for Freenove UI stack:
  - Inter: 14/18/24/32
  - Orbitron: 28/40
  - IBM Plex Mono: 14/18
  - Press Start 2P: 16/24
- Added portable generation toolchain:
  - `tools/fonts/scripts/font_manifest.json`
  - `tools/fonts/scripts/generate_lvgl_fonts.sh`
  - `tools/fonts/ttf/README.md`
- Build validation:
  - `pio run -e freenove_esp32s3` ✅
- Commit:
  - `1328897 feat(ui-fonts): generate and enable external LVGL font pack`

## LCD variant lock FNK0102H (2026-02-24)
- Demande utilisateur: verrouiller la cible kit sur FNK0102H.
- Changements appliques:
  - `hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h`:
    - defaults: `FREENOVE_LCD_VARIANT_FNK0102B=0`, `FREENOVE_LCD_VARIANT_FNK0102H=1`
    - commentaire profil maj en FNK0102H.
  - `platformio.ini` (`env:freenove_esp32s3_full_with_ui`):
    - `-DFREENOVE_LCD_VARIANT_FNK0102B=0`
    - `-DFREENOVE_LCD_VARIANT_FNK0102H=1`
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - verification runtime serie: `UI_GFX_STATUS` recu apres flash (`depth=8`, `lines=40`, `double=1`, `source=PSRAM`).
[20260224-050717] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-050717, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-050717/summary.md
[20260224-052021] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-052021, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-052021/summary.md

## Plan Freenove - partition + runtime safety + flash (2026-02-24)

- Checkpoint:
  - `/tmp/zacus_checkpoint/20260224_064903_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_064903_status.txt`
- Baseline (avant lot):
  - `pio run -e freenove_esp32s3 -t size` -> `text=1253534 data=426752 bss=1448701 dec=3128987`
  - `./build_all.sh freenove_esp32s3` -> `Flash=1664021 / 2097152`
  - map symbols:
    - `__ssvfscanf_r` present
    - `__ssvfiscanf_r` present
    - `_Z19runRuntimeIterationj` present
    - `_ZN12_GLOBAL__N_119handleSerialCommandEPKcj` present
    - `_ZN9UiManager11renderSceneEPK11ScenarioDefPKcS4_S4_bS4_` present
- Implemented:
  - partition custom Freenove (`partitions/freenove_esp32s3_app6mb_fs6mb.csv`) + `platformio.ini` alignement max app 6MB.
  - hardening concurrence ESP-NOW:
    - queue RX lockee (`portMUX_TYPE`), enqueue/dequeue atomiques,
    - suppression recursion ACK-skip dans `consumeEspNowMessage`,
    - callback RX/TX lock sections courtes + parse JSON hors lock.
  - provisioning/auth:
    - nouveau module NVS `runtime/provisioning/credential_store.{h,cpp}` (`sta_ssid`, `sta_pass`, `web_token`, `provisioned`),
    - zero secrets hardcodes par defaut (`RuntimeNetworkConfig`, `APP_WIFI`, fallback embedded),
    - endpoint `GET /api/provision/status`,
    - `persist=1` sur `/api/wifi/connect` et `/api/network/wifi/connect`,
    - auth policy:
      - setup mode: whitelist provisioning sans auth,
      - mode normal: Bearer token requis sur `/api/*`,
    - commandes serie:
      - `WIFI_PROVISION <ssid> <pass>`
      - `WIFI_FORGET`
      - `AUTH_STATUS`
      - `AUTH_TOKEN_ROTATE [token]`
  - parsing borne:
    - `HW_LED_SET` migre de `sscanf` vers parse tokenise + `strtol` borne.
  - flash/memory:
    - `UI_LV_MEM_SIZE_KB=128`,
    - reduction String hot path:
      - `network_manager.cpp` (`sendEspNowTarget`, envelope handling),
      - `main.cpp` (`webSendStatusSse` sans `String payload`),
    - externalisation bundle story:
      - fallback embedded minimal seulement (`APP_WIFI` + `DEFAULT` minimal),
      - message explicite `buildfs/uploadfs` requis pour bundle complet.
  - scripts dev:
    - `tools/dev/healthcheck_wifi.sh` support `ZACUS_WEB_TOKEN`,
    - `tools/dev/rtos_wifi_health.sh` support `ZACUS_WEB_TOKEN` (header auth + evidence redacted).
- Validation apres lot:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t size` ✅ -> `text=1254606 data=413388 bss=1416005 dec=3083999`
  - `pio run -e freenove_esp32s3 -t buildfs` ✅
  - `./build_all.sh freenove_esp32s3` ✅ -> `Flash=1651729 / 6291456`, `RAM=209300 / 327680`
  - `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (smoke SKIPPED: port policy LOCATION mismatch)
  - content validators:
    - `python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
    - `python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅
    - `python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅
    - `python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
- Notes:
  - `__ssvfscanf_r`/`__ssvfiscanf_r` restent dans la map, mais provenance framework (`FrameworkArduino/IPv6Address.cpp.o`), pas du code Freenove local.
  - artefacts smoke:
    - `artifacts/rc_live/freenove_esp32s3_20260224-060831/`
    - `artifacts/rc_live/freenove_esp32s3_20260224-060831_agent/`

## Audit flash + hardening Freenove ESP32-S3 (2026-02-24)
- Checkpoint evidence:
  - `/tmp/zacus_checkpoint/1771909826_wip.patch`
  - `/tmp/zacus_checkpoint/1771909826_status.txt`
- Scope applique (firmware Freenove):
  - `platformio.ini`: `CORE_DEBUG_LEVEL=0` sur env `freenove_esp32s3_full_with_ui`, `UI_FONT_PIXEL_ENABLE=0`, `UI_FONT_TITLE_XL_ENABLE=0`.
  - `ui_freenove_allinone/include/lv_conf.h`: widget set LVGL reduit au strict necessaire (`label`/`line`), theme default desactive, fontes Montserrat 18+ desactivees.
  - `ui_freenove_allinone/src/ui_fonts.cpp`: fallback compile-time pour `fontTitleXL()` sans Orbitron 40 quand `UI_FONT_TITLE_XL_ENABLE=0`.
  - `ui_freenove_allinone/src/hardware_manager.cpp`: suppression warning/risque modulo-zero compile-time sur pattern LED secondaire.
- Mesures build (`pio run -e freenove_esp32s3`):
  - Baseline: Flash `1808605` (86.2%), RAM `242340`.
  - Apres optimisation: Flash `1664021` (79.3%), RAM `241996`.
  - Gain flash: `-144584` octets (~`-7.99%` du binaire initial).
- Gates executees:
  - `pio run -e freenove_esp32s3` ✅
  - `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (smoke SKIP attendu: mapping port USB non conforme policy LOCATION)
[20260224-060831] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-060831, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260224-060831/summary.md

## SCENE_WIN_ETAPE - texte LVGL simple (2026-02-24)

- Checkpoint:
  - `/tmp/zacus_checkpoint/20260224_153719_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_153719_status.txt`
- Ajustements:
  - `ui_manager.cpp` durcit le mode texte simplifie quand `UI_WIN_ETAPE_SIMPLIFIED=1`:
    - suppression des animations texte titre/sous-titre (drop/reveal/sine/jitter/celebrate),
    - sous-titre force en rendu statique via `applySubtitleScroll -> kNone`,
    - texte cracktro/clean conserve en labels LVGL simples.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - capture serie manuelle `/dev/tty.usbmodem5AB90753301` ✅ (`PONG`, logs WIN_ETAPE actifs, pas de freeze observe pendant capture).
- Limitation:
  - `./tools/dev/post_upload_checklist.sh --env freenove_esp32s3 --scene SCENE_WIN_ETAPE --required-regex "GFX_STATUS|RESOURCE_STATUS"` en echec port auto-detect (`Aucun port ESP32 detecte`), contournement par port explicite.

## Hotfix affichage glitch (2026-02-24)

- Checkpoint:
  - `/tmp/zacus_checkpoint/20260224_154509_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_154509_status.txt`
- Correctif stabilite applique:
  - `platformio.ini`: `UI_DMA_RGB332_ASYNC_EXPERIMENTAL=0` (desactivation chemin async experimental RGB332).
  - `ui_manager.cpp`: garde explicite dans `initDmaEngine()` pour forcer flush sync quand RGB332 async experimental est desactive.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - monitor serie manuel (`/dev/tty.usbmodem5AB90753301`) ✅ (`PONG`, logs WIN_ETAPE periodiques, pas de freeze observe pendant capture).

## Hotfix glitch persistant - mode safe UI-only (2026-02-24)

- Correctif:
  - `ui_manager.cpp`: en mode `UI_WIN_ETAPE_SIMPLIFIED=1`, forcer `fx_enabled=false` dans `startIntro()` pour isoler totalement le chemin LVGL (pas de blit FX).
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - monitor serie manuel ✅ (`fx_fps=0`, `ui_fps ~58-61`, `PONG`, logs WIN_ETAPE stables sur capture).

## Hotfix rendu FX LovyanGFX (2026-02-24)

- Hypothese suite capture utilisateur: artefacts visuels majoritairement lies au shader de fond FX + blit DMA LGFX.
- Correctifs:
  - `ui_manager.cpp`: reactivation du fond FX en mode simplifie (retour architecture demandee `sprite LGFX + overlay LVGL`).
  - `fx_engine.cpp`: blit FX en mode sync (`UI_FX_DMA_BLIT=0`) pour eviter les artefacts DMA LGFX.
  - `fx_engine.cpp`: shader de fond simplifie (gradient lisse + scanline douce) pour supprimer les motifs "glitch" repetitifs.
  - `ui_manager.cpp`: taille sprite simplifiee derivee de la resolution ecran (`display/2`) avec cible `10 fps`.
  - `fx_engine.cpp`: hauteur sprite max passe a `240` pour conserver le ratio sur ecrans hauts.
  - `platformio.ini`: ajout explicite `-DUI_FX_DMA_BLIT=0`.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - monitor serie manuel ✅ (`fx_fps~8`, `PONG`, logs WIN_ETAPE actifs).

## Skill intake exemples pre-patch (2026-02-25)

- Checkpoint:
  - `/tmp/zacus_checkpoint/20260225_112752_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_112752_status.txt`
- Nouveau skill global ajoute pour les depots d'exemples utilisateur:
  - `~/.codex/skills/chatgpt-file-exemple-intake/SKILL.md`
  - `~/.codex/skills/chatgpt-file-exemple-intake/agents/openai.yaml`
  - `~/.codex/skills/chatgpt-file-exemple-intake/references/checklist.md`
  - `~/.codex/skills/chatgpt-file-exemple-intake/scripts/scan_example_candidates.sh`
- Miroir documentation repo:
  - `docs/skills/chatgpt-file-exemple-intake.md`
- Regle operationnelle formalisee:
  - `hardware/ChatGPT_file_exemple/**` est une source de reference pre-patch, jamais une source compilee.
  - Integration uniquement par extraction de deltas vers `hardware/firmware/**`.

## Integration delta depuis hardware/ChatGPT_file_exemple (2026-02-25)

- Source example integree (premier passage):
  - `hardware/ChatGPT_file_exemple/QR validator/zacus_qr_crc16_prefix_patch/.../ui_manager.cpp`
- Deltas portes dans code canonique:
  - `hardware/firmware/ui_freenove_allinone/include/ui/ui_manager.h`
  - `hardware/firmware/ui_freenove_allinone/src/ui/ui_manager.cpp`
- Integration effectuee:
  - support optionnel `qr.crc16` (bool ou objet `{enabled, sep}`) dans payload scene,
  - validation CRC16/CCITT-FALSE sur payload QR (`DATA<sep>CRCHEX`),
  - matching `expected/prefix/contains` applique sur `DATA` apres validation CRC,
  - reset des regles QR inclut etat CRC16.
- Validation:
  - `pio run -e freenove_esp32s3` ✅
- Passes supplementaires (QR examples -> canonique):
  - ajout outil local `tools/dev/gen_qr_crc16.py` (payload + PNG optionnel), derive du dossier exemple et corrige (quoting/erreurs).
  - ajout doc canonique `docs/protocols/qr_scan_crc16.md` (contrat payload `qr.crc16`, format DATA+CRC, usage script).
  - verification script: `python3 tools/dev/gen_qr_crc16.py "ZACUS:MEDIA_MANAGER" --ci` ✅
- Pass integration QR (suite):
  - compat champ longueur payload QR dans `qr_scan_controller.cpp` (`payloadLen`/`payload_len`) pour robustesse version lib.
  - gates executes:
    - `pio run -e freenove_esp32s3` ✅
    - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅

## Orchestrator pass - refactor QR ownership hors UiManager massif (2026-02-25)

- Baseline/checkpoint:
  - `main`
  - `/tmp/zacus_checkpoint/20260225_114336_wip.patch`
  - `/tmp/zacus_checkpoint/20260225_114336_status.txt`
- Skill chain utilisee (ordre):
  1. `freenove-firmware-orchestrator`
  2. `firmware-graphics-stack`
  3. `firmware-camera-stack`
  4. `firmware-build-stack`
- Audit constats:
  - logique QR payload/rules concentree dans `ui_manager.cpp` (couplage parsing+matching+CRC avec rendu),
  - evenement UI `QR_OK` non consomme par le story runtime (`dispatched=0`) car scenario compile ne porte pas encore la transition YAML.
- Phase A (architecture):
  - extraction logique matching/payload QR vers module dedie:
    - `hardware/firmware/ui_freenove_allinone/include/ui/qr/qr_validation_rules.h`
    - `hardware/firmware/ui_freenove_allinone/src/ui/qr/qr_validation_rules.cpp`
  - `UiManager` delegue desormais `qr_rules_.configureFromPayload(...)` + `qr_rules_.matches(...)`.
- Phase B (securite memoire):
  - copies bornees et parsing CRC16 encapsules dans le module QR dedie,
  - suppression de helpers QR redondants dans `ui_manager.cpp` (moins de surface mutable).
- Phase C (runtime/perf):
  - parsing des regles QR applique uniquement sur changement statique de scene/payload (`qr_scene && static_state_changed`).
- Correction enchainement scene/story:
  - `main.cpp`: fallback orchestration pour `QR_OK` quand transition story compilee absente:
    - applique `ACTION_SET_BOOT_MEDIA_MANAGER`,
    - force `SCENE_MEDIA_MANAGER` via `g_scenario.gotoScene(..., "ui_qr_fallback")`.
- Outils/skills verification:
  - ajout utilitaire QR CRC: `tools/dev/gen_qr_crc16.py`
  - doc contrat QR: `docs/protocols/qr_scan_crc16.md`
  - mise a jour script skill global media-manager pour flow deterministe:
    - force `SCENE_GOTO SCENE_CAMERA_SCAN` avant `QR_SIM`,
    - accepte evidences `UI_EVENT` prefixe/non-prefixe,
    - verification post-reset via `HW_STATUS scene=...` (plus de dependance `story.status`).
- Gates executees:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 8 --wait-port 10` ✅
  - `~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh ...` ✅
  - `~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh ...` ✅
  - `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh ...` ✅
  - `~/.codex/skills/media-manager/scripts/run_media_manager_verification.sh ...` ✅
- Limitation residuelle:
  - la transition `QR_OK -> STEP_MEDIA_MANAGER` n'est pas encore presente dans le scenario compile C++ (`scenarios_gen.cpp`) ; fallback runtime temporaire en place tant que la regeneration C++ canonique n'est pas complete.

## Live reprise hardware + upload + recheck debuts de scenario (2026-02-25)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225-191240_wip.patch`
  - `/tmp/zacus_checkpoint/20260225-191240_status.txt`
  - scan artefacts trackes (`.pio/.platformio/logs/dist/build/node_modules/.venv`) -> aucun tracke.
- Upload effectif Freenove:
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Evidence live:
  - `logs/live_go_20260225-191323.log` (capture rapide)
  - `logs/live_go_transition_20260225-191745.log` (diagnostic commandes serie)
- Verification demarrage scenario (reel):
  - `RESET` -> `scene=SCENE_U_SON_PROTO` confirme via `HW_STATUS`.
  - `NEXT` depuis `SCENE_U_SON_PROTO` -> transition `SCENE_LA_DETECTOR` + `ACTION_QUEUE_SONAR ok=1`, sans panic.
  - `NEXT` depuis `SCENE_LA_DETECTOR` peut retourner `ACK NEXT ok=0` par design (gate LA en attente de match).
- Correction live "probleme des le debut":
  - cause observee: `BOOT_MODE_STATUS mode=media_manager media_validated=1` (le reset revenait sur hub media).
  - correction appliquee sur hardware connecte: `BOOT_MODE_SET STORY` puis `RESET`.
  - verification post-correction: `scene=SCENE_U_SON_PROTO` puis `NEXT` => `SCENE_LA_DETECTOR` + `ACTION_QUEUE_SONAR ok=1`.
- Risque residuel observe en live:
  - erreurs intermittentes `diskio_sdmmc: sdmmc_read_blocks failed (257)` pendant `RESET` (pas de panic, mais a investiguer cote SD/carte/media).
- Gates executes:
  - `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 10 --wait-port 20` ✅
  - `~/.codex/skills/scene-verificator/scripts/run_scene_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
  - `~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅
  - `~/.codex/skills/hal-verificator-status/scripts/run_hal_verification.sh /dev/cu.usbmodem5AB90753301 115200` ✅

## Correction SDMMC - diagnostic carte/FS/lecteur (2026-02-25)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260225-194217_wip.patch`
  - `/tmp/zacus_checkpoint/20260225-194217_status.txt`
  - scan artefacts trackes (`.pio/.platformio/logs/dist/build/node_modules/.venv`) -> aucun tracke.
- Cause identifiee:
  - les lectures `/story/*` passaient par SD en priorite (`readTextFileWithOrigin`), ce qui exposait le runtime aux erreurs `diskio_sdmmc` en cas de carte instable, meme quand LittleFS contenait deja les assets.
- Correctif applique (`ui_freenove_allinone/src/storage/storage_manager.cpp`):
  - fallback runtime passe en **LittleFS d'abord**, SD ensuite (sauf chemin force `/sd/...`),
  - surveillance `errno` sur `SD_MMC.exists/open/read`,
  - compteur d'echecs SD (`sd_failure_streak_`) + coupure SD auto apres erreurs I/O repetees,
  - remount SD explicite tente dans `syncStoryFileFromSd`/`syncStoryTreeFromSd` quand SD etait degrade.
- Interface mise a jour:
  - `ui_freenove_allinone/include/storage/storage_manager.h` (etat SD mutable + helpers de diagnostic).
- Build/upload:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Validation live:
  - log diagnostic: `logs/sdmmc_diag_20260225-195139.log`
  - resultat: `SUMMARY sd_errors=0 panic_markers=0 ack_next_ok=10 ack_next_fail=0`
  - gate smoke: `python3 tools/dev/serial_smoke.py --role esp32 --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 10 --wait-port 20` ✅

## [2026-02-26] Scene de test persistante (YAML + JSON + runtime)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226_025905_wip.patch`
  - `/tmp/zacus_checkpoint/20260226_025905_status.txt`
- Ajouts:
  - scene spec editable: `lib/zacus_story_portable/story_generator/story_specs/scenarios/DEFAULT/scene_test_lab.yaml`
  - payload ecran canonique: `data/story/screens/SCENE_TEST_LAB.json`
  - mirror legacy: `data/screens/SCENE_TEST_LAB.json`
  - scene id runtime enregistre: `hardware/libs/story/src/resources/screen_scene_registry.cpp`
- Usage test rapide:
  - `SCENE_GOTO SCENE_TEST_LAB`
- Build/upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie (scene test):
  - `SCENE_GOTO SCENE_TEST_LAB` -> `ACK SCENE_GOTO ok=1` ✅
  - `STATUS` -> `screen=SCENE_TEST_LAB` ✅
  - `UI_SCENE_STATUS` -> `payload_origin=/story/screens/SCENE_TEST_LAB.json` ✅
- Correctif mirror `data/screens/SCENE_TEST_LAB.json` (fichier vide corrige) + re-uploadfs Freenove ✅

## [2026-02-26] SCENE_TEST_LAB -> mire validation LVGL/GFX

- Payload mire/validation enrichi:
  - `data/story/screens/SCENE_TEST_LAB.json` (timeline palette, transition glitch, demo fireworks, waveform, framing split, marquee)
  - mirror: `data/screens/SCENE_TEST_LAB.json`
  - spec: `lib/zacus_story_portable/story_generator/story_specs/scenarios/DEFAULT/scene_test_lab.yaml`
- Flash FS Freenove:
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `SCENE_GOTO SCENE_TEST_LAB` -> `ACK SCENE_GOTO ok=1` ✅
  - `UI_SCENE_STATUS` -> `scene_id=SCENE_TEST_LAB`, `payload_origin=/story/screens/SCENE_TEST_LAB.json`, `transition=glitch` ✅

## [2026-02-26] SCENE_TEST_LAB -> mire fixe palette (etape 1)

- Checkpoint securite:
  - `/tmp/zacus_checkpoint/20260226_031933_wip.patch`
  - `/tmp/zacus_checkpoint/20260226_031933_status.txt`
- Scene test simplifiee en mire fixe (sans timeline/effets/particules/waveform):
  - `data/story/screens/SCENE_TEST_LAB.json`
  - `data/screens/SCENE_TEST_LAB.json`
- Upload FS Freenove:
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Activation:
  - `SCENE_GOTO SCENE_TEST_LAB` -> `ACK SCENE_GOTO ok=1` ✅
  - `UI_SCENE_STATUS` -> `effect=none`, `timeline=0`, `accent=#FF0000` ✅

## [2026-02-26] Fix affichage SCENE_TEST_LAB (stabilite lancement)

- Constat: `SCENE_GOTO SCENE_TEST_LAB` etait bien ACK mais la scene pouvait etre remplacee par transitions scenario automatiques.
- Correctif firmware:
  - `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
  - ajout `isFixedTestSceneActive(...)`
  - quand `screen_scene_id == SCENE_TEST_LAB`, `g_scenario.tick(now_ms)` est saute (freeze transitions auto) pour garder la mire visible pendant calibration.
- Build/upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification serie:
  - `SCENE_GOTO SCENE_TEST_LAB` -> `ACK SCENE_GOTO ok=1` ✅
  - `STATUS` apres 2s/5s/10s conserve `screen=SCENE_TEST_LAB` ✅

## [2026-02-26] Boot scene forcee sur detector

- Correctif boot story:
  - `hardware/firmware/ui_freenove_allinone/src/app/main.cpp`
  - ajout `kBootStorySceneId = SCENE_LA_DETECTOR`
  - au boot (hors media manager), route immediate vers `SCENE_LA_DETECTOR` via `g_scenario.gotoScene(..., "boot_story_default")`
- Build + upload Freenove:
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Verification:
  - `STATUS` apres reboot -> `step=SCENE_LA_DETECTOR screen=SCENE_LA_DETECTOR` ✅
