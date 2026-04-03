# 3D Enclosure Specifications — Zacus V3

All enclosures designed for FDM 3D printing unless noted. STL source files live in `hardware/enclosures/`.
OpenSCAD source: `hardware/enclosures/*.scad` (see plan Task 2.3 for parametric source).

---

## P1 — Boîte Sonore (Séquence Sonore)

**Dimensions:** 150 × 100 × 80 mm (L × W × H)
**Material:** PETG (preferred for durability) or PLA+
**Color:** Black — for stage/theatrical look; buttons are colored by their caps

### Openings & Cutouts

| Feature          | Location               | Size          | Notes                                        |
|------------------|------------------------|---------------|----------------------------------------------|
| Speaker grille   | Front face, upper half | 40 × 40 mm    | Honeycomb pattern — 2mm hex holes, 1mm walls |
| Button hole RED  | Top face, x=20         | ⌀30 mm        | Sanwa OBSF-30 standard mounting              |
| Button hole BLUE | Top face, x=60         | ⌀30 mm        | Sanwa OBSF-30                                |
| Button hole YEL  | Top face, x=90         | ⌀30 mm        | Sanwa OBSF-30                                |
| Button hole GRN  | Top face, x=130        | ⌀30 mm        | Sanwa OBSF-30                                |
| USB-C slot       | Right side, bottom     | 12 × 6 mm     | Power input access                           |
| LED strip window | Front face, lower strip| 100 × 8 mm    | Diffused acrylic insert (frosted tape ok)    |
| Lid vent grid    | Lid top                | 4×1 mm slots  | 10mm grid — passive thermal                  |
| PCB mount studs  | Interior floor         | M3 standoffs  | 4× M3×5mm brass inserts                      |

### Assembly
- Box body + removable lid (M3 screws, 4 corners)
- Speaker mounted on interior face panel with 2× M2.5 self-tappers
- MAX98357A module hot-glued behind speaker grille
- ESP32-S3 DevKit velcro-mounted to floor (removable for flashing)
- Buttons snap into top holes with Sanwa mounting nut

### Print Settings
| Parameter       | Value                  |
|-----------------|------------------------|
| Layer height    | 0.2 mm                 |
| Infill          | 25% gyroid             |
| Walls           | 3 perimeters           |
| Top/bottom      | 4 layers               |
| Supports        | Yes — lid vent overhang only |
| Print time est. | ~5h (box) + 1h (lid)  |
| Filament        | ~180g PETG             |

---

## P4 — Poste Radio Rétro (Fréquence Radio)

**Dimensions:** 200 × 150 × 120 mm (L × W × H)
**Material:** PLA (retro aesthetic — easier to sand and paint)
**Color:** Print in dark brown or beige PLA; hand-paint or vinyl wrap for retro finish

### Openings & Cutouts

| Feature             | Location                     | Size            | Notes                                         |
|---------------------|------------------------------|-----------------|-----------------------------------------------|
| Rotary encoder hole | Front face, right side       | ⌀14 mm          | KY-040 shaft + nut clearance                  |
| Encoder knob recess | Front face, around encoder   | ⌀35 mm, 5mm deep| For large Bakelite-style knob                 |
| OLED window         | Front face, left-center      | 35 × 18 mm      | Acrylic window insert — glued from inside     |
| Speaker grille      | Front face, upper center     | ⌀80 mm (circular)| 19-hole hex pattern — retro radio aesthetic   |
| USB-C slot          | Bottom face                  | 12 × 6 mm       | Recessed — hidden when on table               |
| Battery access door | Back face                    | 60 × 30 mm      | Sliding door for 18650 replacement            |
| Antenna cutout      | Top face, right side         | 8 × 60 mm slot  | Decorative antenna rod (wooden dowel)         |
| Corner radius       | All edges                    | R15 mm          | Rounded hull — retro form factor              |
| Vent holes          | Back face, bottom            | 4 × 8mm slots   | Battery charging ventilation                  |

### Assembly
- Body printed in 2 halves (top + bottom) — split at midline
- OLED glued behind acrylic window from inside
- KY-040 mounted through front panel, locked with hex nut
- Speaker front-mounted, MAX98357A behind grille
- 18650 holder slides into battery compartment from back door
- ESP32 DevKit mounted on standoffs — accessible from bottom removal

### Print Settings
| Parameter       | Value                    |
|-----------------|--------------------------|
| Layer height    | 0.2 mm (0.15 for details)|
| Infill          | 20% gyroid               |
| Walls           | 4 perimeters             |
| Top/bottom      | 5 layers                 |
| Supports        | Yes — rounded hull corners, antenna slot |
| Print time est. | ~9h (body halves) + 1h (door) |
| Filament        | ~320g PLA                |
| Post-process    | Sand 220 grit → prime → paint (retro brown/beige) |

---

## P5 — Télégraphe (Code Morse)

**Dimensions:** 180 × 80 × 60 mm (L × W × H) — for base only; brass key mounted on top
**Material:** PETG dark brown or woodPLA for telegraph aesthetic
**Color:** Dark brown — telegraph base aesthetic

### Openings & Cutouts

| Feature             | Location              | Size          | Notes                                           |
|---------------------|-----------------------|---------------|-------------------------------------------------|
| Key mounting posts  | Top face, right half  | 2× M3 holes   | Brass telegraph key bolted down                 |
| Buzzer grille       | Front face            | 20 × 20 mm    | Active buzzer — 2mm hex holes                   |
| LED red (key press) | Front face, left      | ⌀5 mm         | 5mm LED holder press-fit                        |
| LED green (valid)   | Front face, right     | ⌀5 mm         | 5mm LED holder press-fit                        |
| WS2812B window      | Top face, left area   | 15 × 8 mm     | Diffused window for light-mode morse            |
| USB-C slot          | Left side, bottom     | 12 × 6 mm     | Power input                                     |
| ESP32 access slot   | Bottom face           | 80 × 20 mm    | Removable bottom panel for programming          |
| Cable routing slot  | Interior              | 6 mm channels | Wires from key to PCB                           |

### Assembly
- Single-piece base with snap-on bottom panel
- Brass telegraph key (Aliexpress morse key) mounted with M3 bolts
- Key spring contact wired to GPIO4 with 2-pin JST connector
- Buzzer hot-glued behind front grille
- 5mm LED holders press-fit into front holes
- WS2812B behind frosted diffuser window on top

### Print Settings
| Parameter       | Value                   |
|-----------------|-------------------------|
| Layer height    | 0.2 mm                  |
| Infill          | 30% gyroid (heavier base)|
| Walls           | 4 perimeters            |
| Top/bottom      | 4 layers                |
| Supports        | Minimal — USB slot only |
| Print time est. | ~4h                     |
| Filament        | ~130g PETG/woodPLA      |

---

## P7 — Coffre Final (Coffre avec verrou électronique)

**Dimensions:** 250 × 200 × 150 mm (L × W × H) — wooden box, NOT 3D printed
**Material:** Pine wood box (hardware store) — ESP32 electronics in internal tray (3D printed)

### Wooden Box Modifications

| Feature             | Location              | Size            | Notes                                          |
|---------------------|-----------------------|-----------------|------------------------------------------------|
| Keypad cutout       | Lid front face        | 75 × 60 mm      | 4×3 membrane keypad recess                     |
| OLED window         | Lid front face        | 35 × 18 mm      | Above keypad — acrylic window                  |
| RGB LED hole        | Lid front face        | ⌀8 mm           | WS2812B status indicator                       |
| Servo mount         | Inside lid            | 30 × 20 mm slot | SG90 servo + latch mechanism                   |
| Latch slot          | Front face (lid edge) | 15 × 8 mm       | Steel latch bar — closes box electromagnetically|
| USB-C slot          | Back face             | 12 × 6 mm       | Hidden — covered by rubber flap                |
| Electronics tray    | Inside base           | 220 × 170 × 40  | 3D printed PETG tray — holds ESP32, PCB, servo |
| Cable grommet       | Tray → lid            | ⌀12 mm          | Spiral cable wrap — lid opening clearance      |

### 3D Printed Electronics Tray (Internal)

**Dimensions:** 220 × 170 × 40 mm
**Material:** PETG
**Features:**
- ESP32 DevKit mount (4× M3 standoffs)
- Servo horn slot and cable routing
- Buzzer pocket (press-fit)
- PCB mount (2× M3)
- Lid hinge clearance notch

### Print Settings (Electronics Tray)
| Parameter       | Value              |
|-----------------|--------------------|
| Layer height    | 0.25 mm            |
| Infill          | 20% grid           |
| Walls           | 3 perimeters       |
| Top/bottom      | 3 layers           |
| Supports        | None               |
| Print time est. | ~3h                |
| Filament        | ~90g PETG          |

### Servo Latch Mechanism
- SG90 at 0° = latch bar extended (locked)
- SG90 at 90° = latch bar retracted (open)
- Latch bar: 3D printed PETG bar 60×8×5mm
- Spring-loaded return (2mm steel wire spring)
- PWM: 50Hz, duty 1ms=closed, 2ms=open

---

## P2, P3, P6 — No Custom Enclosure Needed

| Puzzle | Housing                                             |
|--------|-----------------------------------------------------|
| P2     | Magnetic folding board (self-contained, no enclosure)|
| P3     | Cards only — no enclosure                           |
| P6     | Wooden tablet is the housing (laser cut 3mm plywood)|

---

## Print Summary

| Puzzle | Volume   | Material | Print Time | Filament | Est. Cost |
|--------|----------|----------|------------|----------|-----------|
| P1     | 150×100×80 | PETG   | ~6h        | ~180g    | ~5€       |
| P4     | 200×150×120| PLA    | ~10h       | ~320g    | ~9€       |
| P5     | 180×80×60  | PETG   | ~4h        | ~130g    | ~4€       |
| P7 tray| 220×170×40 | PETG  | ~3h        | ~90g     | ~3€       |
| **Total** |        |          | **~23h**   | **~720g**| **~21€**  |

Filament cost based on ~0.03€/g PETG bulk spool.
Wooden box for P7: ~12€ from hardware store.
