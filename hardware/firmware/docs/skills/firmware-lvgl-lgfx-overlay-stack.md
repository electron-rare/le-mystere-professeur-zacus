---
name: firmware-lvgl-lgfx-overlay-stack
version: 1.1
description: Hybrid render stack ESP32-S3 + ST7796 320x480 SPI: LovyanGFX low-res FX background (sprite rotate/zoom/affine) + LVGL chrome overlay, with strict SPI mutex, PSRAM/DRAM budgeting, RGB332 LVGL mode, and RGB332->RGB565 flush conversion if needed.
---

# Firmware LVGL + LovyanGFX Overlay Stack (ESP32-S3 / ST7796 320×480 SPI) — v1.1

## Mission
Construire un stack hybride "demoscene perf" :
- **Fond FX** rendu par **LovyanGFX** via **sprite low-res** (ex: 160×120) puis **pushRotateZoom / pushAffine** plein écran.
- **Chrome UI** (fenêtres, boutons, scrollers, playlist…) gérée par **LVGL** au-dessus.
- **Un seul maître SPI** (mutex obligatoire), **0 alloc par frame**, **budgets perf** stricts.

## Faits vérifiés (à respecter)
### LVGL 8bpp = RGB332 direct (pas palette indexée)
- `LV_COLOR_DEPTH=8` => LVGL travaille en **RGB332 (8bpp)**.
- Pour des **assets binaires** LVGL en 8bpp, le format attendu est **RGB332** (sinon mismatch).

### DMA et PSRAM (ESP32-S3)
- En ESP-IDF, `MALLOC_CAP_DMA` exclut la PSRAM : les buffers DMA doivent être en **RAM interne DMA**.
- ESP32-S3 peut DMA vers/depuis external RAM mais avec limitations (descripteurs DMA pas en PSRAM, bande passante limitée).
=> Politique : PSRAM pour gros buffers, DRAM interne DMA pour TX/RX DMA.

### DMA SPI (si tu utilises l’API SPI IDF sous le capot)
- Buffers DMA: RAM interne DMA + alignement 32-bit + taille multiple de 4.

### LovyanGFX / M5GFX Sprite
- `pushRotateZoom` a des overloads vers un `LovyanGFX* dst`, et versions avec `dst_x/dst_y`, `angle`, `zoom_x/zoom_y`.
- En **mode palette** (bpp 1/2/4/8 + palette), les fonctions de couleur manipulent des indices, pas des couleurs directes.
=> Reco: pour FX, **sprite 16bpp** est souvent le plus simple. 8bpp palette possible si maîtrisé.

---

# Architecture obligatoire

## 1) Single SPI Master + Mutex global
**Règle:** toute écriture écran passe par une HAL unique + `gfx_lock()/gfx_unlock()`.

Créer/maintenir: `gfx_hal_lgfx.{h,cpp}`
- possède `LGFX lcd;`
- `gfx_lock() / gfx_unlock()`
- API:
  - `gfx_init()`
  - `gfx_push_rgb565(x,y,w,h,pixels)`  // utilisé par LVGL flush_cb
  - `gfx_present_fx_rotzoom(sprite, angle, zx, zy)` // FX plein écran
  - (option) `gfx_present_fx_affine(sprite, matrix6)`

## 2) Ordre de rendu (layering)
**À chaque frame:**
1) FX présenté (LovyanGFX) => plein écran
2) Invalidation overlay LVGL (au minimum `overlay_root_` ou rectangles fenêtres)
3) `lv_timer_handler()` => LVGL reflushe les zones UI au-dessus

⚠️ Sans invalidation overlay après frame FX, le fond peut "manger" la UI.

## 3) Schedulers déterministes (sans blocage)
- FX: 15–20 fps (50–66 ms)
- LVGL: 24–30 fps (33–42 ms)
- Clamp dt pour éviter les “sauts”.

---

# Politique mémoire (pratique & stable)

## PSRAM (gros buffers)
- LVGL draw buffers (buf1/buf2) si configuré
- FX sprite buffers (1 ou 2 sprites low-res)
- Audio ringbuffer PCM
- Buffers caméra / JPEG / framebuffers

## DRAM interne (DMA/stacks/objets)
- Buffers TX SPI DMA (RGB565) réutilisables
- Buffers DMA I2S (driver)
- Stacks FreeRTOS
- Heap LVGL + objets LVGL + queues

### Note perf
PSRAM est plus lente: ok pour gros buffers, éviter d’y faire des copies massives *dans* le hot path DMA.

## Couleur / pipeline

### Mode 256 couleurs (LVGL)
- LVGL en 8bpp RGB332 (style “256 colors”).
- Si le driver écran attend RGB565:
  - LUT 256 entrées `rgb332_to_565[256]`
  - conversion ligne par ligne, sans alloc.

### FX sprite (LovyanGFX)
- Reco: sprite 16bpp (simplicité + pas d’index palette).
- Option 8bpp palette:
  - `setColorDepth(8)` puis `createPalette()`
  - `setPaletteColor(i, r,g,b)`
  - attention: les “colors” deviennent des indices palette.

---

# Budgets perf (hard guardrails)
- FX sprite: 160×120 default (options 120×80 / 200×150)
- FX fps: 15–20
- LVGL obj cap (320×480): ≤ 260
- Particules: ≤ 64 actives (fireworks)
- Stars: cap ≤ 220
- 0 alloc par frame (assert/log)

---

# Build flags recommandés (cohérents)
- `UI_FX_BACKEND_LGFX=1`
- `UI_FX_SPR_W=160`, `UI_FX_SPR_H=120`
- `UI_FX_FPS=20`
- `UI_COLOR_256=1` (LV_COLOR_DEPTH=8)
- `UI_DRAW_BUF_IN_PSRAM=1` (master switch)
- `UI_DMA_TX_IN_DRAM=1`
- `UI_DEMO_AUTORUN_WIN_ETAPE=1` (debug)

Compat:
- si `FREENOVE_PSRAM_UI_DRAW_BUFFER` existe, mappe-le vers `UI_DRAW_BUF_IN_PSRAM` ou supprime-le.

---

# Artifacts obligatoires
Docs:
- `docs/ui/fx_lgfx_overlay.md` (mutex + draw order + invalidation + budgets)
- `docs/ui/graphics_stack.md` (buffers, PSRAM/DRAM, conversion RGB332->RGB565)
- `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md` (constantes + budgets)

Instrumentation:
- boot logs: color depth, sprite size, PSRAM free, DMA heap free
- runtime (toutes 5s): phase, fx_fps, obj_count, stars/particles, mem

# Acceptance checklist
- FX fond visible et stable (>=15 fps)
- LVGL chrome reste au-dessus (pas de disparition UI)
- Pas de collisions SPI (mutex OK)
- Pas d’alloc par frame
- Stable 3–5 minutes (pas de drift / leak)
- Cleanup OK en sortie scène (timers stoppés, pools reset)

# Références (à laisser en doc projet)
- LVGL images / formats (RGB332 en 8-bit) : https://docs.lvgl.io/8.3/overview/image.html
- ESP-IDF DMA memory excludes PSRAM : https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/mem_alloc.html
- ESP-IDF external RAM DMA limitations (ESP32-S3) : https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html
- SPI DMA buffer requirements (alignment/internal) : https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_master.html
- pushRotateZoom overloads (M5GFX/LGFX canvas) : https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_canvas
