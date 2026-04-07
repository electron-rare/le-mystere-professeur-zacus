# PLIP Téléphone — Hardware Implementation Plan (KiCad)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Design and fabricate a compact PCB (ESP32 + Si3210 SLIC) for the Zacus escape room telephone module, using the full EDA MCP pipeline (sourcing → schéma → PCB → review → JLCPCB export).

**Architecture:** KiCad 9 project with custom Si3210 symbol/footprint, ESP32-WROOM-32E module, CP2102N USB-UART bridge, RJ9 connector for handset, micro SD slot, JST UI link. 2-layer PCB < 60x40mm, JLCPCB assembly with LCSC parts.

**Tech Stack:** KiCad 9, MCP servers (jlcmcp-remote, jlcpcb-search, kicad-sch, kicad-design, kicad-fab), kicad-happy skills

**Spec:** `docs/superpowers/specs/2026-04-08-plip-telephone-design.md`

---

## File Structure

### New Files

| File | Responsibility |
|------|---------------|
| `hardware/projects/plip-telephone/plip-telephone.kicad_pro` | KiCad project file |
| `hardware/projects/plip-telephone/plip-telephone.kicad_sch` | Schematic (single sheet) |
| `hardware/projects/plip-telephone/plip-telephone.kicad_pcb` | PCB layout |
| `hardware/projects/plip-telephone/libs/Si3210.kicad_sym` | Custom Si3210 symbol |
| `hardware/projects/plip-telephone/libs/Si3210_QFN38.kicad_mod` | Custom Si3210 QFN-38 footprint |
| `hardware/projects/plip-telephone/libs/RJ9_4P4C.kicad_mod` | RJ9 connector footprint |
| `hardware/projects/plip-telephone/gerbers/` | Gerber output directory |
| `hardware/projects/plip-telephone/jlcpcb/` | BOM + CPL for JLCPCB assembly |
| `hardware/projects/plip-telephone/README.md` | Project documentation |

---

## Task 1: Source Components via JLCPCB MCPs

**Files:**
- Create: `hardware/projects/plip-telephone/README.md`

Before any KiCad work, validate that all components are available on LCSC with confirmed part numbers and stock.

- [ ] **Step 1: Create project directory**

```bash
mkdir -p "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/libs"
mkdir -p "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/gerbers"
mkdir -p "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/jlcpcb"
```

- [ ] **Step 2: Source Si3210 via jlcmcp-remote MCP**

Use the `jlcmcp-remote` MCP tool to search:
```
Search: "Si3210-E-FM" 
Verify: LCSC C6295850, stock > 0, price ~$2.70
```

- [ ] **Step 3: Source ESP32-WROOM-32E**

```
Search: "ESP32-WROOM-32E-N16"
Verify: LCSC C701341, stock > 100
```

- [ ] **Step 4: Source CP2102N USB-UART bridge**

```
Search: "CP2102N QFN"
Verify: LCSC C6568 or variant, stock > 100
```

- [ ] **Step 5: Source 600Ω:600Ω audio transformer**

```
Search: "600 ohm audio transformer SMD"
Alternative search: "telephone line transformer 600"
Fallback: "PE-65612" or "TY-401P" or equivalent
```

If no SMD transformer available, search for through-hole:
```
Search: "600:600 transformer through hole"
```

- [ ] **Step 6: Source RJ9 4P4C connector**

```
Search: "RJ9 4P4C female PCB"
Alternative: "RJ10 4P4C connector"
```

- [ ] **Step 7: Source remaining components**

Search and confirm each:
- ME6211C33M5G-N (LDO 3.3V): C82942
- USB-C 16pin SMD: C2765186
- Micro SD slot: C585353
- JST-SH 4pin: C265021
- 100nF 0402: C307331
- 10µF 0805: C19702
- 10k 0402: C25744
- LED 0402 green: C2286
- ESD protection USB (USBLC6-2SC6): C7519

- [ ] **Step 8: Write README with confirmed BOM**

Create `hardware/projects/plip-telephone/README.md`:

```markdown
# PLIP Téléphone — Zacus Escape Room

## Description
Custom PCB: ESP32-WROOM-32E + Si3210 SLIC for driving a retro
telephone handset in the Zacus escape room.

## Features
- Si3210 SLIC: ring generation, off-hook detect, audio codec
- Bluetooth Classic: A2DP sink + HFP hands-free
- WiFi: commands + Piper TTS streaming
- Micro SD: local audio storage
- UART: UI link to ESP8266/RP2040 display
- RJ9: standard telephone handset connector

## BOM
See `jlcpcb/bom.csv` for full LCSC part list.

## Fabrication
Target: JLCPCB 2-layer, HASL lead-free, SMT assembly top side.
```

- [ ] **Step 9: Commit**

```bash
cd "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus"
git add hardware/projects/plip-telephone/
git commit -m "feat(plip): init project, source BOM"
```

---

## Task 2: Create Si3210 KiCad Symbol

**Files:**
- Create: `hardware/projects/plip-telephone/libs/Si3210.kicad_sym`

The Si3210-E-FM (QFN-38, 5x7mm) is not in the KiCad standard library. Create a custom symbol.

- [ ] **Step 1: Create the Si3210 symbol file**

Create `hardware/projects/plip-telephone/libs/Si3210.kicad_sym` with all 38 pins organized by function:

**Pin groups (left side):**
- SPI: SCLK(34), SDI(33), SDO(32), CS(35)
- PCM: PCLK(37), FSYNC(2), DTX(1), DRX(38)
- Control: RESET(3), INT(36), TEST(28)

**Pin groups (right side):**
- Line TIP: STIPAC(16), STIPDC(11), STIPE(13), ITIPP(24), ITIPN(25)
- Line RING: SRINGAC(17), SRINGDC(12), SRINGE(15), IRINGP(22), IRINGN(21)
- DC-DC: DCDRV(30), DCFF(29), SVBAT(14)

**Pin groups (top):**
- Power: VDDA1(6), VDDA2(23), VDDD(26)

**Pin groups (bottom):**
- Ground: GNDA(19), GNDD(27), QGND(9)
- Analog: IREF(7), CAPP(8), CAPM(10), IGMP(20), IGMN(18)
- Serial: SDCH(4), SDCL(5), SDITHRU(31)

Use the `kicad-sch` MCP or manually create the .kicad_sym file following KiCad 9 format.

- [ ] **Step 2: Verify the symbol loads in KiCad**

```bash
kicad-cli sym export svg \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/libs/Si3210.kicad_sym" \
  -o /tmp/si3210_sym.svg
open /tmp/si3210_sym.svg
```

- [ ] **Step 3: Commit**

```bash
git add hardware/projects/plip-telephone/libs/Si3210.kicad_sym
git commit -m "feat(plip): add Si3210 KiCad symbol"
```

---

## Task 3: Create Si3210 QFN-38 Footprint

**Files:**
- Create: `hardware/projects/plip-telephone/libs/Si3210_QFN38.kicad_mod`

QFN-38, 5x7mm body, 0.5mm pitch, exposed pad.

- [ ] **Step 1: Create the footprint**

Use the `kicad-design` MCP or KiCad footprint editor. QFN-38 specs from Si3210 datasheet:
- Body: 5.0 x 7.0 mm
- Pad pitch: 0.5 mm
- Pad size: 0.25 x 0.75 mm
- Pin 1 marker: top-left
- Exposed pad: 3.4 x 5.4 mm (thermal)
- Pin distribution: 10 pins on 5mm sides, 9 pins on 7mm sides

- [ ] **Step 2: Verify footprint**

```bash
kicad-cli fp export svg \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/libs/Si3210_QFN38.kicad_mod" \
  -o /tmp/si3210_fp.svg
open /tmp/si3210_fp.svg
```

- [ ] **Step 3: Commit**

```bash
git add hardware/projects/plip-telephone/libs/Si3210_QFN38.kicad_mod
git commit -m "feat(plip): add Si3210 QFN-38 footprint"
```

---

## Task 4: Create KiCad Schematic

**Files:**
- Create: `hardware/projects/plip-telephone/plip-telephone.kicad_pro`
- Create: `hardware/projects/plip-telephone/plip-telephone.kicad_sch`

Single-sheet schematic with 6 blocks.

- [ ] **Step 1: Create KiCad project**

Create `plip-telephone.kicad_pro` (KiCad 9 project file) with library paths pointing to `./libs/`.

- [ ] **Step 2: Place ESP32-WROOM-32E**

Use KiCad standard library symbol `RF_Module:ESP32-WROOM-32E`. Connect:
- 3V3 → power rail
- GND → ground
- EN → 10k pull-up to 3V3
- GPIO0 → 10k pull-up + DTR auto-reset circuit
- IO18 → SPI SCLK
- IO23 → SPI MOSI
- IO19 → SPI MISO
- IO5 → Si3210 CS
- IO15 → SD CS
- IO4 → Si3210 INT
- IO2 → Si3210 RESET
- IO26 → I2S BCLK
- IO25 → I2S LRCLK
- IO22 → I2S DOUT
- IO35 → I2S DIN
- IO17 → UART2 TX (UI)
- IO16 → UART2 RX (UI)
- IO1 → CP2102N RXD
- IO3 → CP2102N TXD

- [ ] **Step 3: Place Si3210 SLIC**

Use the custom Si3210 symbol from Task 2. Connect:
- SPI (SCLK, SDI, SDO, CS) → ESP32 SPI bus
- PCM (PCLK, FSYNC, DTX, DRX) → ESP32 I2S pins
- RESET → ESP32 IO2
- INT → ESP32 IO4
- VDDA1, VDDA2 → 3.3V with 100nF decoupling each
- VDDD → 3.3V with 100nF decoupling
- GNDA, GNDD, QGND → GND
- DC-DC section: DCDRV → inductor 100µH → SVBAT, DCFF → resistor divider feedback
- Line section: STIPAC/SRINGAC → 600Ω transformer primary → RJ9
- IREF → 20kΩ to GND (reference current)
- CAPP/CAPM → 100nF cap between pins
- TEST → GND (normal operation)

- [ ] **Step 4: Place CP2102N USB-UART bridge**

Use KiCad standard library `Interface_USB:CP2102N-A02-GQFN24`. Connect:
- D+/D- → USB-C data pins (via 22Ω series resistors)
- TXD → ESP32 IO3 (RX)
- RXD → ESP32 IO1 (TX)
- DTR → auto-reset circuit (100nF cap → ESP32 EN)
- RTS → auto-reset circuit (100nF cap → ESP32 IO0)
- VDD → USB 5V via ferrite bead
- REGIN → USB 5V
- VBUS → USB 5V (for USB detect)

- [ ] **Step 5: Place power section**

- USB-C connector (16pin) with CC1/CC2 5.1kΩ pull-down (UFP identification)
- ESD protection: USBLC6-2SC6 on D+/D-
- ME6211C33 LDO: VIN ← USB 5V, VOUT → 3.3V rail, CIN 1µF, COUT 1µF
- Bulk capacitors: 10µF on 3.3V rail (×2)
- Decoupling: 100nF on each IC VDD pin

- [ ] **Step 6: Place SD card and connectors**

- Micro SD slot: SPI mode (CLK, MOSI, MISO, CS) + 10kΩ pull-ups on all data lines
- RJ9 4P4C: 4 pins → transformer secondary
- JST-SH 4pin: TX, RX, 3V3, GND (UART UI link)
- 2× LEDs 0402 (power + status) with 1kΩ series resistors

- [ ] **Step 7: Add transformer and line protection**

600Ω:600Ω audio transformer between Si3210 AC outputs (STIPAC, SRINGAC) and RJ9 pins.
Add protection diodes (TVS) on RJ9 side for ESD.

- [ ] **Step 8: Run ERC**

```bash
kicad-cli sch erc \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/plip-telephone.kicad_sch"
```

Expected: 0 errors (warnings acceptable for unconnected pins like TEST).

- [ ] **Step 9: Commit**

```bash
git add hardware/projects/plip-telephone/plip-telephone.kicad_pro
git add hardware/projects/plip-telephone/plip-telephone.kicad_sch
git commit -m "feat(plip): complete schematic"
```

---

## Task 5: PCB Layout and Routing

**Files:**
- Create: `hardware/projects/plip-telephone/plip-telephone.kicad_pcb`

Target: < 60 x 40 mm, 2 layers.

- [ ] **Step 1: Set design rules**

Use `kicad-design` MCP:
- Clearance: 0.15mm
- Min trace width: 0.2mm
- Min via drill: 0.3mm
- Power trace width: 0.4mm
- Board outline: 58 x 38 mm (rounded corners 1mm)

- [ ] **Step 2: Component placement**

Layout strategy:
```
┌──────────────────────────────────┐
│  USB-C    CP2102N    SD card     │ ← Top edge
│                                  │
│  ME6211   ESP32-WROOM-32E       │ ← Center (antenna → right edge, clear)
│  (LDO)                          │
│                                  │
│  Si3210   Transfo    RJ9        │ ← Bottom edge
│  + DCDC   600Ω                  │
│                                  │
│  JST(UI)  LEDs                  │ ← Left edge
└──────────────────────────────────┘
```

Key rules:
- ESP32 antenna area: NO copper (GND or traces) under antenna on BOTH layers
- Si3210 exposed pad: large thermal via array to bottom GND plane
- Transformer: keep close to Si3210 and RJ9, short traces
- USB-C: close to CP2102N, differential pair routing

- [ ] **Step 3: Route with Freerouting**

Use `kicad-design` MCP with Freerouting Docker:
```
1. Export DSN from KiCad
2. Run Freerouting autorouter
3. Import SES result
4. Manual cleanup of critical traces (USB diff pair, I2S, SPI)
```

- [ ] **Step 4: Manual cleanup**

After autoroute:
- Verify USB D+/D- are routed as differential pair (90Ω impedance)
- Verify SPI and I2S traces are short and direct
- Verify power traces are wide enough (0.4mm+)
- Add stitching vias around board edges
- Fill remaining areas with GND copper pour (both layers)

- [ ] **Step 5: Run DRC**

```bash
kicad-cli pcb drc \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/plip-telephone.kicad_pcb"
```

Expected: 0 errors.

- [ ] **Step 6: Commit**

```bash
git add hardware/projects/plip-telephone/plip-telephone.kicad_pcb
git commit -m "feat(plip): PCB layout and routing"
```

---

## Task 6: Design Review (kicad-happy skills)

**Files:** No new files — review only.

- [ ] **Step 1: DFM review**

Use kicad-happy `kicad` skill:
- Check minimum trace/space against JLCPCB capabilities
- Check via sizes
- Check silk clearances
- Check board edge clearance

- [ ] **Step 2: EMC pre-compliance**

Use kicad-happy `emc` skill:
- Check decoupling capacitor placement (< 5mm from IC pins)
- Check return path continuity (no GND plane splits under signal traces)
- Check USB differential pair routing
- Check I2S/SPI clock trace lengths

- [ ] **Step 3: Fix any issues found**

Apply corrections from review, re-run DRC.

- [ ] **Step 4: Commit fixes**

```bash
git add hardware/projects/plip-telephone/plip-telephone.kicad_pcb
git commit -m "fix(plip): apply DFM/EMC review fixes"
```

---

## Task 7: JLCPCB Export

**Files:**
- Create: `hardware/projects/plip-telephone/gerbers/*`
- Create: `hardware/projects/plip-telephone/jlcpcb/bom.csv`
- Create: `hardware/projects/plip-telephone/jlcpcb/cpl.csv`

- [ ] **Step 1: Export Gerbers**

```bash
kicad-cli pcb export gerbers \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/plip-telephone.kicad_pcb" \
  -o "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/gerbers/"

kicad-cli pcb export drill \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/plip-telephone.kicad_pcb" \
  -o "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/gerbers/"
```

- [ ] **Step 2: Generate BOM for JLCPCB**

Use kicad-happy `bom` skill or `kicad-design` MCP `export_jlcpcb` tool:
- Format: `Designator,Package,Quantity,LCSC Part Number`
- Ensure all LCSC IDs are populated from Task 1 sourcing

Save to `jlcpcb/bom.csv`.

- [ ] **Step 3: Generate CPL (Component Placement List)**

Export component positions:
```bash
kicad-cli pcb export pos \
  "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/plip-telephone.kicad_pcb" \
  --format csv --units mm --side front \
  -o "/Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus/hardware/projects/plip-telephone/jlcpcb/cpl.csv"
```

Use kicad-happy `jlcpcb` skill to apply rotation offset corrections (JLCPCB rotation table).

- [ ] **Step 4: Verify JLCPCB package**

Check:
- All Gerber layers present (F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts, drill)
- BOM has LCSC part numbers for all assembled components
- CPL has correct rotations (preview in JLCPCB)

- [ ] **Step 5: Commit**

```bash
git add hardware/projects/plip-telephone/gerbers/
git add hardware/projects/plip-telephone/jlcpcb/
git commit -m "feat(plip): JLCPCB export (gerbers+BOM+CPL)"
```

---

## Validation Criteria

1. **ERC clean** — 0 errors on schematic
2. **DRC clean** — 0 errors on PCB
3. **DFM passed** — kicad-happy review OK
4. **BOM 100% sourced** — all LCSC IDs confirmed in stock
5. **CPL rotations corrected** — kicad-happy JLCPCB rotation table applied
6. **Gerbers complete** — all layers exported
7. **Board < 60x40mm** — 2 layers, HASL lead-free
