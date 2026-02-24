# SCENE_WIN_ETAPE demoscene calibration (A/B/C + C loop)

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
- `demoscene 1998`

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
| Timing | `kB1_CrashMs` | 900 | Crash punch at start of B, then B2 interlude. |
| Timing | `kC_CycleMs` | 20000 | Clean loop cadence before C repeats. |
| Palette | `kBg` | `#000022` | Dark retro background baseline. |
| Palette | `kAccent1` | `#00FFFF` | Copper/scroller cyan. |
| Palette | `kAccent2` | `#FF00FF` | Alternate magenta lines and wireframe. |
| Palette | `kAccent3` | `#FFFF00` | Highlight pulses and fireworks accents. |
| Palette | `kText` | `#FFFFFF` | Foreground readability. |
| Scroll A | `kScrollMidA_PxPerSec` | 216 | Fast cracktro speed in requested range (160..280). |
| Scroll A | `kScrollBotA_PxPerSec` | 108 | Bottom rollback speed in requested range (60..160). |
| Scroll C | `kScrollC_PxPerSec` | 72 | Slow clean ping-pong in requested range (40..110). |
| Sine | `kSineAmpA` | 18 px | Strong wave for cracktro text readability. |
| Sine | `kSineAmpC` | 12 px | Softer clean wave within half-height band. |
| Sine | `kSinePeriodPx` | 104 px | Mid-wave spacing suited to 480x320 and similar panels. |
| Sine | `kSinePhaseSpeed` | 1.9 rad/s | Stable oscillation without jitter. |
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
- Tick: `33 ms` (target `30 FPS`).
- No malloc/new in per-frame update loops.
- Object caps for this scene:
  - small displays: `<= 140` LVGL objects
  - larger displays: `<= 260` LVGL objects
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
- Sequence: `A(30000) -> B(15000: B1 crash + B2 interlude) -> C(20000) -> C loop`.
- Skip behavior: input in A/B jumps to C clean start.
- C scroller: horizontal ping-pong + sine, bounded to center half-height band.
- Cleanup: intro timer paused on exit, all intro objects hidden/reset.
