# LVGL Font Pack (ESP32-S3 / Freenove)

## Scope
This firmware uses pre-generated LVGL C fonts (no TTF runtime loading) for the UI stack.

Families and sizes:
- Inter: 14 / 18 / 24 / 32
- Orbitron: 28 / 40
- IBM Plex Mono: 14 / 18
- Press Start 2P: 16 / 24 (optional via `UI_FONT_PIXEL_ENABLE`)

Generated files live in:
- `hardware/firmware/ui_freenove_allinone/src/ui/fonts/`

## Glyph subset and quality
- Font bpp: `4`
- Unicode ranges:
  - `0x20-0x7E` (ASCII)
  - `0xA0-0x00FF` (Latin-1)
  - `0x0100-0x017F` (Latin Extended-A)
  - `0x2010-0x2030` (typographic punctuation)
- Forced symbols:
  - `’”“«»…•–—°œŒ`
  - FR accents used by scenes (`àâäçéèêëîïôöùûüÿ` and uppercase variants)

Source of truth:
- `tools/fonts/scripts/font_manifest.json`

## UI registry and reusable styles
Registry:
- `hardware/firmware/ui_freenove_allinone/include/ui_fonts.h`
- `hardware/firmware/ui_freenove_allinone/src/ui_fonts.cpp`

Font getters:
- `fontBody()`
- `fontBodyBoldOrTitle()`
- `fontTitle()`
- `fontMono()`
- `fontPixel()`
- plus size-specific helpers (`fontBodyS/M/L`, `fontTitleXL`)

Styles:
- `styleBody()`
- `styleTitle()`
- `styleTitleXL()`
- `styleMono()`
- `stylePixel()`

Optional 1px demoscene shadow:
- enabled by default with `UI_FONT_STYLE_SHADOW=1`
- can be disabled with `-DUI_FONT_STYLE_SHADOW=0`

## Regeneration
1. Put/update TTF files in `tools/fonts/ttf/`.
2. Run:

```bash
tools/fonts/scripts/generate_lvgl_fonts.sh
```

3. Rebuild firmware:

```bash
pio run -e freenove_esp32s3
```

## Flash/RAM impact notes
- Fonts are compiled into flash (`.rodata`), not in `LV_MEM_SIZE`.
- Total generated C source size is about `1.5 MB` (`lv_font_*.c` sum), with final firmware flash usage validated by PlatformIO size output.
- Runtime RAM impact is mainly LVGL object/style usage, not the static font blobs.
