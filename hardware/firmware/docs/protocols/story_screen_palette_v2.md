# Story Screen Palette V3

Source de verite edition UI:
- `data/story/palette/screens_palette_v3.yaml`

Artefacts generes et commits:
- `data/story/screens/SCENE_*.json` (canoniques runtime)
- `legacy_payloads/fs_excluded/screens/*.json` (archive legacy hors LittleFS)

Commande de synchro:
- `./tools/dev/story-gen sync-screens` (ecrit les artefacts)
- `./tools/dev/story-gen sync-screens --check` (gate drift, sans ecriture)

## Contrat runtime

- Le runtime Freenove charge d'abord `/story/screens/<SCENE>.json`.
- Le fallback `/screens/<slug|SCENE>.json` reste supporte pour compat legacy.
- Les mirrors legacy sont maintenus hors `data/` pour ne pas etre embarques dans LittleFS par defaut.

## Scenes canoniques (registre runtime)

Source: `hardware/libs/story/src/resources/screen_scene_registry.cpp` (`kScenes`).

- `SCENE_LOCKED`
- `SCENE_BROKEN`
- `SCENE_U_SON_PROTO`
- `SCENE_SEARCH`
- `SCENE_LA_DETECTOR`
- `SCENE_LEFOU_DETECTOR`
- `SCENE_WARNING`
- `SCENE_CAMERA_SCAN`
- `SCENE_QR_DETECTOR`
- `SCENE_SIGNAL_SPIKE`
- `SCENE_REWARD`
- `SCENE_WIN_ETAPE1`
- `SCENE_WIN_ETAPE2`
- `SCENE_FINAL_WIN`
- `SCENE_MEDIA_ARCHIVE`
- `SCENE_READY`
- `SCENE_WIN`
- `SCENE_WINNER`
- `SCENE_FIREWORKS`
- `SCENE_WIN_ETAPE`
- `SCENE_MP3_PLAYER`
- `SCENE_MEDIA_MANAGER`
- `SCENE_PHOTO_MANAGER`

Aliases legacy (normalises):
- `SCENE_LA_DETECT -> SCENE_LA_DETECTOR`
- `SCENE_U_SON -> SCENE_U_SON_PROTO`
- `SCENE_LE_FOU_DETECTOR -> SCENE_LEFOU_DETECTOR`
- `SCENE_LOCK|LOCK|LOCKED -> SCENE_LOCKED`
- `SCENE_AUDIO_PLAYER|SCENE_MP3 -> SCENE_MP3_PLAYER`

## Structure ecran generee

Champs attendus dans chaque `SCENE_*.json`:
- `id`, `title`, `subtitle`, `symbol`
- `effect`, `effect_speed_ms`
- `theme.bg|accent|text` (hex RGB)
- `transition.effect|duration_ms`
- `text.show_*`, `text.*_case`, `text.*_align`
- `framing`, `scroll`, `demo`
- `timeline` (`loop`, `duration_ms`, `keyframes[]`)

## Tokens supportes

`effect`:
- `none`, `pulse`, `scan`, `radar`, `wave`, `blink`, `glitch`, `celebrate`
- aliases normalises: `steady->none`, `camera_flash->glitch`, `reward->celebrate`

`transition.effect`:
- `none`, `fade`, `slide_left`, `slide_right`, `slide_up`, `slide_down`, `zoom`, `glitch`
- aliases normalises:
  - `crossfade->fade`
  - `left/right/up/down -> slide_*`
  - `zoom_in->zoom`
  - `flash|camera_flash->glitch`
  - `wipe->slide_left`

## Validation recommandee

- `./tools/dev/story-gen validate`
- `./tools/dev/story-gen sync-screens --check`
- `./tools/dev/story-gen generate-bundle --out-dir /tmp/zacus_bundle_<tag>`

Si `sync-screens --check` echoue, regenerer puis commit les JSON ecrans.
