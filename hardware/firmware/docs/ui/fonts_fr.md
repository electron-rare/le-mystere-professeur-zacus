# UI Fonts (FR support)

## Registry
`ui_fonts.h/.cpp` exposes a single registry used by UI scenes:

- `fontBody()` / `fontBodyBoldOrTitle()` (compat aliases)
- `fontBodyS()`
- `fontBodyM()`
- `fontBodyL()`
- `fontTitle()`
- `fontTitleXL()`
- `fontMono()`
- `fontPixel()`

Styles are also exposed (`styleBody`, `styleTitle`, `styleTitleXL`, `styleMono`, `stylePixel`).
Optional 1px shadow is enabled on title/pixel styles (`UI_FONT_STYLE_SHADOW=1`, disable with `-DUI_FONT_STYLE_SHADOW=0`).

## Families and sizes
Target families:
- Inter: `14, 18, 24, 32`
- Orbitron: `28, 40`
- IBM Plex Mono: `14, 18`
- Press Start 2P (optional): `16, 24`

Runtime defaults now use the external generated set (`UI_FONT_EXTERNAL_SET=1` by default in `ui_fonts.cpp`).
You can force fallback to Montserrat with `-DUI_FONT_EXTERNAL_SET=0`.

## FR glyph coverage requirements
Font generation manifest enforces:
- `0x20-0x7E` (Basic Latin)
- `0xA0-0x00FF` (Latin-1)
- `0x0100-0x017F` (Latin Extended-A)
- `0x2010-0x2030` (typographic punctuation)

Forced symbols:
- `’”“«»…•–—°œŒ`
- FR accents used by scenes (`àâäçéèêëîïôöùûüÿ` + uppercase variants)

## Regeneration workflow
1. Place TTF files in `tools/fonts/ttf/`.
2. Run:
   - `tools/fonts/scripts/generate_lvgl_fonts.sh`
3. Generated `.c` files are emitted in:
   - `hardware/firmware/ui_freenove_allinone/src/ui/fonts/`

## Notes
- Fonts are converted offline to LVGL C arrays (`bpp=4`).
- Font arrays are flash-resident; LVGL RAM usage is mainly object/style heap.
- Keep enabled sizes minimal to control flash size.
