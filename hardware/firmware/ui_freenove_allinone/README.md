
---
# 🎛️ Zacus Firmware – Freenove All-in-One

![Funk](https://media.giphy.com/media/3o7TKtnuHOHHUjR38Y/giphy.gif)

---

## 📝 Description

Bienvenue dans le cockpit le plus funky du multivers ! Ce firmware fusionne UI, audio, scénarios et hardware sur la carte Freenove Media Kit (ESP32). Ici, chaque pixel danse, chaque bouton groove, et chaque boot est une fête.

> *"Si tu entends un son rétro ou vois un plasma violet, c’est normal. Si le microcontrôleur se met à rapper, c’est probablement un bug… ou un feature caché."*

---

## 🕹️ Mapping hardware (ESP32-S3 Freenove Media Kit)

| Fonction         | Broche ESP32-S3 | Signal TFT/Touch/Audio | Remarques                      |
|------------------|-----------------|-----------------------|--------------------------------|
| TFT SCK          | GPIO 47         | SCK                   | SPI écran (FNK0102B)           |
| TFT MOSI         | GPIO 21         | MOSI                  | SPI écran                      |
| TFT MISO         | -1              | MISO                  | non utilisé                    |
| TFT CS           | -1              | CS                    | câblage board intégré          |
| TFT DC           | GPIO 45         | DC                    | Data/Command écran             |
| TFT RESET        | GPIO 20         | RESET                 | Reset écran                    |
| TFT BL           | GPIO 2          | BL                    | Rétroéclairage                 |
| Touch CS         | GPIO 9          | CS (XPT2046)          | optionnel (`FREENOVE_HAS_TOUCH`) |
| Touch IRQ        | GPIO 15         | IRQ (XPT2046)         | optionnel                      |
| Boutons          | GPIO 19         | ADC ladder (5 touches)| key1..key5 par seuils analogiques |
| Audio I2S WS     | GPIO 41         | WS                    | profil principal Sketch_19     |
| Audio I2S BCK    | GPIO 42         | BCK                   | profil principal Sketch_19     |
| Audio I2S DOUT   | GPIO 1          | DOUT                  | profil principal Sketch_19     |
| Alim écran/audio | 3V3/5V/GND      | -                     | Respecter les tensions         |

**Remarques** :
- Profil audio runtime sélectionnable par série: `AUDIO_PROFILE <idx>` puis `AUDIO_TEST`.
- Profils fournis: `0=sketch19`, `1=swap_bck_ws`, `2=dout2_alt`.
- Le tactile est désactivé par défaut (`FREENOVE_HAS_TOUCH=0`).

---

## 📦 Contenu du dossier

- Sources : `src/` (app, ui, audio, storage, camera, drivers, system)
- Polices : `src/ui/fonts/`
- Layouts d’écran : `src/ui/screens/`
- Partition custom : `partitions/freenove_esp32s3_app6mb_fs6mb.csv`
- Mapping hardware détaillé (voir pour descendre plus haut )

---

## 🚀 Installation & usage

1. Clone ce repo, chausse tes lunettes de soleil.
2. Va dans `hardware/firmware` et build comme un DJ :
  ```sh
  pio run -e freenove_allinone
  pio run -e freenove_allinone -t upload --upload-port <PORT>
  ```
3. Pour la version ESP32-S3 (partition 6MB app / 6MB FS) :
  ```sh
  pio run -e freenove_esp32s3
  pio run -e freenove_esp32s3 -t buildfs
  pio run -e freenove_esp32s3 -t uploadfs --upload-port <PORT>
  pio run -e freenove_esp32s3 -t upload --upload-port <PORT>
  ```
4. Branche, allume, et laisse la magie opérer.

---

## 🧩 Fonctionnalités qui groovent

- Navigation UI dynamique (LVGL, écrans générés depuis fichiers)
- Exécution de scénarios (lecture, transitions, actions, audio)
- Gestion audio (lecture/stop, mapping fichiers LittleFS)
- Gestion boutons et tactile (événements, mapping, callbacks)
- Fallback robuste si fichier manquant (scénario par défaut)
- Génération de logs et artefacts (logs/, artifacts/)
- Validation hardware sur Freenove (affichage, audio, boutons, tactile)
- Documentation et onboarding synchronisés
- Mode autonome (pas besoin d’ESP32 séparé)

> *"Si tu rates un bouton, c’est que tu danses trop fort. Si tu vois un plasma violet, c’est que tu es dans le groove."*

---

## 🛠️ Modules principaux

- `audio_manager` : fait swinguer l’audio (lecture, stop, état)
- `scenario_manager` : enchaîne les étapes comme un DJ
- `ui_manager` : LVGL, écrans dynamiques, FX visuels
- `storage_manager` : LittleFS (init, vérif, groove des assets)
- `button_manager` : boutons physiques, pour les vrais
- `touch_manager` : tactile XPT2046, pour les DJ du futur

---

## 🦄 Funky extras & notes

- Firmware expérimental : fusion audio + UI, mode "party hard" activé
- Pour la compatibilité UI Link, prévoir un mode optionnel
- Si tu veux une intro Amiga92, active la scène `SCENE_WIN_ETAPE` et laisse-toi porter par le FX timeline (plasma, starfield, boingball…)
- Pour tester la scène MP3 : `SCENE_MP3_PLAYER` (overlay AmigaAMP, scan `/music`)
- Pour la caméra : `SCENE_CAMERA_SCAN` (Win311 overlay, boutons = SNAP/SAVE/GALLERY/DELETE/CLOSE)
- Pour tout le reste, consulte les logs, et si tu comprends tout du premier coup, tu gagnes un badge "Zacus Funk Master".

---

## 🤝 Contribuer

Merci de lire [../../../../../../CONTRIBUTING.md](../../../../../../CONTRIBUTING.md) avant toute PR. Les pull requests avec punchlines sont encouragées.

---

## 👤 Contact

Pour toute question, suggestion, ou battle de funk, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)

> *"Ce firmware a été validé par un oscilloscope, un grille-pain, et un synthé vintage. Si tu trouves un bug, c’est peut-être un easter egg ou feat foirée."*
---

# Firmware Freenove Media Kit All-in-One

## Plan d’intégration complète (couverture specs)

- Fichiers de scènes et écrans individuels, stockés sur LittleFS (data/)
- Navigation UI dynamique (LVGL, écrans générés depuis fichiers)
- Exécution de scénarios (lecture, transitions, actions, audio)
- Gestion audio (lecture/stop, mapping fichiers LittleFS)
- Gestion boutons et tactile (événements, mapping, callbacks)
- Fallback robuste si fichier manquant (scénario par défaut)
- Génération de logs et artefacts (logs/, artifacts/)
- Validation hardware sur Freenove (affichage, audio, boutons, tactile)
- Documentation et onboarding synchronisés

## Structure modulaire

- `src/app/*` : orchestration runtime (`main`, `scenario_manager`, coordinator serie)
- `src/ui/*` : LVGL scenes, fonts, FX helpers (`ui_manager`, `ui_fonts`, `ui/fx`)
- `src/audio/*` : gestion audio runtime (`audio_manager`)
- `src/storage/*` : LittleFS/SD et resolution assets (`storage_manager`)
- `src/camera/*` : camera runtime (`camera_manager`)
- `src/drivers/*` : board I/O (input, board, display HAL + SPI bus manager)
- `src/system/*` : metrics, boot report, reseau/media wrappers, task topology
- headers legacy (`include/*_manager.h`) conserves en facades vers les headers modulaires

Ce firmware combine :
- Les fonctions audio/scénario (type ESP32 Audio)
- L’UI locale (affichage, interaction, tactile, boutons)
- Le tout sur un seul microcontrôleur (RP2040 ou ESP32 selon le kit)


## Fonctionnalités
- Lecture audio, gestion scénario, LittleFS
- Affichage TFT tactile (LVGL)
- Boutons physiques, capteurs, extensions
- Mode autonome (pas besoin d’ESP32 séparé)

## Modules principaux
- audio_manager : gestion audio (lecture, stop, état)
- scenario_manager : gestion scénario (étapes, transitions)
- ui_manager : gestion UI (LVGL, écrans dynamiques)
- storage_manager : gestion LittleFS (init, vérification)
- button_manager : gestion boutons physiques
- touch_manager : gestion tactile XPT2046

## Validation hardware
- Compiler et flasher sur le Freenove Media Kit
- Vérifier l’affichage, la réactivité tactile et boutons
- Tester la lecture audio (fichiers dans /data/)
- Consulter les logs série pour le suivi d’exécution

## Artefacts
- Firmware compilé (.bin)
- Logs de test hardware (logs/)
- Rapport de compatibilité assets LittleFS

## Build

Depuis `hardware/firmware` :

```sh
pio run -e freenove_allinone
pio run -e freenove_allinone -t upload --upload-port <PORT>
```

## Build Freenove ESP32-S3 (partition 6MB app / 6MB FS)

```sh
pio run -e freenove_esp32s3
pio run -e freenove_esp32s3 -t buildfs
pio run -e freenove_esp32s3 -t uploadfs --upload-port <PORT>
pio run -e freenove_esp32s3 -t upload --upload-port <PORT>
```

- Partition custom active: `partitions/freenove_esp32s3_app6mb_fs6mb.csv`.
- Le bundle story complet n'est plus embarque dans le firmware: `buildfs/uploadfs` est requis pour charger le contenu complet (screens/audio/actions/apps).
- Fallback embarque minimal conserve: `APP_WIFI` + scenario `DEFAULT` minimal.

## Norme embarquee (audit + refacto)

- Standard projet: `docs/skills/EMBEDDED_CPP_OO_ESP32S3_PIO_ARDUINO.md`.
- Toute revue technique Freenove doit citer cette norme pour les risques temps reel, memoire, concurrence et style OO.

## Provisioning Wi-Fi + Auth WebUI

- Au boot sans credentials NVS:
  - mode setup AP actif;
  - endpoints setup ouverts: `GET /api/provision/status`, `POST /api/wifi/connect`, `POST /api/network/wifi/connect`.
- Hors setup:
  - auth Bearer requise sur `/api/*`:
    - `Authorization: Bearer <token>`.
- Provisioning persistant:
  - API: `POST /api/wifi/connect` (ou `/api/network/wifi/connect`) avec `persist=1`.
  - Serie: `WIFI_PROVISION <ssid> <pass>`.
- Rotation token:
  - Serie: `AUTH_TOKEN_ROTATE [token]`.
- Outils shell:
  - `tools/dev/healthcheck_wifi.sh` et `tools/dev/rtos_wifi_health.sh` supportent `ZACUS_WEB_TOKEN`.

## LVGL graphics stack (ESP32-S3)

- Le runtime Freenove (`env:freenove_esp32s3*`) supporte:
  - `LV_COLOR_DEPTH=8` (RGB332) avec conversion RGB565 au flush.
  - draw buffers lignes en double-buffer.
  - flush DMA asynchrone (overlap draw/transfert) avec fallback sync.
- Flags principaux (`platformio.ini`):
  - `UI_COLOR_256`, `UI_COLOR_565`, `UI_FORCE_THEME_256`
  - `UI_DRAW_BUF_LINES`, `UI_DRAW_BUF_IN_PSRAM`
  - `UI_DMA_FLUSH_ASYNC`, `UI_DMA_TRANS_BUF_LINES`
  - `UI_FULL_FRAME_BENCH`, `UI_LV_MEM_SIZE_KB`
- Commandes debug serie:
  - `UI_GFX_STATUS`
  - `UI_MEM_STATUS`
- Documentation associee:
  - `docs/ui/graphics_stack.md`
  - `docs/ui/lvgl_memory_budget.md`
  - `docs/ui/fonts_fr.md`

## Mapping hardware
- Voir `include/ui_freenove_config.h` pour l’adaptation des pins
- Schéma de branchement : se référer à la doc Freenove

## Notes
- Ce firmware est expérimental et fusionne les logiques audio + UI.
- Pour la compatibilité UI Link, prévoir un mode optionnel.

## Intro Amiga92 (`SCENE_WIN_ETAPE`)

- Activation: l'intro A/B/C est lancee automatiquement quand `screen_scene_id == SCENE_WIN_ETAPE`.
- Mode runtime: `FX_ONLY_V9` avec rendu timeline FX (plasma/starfield/rasterbars + tunnel3d/rotozoom/wirecube + boingball) et overlay LVGL conserve.
- Sequence timeline verrouillee: `A(30000ms) -> B(15000ms) -> C(20000ms) -> C loop`.
- Mapping presets (default):
  - A: `demo`
  - B: `winner`
  - C: `boingball`
- Font scroller default: `italic`.
- BPM default: `125`.
- Boing shadow path: assembleur S3 active par defaut (`UI_BOING_SHADOW_ASM=1`) avec fallback C automatique.
- Log de boot FX: `boing_shadow_path=asm|c`.

### Overrides runtime (TXT + JSON)

- Priorite de lecture:
  1) `/ui/scene_win_etape.json`
  2) `/SCENE_WIN_ETAPE.json`
  3) `/ui/SCENE_WIN_ETAPE.json`
  4) `/ui/scene_win_etape.txt`
- Cles supportees:
  - `A_MS`, `B_MS`, `C_MS`
  - `FX_PRESET_A`, `FX_PRESET_B`, `FX_PRESET_C` (`demo|winner|fireworks|boingball`)
  - `FX_MODE_A`, `FX_MODE_B`, `FX_MODE_C` (`classic|starfield3d|dotsphere3d|voxel|raycorridor`)
  - `FX_SCROLL_TEXT_A`, `FX_SCROLL_TEXT_B`, `FX_SCROLL_TEXT_C`
  - `FX_SCROLL_FONT` (`basic|bold|outline|italic`)
  - `FX_BPM` (`60..220`)
- Rupture de compatibilite volontaire:
  - anciennes cles FX `FX_3D`, `FX_3D_QUALITY`, `FONT_MODE` ne sont plus prises en charge dans ce flux.

Exemples:
- JSON: `data/SCENE_WIN_ETAPE.json`
- TXT: `data/ui/scene_win_etape.txt`

### Perf notes

- Tick fixe: `42 ms` (`~24 FPS` cible), `dt` clamp pour robustesse.
- Zero allocation par frame dans la boucle intro (`tickIntro`).
- Blit LGFX: fast-path 2x active (`UI_FX_BLIT_FAST_2X=1`) si ratio source/ecran exact, fallback scaler general sinon.
- Caps objets scene: `<=140` (petit ecran), `<=260` (grand ecran).
- Pools fixes:
  - stars: `48`
  - fireworks: `72`
  - wave glyph+shadow: `64 + 64`
- `UI_GFX_STATUS` expose `fx_fps/fx_frames/fx_skip_busy` + compteurs `flush_block/overflow` pour diagnostiquer les saccades LVGL/FX.

## Scenes demoscene exposees

- IDs canoniques ajoutes: `SCENE_WINNER`, `SCENE_FIREWORKS`.
- Fichiers data:
  - `data/story/screens/SCENE_WINNER.json`
  - `data/story/screens/SCENE_FIREWORKS.json`
- Registry story: `hardware/libs/story/src/resources/screen_scene_registry.cpp`.

## Scene lecteur MP3 (`SCENE_MP3_PLAYER`)

- Scene canonique: `SCENE_MP3_PLAYER`.
- Aliases supportes: `SCENE_AUDIO_PLAYER`, `SCENE_MP3`.
- Data scene: `data/story/screens/SCENE_MP3_PLAYER.json`.
- Mode runtime: overlay LVGL "AmigaAMP" + backend `AudioPlayerService` (scan `/music`, fallback `/audio/music`, fallback `/audio`).
- Arbitrage audio:
  - en scene MP3, l'audio scenario est suspendu;
  - en sortie de scene MP3, pipeline audio scenario restaure.
- Commandes serie:
  - `AMP_SHOW`, `AMP_HIDE`, `AMP_TOGGLE`
  - `AMP_SCAN`, `AMP_PLAY <idx|path>`, `AMP_NEXT`, `AMP_PREV`, `AMP_STOP`, `AMP_STATUS`

## Scene camera recorder (`SCENE_CAMERA_SCAN`)

- Binding scene: entree `SCENE_CAMERA_SCAN` -> session recorder camera active (`RGB565/QVGA`) + overlay Win311 visible.
- Sortie scene: overlay masque, frame gelee purgee, retour mode snapshot legacy (`JPEG`).
- Mapping boutons physiques:
  - `BTN1`: `SNAP/LIVE`
  - `BTN2`: `SAVE`
  - `BTN3`: `GALLERY` (appui long: `NEXT`)
  - `BTN4`: `DELETE`
  - `BTN5`: `CLOSE` overlay
- Commandes serie:
  - `CAM_UI_SHOW`, `CAM_UI_HIDE`, `CAM_UI_TOGGLE`
  - `CAM_REC_SNAP`, `CAM_REC_SAVE [auto|bmp|jpg|raw]`
  - `CAM_REC_GALLERY`, `CAM_REC_NEXT`, `CAM_REC_DELETE`, `CAM_REC_STATUS`
- Ownership runtime:
  - pendant `SCENE_CAMERA_SCAN`, les boutons physiques ne sont pas forwardes au scenario;
  - commandes legacy `CAM_ON/CAM_OFF/CAM_SNAPSHOT` renvoient `camera_busy_recorder_owner`.

### Visual verification mode

- Build flags (env `freenove_esp32s3`) pour tests scene:
  - `UI_FULL_FRAME_BENCH=1`
  - `UI_DEMO_AUTORUN_WIN_ETAPE=1`
- Quand ces flags sont actifs:
  - la scene `SCENE_WIN_ETAPE` demarre automatiquement au boot;
  - la phase C continue en boucle tant que la scene reste active;
  - `UI_GFX_STATUS` permet de verifier le mode runtime (depth/full_frame/source).

### References consulted

- https://www.pouet.net/prodlist.php?type%5B0%5D=cracktro
- https://www.youtube.com/results?search_query=amiga+cracktro
- https://www.youtube.com/results?search_query=amiga+demoscene+1992
- https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_2.html
- https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/
- https://www.markwrobel.dk/project/amigamachinecode/
- https://www.markwrobel.dk/post/amiga-machine-code-letter3-copper-revisited/
- https://www.markwrobel.dk/post/amiga-machine-code-letter12-starfield-effect/
- https://www.markwrobel.dk/post/amiga-machine-code-letter12-wave/
- https://github.com/mrandreastoth/AmigaStyleDemo
