## Mapping hardware ESP32-S3 Freenove Media Kit

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

- audio_manager.{h,cpp} : gestion audio
- scenario_manager.{h,cpp} : gestion scénario
- ui_manager.{h,cpp} : gestion UI dynamique (LVGL)
- button_manager.{h,cpp} : gestion boutons
- touch_manager.{h,cpp} : gestion tactile
- storage_manager.{h,cpp} : gestion LittleFS, fallback

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

## Mapping hardware
- Voir `include/ui_freenove_config.h` pour l’adaptation des pins
- Schéma de branchement : se référer à la doc Freenove

## Notes
- Ce firmware est expérimental et fusionne les logiques audio + UI.
- Pour la compatibilité UI Link, prévoir un mode optionnel.

## Intro Amiga92 (`SCENE_WIN_ETAPE`)

- Activation: l'intro A/B/C est lancee automatiquement quand `screen_scene_id == SCENE_WIN_ETAPE`.
- Sequence:
  - A (30000 ms): copper circular/wavy + starfield 3 couches + logo overshoot + scroller milieu rapide + rollback bas.
  - B (15000 ms): `B1` crash court (700..1000 ms) + `B2` interlude (roto/tunnel + copper overlay + pulses fireworks).
  - C (20000 ms): fond sobre + reveal `BRAVO Brigade Z` + scroller milieu ping-pong/wavy.
  - ensuite: boucle de la phase C.
- Skip: tout appui bouton ou touch pendant l'intro declenche un OUTRO court (400 ms).

### Overrides runtime (TXT + JSON)

- Priorite de lecture:
  1) `/ui/scene_win_etape.txt`
  2) `/ui/scene_win_etape.json`
  3) `/SCENE_WIN_ETAPE.json`
  4) `/ui/SCENE_WIN_ETAPE.json`
- Cles supportees (TXT/JSON aliases):
  - `logo_text`
  - `mid_a_scroll`
  - `bot_a_scroll`
  - `clean_title`
  - `clean_scroll`
  - `a_ms`, `b_ms`, `c_ms`, `b1_ms`
  - `speed_mid_a`, `speed_bot_a`, `speed_c`
  - `stars`, `fx_3d`, `fx_3d_quality`

Exemples:
- JSON: `data/SCENE_WIN_ETAPE.json`
- TXT: `data/ui/scene_win_etape.txt`

### Perf notes

- Tick fixe: `33 ms` (`~30 FPS` cible), `dt` clamp pour robustesse.
- Zero allocation par frame dans la boucle intro (`tickIntro`).
- Caps objets scene: `<=140` (petit ecran), `<=260` (grand ecran).
- Pools fixes:
  - stars: `48`
  - fireworks: `72`
  - wave glyph+shadow: `64 + 64`

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
