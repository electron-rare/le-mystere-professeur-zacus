# SCENE_WIN_ETAPE demoscene calibration (A/B/C + C loop)

## FX runtime lock (current)
- Render mode: `FX_ONLY_V9` on `SCENE_WIN_ETAPE` (legacy intro widgets hidden, LGFX FX + LVGL overlay kept).
- Timeline: `A=30000ms`, `B=15000ms`, `C=20000ms`, then infinite `C` loop.
- Presets by phase:
  - `A -> demo`
  - `B -> winner`
  - `C/C_LOOP -> boingball`
- Modes 3D by phase (default):
  - `A -> starfield3d`
  - `B -> dotsphere3d`
  - `C/C_LOOP -> raycorridor`
- V9 timeline mapping:
  - `demo -> /ui/fx/timelines/demo_3d.json` (fallback `/ui/fx/timelines/demo_90s.json`)
  - `winner -> /ui/fx/timelines/winner.json`
  - `fireworks -> /ui/fx/timelines/fireworks.json`
  - `boingball -> /ui/fx/timelines/boingball.json`
- Scroller:
  - font: `italic` by default (`basic|bold|outline|italic` supported)
  - text: phase-specific via `FX_SCROLL_TEXT_A/B/C`
  - BPM sync: `FX_BPM`, default `125` (`60..220`)
- Override schema (new keys only): `A_MS`, `B_MS`, `C_MS`, `FX_PRESET_*`, `FX_MODE_*`, `FX_SCROLL_TEXT_*`, `FX_SCROLL_FONT`, `FX_BPM`.
- Compatibility break: legacy keys `FX_3D`, `FX_3D_QUALITY`, `FONT_MODE` are intentionally ignored.
- Memory/perf constraints:
  - no per-frame allocations in FX render loop
  - boing shadow darken path: ASM S3 by default (`UI_BOING_SHADOW_ASM=1`), C fallback if unavailable
  - boot evidence log: `boing_shadow_path=asm|c`
  - fast blit path: 2x scaler (`UI_FX_BLIT_FAST_2X=1`) when source/display ratio is exact
  - LGFX line-buffer blit path preserved; `UI_GFX_STATUS` must keep `fx_fps > 0` and no panic/reboot markers
  - scenes `SCENE_WINNER` and `SCENE_FIREWORKS` exposed in story registry/data

## References consulted

### Query set executed
- `demoscene waving scroller starfield`
- `copper bars raster bars effect tutorial`
- `how to code sine scroller`
- `starfield effect tutorial fixed point`
- `demoscene rotozoom effect tutorial`
- `demoscene tunnel effect tutorial`
- `wireframe cube perspective projection fixed point`
- `LVGL canvas draw performance ESP32`
- `demoscene amiga 1998`

### Visual references
1. https://www.pouet.net/prodlist.php?type%5B0%5D=cracktro
2. https://www.awsm.de/jscracktros/
3. https://www.youtube.com/watch?v=g2Vb5Bdyp7Y
4. https://www.youtube.com/results?search_query=amiga+cracktro
5. https://www.youtube.com/results?search_query=demoscene+1998
6. https://www.youtube.com/results?search_query=shader+showdown+live+coding
7. https://maque.github.io/roto-zoom/
8. https://lodev.org/cgtutor/tunnel.html

### Technical references
1. https://rocket.github.io/
2. https://github.com/rocket/rocket
3. https://ginnov.github.io/littlethings/tutorials/copper_tutorial.html
4. https://wiki.abime.net/hardware/copper_programming_model
5. https://www.stashofcode.com/how-to-code-a-sine-scroll-on-amiga-5/
6. https://projectf.io/posts/sinescroll/
7. https://www.kmjn.org/notes/3d_rendering_intro.html
8. https://docs.lvgl.io/master/widgets/canvas.html
9. https://docs.lvgl.io/master/main-modules/display/refreshing.html
10. https://docs.lvgl.io/master/main-modules/display/overview.html

## What we imitate (demoscene patterns)
- Cracktro long format: copper circular/wavy + layered stars + aggressive middle scroller.
- Transition split: short crash/glitch impact, then calmer interlude with pseudo-3D background and periodic fireworks.
- Clean loop: slower ping-pong sine scroller, better readability, wavy gradient background.
- Retro typography: 1px shadow, double labels, overshoot logo, short blink highlights.

## Calibration table (locked runtime constants)

| Domain | Constant | Value | Why / reference pattern |
|---|---:|---:|---|
| Timing | `kA_DurationMs` | 30000 | Long cracktro section requested for launch. |
| Timing | `kB_DurationMs` | 15000 | Long transition block with explicit interlude. |
| Timing | `kB1_CrashMs` | 4000 (clamp 3000..5000) | Crash punch at start of B, then B2 interlude. |
| Timing | `kC_CycleMs` | 20000 | Clean loop cadence before C repeats. |
| Palette | `kBg` | `#000022` | Dark retro background baseline. |
| Palette | `kAccent1` | `#00FFFF` | Copper/scroller cyan. |
| Palette | `kAccent2` | `#FF00FF` | Alternate magenta lines and wireframe. |
| Palette | `kAccent3` | `#FFFF00` | Highlight pulses and fireworks accents. |
| Palette | `kText` | `#FFFFFF` | Foreground readability. |
| Scroll A | `kScrollMidA_PxPerSec` | 216 | Fast cracktro speed in requested range (160..280). |
| Scroll A | `kScrollBotA_PxPerSec` | 108 | Bottom rollback speed in requested range (60..160). |
| Scroll C | `kScrollC_PxPerSec` | 72 | Slow clean ping-pong in requested range (40..110). |
| Sine | `kSineAmpA` | dynamic `max(80, H/4 - font_h/2)` | Keep center scroller spanning >= 50% screen height. |
| Sine | `kSineAmpC` | dynamic `max(80, H/4 - font_h/2)` | Same readability rule in clean loop. |
| Sine | `kSinePeriodPx` | 104 px | Mid-wave spacing suited to 480x320 and similar panels. |
| Sine | `kSinePhaseSpeed` | 1.9 rad/s | Stable oscillation without jitter. |
| Center text | `band_top/bottom` | `[H/4, 3H/4]` | Vertical span lock for center scrollers. |
| Center text | `padding_spaces` | `14` each side | Avoid visual clipping against screen edges. |
| Stars | `stars_total` | `clamp((w*h)/1200, 60..220)` | Density target from cracktro references. |
| Stars | `layers` | `3 (50/30/20)` | Standard depth split. |
| Stars | `speed_l1/l2/l3` | `38 / 96 / 182 px/s` | Far-mid-near parallax. |
| Copper | `rings_count` | `clamp(h/22, 8..18)` | Screen-relative ring density. |
| Copper | `phase_speed` | `1.35 rad/s` | Circular/wavy motion without overload. |
| WireCube | `fov` | 156 | Stable perspective on Freenove display. |
| WireCube | `zoff` | 320 | Avoid clipping while rotating/morphing. |
| WireCube | `morph` | `m=0.5*(1-cos(phase))` | Geometric cube<->sphere blend. |
| Fireworks | `burst` | `24..48 particles` | Visual punch with bounded pool usage. |
| Fireworks | `gravity` | `180 px/s2` | Arc readability over short lifetime. |
| Glitch | `jitter` | `3..10 px` | Crash intensity while preserving text legibility. |
| Glitch | `blink` | `8..14 Hz` | Classic short strobe feel. |

## Performance budget
- Tick: `42 ms` (target `24 FPS`).
- No malloc/new in per-frame update loops.
- Object caps for this scene:
  - small displays: `<= 140` LVGL objects
  - larger displays: `<= 260` LVGL objects
- CPU budget target: intro tick update `<= 14 ms` average (headroom for DMA flush path).
- 3D quality policy:
  - `low`: stripe count reduced, lower opacity
  - `med`: balanced defaults
  - `high`: denser stripes and stars
- Fireworks pool cap: `72` hard maximum.
- Wavy scroller pool cap: `64 glyph + 64 shadow`.
- Canvas-equivalent budgets for future per-pixel fallback:
  - `low: 120x80`, `med: 160x120`, `high: 200x150`.

## Mapping: reference -> parameter -> skill

| Reference source | Parameter extracted | Skill impacted |
|---|---|---|
| Pouet + JS cracktros | Copper pacing, palette cycling, scroller rhythm | `Skill_CopperWavyBars`, `Skill_WavySineScroller` |
| Sine scroller tutorials | `kScrollMidA`, `kScrollC`, amp/period/phase | `Skill_WavySineScroller` |
| Wireframe perspective notes | FOV, z offset, morph blending constraints | `Skill_3D_WireCubeMorph` |
| Tunnel/rotozoom references | B2/C background pacing and opacity | `Skill_3D_RotoZoom` / `Skill_3D_Tunnel` |
| Copper programming refs | Ring density and phase speed | `Skill_CopperWavyBars` |
| Rocket docs | Deterministic timeline + phase transitions | Intro state machine |
| LVGL display/canvas refs | fixed tick, dt clamp, object caps | All runtime skills |

## Final firmware behavior lock
- Sequence: `A(30000) -> B(15000) -> C(20000) -> C loop`.
- Phase preset lock: `A=demo`, `B=winner`, `C=boingball`.
- Phase mode lock: `A=starfield3d`, `B=dotsphere3d`, `C=raycorridor`.
- Override lock: `A_MS/B_MS/C_MS + FX_PRESET_* + FX_MODE_* + FX_SCROLL_* + FX_BPM` uniquement.
- Inputs disabled for sequence control (A/B skip + overlay toggle removed).
- Autorun test loop: full A->B->C chain repeats every 2 minutes (`UI_DEMO_AUTORUN_WIN_ETAPE` context).
- Cleanup: intro timer paused on exit, all intro legacy objects hidden/reset.
- Runtime telemetry every 5s: `phase`, `t`, `obj`, `stars`, `particles`, `fx_fps`, `heap_internal`, `heap_psram`, `largest_dma`.
