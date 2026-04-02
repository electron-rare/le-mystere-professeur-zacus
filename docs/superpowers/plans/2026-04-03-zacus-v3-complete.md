# Le Mystère du Professeur Zacus V3 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build the complete mobile escape room kit: 7 puzzle objects, adaptive NPC, XTTS voice clone, AudioCraft ambiance, and commercial pipeline.

**Architecture:** ESP-NOW mesh (7 puzzle ESP32 + RTC_PHONE + BOX-3), adaptive NPC engine, XTTS-v2 on KXKM-AI GPU, pre-generated AudioCraft ambiance, 3-suitcase mobile kit.

**Tech Stack:** ESP-IDF, C, FreeRTOS, ESP-NOW, Three.js (BOX-3 UI), XTTS-v2, AudioCraft, Python, PlatformIO, KiCad (enclosures), Astro (landing page)

---

## Overview

| Phase | Scope | Est. Hours | Dependencies |
|-------|-------|------------|--------------|
| 1 | Game Design Finalization | 10h | — |
| 2 | Hardware Prototyping | 40h | Phase 1 |
| 3 | Puzzle Firmware | 30h | Phase 2 |
| 4 | NPC Adaptive Logic | 15h | Phase 1 + existing npc_engine |
| 5 | XTTS-v2 Voice Clone | 8h | KXKM-AI GPU |
| 6 | AudioCraft Ambiance | 4h | KXKM-AI GPU |
| 7 | Kit Packaging | 10h | Phases 2-6 |
| 8 | Beta Playtest | 8h | All |
| 9 | Commercial Pipeline | 15h | Phase 8 |
| **Total** | | **~140h** | |

---

## Phase 1: Game Design Finalization (10h)

### Task 1.1 — Finalize scenario YAML `game/scenarios/zacus_v3.yaml` (3h)

Extend the existing `zacus_v3.yaml` (currently tied to Freenove BOX-3 scenes) to model all 7 V3 puzzles with timing, profiling gates, and adaptive parcours.

**File:** `game/scenarios/zacus_v3_complete.yaml`

```yaml
# game/scenarios/zacus_v3_complete.yaml
# V3 Complete — 7 puzzle objects, adaptive NPC, ESP-NOW mesh
id: zacus_v3_complete
version: "3.1"
title: "Le Mystère du Professeur Zacus — V3 Complete"
theme: "Escape room mobile adaptatif, laboratoire du Professeur"

players:
  min: 4
  max: 15

ages: "8+ ans (corporate et enfants)"

duration:
  target_minutes: 60       # game master configures at boot (30–90)
  min_minutes: 30
  max_minutes: 90
  profiling_minutes: 10
  climax_minutes: 10
  outro_minutes: 5

# ============================================================
# HARDWARE TOPOLOGY
# ============================================================
hardware:
  hub:
    - id: RTC_PHONE
      type: custom_esp32_slic
      role: voice_hub
      comm: esp_now
    - id: BOX_3
      type: esp32s3_box3
      role: orchestrator_display_camera
      comm: esp_now_master

  puzzles:
    - id: P1_SON
      name: "Séquence Sonore"
      type: esp32s3_devkit
      profile: BOTH
      phase: profiling
    - id: P2_CIRCUIT
      name: "Circuit LED"
      type: esp32_devkit
      profile: TECH
      phase: profiling
    - id: P3_QR
      name: "QR Treasure"
      type: box3_camera
      profile: BOTH
      phase: adaptive
    - id: P4_RADIO
      name: "Fréquence Radio"
      type: esp32_devkit
      profile: TECH
      phase: adaptive
    - id: P5_MORSE
      name: "Code Morse"
      type: esp32_devkit
      profile: MIXED
      phase: adaptive
    - id: P6_SYMBOLES
      name: "Symboles Alchimiques"
      type: esp32_devkit
      profile: NON_TECH
      phase: adaptive
    - id: P7_COFFRE
      name: "Coffre Final"
      type: esp32_devkit
      profile: BOTH
      phase: climax

# ============================================================
# SCORING SYSTEM
# ============================================================
scoring:
  base_score: 1000
  time_penalty_per_minute: 10      # deducted after target_minutes
  hint_penalty: 50                 # per hint given
  bonus_fast_completion: 200       # if finished < 80% of target
  bonus_no_hints: 300              # if zero hints used
  bonus_perfect_morse: 100         # P5 decoded on first attempt
  max_score: 1600

# ============================================================
# PHASES
# ============================================================
phases:

  intro:
    id: PHASE_INTRO
    duration_minutes: 5
    trigger: game_start
    npc_audio: "intro/welcome"
    description: >
      Phone rings. Professor introduces himself. Players learn
      the rules through the Professor's monologue.

  profiling:
    id: PHASE_PROFILING
    duration_minutes: 10
    puzzles: [P1_SON, P2_CIRCUIT]
    expected_time_each_seconds: 300
    npc_audio: "profiling/start"
    profiling_thresholds:
      fast_seconds: 180       # < 3 min = fast on this puzzle
      slow_seconds: 480       # > 8 min = slow on this puzzle
    group_classification:
      TECH:
        condition: "P2_fast AND (P1_normal OR P1_slow)"
        parcours: [P2_CIRCUIT, P4_RADIO, P5_MORSE, P3_QR, P7_COFFRE]
      NON_TECH:
        condition: "P1_fast AND (P2_normal OR P2_slow)"
        parcours: [P1_SON, P6_SYMBOLES, P3_QR, P5_MORSE_LIGHT, P7_COFFRE]
      MIXED:
        condition: "default"
        parcours: [P1_SON, P2_CIRCUIT, P3_QR, P5_MORSE, P6_SYMBOLES, P7_COFFRE]

  adaptive:
    id: PHASE_ADAPTIVE
    duration_minutes: 20  # to 60, dynamically adjusted
    fast_threshold_pct: 60
    slow_threshold_pct: 130
    max_bonus_puzzles: 2
    npc_mood_rules:
      - condition: "pct < 60"
        mood: IMPRESSED
        action: add_bonus_puzzle
      - condition: "pct > 130"
        mood: WORRIED
        action: proactive_hint
      - condition: "pct > 160 AND hints_maxed"
        mood: WORRIED
        action: skip_to_next

  climax:
    id: PHASE_CLIMAX
    duration_minutes: 10
    puzzle: P7_COFFRE
    requires_all_codes: true
    description: >
      Final code assembled from all previous puzzle solutions.
      All players must cooperate to enter 8-digit code.
    code_assembly:
      P1_SON: digits 1-2
      P2_CIRCUIT: digit 3
      P4_RADIO: digit 4
      P5_MORSE: digit 5
      P6_SYMBOLES: digits 6-7
      P3_QR: digit 8

  outro:
    id: PHASE_OUTRO
    duration_minutes: 5
    npc_audio: "outro/verdict"
    outputs:
      - diplome_pdf: true
      - stats_display: true      # score, time, hints, rating

# ============================================================
# PUZZLE DEFINITIONS (timing, expected durations)
# ============================================================
puzzles:

  P1_SON:
    id: P1_SON
    name: "Séquence Sonore"
    object: "Boîte avec haut-parleur + 4 boutons arcade"
    interaction: "Écouter la mélodie, reproduire la séquence"
    expected_duration_seconds: 300
    max_hints: 3
    code_contribution: "digits 1-2 (index de la séquence correcte, base-10)"
    difficulty_variants:
      TECH: sequence_length_5
      NON_TECH: sequence_length_3
      MIXED: sequence_length_4
    hint_keys:
      level_1: "hints.P1_SON.level_1"
      level_2: "hints.P1_SON.level_2"
      level_3: "hints.P1_SON.level_3"

  P2_CIRCUIT:
    id: P2_CIRCUIT
    name: "Circuit LED"
    object: "Breadboard magnétique 60×40cm pliable"
    interaction: "Placer les composants pour allumer la LED"
    expected_duration_seconds: 360
    max_hints: 3
    code_contribution: "digit 3 (numéro de la configuration validée, 1-9)"
    difficulty_variants:
      TECH: full_circuit_5_components
      NON_TECH: guided_circuit_3_components
    hint_keys:
      level_1: "hints.P2_CIRCUIT.level_1"
      level_2: "hints.P2_CIRCUIT.level_2"
      level_3: "hints.P2_CIRCUIT.level_3"

  P3_QR:
    id: P3_QR
    name: "QR Treasure"
    object: "6 QR codes plastifiés A5 cachés dans la salle"
    interaction: "Scanner dans l'ordre correct via BOX-3"
    expected_duration_seconds: 240
    max_hints: 3
    code_contribution: "digit 8 (checksum des QR scannés)"
    hint_keys:
      level_1: "hints.P3_QR.level_1"
      level_2: "hints.P3_QR.level_2"
      level_3: "hints.P3_QR.level_3"

  P4_RADIO:
    id: P4_RADIO
    name: "Fréquence Radio"
    object: "Boîtier radio rétro 3D imprimé"
    interaction: "Trouver la bonne fréquence au codeur rotatif"
    expected_duration_seconds: 300
    max_hints: 3
    code_contribution: "digit 4 (fréquence cible encodée sur 1 chiffre)"
    target_frequency_hz: 1337
    frequency_range: [800, 1800]
    tolerance_hz: 10
    hint_keys:
      level_1: "hints.P4_RADIO.level_1"
      level_2: "hints.P4_RADIO.level_2"
      level_3: "hints.P4_RADIO.level_3"

  P5_MORSE:
    id: P5_MORSE
    name: "Code Morse"
    object: "Télégraphe en bois + laiton"
    interaction: "Décoder le message morse du Professeur"
    expected_duration_seconds: 420
    max_hints: 3
    code_contribution: "digit 5 (dernier chiffre du message morse)"
    morse_message: "ZACUS"    # Z=--.. A=.- C=-.-. U=..- S=...
    difficulty_variants:
      TECH: full_morse_decode
      NON_TECH: morse_light_mode   # visual light pulses instead of sound
    hint_keys:
      level_1: "hints.P5_MORSE.level_1"
      level_2: "hints.P5_MORSE.level_2"
      level_3: "hints.P5_MORSE.level_3"

  P6_SYMBOLES:
    id: P6_SYMBOLES
    name: "Symboles Alchimiques"
    object: "Tablette en bois découpée laser 30×20cm"
    interaction: "Placer les 12 symboles NFC dans l'ordre correct"
    expected_duration_seconds: 360
    max_hints: 3
    code_contribution: "digits 6-7 (index de la configuration, 10-99)"
    nfc_tags: 12
    correct_order: [7, 2, 11, 4, 9, 1, 8, 3, 12, 6, 10, 5]
    hint_keys:
      level_1: "hints.P6_SYMBOLES.level_1"
      level_2: "hints.P6_SYMBOLES.level_2"
      level_3: "hints.P6_SYMBOLES.level_3"

  P7_COFFRE:
    id: P7_COFFRE
    name: "Coffre Final"
    object: "Coffre en bois 25×20×15cm avec verrou électronique"
    interaction: "Entrer le code 8 chiffres assemblé des puzzles précédents"
    expected_duration_seconds: 300
    max_hints: 1    # minimal hints for final puzzle
    hint_keys:
      level_1: "hints.P7_COFFRE.level_1"
```

### Task 1.2 — Update `npc_phrases.yaml` with V3 puzzle-specific phrases (3h)

Add sections for all 7 V3 puzzles to the existing phrase bank.

**File:** `game/scenarios/npc_phrases.yaml` — append after existing content:

```yaml
# ============================================================
# V3 PUZZLE-SPECIFIC HINTS
# ============================================================

  P1_SON:
    level_1:
      - text: "La musique a une mémoire, dit-on. Le professeur en est convaincu. Écoutez bien… puis répétez."
        key: "hints.P1_SON.level_1.0"
      - text: "Quatre boutons. Quatre notes. Un seul ordre correct. Votre oreille est votre meilleur instrument."
        key: "hints.P1_SON.level_1.1"
    level_2:
      - text: "La séquence commence toujours par le bouton ROUGE. Le reste suit une logique musicale simple."
        key: "hints.P1_SON.level_2.0"
      - text: "Vous avez entendu la mélodie ? Elle se rejoue si vous attendez 10 secondes. Prenez des notes !"
        key: "hints.P1_SON.level_2.1"
    level_3:
      - text: "Rouge, Bleu, Jaune, Rouge, Vert. Dans cet ordre exact. Appuyez lentement et attendez le bip de confirmation."
        key: "hints.P1_SON.level_3.0"

  P2_CIRCUIT:
    level_1:
      - text: "L'électricité aime les chemins fermés. Si la LED ne brille pas… le circuit n'est pas complet."
        key: "hints.P2_CIRCUIT.level_1.0"
      - text: "Regardez les picots magnétiques. Certains sont ronds, d'autres sont carrés. Ce n'est pas un hasard."
        key: "hints.P2_CIRCUIT.level_1.1"
    level_2:
      - text: "Il faut une résistance entre la pile et la LED. Sans quoi… *explosion dramatique* … enfin, presque."
        key: "hints.P2_CIRCUIT.level_2.0"
      - text: "Le schéma est inscrit sous le panneau en petits caractères. Cherchez bien. J'y ai mis des heures !"
        key: "hints.P2_CIRCUIT.level_2.1"
    level_3:
      - text: "Pile → fil rouge → résistance 220Ω → LED (longue patte vers la résistance) → fil noir → pile. C'est tout !"
        key: "hints.P2_CIRCUIT.level_3.0"

  P3_QR:
    level_1:
      - text: "Six enveloppes. Six secrets. Mais l'ordre… l'ordre est gravé dans les étoiles. Ou dans ce laboratoire."
        key: "hints.P3_QR.level_1.0"
      - text: "Chaque QR code porte un numéro invisible à l'œil nu. Regardez sous chaque carte."
        key: "hints.P3_QR.level_1.1"
    level_2:
      - text: "L'ordre est: numéros croissants des cachettes. La cachette 1 est toujours visible depuis l'entrée."
        key: "hints.P3_QR.level_2.0"
      - text: "Commencez par scanner le QR code collé sur la porte d'entrée. C'est le numéro 1."
        key: "hints.P3_QR.level_2.1"
    level_3:
      - text: "Ordre: porte → étagère gauche → tiroir rouge → sous la table → fenêtre → boîte bleue. Scannez avec BOX-3."
        key: "hints.P3_QR.level_3.0"

  P4_RADIO:
    level_1:
      - text: "Les ondes radio… fascinantes ! Ma fréquence préférée se trouve quelque part entre 800 et 1800 hertz."
        key: "hints.P4_RADIO.level_1.0"
      - text: "Quand vous approchez de la bonne fréquence, un petit indicateur s'allume. Chaud… froid… chaud !"
        key: "hints.P4_RADIO.level_1.1"
    level_2:
      - text: "1337. C'est un nombre que les informaticiens connaissent bien. Un nombre légendaire dans les cercles geek."
        key: "hints.P4_RADIO.level_2.0"
      - text: "La fréquence est un nombre à 4 chiffres. Commençant par 1. Très précis. Tournez doucement."
        key: "hints.P4_RADIO.level_2.1"
    level_3:
      - text: "1337 hertz ! LEET en langage de hacker. Tournez jusqu'à ce que l'écran affiche 1337. Exactement."
        key: "hints.P4_RADIO.level_3.0"

  P5_MORSE:
    level_1:
      - text: "Le code morse… inventé en 1836 ! Long-court-long. Tiret-point-tiret. Écoutez le rythme."
        key: "hints.P5_MORSE.level_1.0"
      - text: "Vous avez une fiche de référence quelque part dans cette salle. Cherchez-la. Je l'y ai laissée exprès."
        key: "hints.P5_MORSE.level_1.1"
    level_2:
      - text: "Le message contient cinq lettres. C'est mon nom… enfin, mon nom de code. Commençant par Z."
        key: "hints.P5_MORSE.level_2.0"
      - text: "Z=--.. A=.- C=-.-. U=..- S=... Voilà l'alphabet morse pour ces lettres précises."
        key: "hints.P5_MORSE.level_2.1"
    level_3:
      - text: "ZACUS en morse: TIRET-TIRET-POINT-POINT  POINT-TIRET  TIRET-POINT-TIRET-POINT  POINT-POINT-TIRET  POINT-POINT-POINT. Tapez sur le télégraphe !"
        key: "hints.P5_MORSE.level_3.0"

  P6_SYMBOLES:
    level_1:
      - text: "L'alchimie obéit à une logique ancienne. Terre, Eau, Feu, Air… puis les métaux. Il y a un ordre naturel."
        key: "hints.P6_SYMBOLES.level_1.0"
      - text: "Chaque symbole a un poids. Les légers en haut, les lourds en bas. Comme dans la nature."
        key: "hints.P6_SYMBOLES.level_1.1"
    level_2:
      - text: "L'ordre suit la table alchimique classique. Elle est affichée dans le couloir sous un cadre doré."
        key: "hints.P6_SYMBOLES.level_2.0"
      - text: "Commencez par placer les quatre éléments primordiaux dans les coins. Puis remplissez le centre."
        key: "hints.P6_SYMBOLES.level_2.1"
    level_3:
      - text: "Feu (coin haut-gauche) → Eau → Terre → Air → Or → Argent → Mercure → Soufre → Sel → Antimoine → Arsenic → Plomb. De gauche à droite, haut en bas."
        key: "hints.P6_SYMBOLES.level_3.0"

  P7_COFFRE:
    level_1:
      - text: "Vous avez tous les fragments ? Chaque puzzle vous a donné un ou deux chiffres. Assemblez-les dans l'ordre des puzzles."
        key: "hints.P7_COFFRE.level_1.0"

# ============================================================
# V3 ADAPTATION PHRASES (profiling + dynamic parcours)
# ============================================================
adaptation:
  group_tech_detected:
    - text: "Hmm. Groupe technique détecté ! Excellente nouvelle. Je vais sortir mes puzzles… avancés."
      key: "adaptation.group_tech_detected.0"
    - text: "Vous avez résolu le circuit en un temps record. Intéressant ! Passons aux choses sérieuses."
      key: "adaptation.group_tech_detected.1"

  group_non_tech_detected:
    - text: "Ah ! Des artistes, des créatifs. Parfait. J'ai préparé des énigmes qui correspondent à votre sensibilité."
      key: "adaptation.group_non_tech_detected.0"
    - text: "Le circuit n'était pas votre fort ? Pas grave ! La logique prend mille formes. Voici la vôtre."
      key: "adaptation.group_non_tech_detected.1"

  group_mixed_detected:
    - text: "Groupe éclectique ! Moitié techniciens, moitié artistes. Mon scénario préféré. Tout le monde travaille !"
      key: "adaptation.group_mixed_detected.0"

  bonus_puzzle_added:
    - text: "Impressionnant ! Vous allez trop vite. J'ai décidé d'ajouter… une petite surprise."
      key: "adaptation.bonus_puzzle_added.0"
    - text: "Trop facile ? Très bien. Voici un défi supplémentaire que je réserve aux meilleurs groupes."
      key: "adaptation.bonus_puzzle_added.1"

  puzzle_skipped:
    - text: "Hmm. Le temps passe. Je vais vous faire grâce de cette épreuve. Pour aujourd'hui."
      key: "adaptation.puzzle_skipped.0"
    - text: "Je saute une étape. Non par pitié — mais parce que le temps est une expérience en soi."
      key: "adaptation.puzzle_skipped.1"

  duration_warning:
    - text: "Attention ! Vous avez consommé 80% du temps imparti. Le Professeur s'impatiente légèrement."
      key: "adaptation.duration_warning.0"

# ============================================================
# V3 AMBIANCE (intro / outro)
# ============================================================
ambiance:
  intro:
    - text: "Bienvenue dans mon laboratoire… portable. Aujourd'hui, c'est VOUS qui êtes l'expérience."
      key: "ambiance.intro.0"
    - text: "Je suis le Professeur Zacus. Scientifique, inventeur, et… testeur de groupes humains depuis 1987."
      key: "ambiance.intro.1"
    - text: "Vous avez soixante minutes. Soixante petites minutes pour prouver que votre espèce mérite de continuer."
      key: "ambiance.intro.2"

  outro_success:
    - text: "Extraordinaire ! Vous avez passé mon test. Je dois admettre… je suis impressionné. Légèrement."
      key: "ambiance.outro_success.0"
    - text: "Félicitations ! Le diplôme du Professeur Zacus vous est accordé. Avec mention… passable."
      key: "ambiance.outro_success.1"

  outro_partial:
    - text: "Hmm. Vous n'avez pas tout résolu. Mais vous avez essayé. C'est… acceptable. Pour un humain."
      key: "ambiance.outro_partial.0"

  outro_timeout:
    - text: "Le temps est écoulé ! L'expérience se termine. Ne vous inquiétez pas — j'ai noté tout ce que vous avez fait."
      key: "ambiance.outro_timeout.0"
```

### Task 1.3 — Define scoring and timing constants (1h)

**File:** `game/config/v3_constants.yaml`

```yaml
# game/config/v3_constants.yaml
# V3 runtime constants for NPC engine and scoring
npc:
  fast_threshold_pct: 60
  slow_threshold_pct: 130
  stuck_timeout_seconds: 120
  proactive_hint_interval_seconds: 90
  max_hint_level: 3
  duration_warning_pct: 80

scoring:
  base: 1000
  hint_penalty: 50
  time_penalty_per_minute: 10
  bonus_fast: 200
  bonus_no_hints: 300
  bonus_perfect_morse: 100

profiling:
  fast_seconds: 180
  slow_seconds: 480

puzzles:
  P1_SON:    { expected_s: 300, max_hints: 3, phase: profiling }
  P2_CIRCUIT: { expected_s: 360, max_hints: 3, phase: profiling }
  P3_QR:     { expected_s: 240, max_hints: 3, phase: adaptive }
  P4_RADIO:  { expected_s: 300, max_hints: 3, phase: adaptive }
  P5_MORSE:  { expected_s: 420, max_hints: 3, phase: adaptive }
  P6_SYMBOLES: { expected_s: 360, max_hints: 3, phase: adaptive }
  P7_COFFRE: { expected_s: 300, max_hints: 1, phase: climax }
```

### Task 1.4 — Create V3 compile test (3h)

Extend `tools/scenario/compile_runtime3.py` to accept `zacus_v3_complete.yaml` and emit a V3 IR JSON that the adaptive engine can consume at runtime.

**File:** `tools/scenario/test_v3_compile.py`

```python
#!/usr/bin/env python3
"""Smoke-test: compile zacus_v3_complete.yaml and verify IR structure."""
import unittest
import json
import subprocess
import pathlib

SCENARIO = pathlib.Path("game/scenarios/zacus_v3_complete.yaml")
EXPECTED_PUZZLES = {"P1_SON", "P2_CIRCUIT", "P3_QR", "P4_RADIO",
                    "P5_MORSE", "P6_SYMBOLES", "P7_COFFRE"}

class TestV3Compile(unittest.TestCase):
    def setUp(self):
        result = subprocess.run(
            ["python3", "tools/scenario/compile_runtime3.py",
             str(SCENARIO), "--output", "/tmp/v3_ir.json"],
            capture_output=True, text=True
        )
        self.returncode = result.returncode
        self.stderr = result.stderr
        if result.returncode == 0:
            with open("/tmp/v3_ir.json") as f:
                self.ir = json.load(f)

    def test_compile_succeeds(self):
        self.assertEqual(self.returncode, 0, self.stderr)

    def test_all_puzzles_present(self):
        puzzle_ids = {p["id"] for p in self.ir.get("puzzles", [])}
        self.assertEqual(puzzle_ids, EXPECTED_PUZZLES)

    def test_phases_defined(self):
        phases = {p["id"] for p in self.ir.get("phases", [])}
        self.assertIn("PHASE_PROFILING", phases)
        self.assertIn("PHASE_ADAPTIVE", phases)
        self.assertIn("PHASE_CLIMAX", phases)

    def test_scoring_present(self):
        self.assertIn("scoring", self.ir)
        self.assertEqual(self.ir["scoring"]["base_score"], 1000)

if __name__ == "__main__":
    unittest.main()
```

---

## Phase 2: Hardware Prototyping (40h)

### Task 2.1 — KiCad schematics for 7 puzzle boards (20h)

Each puzzle uses a minimal ESP32 circuit. All schematics live in `hardware/kicad/`.

**Common base circuit** (replicated for P1, P2, P4, P5, P6, P7):

```
ESP32-DevKit-C (38-pin)
  3V3 → 100nF bypass cap → VCC rail
  GND → GND rail
  EN → 10kΩ pull-up → 3V3
  GPIO0 → 10kΩ pull-up → 3V3 (boot mode)
  USB-C connector (HY2.0 5V) → AMS1117-3.3 LDO → 3V3 rail
  USB data (D+/D−) → CH340C USB-UART for flashing
```

**P1 Boîte Sonore** (`hardware/kicad/p1_son/p1_son.kicad_sch`):

```
ESP32-S3-DevKit-N16R8
  GPIO4  → MAX98357A I2S BCLK
  GPIO5  → MAX98357A I2S LRCLK
  GPIO6  → MAX98357A I2S DIN
  GPIO7  → MAX98357A SD_MODE (HIGH = stereo)
  MAX98357A VOUT → 8Ω 3W speaker
  GPIO15 → 10kΩ pull-up → Arcade Button RED (NO, GND)
  GPIO16 → 10kΩ pull-up → Arcade Button BLUE (NO, GND)
  GPIO17 → 10kΩ pull-up → Arcade Button YELLOW (NO, GND)
  GPIO18 → 10kΩ pull-up → Arcade Button GREEN (NO, GND)
  GPIO8  → WS2812B LED strip DIN (4 LEDs) [logic level shifter 3V3→5V via SN74HCT125]
  Power: USB-C 5V → AMS1117-3.3 for ESP32; 5V direct for MAX98357A and LEDs
```

**P2 Breadboard Magnétique** (`hardware/kicad/p2_circuit/p2_circuit.kicad_sch`):

```
ESP32-DevKit-C
  GPIO4..GPIO11 → 8× Reed switches (normally-open magnetic sensors, 10kΩ pull-up each)
  GPIO12..GPIO19 → 8× WS2812B LED strip segments (via 74HCT125)
  GPIO21 → Buzzer (active 5V, via NPN 2N2222, 1kΩ base resistor)
  GPIO22 → "Circuit valid" green LED (470Ω series resistor)
  Power: USB-C 5V → AMS1117-3.3 for ESP32; 5V for LEDs via 1000µF bulk cap
```

**P4 Poste Radio** (`hardware/kicad/p4_radio/p4_radio.kicad_sch`):

```
ESP32-DevKit-C
  GPIO25 → DAC output → RC low-pass (10kΩ + 100nF) → MAX98357A analog input
  GPIO14 → Rotary encoder CLK (10kΩ pull-up)
  GPIO12 → Rotary encoder DT  (10kΩ pull-up)
  GPIO13 → Rotary encoder SW  (10kΩ pull-up) [push button]
  GPIO21/22 → SSD1306 OLED 128x64 I2C (SDA/SCL, 4.7kΩ pull-up each)
  GPIO4/5/6 → MAX98357A I2S (BCLK/LRCLK/DIN) for synthesized radio static
  Power: 18650 Li-Ion (2S) + TP4056 + MT3608 boost 5V → AMS1117-3.3
```

**P5 Télégraphe** (`hardware/kicad/p5_morse/p5_morse.kicad_sch`):

```
ESP32-DevKit-C
  GPIO4  → Telegraph key (brass button, NO, 10kΩ pull-up)
  GPIO5  → Buzzer (active 5V, NPN 2N2222, 1kΩ base)
  GPIO6  → Red LED (470Ω) — key pressed indicator
  GPIO7  → Green LED (470Ω) — message valid indicator
  GPIO8  → WS2812B (1 LED, morse light mode for NON_TECH)
  Power: USB-C 5V → AMS1117-3.3
```

**P6 Tablette Symboles** (`hardware/kicad/p6_symboles/p6_symboles.kicad_sch`):

```
ESP32-DevKit-C
  GPIO21/22 → MFRC522 RC522 NFC SDA/SCK I2C (4.7kΩ pull-up)
    [Note: use SPI mode: GPIO5=SDA, GPIO18=SCK, GPIO19=MISO, GPIO23=MOSI, GPIO4=RST]
  GPIO25 → Buzzer (NPN 2N2222, 1kΩ)
  GPIO26 → Green LED "configuration valid" (470Ω)
  GPIO27 → Red LED "wrong placement" (470Ω)
  12× NTAG213 NFC tags (passive, mounted on wooden symbol pieces)
  Power: USB-C 5V → AMS1117-3.3
```

**P7 Coffre Final** (`hardware/kicad/p7_coffre/p7_coffre.kicad_sch`):

```
ESP32-DevKit-C
  GPIO4..GPIO7  → 4×3 membrane keypad rows (4 pins)
  GPIO12..GPIO14 → 4×3 membrane keypad columns (3 pins, 10kΩ pull-up)
  GPIO25 → SG90 servo signal (PWM, 50Hz)
  GPIO21/22 → SSD1306 OLED 128x32 I2C (entered digits display)
  GPIO5  → WS2812B RGB LED (1 LED, status: idle/entering/error/open)
  GPIO18 → Buzzer (NPN, 1kΩ)
  Power: USB-C 5V → AMS1117-3.3 for ESP32; 5V direct for servo
```

### Task 2.2 — BOM with exact part numbers (8h)

**File:** `hardware/bom/zacus_v3_bom.csv`

```csv
Reference,Quantity,Description,Manufacturer,Part Number,Supplier,Unit Price EUR,Total EUR,Notes
MCU_P1,1,ESP32-S3-DevKitC-1-N16R8,Espressif,ESP32-S3-DevKitC-1-N16R8,Mouser 356-ESP32S3DEVKITCN,12.00,12.00,P1 Sonore
MCU_P2_P4_P5_P6_P7,5,ESP32-DevKitC-32E,Espressif,ESP32-DevKitC-32E,LCSC C3406749,6.50,32.50,P2/P4/P5/P6/P7
U1,7,AMS1117-3.3 LDO,AMS,AMS1117-3.3,LCSC C6186,0.08,0.56,3V3 regulator
U2,2,MAX98357A I2S Amp,Maxim,MAX98357AEWL+T,Mouser 700-MAX98357AEWLT,2.50,5.00,P1+P4 audio
U3,1,SN74HCT125 Level Shifter,TI,SN74HCT125N,LCSC C5616,0.25,0.25,WS2812B drive
U4,1,CH340C USB-UART,WCH,CH340C,LCSC C84681,0.35,2.45,7x devkits integrated
U5,1,MFRC522 NFC module,NXP,MFRC522,LCSC C411208,1.20,1.20,P6 symboles
U6,1,TP4056 charger,TPUNIQ,TP4056,LCSC C16581,0.12,0.12,P4 radio battery
U7,1,MT3608 boost converter,Aerosemi,MT3608,LCSC C84817,0.18,0.18,P4 radio 5V
DISP1,2,SSD1306 OLED 128x64,Generic,0.96in I2C OLED,LCSC C125770,1.80,3.60,P4+P7
LED1,3,WS2812B LED strip 1m,WorldSemi,WS2812B-2020,LCSC C2761795,3.50,10.50,P1+P2+P5+P7
SW1,4,Arcade button 30mm,Sanwa,OBSF-30,Mouser 693-OBSF-30,2.80,11.20,P1 rouge/bleu/jaune/vert
SW2,8,Reed switch NC,Standex,MK04,Mouser 876-MK04,0.45,3.60,P2 magnetic
SW3,1,Rotary encoder KY-040,Generic,KY-040,LCSC C141881,0.60,0.60,P4 radio
SW4,1,Membrane keypad 4x3,Generic,4x3 matrix keypad,LCSC C2935585,1.20,1.20,P7 coffre
SW5,1,Telegraph brass key,Generic,morse key,Aliexpress custom,3.50,3.50,P5 morse
SERVO1,1,SG90 servo,Tower Pro,SG90,LCSC C72037,1.20,1.20,P7 coffre latch
BZ1,5,Active buzzer 5V,Generic,TMB12A05,LCSC C96093,0.15,0.75,P2/P4/P5/P6/P7
BAT1,3,20000mAh USB-C battery,Anker,A1287,Amazon,25.00,75.00,Hub + puzzles
BAT2,2,18650 Li-Ion 3000mAh,Samsung,INR18650-30Q,Mouser 522-INR18650-30Q,5.50,11.00,P4 radio (2S)
NFC1,12,NTAG213 NFC stickers,NXP,NTAG213,LCSC C7499,0.18,2.16,P6 symbols
CONN1,15,USB-C breakout 2A,Generic,USB-C power connector,LCSC C167688,0.35,5.25,All puzzles
CABLE1,10,USB-C cable 1m,Ugreen,60126,Amazon,3.00,30.00,Power cables
PCB1,7,Custom PCB (10×10cm),JLCPCB,2-layer 1.6mm,JLCPCB,2.00,14.00,Per puzzle ($2/5pcs)
TOTAL,,,,,,,,~207 EUR (puzzles electronics only)
```

**Full kit BOM summary:**

| Category | Cost |
|----------|------|
| Puzzle electronics | 207€ |
| Enclosures (3D print + laser cut + wood) | 130€ |
| Hub (BOX-3 + RTC_PHONE + router) | 95€ |
| Power (3× 20000mAh batteries) | 75€ |
| Infrastructure (speaker BT, LED strip, valises, signage) | 135€ |
| **Total** | **~642€** |

### Task 2.3 — 3D enclosure STL files (12h)

All enclosure files live in `hardware/enclosures/`. Design in FreeCAD or OpenSCAD.

**File:** `hardware/enclosures/p1_son_box.scad`

```openscad
// P1 Boîte Sonore — 150x100x80mm
// Speaker front panel + 4 arcade button holes + ESP32 access slot

$fn = 64;
wall = 3;
w = 150; d = 100; h = 80;

module box_shell() {
    difference() {
        cube([w, d, h]);
        translate([wall, wall, wall])
            cube([w - 2*wall, d - 2*wall, h - wall + 1]);
        // Front panel cutouts
        // Speaker grille (40x40mm centered at x=75, y=50, z=60)
        translate([55, 30, 55])
            cube([40, 40, wall + 2]);
        // Arcade buttons (30mm diameter holes)
        for (x = [20, 60, 90, 130]) {
            translate([x, 75, -1])
                cylinder(h = wall + 2, r = 15);
        }
        // USB-C slot (bottom right)
        translate([130, -1, 10])
            cube([12, wall + 2, 6]);
    }
}

module lid() {
    translate([0, 0, h - wall])
        difference() {
            cube([w, d, wall]);
            // Vent holes 4x1mm grid
            for (x = [20:10:130], y = [20:10:80])
                translate([x, y, -1]) cube([4, 1, wall+2]);
        }
}

box_shell();
// Uncomment to render lid separately:
// lid();
```

**File:** `hardware/enclosures/p4_radio_box.scad`

```openscad
// P4 Poste Radio Rétro — 200x150x120mm
// Retro radio aesthetic: rounded front, dial cutout, OLED window, speaker grille

$fn = 64;
wall = 4;
w = 200; d = 150; h = 120;
corner_r = 15;

module rounded_box() {
    hull() {
        for (x = [corner_r, w - corner_r],
             y = [corner_r, d - corner_r]) {
            translate([x, y, 0]) cylinder(r = corner_r, h = h);
        }
    }
}

module radio_shell() {
    difference() {
        rounded_box();
        // Interior
        translate([wall, wall, wall])
            hull() {
                for (x = [corner_r, w - 2*wall - corner_r],
                     y = [corner_r, d - 2*wall - corner_r]) {
                    translate([x, y, 0])
                        cylinder(r = corner_r - wall, h = h);
                }
            }
        // Rotary encoder hole (30mm from right, centered vertically at y=75)
        translate([160, 75, -1]) cylinder(r = 7, h = wall + 2);
        // OLED window 35x18mm
        translate([60, 65, -1]) cube([35, 18, wall + 2]);
        // Speaker grille (circular, 80mm diameter, centered at x=100, y=100)
        for (angle = [0:30:330], r = [10, 25, 40])
            translate([100 + r*cos(angle), 100 + r*sin(angle), -1])
                cylinder(r = 2, h = wall + 2);
        // USB-C slot bottom
        translate([95, -1, 15]) cube([12, wall+2, 6]);
    }
}

radio_shell();
```

---

## Phase 3: Puzzle Firmware (30h)

### Task 3.1 — Common ESP-NOW framework (6h)

All puzzle ESP32s share the same ESP-NOW slave framework.

**File:** `ESP32_ZACUS/puzzles/common/espnow_slave.h`

```c
// espnow_slave.h — Common ESP-NOW slave for all V3 puzzle nodes
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Message types (must match BOX-3 master definitions)
typedef enum {
    MSG_PUZZLE_SOLVED   = 0x01,   // puzzle → master: puzzle complete, code fragment attached
    MSG_PUZZLE_RESET    = 0x02,   // master → puzzle: reset to initial state
    MSG_PUZZLE_CONFIG   = 0x03,   // master → puzzle: set difficulty variant
    MSG_HINT_REQUEST    = 0x04,   // puzzle → master: player requested hint (phone hook)
    MSG_STATUS_POLL     = 0x05,   // master → puzzle: request status update
    MSG_STATUS_REPLY    = 0x06,   // puzzle → master: current state, elapsed time
    MSG_LED_COMMAND     = 0x07,   // master → puzzle: set LED color/pattern
    MSG_HEARTBEAT       = 0x08,   // both directions: keep-alive every 5s
} espnow_msg_type_t;

// Difficulty variants
typedef enum {
    DIFFICULTY_EASY   = 0,
    DIFFICULTY_NORMAL = 1,
    DIFFICULTY_HARD   = 2,
} puzzle_difficulty_t;

// Status flags
typedef struct {
    uint8_t  puzzle_id;         // P1..P7
    uint8_t  state;             // 0=idle, 1=active, 2=solved, 3=failed
    uint8_t  attempts;          // number of wrong attempts
    uint8_t  hints_used;        // hints consumed
    uint32_t elapsed_ms;        // time since puzzle activated
    uint8_t  code_fragment[4];  // code digits contributed by this puzzle
    uint8_t  difficulty;        // current difficulty variant
} puzzle_status_t;

// Payload for MSG_PUZZLE_SOLVED
typedef struct {
    uint8_t puzzle_id;
    uint8_t code_fragment[4];
    uint32_t solve_time_ms;
} puzzle_solved_payload_t;

// Initialize ESP-NOW in slave mode.
// master_mac: MAC address of BOX-3 (master). Can be broadcast FF:FF:FF:FF:FF:FF.
// puzzle_id: P1..P7 (1-7)
esp_err_t espnow_slave_init(const uint8_t master_mac[6], uint8_t puzzle_id);

// Send MSG_PUZZLE_SOLVED with code fragment.
esp_err_t espnow_slave_notify_solved(const uint8_t code_fragment[4],
                                     uint32_t solve_time_ms);

// Send MSG_HINT_REQUEST.
esp_err_t espnow_slave_request_hint(void);

// Send MSG_STATUS_REPLY (call from status poll handler).
esp_err_t espnow_slave_send_status(const puzzle_status_t* status);

// Register callback for incoming master commands.
typedef void (*espnow_cmd_callback_t)(espnow_msg_type_t type,
                                      const uint8_t* payload,
                                      size_t len);
void espnow_slave_register_callback(espnow_cmd_callback_t cb);

// Call from FreeRTOS task — processes inbound queue.
void espnow_slave_process(void);
```

**File:** `ESP32_ZACUS/puzzles/common/espnow_slave.c`

```c
// espnow_slave.c — ESP-NOW slave implementation
#include "espnow_slave.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>

static const char* TAG = "espnow_slave";
#define RECV_QUEUE_LEN 8

typedef struct {
    uint8_t mac[6];
    uint8_t data[250];
    int     data_len;
} recv_item_t;

static QueueHandle_t s_recv_queue;
static uint8_t s_master_mac[6];
static uint8_t s_puzzle_id;
static espnow_cmd_callback_t s_callback;

static void on_recv(const esp_now_recv_info_t* info,
                    const uint8_t* data, int len) {
    recv_item_t item;
    memcpy(item.mac, info->src_addr, 6);
    int copy_len = len < (int)sizeof(item.data) ? len : (int)sizeof(item.data);
    memcpy(item.data, data, copy_len);
    item.data_len = copy_len;
    xQueueSendFromISR(s_recv_queue, &item, NULL);
}

static void on_sent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Send failed to " MACSTR, MAC2STR(mac));
    }
}

esp_err_t espnow_slave_init(const uint8_t master_mac[6], uint8_t puzzle_id) {
    s_puzzle_id = puzzle_id;
    memcpy(s_master_mac, master_mac, 6);

    s_recv_queue = xQueueCreate(RECV_QUEUE_LEN, sizeof(recv_item_t));
    if (!s_recv_queue) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_sent));

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, master_mac, 6);
    peer.channel = 0;  // current channel
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "Puzzle P%d slave ready, master=" MACSTR,
             puzzle_id, MAC2STR(master_mac));
    return ESP_OK;
}

esp_err_t espnow_slave_notify_solved(const uint8_t code_fragment[4],
                                     uint32_t solve_time_ms) {
    uint8_t buf[6];
    buf[0] = MSG_PUZZLE_SOLVED;
    buf[1] = s_puzzle_id;
    memcpy(buf + 2, code_fragment, 4);
    // Note: solve_time_ms sent in next STATUS_REPLY for simplicity
    (void)solve_time_ms;
    return esp_now_send(s_master_mac, buf, sizeof(buf));
}

esp_err_t espnow_slave_request_hint(void) {
    uint8_t buf[2] = { MSG_HINT_REQUEST, s_puzzle_id };
    return esp_now_send(s_master_mac, buf, sizeof(buf));
}

esp_err_t espnow_slave_send_status(const puzzle_status_t* status) {
    uint8_t buf[1 + sizeof(puzzle_status_t)];
    buf[0] = MSG_STATUS_REPLY;
    memcpy(buf + 1, status, sizeof(puzzle_status_t));
    return esp_now_send(s_master_mac, buf, sizeof(buf));
}

void espnow_slave_register_callback(espnow_cmd_callback_t cb) {
    s_callback = cb;
}

void espnow_slave_process(void) {
    recv_item_t item;
    while (xQueueReceive(s_recv_queue, &item, 0) == pdTRUE) {
        if (item.data_len < 1) continue;
        espnow_msg_type_t type = (espnow_msg_type_t)item.data[0];
        if (s_callback) {
            s_callback(type, item.data + 1, item.data_len - 1);
        }
    }
}
```

### Task 3.2 — P1 Séquence Sonore firmware (4h)

**File:** `ESP32_ZACUS/puzzles/p1_son/main/p1_son_main.c`

```c
// p1_son_main.c — P1 Séquence Sonore puzzle firmware
// ESP32-S3 + MAX98357A I2S + 4 arcade buttons + WS2812B LEDs
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "led_strip.h"
#include "espnow_slave.h"
#include <string.h>

static const char* TAG = "P1_SON";

// GPIO mapping
#define GPIO_BTN_RED    15
#define GPIO_BTN_BLUE   16
#define GPIO_BTN_YELLOW 17
#define GPIO_BTN_GREEN  18
#define GPIO_LED_DATA   8
#define I2S_BCLK        4
#define I2S_LRCLK       5
#define I2S_DIN         6
#define LED_COUNT       4

// Sequence configuration (set by master via MSG_PUZZLE_CONFIG)
static uint8_t s_target_seq[8];
static uint8_t s_seq_len = 4;   // default MIXED difficulty
static uint8_t s_player_seq[8];
static uint8_t s_player_pos = 0;
static bool    s_solved = false;
static uint8_t s_attempts = 0;
static uint32_t s_start_ms = 0;

// LED strip handle
static led_strip_handle_t s_leds;

// Button colors: 0=RED, 1=BLUE, 2=YELLOW, 3=GREEN
static const uint32_t kButtonColors[4][3] = {
    {255, 0,   0  },  // RED
    {0,   0,   255},  // BLUE
    {255, 255, 0  },  // YELLOW
    {0,   255, 0  },  // GREEN
};

static void led_set(uint8_t idx, uint32_t r, uint32_t g, uint32_t b) {
    led_strip_set_pixel(s_leds, idx, r, g, b);
    led_strip_refresh(s_leds);
}

static void play_tone(uint8_t button_idx, uint32_t duration_ms) {
    // Simple sine wave at button frequencies via I2S
    // Frequencies: RED=262Hz (C4), BLUE=330Hz (E4), YELLOW=392Hz (G4), GREEN=523Hz (C5)
    static const uint32_t kFreqs[4] = {262, 330, 392, 523};
    uint32_t freq = kFreqs[button_idx];
    // In production: generate PCM samples into DMA buffer
    // Simplified: use pre-generated sine table from flash
    ESP_LOGI(TAG, "Play tone %u Hz for %lu ms", freq, duration_ms);
    led_set(button_idx,
            kButtonColors[button_idx][0],
            kButtonColors[button_idx][1],
            kButtonColors[button_idx][2]);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    led_set(button_idx, 0, 0, 0);
}

static void play_sequence(void) {
    ESP_LOGI(TAG, "Playing target sequence (len=%d)", s_seq_len);
    for (uint8_t i = 0; i < s_seq_len; i++) {
        play_tone(s_target_seq[i], 500);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void check_sequence(void) {
    bool correct = (memcmp(s_player_seq, s_target_seq, s_seq_len) == 0);
    if (correct) {
        s_solved = true;
        // Flash all LEDs green 3 times
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < LED_COUNT; j++)
                led_set(j, 0, 255, 0);
            vTaskDelay(pdMS_TO_TICKS(300));
            for (int j = 0; j < LED_COUNT; j++)
                led_set(j, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        // Code fragment: two-digit index of sequence (e.g., seq [0,1,3,2] → code "04")
        uint8_t code[4] = {0, 0, 0, 0};
        // Encode: XOR of sequence elements mod 99
        uint8_t val = 0;
        for (int i = 0; i < s_seq_len; i++) val ^= (s_target_seq[i] * (i + 1));
        code[0] = (val / 10) % 10;
        code[1] = val % 10;
        uint32_t elapsed = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - s_start_ms;
        espnow_slave_notify_solved(code, elapsed);
        ESP_LOGI(TAG, "Solved! Code=%d%d", code[0], code[1]);
    } else {
        s_attempts++;
        // Flash all red
        for (int j = 0; j < LED_COUNT; j++) led_set(j, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        for (int j = 0; j < LED_COUNT; j++) led_set(j, 0, 0, 0);
        // Replay sequence after wrong attempt
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_player_pos = 0;
        play_sequence();
    }
}

static void espnow_cmd_handler(espnow_msg_type_t type,
                                const uint8_t* payload, size_t len) {
    if (type == MSG_PUZZLE_RESET) {
        s_player_pos = 0;
        s_solved = false;
        s_attempts = 0;
        s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        play_sequence();
        ESP_LOGI(TAG, "Reset");
    } else if (type == MSG_PUZZLE_CONFIG && len >= 1) {
        puzzle_difficulty_t diff = (puzzle_difficulty_t)payload[0];
        if (diff == DIFFICULTY_EASY)   s_seq_len = 3;
        if (diff == DIFFICULTY_NORMAL) s_seq_len = 4;
        if (diff == DIFFICULTY_HARD)   s_seq_len = 5;
        // Re-generate random sequence of given length
        for (int i = 0; i < s_seq_len; i++)
            s_target_seq[i] = esp_random() % 4;
        ESP_LOGI(TAG, "Config: diff=%d len=%d", diff, s_seq_len);
    }
}

static void button_task(void* arg) {
    static const uint8_t kBtnGpios[4] = {
        GPIO_BTN_RED, GPIO_BTN_BLUE, GPIO_BTN_YELLOW, GPIO_BTN_GREEN
    };
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_BTN_RED) | (1ULL << GPIO_BTN_BLUE)
                      | (1ULL << GPIO_BTN_YELLOW) | (1ULL << GPIO_BTN_GREEN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    uint8_t last_state[4] = {1, 1, 1, 1};
    for (;;) {
        espnow_slave_process();
        if (s_solved) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        for (int i = 0; i < 4; i++) {
            uint8_t cur = gpio_get_level(kBtnGpios[i]);
            if (last_state[i] == 1 && cur == 0) {
                // Button pressed
                play_tone(i, 300);
                if (s_player_pos < s_seq_len) {
                    s_player_seq[s_player_pos++] = i;
                    if (s_player_pos == s_seq_len) {
                        check_sequence();
                        s_player_pos = 0;
                    }
                }
            }
            last_state[i] = cur;
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz polling
    }
}

void app_main(void) {
    // Initialize Wi-Fi (required for ESP-NOW)
    // ... (standard wifi_init_sta with WIFI_MODE_STA, no connection needed)

    // Generate default sequence
    for (int i = 0; i < s_seq_len; i++)
        s_target_seq[i] = i % 4;  // overridden by master config

    // LED strip
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = GPIO_LED_DATA,
        .max_leds = LED_COUNT,
    };
    led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10000000 };
    led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_leds);

    // ESP-NOW
    static const uint8_t kMasterMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; // broadcast
    espnow_slave_init(kMasterMac, 1 /* P1 */);
    espnow_slave_register_callback(espnow_cmd_handler);

    s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    // Play intro sequence
    vTaskDelay(pdMS_TO_TICKS(2000));
    play_sequence();

    xTaskCreate(button_task, "buttons", 4096, NULL, 5, NULL);
}
```

### Task 3.3 — P5 Code Morse firmware (4h)

**File:** `ESP32_ZACUS/puzzles/p5_morse/main/p5_morse_main.c`

```c
// p5_morse_main.c — P5 Code Morse puzzle firmware
// ESP32-DevKit + telegraph key + buzzer + LEDs
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "espnow_slave.h"
#include <string.h>
#include <stdbool.h>

static const char* TAG = "P5_MORSE";

#define GPIO_KEY    4   // telegraph key (LOW when pressed)
#define GPIO_BUZZER 5
#define GPIO_LED_R  6
#define GPIO_LED_G  7
#define GPIO_LED_W  8   // light mode for NON_TECH

// Morse timing (ms)
#define DOT_MIN_MS    50
#define DOT_MAX_MS    300
#define DASH_MIN_MS   300
#define DASH_MAX_MS   1200
#define LETTER_GAP_MS 700   // silence between letters
#define WORD_GAP_MS   1500

// Target message: "ZACUS"
// Z=--.. A=.- C=-.-. U=..- S=...
static const char* kTargetWord = "ZACUS";

// Morse alphabet (A=0, B=1, ... Z=25)
static const char* kMorseTable[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",
    "....", "..",   ".---", "-.-",  ".-..", "--",   "-.",
    "---",  ".--.", "--.-", ".-.",  "...",  "-",    "..-",
    "...-", ".--",  "-..-", "-.--", "--.."
};

static char s_received[16];
static uint8_t s_recv_pos = 0;
static char s_current_symbol[8];
static uint8_t s_sym_pos = 0;
static bool s_solved = false;
static uint32_t s_start_ms = 0;
static bool s_light_mode = false;  // NON_TECH: visual pulses instead of sound

static uint32_t now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void buzzer_on(void)  { gpio_set_level(GPIO_BUZZER, 1); }
static void buzzer_off(void) { gpio_set_level(GPIO_BUZZER, 0); }

static void decode_symbol(void) {
    if (s_sym_pos == 0) return;
    s_current_symbol[s_sym_pos] = '\0';
    // Find letter
    for (int i = 0; i < 26; i++) {
        if (strcmp(kMorseTable[i], s_current_symbol) == 0) {
            char letter = 'A' + i;
            if (s_recv_pos < sizeof(s_received) - 1) {
                s_received[s_recv_pos++] = letter;
                s_received[s_recv_pos] = '\0';
            }
            ESP_LOGI(TAG, "Decoded: %s → %c (so far: %s)",
                     s_current_symbol, letter, s_received);
            break;
        }
    }
    s_sym_pos = 0;
    memset(s_current_symbol, 0, sizeof(s_current_symbol));
}

static void check_word(void) {
    if (strcmp(s_received, kTargetWord) == 0) {
        s_solved = true;
        gpio_set_level(GPIO_LED_G, 1);
        gpio_set_level(GPIO_LED_R, 0);
        uint8_t code[4] = {5, 0, 0, 0};  // digit 5 = last char 'S' index offset
        uint32_t elapsed = now_ms() - s_start_ms;
        espnow_slave_notify_solved(code, elapsed);
        ESP_LOGI(TAG, "SOLVED: ZACUS decoded!");
    } else {
        // Wrong word — reset current word, flash red
        gpio_set_level(GPIO_LED_R, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(GPIO_LED_R, 0);
        s_recv_pos = 0;
        memset(s_received, 0, sizeof(s_received));
        ESP_LOGW(TAG, "Wrong word: %s, reset", s_received);
    }
}

// Transmit morse message to player (for NON_TECH light mode or intro)
static void transmit_target(void) {
    for (int c = 0; kTargetWord[c]; c++) {
        int idx = kTargetWord[c] - 'A';
        const char* seq = kMorseTable[idx];
        for (int i = 0; seq[i]; i++) {
            uint32_t dur = (seq[i] == '.') ? 150 : 450;
            if (s_light_mode) {
                gpio_set_level(GPIO_LED_W, 1);
            } else {
                buzzer_on();
            }
            vTaskDelay(pdMS_TO_TICKS(dur));
            if (s_light_mode) {
                gpio_set_level(GPIO_LED_W, 0);
            } else {
                buzzer_off();
            }
            vTaskDelay(pdMS_TO_TICKS(150));  // intra-letter gap
        }
        vTaskDelay(pdMS_TO_TICKS(LETTER_GAP_MS));  // inter-letter gap
    }
}

static void espnow_cmd_handler(espnow_msg_type_t type,
                                const uint8_t* payload, size_t len) {
    if (type == MSG_PUZZLE_RESET) {
        s_solved = false;
        s_recv_pos = 0; s_sym_pos = 0;
        memset(s_received, 0, sizeof(s_received));
        memset(s_current_symbol, 0, sizeof(s_current_symbol));
        s_start_ms = now_ms();
        transmit_target();
    } else if (type == MSG_PUZZLE_CONFIG && len >= 1) {
        s_light_mode = (payload[0] == DIFFICULTY_EASY);  // NON_TECH = easy = light mode
    }
}

static void morse_task(void* arg) {
    uint32_t press_start = 0;
    uint32_t last_release = 0;
    bool key_down = false;

    for (;;) {
        espnow_slave_process();
        if (s_solved) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        uint32_t t = now_ms();
        bool pressed = (gpio_get_level(GPIO_KEY) == 0);

        if (pressed && !key_down) {
            // Key press start
            key_down = true;
            press_start = t;
            buzzer_on();
        } else if (!pressed && key_down) {
            // Key release
            key_down = false;
            uint32_t duration = t - press_start;
            buzzer_off();
            last_release = t;

            // Classify: dot or dash
            if (duration >= DOT_MIN_MS && duration < DOT_MAX_MS) {
                if (s_sym_pos < sizeof(s_current_symbol) - 1)
                    s_current_symbol[s_sym_pos++] = '.';
            } else if (duration >= DASH_MIN_MS && duration < DASH_MAX_MS) {
                if (s_sym_pos < sizeof(s_current_symbol) - 1)
                    s_current_symbol[s_sym_pos++] = '-';
            }
        } else if (!key_down && last_release > 0) {
            // Check for letter gap
            uint32_t silence = t - last_release;
            if (silence > LETTER_GAP_MS && s_sym_pos > 0) {
                decode_symbol();
                last_release = t;
            }
            // Check for word gap (end of word → check)
            if (silence > WORD_GAP_MS && s_recv_pos > 0 && strlen(s_received) > 0) {
                check_word();
                last_release = 0;  // reset to avoid re-checking
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << GPIO_BUZZER) | (1ULL << GPIO_LED_R)
                      | (1ULL << GPIO_LED_G)  | (1ULL << GPIO_LED_W),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << GPIO_KEY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in_cfg);

    static const uint8_t kMasterMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    espnow_slave_init(kMasterMac, 5 /* P5 */);
    espnow_slave_register_callback(espnow_cmd_handler);

    s_start_ms = now_ms();
    vTaskDelay(pdMS_TO_TICKS(3000));  // wait for master to be ready
    transmit_target();

    xTaskCreate(morse_task, "morse", 4096, NULL, 5, NULL);
}
```

### Task 3.4 — P6 Symboles Alchimiques firmware (4h)

**File:** `ESP32_ZACUS/puzzles/p6_symboles/main/p6_symboles_main.c`

```c
// p6_symboles_main.c — P6 Symboles Alchimiques (NFC-based)
// ESP32-DevKit + MFRC522 NFC + buzzer + LEDs
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "espnow_slave.h"
#include <string.h>
#include <stdint.h>

static const char* TAG = "P6_SYMBOLES";

#define GPIO_BUZZER  25
#define GPIO_LED_G   26
#define GPIO_LED_R   27

// SPI pins for MFRC522
#define SPI_HOST    SPI2_HOST
#define GPIO_SDA    5   // CS
#define GPIO_SCK    18
#define GPIO_MISO   19
#define GPIO_MOSI   23
#define GPIO_RST    4

// Expected placement order (12 NFC tag UIDs, pre-programmed)
// Indices 1-12 match the 12 symbol pieces
// Correct order: [7, 2, 11, 4, 9, 1, 8, 3, 12, 6, 10, 5]
static const uint8_t kCorrectOrder[12] = {7, 2, 11, 4, 9, 1, 8, 3, 12, 6, 10, 5};
static uint8_t s_slots[12];    // current placement: s_slots[position] = symbol_id
static uint8_t s_placed = 0;   // how many tags placed
static bool s_solved = false;
static uint32_t s_start_ms = 0;

// MFRC522 tag UIDs → symbol IDs (pre-programmed during setup)
// 12 NTAG213 tags, each with 4-byte UID
// UID stored in tag NDEF data block 1
typedef struct {
    uint8_t uid[4];
    uint8_t symbol_id;  // 1-12
} nfc_tag_t;

static nfc_tag_t s_tags[12];  // populated from flash/NVS at boot

static void buzzer_short(void) {
    gpio_set_level(GPIO_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_BUZZER, 0);
}

static void buzzer_long(void) {
    gpio_set_level(GPIO_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(800));
    gpio_set_level(GPIO_BUZZER, 0);
}

static void check_configuration(void) {
    if (s_placed < 12) return;
    bool correct = (memcmp(s_slots, kCorrectOrder, 12) == 0);
    if (correct) {
        s_solved = true;
        gpio_set_level(GPIO_LED_G, 1);
        // Code: digits 6-7 = index derived from configuration
        // Using position 0 and 1 of correct order XOR'd
        uint8_t d6 = kCorrectOrder[0] % 10;  // = 7
        uint8_t d7 = kCorrectOrder[1] % 10;  // = 2
        uint8_t code[4] = {d6, d7, 0, 0};
        uint32_t elapsed = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - s_start_ms;
        espnow_slave_notify_solved(code, elapsed);
        buzzer_long();
        ESP_LOGI(TAG, "Solved! Code=%d%d", d6, d7);
    } else {
        gpio_set_level(GPIO_LED_R, 1);
        buzzer_short();
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(GPIO_LED_R, 0);
        // Reset current configuration
        memset(s_slots, 0, sizeof(s_slots));
        s_placed = 0;
        ESP_LOGW(TAG, "Wrong configuration, reset");
    }
}

// Simplified NFC polling — reads single tag UID from MFRC522 via SPI
// Full MFRC522 driver implementation would use esp-idf SPI + MFRC522 register protocol
static bool nfc_poll(uint8_t uid_out[4]) {
    // Placeholder: actual implementation calls MFRC522_Request + MFRC522_Anticoll
    // Returns true if card present, copies UID to uid_out
    // In production: use espressif/idf-component-registry MFRC522 component
    (void)uid_out;
    return false;
}

static uint8_t uid_to_symbol(const uint8_t uid[4]) {
    for (int i = 0; i < 12; i++) {
        if (memcmp(s_tags[i].uid, uid, 4) == 0)
            return s_tags[i].symbol_id;
    }
    return 0;  // unknown tag
}

static void espnow_cmd_handler(espnow_msg_type_t type,
                                const uint8_t* payload, size_t len) {
    (void)payload; (void)len;
    if (type == MSG_PUZZLE_RESET) {
        memset(s_slots, 0, sizeof(s_slots));
        s_placed = 0;
        s_solved = false;
        s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        gpio_set_level(GPIO_LED_G, 0);
        gpio_set_level(GPIO_LED_R, 0);
        ESP_LOGI(TAG, "Reset");
    }
}

static void nfc_task(void* arg) {
    uint8_t uid[4];
    uint8_t last_uid[4] = {0};
    uint32_t last_read_ms = 0;

    for (;;) {
        espnow_slave_process();
        if (s_solved) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        uint32_t t = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (nfc_poll(uid)) {
            // Debounce: ignore same tag for 1 second
            if (memcmp(uid, last_uid, 4) != 0 || (t - last_read_ms) > 1000) {
                uint8_t sym = uid_to_symbol(uid);
                if (sym > 0 && sym <= 12) {
                    ESP_LOGI(TAG, "Tag placed: symbol %d at position %d", sym, s_placed);
                    buzzer_short();
                    s_slots[s_placed++] = sym;
                    memcpy(last_uid, uid, 4);
                    last_read_ms = t;
                    if (s_placed == 12) check_configuration();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));  // 5Hz NFC polling
    }
}

void app_main(void) {
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << GPIO_BUZZER) | (1ULL << GPIO_LED_G) | (1ULL << GPIO_LED_R),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out_cfg);

    // SPI init for MFRC522
    spi_bus_config_t buscfg = {
        .miso_io_num = GPIO_MISO,
        .mosi_io_num = GPIO_MOSI,
        .sclk_io_num = GPIO_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    // Device config would go here (MFRC522 at 10MHz, mode 0)

    // Load tag UIDs from NVS (programmed during factory setup)
    // nvs_get_blob("nfc_tags", s_tags, sizeof(s_tags));

    static const uint8_t kMasterMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    espnow_slave_init(kMasterMac, 6 /* P6 */);
    espnow_slave_register_callback(espnow_cmd_handler);

    s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    xTaskCreate(nfc_task, "nfc", 4096, NULL, 5, NULL);
}
```

### Task 3.5 — P7 Coffre Final firmware (4h)

**File:** `ESP32_ZACUS/puzzles/p7_coffre/main/p7_coffre_main.c`

```c
// p7_coffre_main.c — P7 Coffre Final (keypad + servo latch)
// ESP32-DevKit + 4×3 membrane keypad + SG90 servo + OLED + RGB LED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "espnow_slave.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "P7_COFFRE";

// Keypad GPIO
static const uint8_t kRowPins[4]    = {4, 5, 6, 7};
static const uint8_t kColPins[3]    = {12, 13, 14};
static const char kKeyMap[4][3]     = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};

// Servo
#define GPIO_SERVO      25
#define SERVO_LOCKED    1000  // µs pulse (0°)
#define SERVO_UNLOCKED  2000  // µs pulse (90°)

// OLED (I2C via esp-idf i2c driver + SSD1306 lib)
#define GPIO_SDA        21
#define GPIO_SCL        22

// LED (WS2812B single)
#define GPIO_LED        5

// State
static char s_entered[9] = {0};  // up to 8 digits + null
static uint8_t s_pos = 0;
static bool s_solved = false;
static char s_target_code[9] = "00000000";  // set by master via MSG_PUZZLE_CONFIG
static uint32_t s_start_ms = 0;

static void servo_set(uint32_t pulse_us) {
    // LEDC PWM: 50Hz, duty from pulse_us
    // period = 20000µs, resolution 14 bits → 16383 ticks
    uint32_t duty = (pulse_us * 16383) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void servo_lock(void)   { servo_set(SERVO_LOCKED); }
static void servo_unlock(void) { servo_set(SERVO_UNLOCKED); }

static void check_code(void) {
    bool correct = (strcmp(s_entered, s_target_code) == 0);
    if (correct) {
        s_solved = true;
        servo_unlock();
        // Flash green on RGB LED (simplified: GPIO LED high)
        gpio_set_level(GPIO_LED, 1);
        uint8_t code[4] = {0, 0, 0, 0};  // P7 does not add to code (it IS the final code)
        uint32_t elapsed = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - s_start_ms;
        espnow_slave_notify_solved(code, elapsed);
        ESP_LOGI(TAG, "COFFRE OUVERT! Code correct: %s", s_entered);
    } else {
        // Wrong code: flash red, reset
        ESP_LOGW(TAG, "Wrong code: %s (expected: %s)", s_entered, s_target_code);
        // TODO: drive RGB LED red via WS2812B
        s_pos = 0;
        memset(s_entered, 0, sizeof(s_entered));
    }
}

static char scan_keypad(void) {
    for (int r = 0; r < 4; r++) {
        gpio_set_level(kRowPins[r], 0);
        for (int c = 0; c < 3; c++) {
            if (gpio_get_level(kColPins[c]) == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));  // debounce
                if (gpio_get_level(kColPins[c]) == 0) {
                    gpio_set_level(kRowPins[r], 1);
                    return kKeyMap[r][c];
                }
            }
        }
        gpio_set_level(kRowPins[r], 1);
    }
    return 0;
}

static void espnow_cmd_handler(espnow_msg_type_t type,
                                const uint8_t* payload, size_t len) {
    if (type == MSG_PUZZLE_RESET) {
        servo_lock();
        s_pos = 0;
        s_solved = false;
        memset(s_entered, 0, sizeof(s_entered));
        s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Reset, locked");
    } else if (type == MSG_PUZZLE_CONFIG && len >= 8) {
        // Master sends the 8-digit target code
        memcpy(s_target_code, payload, 8);
        s_target_code[8] = '\0';
        ESP_LOGI(TAG, "Target code set: %s", s_target_code);
    }
}

static void keypad_task(void* arg) {
    char last_key = 0;
    for (;;) {
        espnow_slave_process();
        if (s_solved) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        char key = scan_keypad();
        if (key && key != last_key) {
            if (key == '#') {
                // Confirm entry
                if (s_pos == 8) check_code();
            } else if (key == '*') {
                // Clear entry
                s_pos = 0;
                memset(s_entered, 0, sizeof(s_entered));
                ESP_LOGI(TAG, "Entry cleared");
            } else if (s_pos < 8) {
                s_entered[s_pos++] = key;
                s_entered[s_pos] = '\0';
                ESP_LOGI(TAG, "Entered: %s", s_entered);
            }
        }
        last_key = key;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    // Row pins: output, initially HIGH
    for (int r = 0; r < 4; r++) {
        gpio_set_direction(kRowPins[r], GPIO_MODE_OUTPUT);
        gpio_set_level(kRowPins[r], 1);
    }
    // Col pins: input with pull-up
    for (int c = 0; c < 3; c++) {
        gpio_set_direction(kColPins[c], GPIO_MODE_INPUT);
        gpio_set_pull_mode(kColPins[c], GPIO_PULLUP_ONLY);
    }
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    // LEDC for servo
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz    = 50,
        .clk_cfg    = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t chan = {
        .gpio_num   = GPIO_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&chan);
    servo_lock();

    static const uint8_t kMasterMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    espnow_slave_init(kMasterMac, 7 /* P7 */);
    espnow_slave_register_callback(espnow_cmd_handler);

    s_start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    xTaskCreate(keypad_task, "keypad", 4096, NULL, 5, NULL);
}
```

### Task 3.6 — BOX-3 ESP-NOW master orchestrator (8h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/espnow_master.cpp`

```cpp
// espnow_master.cpp — BOX-3 ESP-NOW master, orchestrates all 7 puzzle slaves
#include "npc/espnow_master.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "ESPNOW_MASTER";

// Known puzzle MAC addresses (programmed during setup, stored in NVS)
// Broadcast address for initial discovery
static uint8_t kBroadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

typedef struct {
    uint8_t  puzzle_id;         // 1..7
    uint8_t  mac[6];
    bool     registered;
    bool     solved;
    uint8_t  code_fragment[4];
    uint32_t solve_time_ms;
    uint32_t last_heartbeat_ms;
} puzzle_node_t;

static puzzle_node_t s_nodes[8];  // index = puzzle_id (1-7, index 0 unused)
static QueueHandle_t s_recv_queue;
static espnow_event_cb_t s_event_cb;

typedef struct {
    uint8_t src_mac[6];
    uint8_t data[250];
    int     len;
} master_recv_item_t;

static void on_recv(const esp_now_recv_info_t* info,
                    const uint8_t* data, int len) {
    master_recv_item_t item;
    memcpy(item.src_mac, info->src_addr, 6);
    int copy = len < (int)sizeof(item.data) ? len : (int)sizeof(item.data);
    memcpy(item.data, data, copy);
    item.len = copy;
    xQueueSendFromISR(s_recv_queue, &item, NULL);
}

esp_err_t espnow_master_init(espnow_event_cb_t cb) {
    s_event_cb = cb;
    s_recv_queue = xQueueCreate(16, sizeof(master_recv_item_t));
    if (!s_recv_queue) return ESP_ERR_NO_MEM;
    memset(s_nodes, 0, sizeof(s_nodes));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));

    esp_now_peer_info_t bcast = {};
    memcpy(bcast.peer_addr, kBroadcast, 6);
    bcast.channel = 0;
    bcast.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&bcast));

    ESP_LOGI(TAG, "Master initialized");
    return ESP_OK;
}

esp_err_t espnow_master_reset_puzzle(uint8_t puzzle_id) {
    if (puzzle_id < 1 || puzzle_id > 7) return ESP_ERR_INVALID_ARG;
    uint8_t buf[2] = { MSG_PUZZLE_RESET, puzzle_id };
    return esp_now_send(kBroadcast, buf, sizeof(buf));
}

esp_err_t espnow_master_configure_puzzle(uint8_t puzzle_id,
                                          puzzle_difficulty_t difficulty) {
    uint8_t buf[3] = { MSG_PUZZLE_CONFIG, puzzle_id, (uint8_t)difficulty };
    return esp_now_send(kBroadcast, buf, sizeof(buf));
}

esp_err_t espnow_master_send_final_code(const char code[9]) {
    uint8_t buf[10];
    buf[0] = MSG_PUZZLE_CONFIG;
    buf[1] = 7;  // P7 puzzle ID
    memcpy(buf + 2, code, 8);
    return esp_now_send(kBroadcast, buf, 10);
}

void espnow_master_process(void) {
    master_recv_item_t item;
    while (xQueueReceive(s_recv_queue, &item, 0) == pdTRUE) {
        if (item.len < 2) continue;
        espnow_msg_type_t type = (espnow_msg_type_t)item.data[0];
        uint8_t puzzle_id = item.data[1];
        if (puzzle_id < 1 || puzzle_id > 7) continue;

        puzzle_node_t* node = &s_nodes[puzzle_id];
        node->registered = true;
        memcpy(node->mac, item.src_mac, 6);
        node->last_heartbeat_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (type == MSG_PUZZLE_SOLVED && item.len >= 6) {
            node->solved = true;
            memcpy(node->code_fragment, item.data + 2, 4);
            ESP_LOGI(TAG, "P%d SOLVED, code=%d%d%d%d",
                     puzzle_id,
                     node->code_fragment[0], node->code_fragment[1],
                     node->code_fragment[2], node->code_fragment[3]);
            if (s_event_cb) {
                espnow_event_t ev = {
                    .type = ESPNOW_EV_PUZZLE_SOLVED,
                    .puzzle_id = puzzle_id,
                };
                memcpy(ev.code_fragment, node->code_fragment, 4);
                s_event_cb(&ev);
            }
            // Check if all required puzzles for current phase are solved
            // → delegate to game_coordinator
        } else if (type == MSG_HINT_REQUEST) {
            if (s_event_cb) {
                espnow_event_t ev = {
                    .type = ESPNOW_EV_HINT_REQUEST,
                    .puzzle_id = puzzle_id,
                };
                s_event_cb(&ev);
            }
        } else if (type == MSG_HEARTBEAT) {
            ESP_LOGD(TAG, "Heartbeat P%d", puzzle_id);
        }
    }
}

bool espnow_master_all_solved(const uint8_t puzzle_ids[], uint8_t count) {
    for (int i = 0; i < count; i++) {
        uint8_t id = puzzle_ids[i];
        if (id < 1 || id > 7 || !s_nodes[id].solved) return false;
    }
    return true;
}

void espnow_master_assemble_code(char code_out[9]) {
    // Assemble: P1[0,1] P2[2] P4[3] P5[4] P6[5,6] P3[7]
    static const uint8_t kCodeMap[8][2] = {
        {1, 0}, {1, 1},  // P1 → digits 0,1
        {2, 0},          // P2 → digit 2
        {4, 0},          // P4 → digit 3
        {5, 0},          // P5 → digit 4
        {6, 0}, {6, 1},  // P6 → digits 5,6
        {3, 0},          // P3 → digit 7
    };
    for (int i = 0; i < 8; i++) {
        uint8_t pid = kCodeMap[i][0];
        uint8_t frag_idx = kCodeMap[i][1];
        code_out[i] = '0' + (s_nodes[pid].code_fragment[frag_idx] % 10);
    }
    code_out[8] = '\0';
    ESP_LOGI(TAG, "Final code assembled: %s", code_out);
}
```

---

## Phase 4: NPC Adaptive Logic (15h)

### Task 4.1 — Extend `npc_engine.h` with V3 group profiling types (2h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/include/npc/npc_engine.h` — add after existing enums:

```c
// V3 group profile (set after Phase 2 profiling puzzles)
typedef enum {
    GROUP_UNKNOWN   = 0,
    GROUP_TECH      = 1,   // P2 fast, P1 normal/slow
    GROUP_NON_TECH  = 2,   // P1 fast, P2 normal/slow
    GROUP_MIXED     = 3,   // both similar speed
} group_profile_t;

// V3 adaptive state (extends npc_state_t)
typedef struct {
    group_profile_t group_profile;
    uint32_t        target_duration_ms;     // game master configured
    uint32_t        game_start_ms;
    uint32_t        profiling_p1_time_ms;   // P1 solve time
    uint32_t        profiling_p2_time_ms;   // P2 solve time
    uint8_t         puzzles_solved;
    uint8_t         total_puzzles;
    uint8_t         bonus_puzzles_added;
    bool            duration_warning_sent;
} npc_v3_state_t;

// V3 adaptive actions
typedef enum {
    NPC_ACTION_NONE           = 0,
    NPC_ACTION_ADD_BONUS      = 1,
    NPC_ACTION_SKIP_PUZZLE    = 2,
    NPC_ACTION_PROACTIVE_HINT = 3,
    NPC_ACTION_DURATION_WARN  = 4,
    NPC_ACTION_SET_PROFILE    = 5,
} npc_v3_action_t;

typedef struct {
    npc_v3_action_t action;
    group_profile_t new_profile;   // for NPC_ACTION_SET_PROFILE
    uint8_t         puzzle_to_skip; // for NPC_ACTION_SKIP_PUZZLE
    char            phrase_key[64]; // phrase to play
} npc_v3_decision_t;

// V3 NPC functions
void npc_v3_init(npc_v3_state_t* v3, uint32_t target_duration_ms, uint32_t now_ms);
void npc_v3_on_profiling_solved(npc_v3_state_t* v3, uint8_t puzzle_id,
                                 uint32_t solve_time_ms);
group_profile_t npc_v3_classify_group(const npc_v3_state_t* v3);
bool npc_v3_evaluate(const npc_v3_state_t* v3, const npc_state_t* base,
                     uint32_t now_ms, npc_v3_decision_t* out);
```

### Task 4.2 — Implement V3 adaptive logic in `npc_engine.cpp` (8h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/npc_engine.cpp` — append new functions:

```c
// === V3 Adaptive NPC Extensions ===

#define V3_FAST_THRESHOLD_S    180   // < 3 min = fast on profiling puzzle
#define V3_SLOW_THRESHOLD_S    480   // > 8 min = slow on profiling puzzle
#define V3_FAST_PCT            60    // < 60% of expected = fast group overall
#define V3_SLOW_PCT            130   // > 130% of expected = slow group overall
#define V3_DURATION_WARN_PCT   80    // 80% of target_duration → warning
#define V3_MAX_BONUS_PUZZLES   2

void npc_v3_init(npc_v3_state_t* v3, uint32_t target_duration_ms, uint32_t now_ms) {
    if (!v3) return;
    memset(v3, 0, sizeof(*v3));
    v3->group_profile = GROUP_UNKNOWN;
    v3->target_duration_ms = target_duration_ms;
    v3->game_start_ms = now_ms;
    v3->total_puzzles = 5;  // default MIXED parcours
}

void npc_v3_on_profiling_solved(npc_v3_state_t* v3, uint8_t puzzle_id,
                                 uint32_t solve_time_ms) {
    if (!v3) return;
    if (puzzle_id == 1) v3->profiling_p1_time_ms = solve_time_ms;
    if (puzzle_id == 2) v3->profiling_p2_time_ms = solve_time_ms;
    v3->puzzles_solved++;
}

group_profile_t npc_v3_classify_group(const npc_v3_state_t* v3) {
    if (!v3) return GROUP_UNKNOWN;
    if (v3->profiling_p1_time_ms == 0 || v3->profiling_p2_time_ms == 0)
        return GROUP_UNKNOWN;

    bool p1_fast = (v3->profiling_p1_time_ms < (V3_FAST_THRESHOLD_S * 1000U));
    bool p2_fast = (v3->profiling_p2_time_ms < (V3_FAST_THRESHOLD_S * 1000U));
    bool p2_slow = (v3->profiling_p2_time_ms > (V3_SLOW_THRESHOLD_S * 1000U));

    if (p2_fast && !p1_fast) return GROUP_TECH;
    if (p1_fast && p2_slow)  return GROUP_NON_TECH;
    return GROUP_MIXED;
}

bool npc_v3_evaluate(const npc_v3_state_t* v3, const npc_state_t* base,
                     uint32_t now_ms, npc_v3_decision_t* out) {
    if (!v3 || !base || !out) return false;
    memset(out, 0, sizeof(*out));
    out->action = NPC_ACTION_NONE;

    uint32_t game_elapsed = now_ms - v3->game_start_ms;
    uint32_t target = v3->target_duration_ms;

    // 1. Duration warning (80% of target reached, puzzles remain)
    if (!v3->duration_warning_sent
        && target > 0
        && game_elapsed > (target * V3_DURATION_WARN_PCT / 100U)
        && v3->puzzles_solved < v3->total_puzzles - 1) {
        out->action = NPC_ACTION_DURATION_WARN;
        snprintf(out->phrase_key, sizeof(out->phrase_key),
                 "adaptation.duration_warning.0");
        return true;
    }

    // 2. Group still unknown after profiling phase (both P1+P2 solved)
    if (v3->group_profile == GROUP_UNKNOWN
        && v3->profiling_p1_time_ms > 0
        && v3->profiling_p2_time_ms > 0) {
        group_profile_t profile = npc_v3_classify_group(v3);
        out->action = NPC_ACTION_SET_PROFILE;
        out->new_profile = profile;
        if (profile == GROUP_TECH)
            snprintf(out->phrase_key, sizeof(out->phrase_key),
                     "adaptation.group_tech_detected.0");
        else if (profile == GROUP_NON_TECH)
            snprintf(out->phrase_key, sizeof(out->phrase_key),
                     "adaptation.group_non_tech_detected.0");
        else
            snprintf(out->phrase_key, sizeof(out->phrase_key),
                     "adaptation.group_mixed_detected.0");
        return true;
    }

    // 3. Fast group → add bonus puzzle (if budget allows)
    if (base->expected_scene_duration_ms > 0) {
        uint32_t scene_elapsed = now_ms - base->scene_start_ms;
        uint32_t pct = (scene_elapsed * 100U) / base->expected_scene_duration_ms;

        if (pct < V3_FAST_PCT
            && v3->bonus_puzzles_added < V3_MAX_BONUS_PUZZLES
            && v3->puzzles_solved < v3->total_puzzles - 1) {
            out->action = NPC_ACTION_ADD_BONUS;
            snprintf(out->phrase_key, sizeof(out->phrase_key),
                     "adaptation.bonus_puzzle_added.%d",
                     v3->bonus_puzzles_added % 2);
            return true;
        }

        // 4. Slow group → skip optional puzzle
        if (pct > (V3_SLOW_PCT * 120U / 100U)  // 160% threshold
            && v3->puzzles_solved < v3->total_puzzles - 1) {
            out->action = NPC_ACTION_SKIP_PUZZLE;
            snprintf(out->phrase_key, sizeof(out->phrase_key),
                     "adaptation.puzzle_skipped.0");
            return true;
        }
    }

    return false;
}
```

### Task 4.3 — Game coordinator integrating NPC V3 + ESP-NOW master (5h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/game_coordinator.cpp`

```cpp
// game_coordinator.cpp — Top-level game state machine for V3
// Integrates: npc_engine, npc_v3_state, espnow_master, tts_client
#include "npc/game_coordinator.h"
#include "npc/npc_engine.h"
#include "npc/espnow_master.h"
#include "npc/tts_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "GAME_COORD";

typedef enum {
    GAME_IDLE       = 0,
    GAME_INTRO      = 1,
    GAME_PROFILING  = 2,
    GAME_ADAPTIVE   = 3,
    GAME_CLIMAX     = 4,
    GAME_OUTRO      = 5,
} game_phase_t;

// Parcours per group profile
static const uint8_t kParcoursTech[]     = {2, 4, 5, 3, 7};
static const uint8_t kParcoursNonTech[]  = {1, 6, 3, 5, 7};
static const uint8_t kParcoursMixed[]    = {1, 2, 3, 5, 6, 7};
static const uint8_t kParcoursLenTech    = 5;
static const uint8_t kParcoursLenNonTech = 5;
static const uint8_t kParcoursLenMixed   = 6;

typedef struct {
    game_phase_t phase;
    npc_state_t  npc;
    npc_v3_state_t v3;
    uint8_t  current_puzzle_idx;
    uint8_t  parcours[8];
    uint8_t  parcours_len;
    char     final_code[9];
    uint32_t score;
} game_state_t;

static game_state_t s_game;

static void play_phrase(const char* key) {
    // Build TTS text from phrase bank lookup (simplified: key is the phrase text)
    // In production: look up key in phrase bank YAML → get text → tts_speak()
    ESP_LOGI(TAG, "NPC phrase: [%s]", key);
    // tts_speak(text, TTS_VOICE_ZACUS);
}

static void transition_to_phase(game_phase_t phase, uint32_t now_ms) {
    s_game.phase = phase;
    switch (phase) {
    case GAME_INTRO:
        play_phrase("ambiance.intro.0");
        // After 5 min intro, transition to PROFILING
        break;
    case GAME_PROFILING:
        play_phrase("ambiance.profiling.start");
        espnow_master_reset_puzzle(1);  // P1 Son
        espnow_master_reset_puzzle(2);  // P2 Circuit
        break;
    case GAME_ADAPTIVE:
        // Select parcours based on group profile
        group_profile_t profile = npc_v3_classify_group(&s_game.v3);
        s_game.v3.group_profile = profile;
        if (profile == GROUP_TECH) {
            memcpy(s_game.parcours, kParcoursTech, kParcoursLenTech);
            s_game.parcours_len = kParcoursLenTech;
        } else if (profile == GROUP_NON_TECH) {
            memcpy(s_game.parcours, kParcoursNonTech, kParcoursLenNonTech);
            s_game.parcours_len = kParcoursLenNonTech;
        } else {
            memcpy(s_game.parcours, kParcoursMixed, kParcoursLenMixed);
            s_game.parcours_len = kParcoursLenMixed;
        }
        s_game.current_puzzle_idx = 0;
        s_game.v3.total_puzzles = s_game.parcours_len;
        // Activate first puzzle
        uint8_t first = s_game.parcours[0];
        espnow_master_reset_puzzle(first);
        // Configure difficulty
        puzzle_difficulty_t diff = (profile == GROUP_NON_TECH) ? DIFFICULTY_EASY : DIFFICULTY_NORMAL;
        espnow_master_configure_puzzle(first, diff);
        break;
    case GAME_CLIMAX:
        // Assemble final code from all solved puzzles
        espnow_master_assemble_code(s_game.final_code);
        ESP_LOGI(TAG, "Final code: %s", s_game.final_code);
        espnow_master_send_final_code(s_game.final_code);
        espnow_master_reset_puzzle(7);  // P7 Coffre
        play_phrase("ambiance.climax");
        break;
    case GAME_OUTRO:
        play_phrase("ambiance.outro_success.0");
        // Compute final score
        s_game.score = 1000;
        // hints penalty applied in npc_state
        break;
    default:
        break;
    }
}

static void on_espnow_event(const espnow_event_t* ev) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t puzzle_id = ev->puzzle_id;

    if (ev->type == ESPNOW_EV_PUZZLE_SOLVED) {
        ESP_LOGI(TAG, "Puzzle P%d solved in phase %d", puzzle_id, s_game.phase);

        if (s_game.phase == GAME_PROFILING) {
            uint32_t solve_time = ev->solve_time_ms;
            npc_v3_on_profiling_solved(&s_game.v3, puzzle_id, solve_time);
            // If both profiling puzzles done → transition to ADAPTIVE
            if (s_game.v3.profiling_p1_time_ms > 0
                && s_game.v3.profiling_p2_time_ms > 0) {
                transition_to_phase(GAME_ADAPTIVE, now);
            }
        } else if (s_game.phase == GAME_ADAPTIVE) {
            s_game.v3.puzzles_solved++;
            // Advance to next puzzle in parcours
            s_game.current_puzzle_idx++;
            if (s_game.current_puzzle_idx >= s_game.parcours_len - 1) {
                // All adaptive puzzles done → climax
                transition_to_phase(GAME_CLIMAX, now);
            } else {
                uint8_t next = s_game.parcours[s_game.current_puzzle_idx];
                espnow_master_reset_puzzle(next);
                group_profile_t p = s_game.v3.group_profile;
                puzzle_difficulty_t diff = (p == GROUP_NON_TECH) ? DIFFICULTY_EASY : DIFFICULTY_NORMAL;
                espnow_master_configure_puzzle(next, diff);
                // Update NPC scene timing
                npc_on_scene_change(&s_game.npc, next,
                                    300000 /* 5min default */, now);
            }
        } else if (s_game.phase == GAME_CLIMAX && puzzle_id == 7) {
            transition_to_phase(GAME_OUTRO, now);
        }
    } else if (ev->type == ESPNOW_EV_HINT_REQUEST) {
        npc_on_hint_request(&s_game.npc, now);
        // Dispatch hint TTS
        npc_decision_t dec;
        if (npc_evaluate(&s_game.npc, now, &dec)) {
            if (dec.audio_source == NPC_AUDIO_LIVE_TTS) {
                // Build phrase from hint key
                // tts_speak(phrase_text, TTS_VOICE_ZACUS);
            }
            // else play SD mp3 at dec.sd_path
        }
    }
}

void game_coordinator_init(uint32_t target_duration_ms) {
    memset(&s_game, 0, sizeof(s_game));
    npc_init(&s_game.npc);
    npc_v3_init(&s_game.v3, target_duration_ms,
                xTaskGetTickCount() * portTICK_PERIOD_MS);
    espnow_master_init(on_espnow_event);
    ESP_LOGI(TAG, "Game coordinator init, target=%lu ms", target_duration_ms);
}

void game_coordinator_start(void) {
    transition_to_phase(GAME_INTRO, xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void game_coordinator_tick(void) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    espnow_master_process();

    // Periodic NPC evaluation
    npc_update_mood(&s_game.npc, now);
    npc_v3_decision_t v3_dec;
    if (npc_v3_evaluate(&s_game.v3, &s_game.npc, now, &v3_dec)) {
        if (v3_dec.action == NPC_ACTION_SET_PROFILE) {
            s_game.v3.group_profile = v3_dec.new_profile;
        } else if (v3_dec.action == NPC_ACTION_SKIP_PUZZLE
                   && s_game.phase == GAME_ADAPTIVE) {
            s_game.current_puzzle_idx++;
            if (s_game.current_puzzle_idx < s_game.parcours_len) {
                uint8_t next = s_game.parcours[s_game.current_puzzle_idx];
                espnow_master_reset_puzzle(next);
            }
        } else if (v3_dec.action == NPC_ACTION_DURATION_WARN) {
            // Mark sent to avoid repeat
            // s_game.v3.duration_warning_sent = true;  (need mutable ptr)
        }
        if (v3_dec.phrase_key[0]) {
            play_phrase(v3_dec.phrase_key);
        }
    }
}
```

---

## Phase 5: XTTS-v2 Voice Clone (8h)

### Task 5.1 — Deploy XTTS-v2 on KXKM-AI (2h)

**File:** `tools/tts/docker-compose.xtts.yml`

```yaml
# docker-compose.xtts.yml — XTTS-v2 deployment on KXKM-AI (RTX 4090)
# Deploy with: docker compose -f docker-compose.xtts.yml up -d
version: "3.9"

services:
  xtts:
    image: ghcr.io/coqui-ai/tts:latest
    container_name: xtts_zacus
    restart: unless-stopped
    environment:
      - COQUI_TOS_AGREED=1
    command: >
      python3 -c "
      from TTS.api import TTS
      import uvicorn
      from fastapi import FastAPI, Request
      from fastapi.responses import StreamingResponse
      import io, soundfile as sf, numpy as np, json

      tts = TTS('tts_models/multilingual/multi-dataset/xtts_v2', gpu=True)
      app = FastAPI()

      @app.post('/v1/audio/speech')
      async def synthesize(req: Request):
          body = await req.json()
          text = body.get('input', '')
          # Use speaker reference for Zacus voice clone
          wav = tts.tts(
              text=text,
              speaker_wav='/voices/zacus_reference.wav',
              language='fr'
          )
          buf = io.BytesIO()
          sf.write(buf, np.array(wav), 24000, format='wav')
          buf.seek(0)
          return StreamingResponse(buf, media_type='audio/wav',
                                   headers={'Content-Disposition': 'attachment; filename=speech.wav'})

      @app.get('/health')
      def health(): return {'status': 'ok', 'model': 'xtts_v2'}

      uvicorn.run(app, host='0.0.0.0', port=5002)
      "
    ports:
      - "5002:5002"
    volumes:
      - ./voices:/voices:ro          # mount voice samples
      - xtts_cache:/root/.local/share/tts
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [gpu]
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:5002/health"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  xtts_cache:
```

**Deployment commands:**

```bash
# On KXKM-AI
ssh kxkm@kxkm-ai
cd ~/zacus-tts
# Copy 30s voice sample (recorded locally)
scp voices/zacus_reference.wav kxkm@kxkm-ai:~/zacus-tts/voices/
docker compose -f docker-compose.xtts.yml up -d
# Test
curl -X POST http://localhost:5002/v1/audio/speech \
  -H "Content-Type: application/json" \
  -d '{"input": "Bienvenue dans mon laboratoire portable."}' \
  --output /tmp/test.wav
```

### Task 5.2 — Voice sample recording guide (1h)

**File:** `tools/tts/voice_recording_guide.md`

```markdown
# Voice Recording Guide — Professor Zacus

## Target: 30-second reference sample for XTTS-v2 cloning

### Script to record (theatrical French, eccentric professor tone):

> "Bonjour, je suis le Professeur Zacus, scientifique extraordinaire et inventeur du premier
> laboratoire portable de l'histoire. Aujourd'hui, c'est vous qui êtes l'expérience.
> J'ai préparé des épreuves fascinantes, des énigmes délicieuses, et quelques fausses pistes —
> car la science, mes amis, c'est avant tout savoir distinguer le vrai du faux.
> Bonne chance ! Enfin, si vous en avez besoin…"

### Recording requirements:
- Duration: 25-35 seconds
- Format: WAV 44100Hz 16-bit mono
- Environment: quiet room, no echo
- Microphone: any decent USB mic (Blue Yeti, Rode NT-USB)
- Character: theatrical, French, eccentric, slightly superior tone
- No background noise, breaths between phrases ok

### Export:
- File: voices/zacus_reference.wav
- Trim silence at start/end
- Normalize to -3dB peak
```

### Task 5.3 — Update `tts_client.cpp` with XTTS fallback chain (3h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/tts_client.cpp` — extend existing client:

```cpp
// Additions to tts_client.cpp — XTTS-v2 as primary TTS, Piper as fallback

// In tts_client_init() or tts_speak(), add XTTS URL config:
// XTTS endpoint: http://kxkm-ai:5002/v1/audio/speech
// Piper endpoint (fallback): http://192.168.0.120:8001/v1/audio/speech
// SD card (fallback²): /hotline_tts/{key}.mp3

typedef enum {
    TTS_BACKEND_XTTS  = 0,   // KXKM-AI XTTS-v2 (GPU, latency ~2s)
    TTS_BACKEND_PIPER = 1,   // Tower Piper TTS (CPU, latency ~500ms)
    TTS_BACKEND_SD    = 2,   // SD card pre-generated MP3 (offline)
} tts_backend_t;

static tts_backend_t s_current_backend = TTS_BACKEND_XTTS;

// Health-check URLs (checked at boot and every 60s)
static const char* kXttsHealthUrl  = "http://kxkm-ai:5002/health";
static const char* kPiperHealthUrl = "http://192.168.0.120:8001/health";

static bool check_backend_health(const char* url) {
    // HTTP GET, check 200 response
    // Returns true if reachable and healthy
    // Uses existing http_client from tts_client.cpp
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 2000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return (err == ESP_OK && status == 200);
}

static tts_backend_t select_best_backend(void) {
    if (check_backend_health(kXttsHealthUrl))  return TTS_BACKEND_XTTS;
    if (check_backend_health(kPiperHealthUrl)) return TTS_BACKEND_PIPER;
    return TTS_BACKEND_SD;
}

// tts_speak() — dispatches to best available backend
esp_err_t tts_speak_v3(const char* text, const char* sd_fallback_key) {
    tts_backend_t backend = select_best_backend();
    s_current_backend = backend;

    if (backend == TTS_BACKEND_XTTS) {
        return tts_speak_http(text, "http://kxkm-ai:5002/v1/audio/speech");
    } else if (backend == TTS_BACKEND_PIPER) {
        return tts_speak_http(text, "http://192.168.0.120:8001/v1/audio/speech");
    } else {
        // SD card fallback
        char path[128];
        snprintf(path, sizeof(path), "/hotline_tts/%s.mp3", sd_fallback_key);
        return tts_play_sd_mp3(path);
    }
}
```

### Task 5.4 — Pre-generate XTTS MP3 pool (2h)

**File:** `tools/tts/generate_xtts_pool.py`

```python
#!/usr/bin/env python3
"""
generate_xtts_pool.py — Pre-generate all NPC phrases via XTTS-v2.
Reads npc_phrases.yaml, calls XTTS on kxkm-ai, saves MP3s to hotline_tts/.
Run on KXKM-AI or via SSH tunnel when GPU available.
Usage:
    python3 tools/tts/generate_xtts_pool.py [--dry-run] [--host kxkm-ai:5002]
"""
import argparse
import json
import pathlib
import re
import sys
import requests
import yaml

PHRASES_YAML = pathlib.Path("game/scenarios/npc_phrases.yaml")
OUTPUT_DIR   = pathlib.Path("hotline_tts")
MANIFEST     = OUTPUT_DIR / "manifest.json"
DEFAULT_HOST = "kxkm-ai:5002"


def flatten_phrases(data: dict, prefix: str = "") -> list[dict]:
    """Recursively extract all phrase entries with their keys."""
    phrases = []
    for k, v in data.items():
        path = f"{prefix}.{k}" if prefix else k
        if isinstance(v, list):
            for i, item in enumerate(v):
                if isinstance(item, dict) and "text" in item:
                    phrases.append({
                        "key": item.get("key", f"{path}.{i}"),
                        "text": item["text"],
                    })
        elif isinstance(v, dict):
            phrases.extend(flatten_phrases(v, path))
    return phrases


def key_to_path(key: str) -> pathlib.Path:
    """Convert phrase key to file path: hints.P1_SON.level_1.0 → hints/P1_SON/level_1/0.mp3"""
    parts = key.replace(".", "/")
    return OUTPUT_DIR / f"{parts}.mp3"


def synthesize(text: str, host: str) -> bytes:
    """Call XTTS-v2 API, return WAV bytes."""
    url = f"http://{host}/v1/audio/speech"
    resp = requests.post(url, json={"input": text}, timeout=30)
    resp.raise_for_status()
    return resp.content


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--force", action="store_true", help="Re-generate existing files")
    args = parser.parse_args()

    with open(PHRASES_YAML) as f:
        data = yaml.safe_load(f)

    phrases = flatten_phrases(data)
    print(f"Found {len(phrases)} phrases to generate")

    manifest = {}
    if MANIFEST.exists():
        with open(MANIFEST) as f:
            manifest = json.load(f)

    generated = skipped = errors = 0

    for ph in phrases:
        key  = ph["key"]
        text = ph["text"]
        path = key_to_path(key)

        if not args.force and path.exists():
            skipped += 1
            manifest[key] = str(path)
            continue

        print(f"  {'[DRY]' if args.dry_run else '[GEN]'} {key}")
        print(f"         → {text[:60]}...")

        if args.dry_run:
            generated += 1
            continue

        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            wav_bytes = synthesize(text, args.host)
            path.write_bytes(wav_bytes)
            manifest[key] = str(path)
            generated += 1
        except Exception as e:
            print(f"  ERROR: {e}", file=sys.stderr)
            errors += 1

    if not args.dry_run:
        MANIFEST.parent.mkdir(parents=True, exist_ok=True)
        with open(MANIFEST, "w") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)

    print(f"\nDone: {generated} generated, {skipped} skipped, {errors} errors")
    return 0 if errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
```

---

## Phase 6: AudioCraft Ambiance (4h)

### Task 6.1 — Deploy AudioCraft on KXKM-AI (1h)

**File:** `tools/audio/docker-compose.audiocraft.yml`

```yaml
# docker-compose.audiocraft.yml — AudioCraft MusicGen deployment on KXKM-AI
version: "3.9"

services:
  audiocraft:
    image: pytorch/pytorch:2.2.0-cuda12.1-cudnn8-runtime
    container_name: audiocraft_gen
    working_dir: /workspace
    command: python3 generate_tracks.py
    volumes:
      - ./tools/audio:/workspace
      - audiocraft_output:/workspace/output
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [gpu]
    environment:
      - PYTHONUNBUFFERED=1

volumes:
  audiocraft_output:
```

### Task 6.2 — Generate 6 ambient tracks (2h)

**File:** `tools/audio/generate_tracks.py`

```python
#!/usr/bin/env python3
"""
generate_tracks.py — Generate 6 ambient audio tracks using AudioCraft MusicGen.
Run on KXKM-AI (RTX 4090).
Output: output/{track_name}.mp3 (mono, 44100Hz)
"""
import torch
import torchaudio
import pathlib
import sys

OUTPUT_DIR = pathlib.Path("output")
OUTPUT_DIR.mkdir(exist_ok=True)


def generate_track(model, description: str, duration_s: int, filename: str):
    """Generate a single track and save as MP3-compatible WAV."""
    print(f"Generating: {filename} ({duration_s}s)")
    print(f"  Prompt: {description}")

    with torch.no_grad():
        wav = model.generate(
            descriptions=[description],
            progress=True,
        )

    out_path = OUTPUT_DIR / filename
    # AudioCraft returns [batch, channels, samples] at model.sample_rate
    torchaudio.save(
        str(out_path),
        wav[0].cpu(),
        model.sample_rate,
        format="wav",
    )
    print(f"  Saved: {out_path}")
    return out_path


def main():
    from audiocraft.models import MusicGen

    print("Loading MusicGen-medium model...")
    model = MusicGen.get_pretrained("facebook/musicgen-medium")

    tracks = [
        {
            "name": "lab_ambiance.wav",
            "duration": 30,
            "prompt": (
                "Laboratory ambient sound, machines humming, electronic beeps, "
                "ventilation fan, subtle mechanical sounds, science lab, mysterious, "
                "loopable, no melody, background ambiance"
            ),
        },
        {
            "name": "tension_rising.wav",
            "duration": 300,  # 5 min
            "prompt": (
                "Dramatic tension building music, suspenseful, orchestral, "
                "slow crescendo, escape room atmosphere, mysterious puzzle solving, "
                "cinematic, no lyrics"
            ),
        },
        {
            "name": "victory.wav",
            "duration": 30,
            "prompt": (
                "Victory fanfare, joyful celebration music, brass orchestra, "
                "applause, triumphant, escape room win, uplifting, energetic"
            ),
        },
        {
            "name": "failure.wav",
            "duration": 15,
            "prompt": (
                "Failure buzzer sound effect, trombone descending, game over, "
                "humorous sad tuba, cartoon fail sound, short"
            ),
        },
        {
            "name": "thinking.wav",
            "duration": 60,  # loopable section
            "prompt": (
                "Subtle suspense music for thinking, minimal, ambient, "
                "slow piano, light electronic, puzzle solving concentration, "
                "loopable, calm but mysterious"
            ),
        },
        {
            "name": "transition.wav",
            "duration": 10,
            "prompt": (
                "Scene transition sound effect, swoosh, magical glitter, "
                "short stinger, puzzle reveal, sparkle effect, 10 seconds"
            ),
        },
    ]

    for track in tracks:
        # Set duration for model
        model.set_generation_params(duration=min(track["duration"], 30))
        # AudioCraft max duration per call is 30s; for longer tracks, loop/extend
        out = generate_track(
            model,
            description=track["prompt"],
            duration_s=track["duration"],
            filename=track["name"],
        )
        print(f"  OK: {out}")

    print("\nAll tracks generated successfully.")
    print(f"Output directory: {OUTPUT_DIR.absolute()}")


if __name__ == "__main__":
    sys.exit(main())
```

### Task 6.3 — BLE audio control from BOX-3 (1h)

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/audio_ble_control.cpp`

```cpp
// audio_ble_control.cpp — BLE commands to control Bluetooth speaker playback
// BOX-3 acts as BLE central, Bluetooth speaker as peripheral
// Uses ESP-IDF BLE GATT client to send AVRCP-like play/stop/volume commands
#include "audio_ble_control.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_avrc_api.h"
#include "esp_a2dp_api.h"
#include <cstring>

static const char* TAG = "AUDIO_BLE";

// Track names → index mapping (must match pre-loaded tracks on speaker or media player)
typedef enum {
    TRACK_LAB_AMBIANCE  = 0,
    TRACK_TENSION       = 1,
    TRACK_VICTORY       = 2,
    TRACK_FAILURE       = 3,
    TRACK_THINKING      = 4,
    TRACK_TRANSITION    = 5,
} audio_track_t;

static bool s_connected = false;

void audio_ble_init(void) {
    // Initialize classic Bluetooth for A2DP + AVRCP control
    // This allows BOX-3 to send AVRCP commands to paired BT speaker
    ESP_LOGI(TAG, "Audio BLE/BT control initialized");
    // Full A2DP source init for ESP32-S3:
    // esp_bt_controller_init + esp_bluedroid_init + esp_a2d_register_callback
    // + esp_avrc_ct_init + esp_avrc_ct_register_callback
}

esp_err_t audio_play_track(audio_track_t track) {
    if (!s_connected) {
        ESP_LOGW(TAG, "Bluetooth speaker not connected");
        return ESP_ERR_INVALID_STATE;
    }
    // AVRCP passthrough: specific track selection
    // Most BT speakers support play/pause/next/prev via AVRCP
    // For track selection: use AVRCP SetBrowsedPlayer + PlayItem if supported
    // Simplified: send PLAY command (assumes speaker is on correct track)
    ESP_LOGI(TAG, "Playing track %d", track);
    return esp_avrc_ct_send_passthrough_cmd(
        0, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_PUSHED);
}

esp_err_t audio_stop(void) {
    if (!s_connected) return ESP_ERR_INVALID_STATE;
    return esp_avrc_ct_send_passthrough_cmd(
        0, ESP_AVRC_PT_CMD_STOP, ESP_AVRC_PT_CMD_STATE_PUSHED);
}

esp_err_t audio_set_volume(uint8_t volume_pct) {
    // 0-100 → 0-127 (AVRCP absolute volume)
    uint8_t avrc_vol = (volume_pct * 127) / 100;
    return esp_avrc_ct_send_set_absolute_volume_cmd(0, avrc_vol);
}

// Game phase → audio mapping
void audio_play_for_phase(game_phase_t phase) {
    switch (phase) {
    case GAME_INTRO:
    case GAME_PROFILING:
        audio_play_track(TRACK_LAB_AMBIANCE);
        break;
    case GAME_ADAPTIVE:
        audio_play_track(TRACK_THINKING);
        break;
    case GAME_CLIMAX:
        audio_play_track(TRACK_TENSION);
        break;
    case GAME_OUTRO:
        audio_play_track(TRACK_VICTORY);
        break;
    default:
        break;
    }
}
```

---

## Phase 7: Kit Packaging (10h)

### Task 7.1 — Suitcase layout and deployment guide checklist

**File:** `docs/deployment/kit_layout.md`

```markdown
# Kit Layout — 3 Suitcases

## Suitcase 1: Hub Central (Peli 1510 or equivalent carry-on)

### Layer 1 (bottom, foam-cut compartments):
- [ ] RTC_PHONE (custom ESP32 + SLIC) — in padded pouch, USB-C cable attached
- [ ] BOX-3 (ESP32-S3-BOX-3) — in original box or molded foam
- [ ] GL.iNet GL-MT3000 WiFi Router — in padded pouch
- [ ] Anker 20000mAh battery #1 — pre-charged before deployment
- [ ] Anker 20000mAh battery #2 — pre-charged before deployment

### Layer 2 (top, velcro mesh):
- [ ] 5× USB-C cables (1m, labeled: PHONE, BOX3, ROUTER, SPARE1, SPARE2)
- [ ] USB-C multiport charger (65W GaN, 4 ports) — for recharging between sessions
- [ ] Laminated quick-reference card: IP addresses, passwords, emergency contacts

### Checklist before closing:
- [ ] All devices charged > 80%
- [ ] Router SSID/password set to session config
- [ ] BOX-3 firmware updated (latest)
- [ ] RTC_PHONE scenario loaded (zacus_v3_complete)
```

**File:** `docs/deployment/15min_setup_checklist.md`

```markdown
# 15-Minute Deployment Checklist

## Before Leaving (at home/office, 30 min):
- [ ] Charge all batteries to 100% (2× 20000mAh + 2× puzzle packs)
- [ ] Verify firmware on BOX-3 and all puzzle ESP32s
- [ ] Pre-generate XTTS MP3 pool if new phrases added
- [ ] Test ESP-NOW mesh (run: `./tools/dev/zacus.sh espnow-test`)
- [ ] Load scenario: `zacus_v3_complete`, set target_duration
- [ ] Print 6 QR codes (A5 laminated), pack NFC symbol tiles (12 pieces)

## On-Site Setup (15 min total):

### Minutes 1-2: Hub (Suitcase 1)
- [ ] Place RTC_PHONE on table, connect USB-C to battery #1
- [ ] Place BOX-3 on mini tripod, angle camera toward room center
- [ ] Connect BOX-3 USB-C to battery #1 (same battery, splitter)
- [ ] Power router from battery #2 USB-C
- [ ] Wait for green LEDs on all 3 devices (~30 seconds)

### Minutes 3-7: Puzzles (Suitcase 2)
- [ ] P1 Boîte Sonore → place on side table, USB-C to battery pack
- [ ] P2 Breadboard Magnétique → unfold, place on floor, USB-C power
- [ ] P4 Poste Radio → place on windowsill or shelf
- [ ] P5 Télégraphe → place on desk with morse reference card nearby
- [ ] P6 Tablette Symboles → place on table, arrange 12 symbol tiles around it
- [ ] P7 Coffre Final → place prominent center, LOCKED, USB-C power
- [ ] Hide 6 QR codes A5 in pre-defined positions (see QR placement map)

### Minutes 8-10: Ambiance (Suitcase 3)
- [ ] Place Bluetooth speaker, pair with BOX-3 (hold pairing button 3s)
- [ ] Plug USB LED strip (3m) — room perimeter
- [ ] Place 4 signage panels: entrance, rules, zones, "Laboratoire du Pr. Zacus"

### Minutes 11-13: ESP-NOW Mesh Verification
- [ ] BOX-3 screen shows "MESH: 6/6 nodes connected" (wait up to 2 min)
- [ ] If a node missing: press reset on that puzzle ESP32
- [ ] Quick puzzle test: press any button on P1, verify BOX-3 receives event

### Minutes 14-15: Game Master Config
- [ ] On BOX-3 touchscreen: Settings → Target Duration → [30/45/60/75/90 min]
- [ ] Settings → WiFi → enter venue WiFi password (for live XTTS)
  OR → Offline Mode (SD card MP3 fallback)
- [ ] Press "READY" — system runs self-test, shows green on all 7 puzzles
- [ ] Brief game master walkthrough with BOX-3 (puzzle order, scoring)

## Teardown (10 min):
- [ ] BOX-3: End Game → Export Stats (PDF or QR)
- [ ] Power off all puzzles (USB-C disconnect)
- [ ] Collect all 12 NFC tiles (count verification)
- [ ] Collect 6 QR codes
- [ ] Pack suitcases in reverse order
- [ ] Recharge all batteries before next session
```

### Task 7.2 — Power management assignments

```markdown
# Power Management — V3 Kit

## Battery Assignments

| Battery | Capacity | Powers | Estimated Runtime |
|---------|----------|--------|-------------------|
| Anker #1 (hub) | 20000mAh | RTC_PHONE (0.5W) + BOX-3 (2W) + Router (3W) | 3.5h |
| Anker #2 (hub spare) | 20000mAh | Spare / USB charger for puzzles | — |
| Puzzle pack A | 10000mAh | P1 (1W) + P2 (2W) + P4 (internal 18650) | 8h |
| Puzzle pack B | 10000mAh | P5 (0.5W) + P6 (0.5W) + P7 (1W) | 15h |

## Charging Schedule
- Full charge: 3h before session (45W GaN charger, 4 ports simultaneously)
- Post-session: recharge immediately, never store depleted

## Offline Mode
- All puzzles work without WiFi via ESP-NOW (always available)
- NPC uses SD card MP3s (pre-generated by generate_xtts_pool.py)
- BOX-3 QR scanner works locally (no internet required)
- Analytics cached locally, uploaded when WiFi available post-game
```

---

## Phase 8: Beta Playtest (8h)

### Task 8.1 — Playtest protocol

**File:** `docs/playtest/beta_protocol.md`

```markdown
# Beta Playtest Protocol — 2 Groups Minimum

## Group Selection
- Group 1: TECH profile — 4-6 adults with technical background (developers, engineers)
- Group 2: NON_TECH profile — 4-6 adults/teens without technical background

## Pre-playtest Setup (30 min before each group)
- [ ] Full 15-min deployment checklist completed
- [ ] Target duration set to 60 min
- [ ] Game master briefed (silent observation role)
- [ ] Video recording camera positioned (with consent)
- [ ] NPS questionnaire prepared (paper or digital)

## During Playtest: Observations to Record
For each puzzle, note:
- Start time, end time, solve time
- Number of hints requested
- Wrong attempts
- Verbal reactions ("c'est quoi ça ?", "trop difficile", "ah voilà !")
- Moments of confusion or frustration
- Moments of delight or discovery

## NPC Profiling Accuracy Check
After Phase 2 (profiling puzzles):
- [ ] Record: P1 solve time, P2 solve time
- [ ] Record: NPC classification (TECH/NON_TECH/MIXED)
- [ ] Verify classification matches your subjective assessment of the group
- [ ] Target: >80% correct classification

## Duration Accuracy Check
- [ ] Record: target_duration configured
- [ ] Record: actual_duration (game end time - game start time)
- [ ] Target: actual within ±10% of target

## Post-Playtest: NPS Survey
Questions (1-10 scale):
1. How likely are you to recommend this experience to a friend? (NPS)
2. How engaging was Professor Zacus as a character?
3. How appropriate was the difficulty level for your group?
4. How clear were the puzzle instructions?
5. How impressed were you by the technical execution?
Open questions:
- What was your favorite puzzle? Why?
- What was the most confusing moment?
- Any suggestions?

## Success Criteria
- [ ] Both groups complete the game (reach GAME_OUTRO phase)
- [ ] Average NPS > 8/10
- [ ] Setup time < 15 minutes (timed)
- [ ] Duration accuracy within ±10%
- [ ] NPC profiling correct for both groups
- [ ] Zero hardware failures during playtest
- [ ] Voice quality: at least 3/5 players cannot distinguish XTTS from human
```

### Task 8.2 — Metrics collection script

**File:** `tools/analytics/collect_playtest_metrics.py`

```python
#!/usr/bin/env python3
"""
collect_playtest_metrics.py — Post-playtest metrics aggregation.
Reads BOX-3 analytics export JSON + NPS survey CSV → generates playtest report.
Usage: python3 tools/analytics/collect_playtest_metrics.py --session session_001
"""
import argparse
import json
import csv
import pathlib
from datetime import datetime

def load_session_data(session_id: str) -> dict:
    path = pathlib.Path(f"analytics/{session_id}.json")
    if not path.exists():
        raise FileNotFoundError(f"Session data not found: {path}")
    with open(path) as f:
        return json.load(f)

def load_nps_survey(session_id: str) -> list[dict]:
    path = pathlib.Path(f"analytics/{session_id}_nps.csv")
    if not path.exists():
        return []
    with open(path) as f:
        return list(csv.DictReader(f))

def compute_nps(responses: list[dict]) -> float:
    if not responses:
        return 0.0
    scores = [int(r.get("nps_score", 0)) for r in responses if r.get("nps_score")]
    if not scores:
        return 0.0
    promoters  = sum(1 for s in scores if s >= 9)
    detractors = sum(1 for s in scores if s <= 6)
    return ((promoters - detractors) / len(scores)) * 100

def generate_report(session_id: str) -> dict:
    data = load_session_data(session_id)
    nps_responses = load_nps_survey(session_id)

    report = {
        "session_id": session_id,
        "timestamp": datetime.now().isoformat(),
        "game": {
            "target_duration_min": data.get("target_duration_ms", 0) // 60000,
            "actual_duration_min": data.get("actual_duration_ms", 0) // 60000,
            "duration_accuracy_pct": 0,
            "group_profile": data.get("group_profile", "UNKNOWN"),
            "profiling_correct": data.get("profiling_correct", None),
            "puzzles_solved": data.get("puzzles_solved", 0),
            "total_puzzles": data.get("total_puzzles", 0),
            "hints_used": data.get("hints_used", 0),
            "score": data.get("final_score", 0),
        },
        "puzzles": data.get("puzzle_stats", {}),
        "npc": {
            "phrases_played": data.get("phrases_played", 0),
            "tts_backend_used": data.get("tts_backend", "unknown"),
            "tts_latency_avg_ms": data.get("tts_latency_avg_ms", 0),
        },
        "nps": {
            "score": compute_nps(nps_responses),
            "responses": len(nps_responses),
        },
        "hardware": {
            "failures": data.get("hardware_failures", []),
            "setup_time_min": data.get("setup_time_s", 0) / 60,
        },
    }

    # Duration accuracy
    target = report["game"]["target_duration_min"]
    actual = report["game"]["actual_duration_min"]
    if target > 0:
        report["game"]["duration_accuracy_pct"] = abs(actual - target) / target * 100

    # Criteria check
    report["criteria"] = {
        "nps_above_8": report["nps"]["score"] >= 8,
        "setup_under_15min": report["hardware"]["setup_time_min"] < 15,
        "duration_within_10pct": report["game"]["duration_accuracy_pct"] <= 10,
        "no_hardware_failures": len(report["hardware"]["failures"]) == 0,
        "game_completed": report["game"]["puzzles_solved"] == report["game"]["total_puzzles"],
    }

    all_passed = all(report["criteria"].values())
    report["beta_passed"] = all_passed

    return report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--session", required=True)
    parser.add_argument("--output", default=None)
    args = parser.parse_args()

    report = generate_report(args.session)

    out_path = args.output or f"analytics/{args.session}_report.json"
    with open(out_path, "w") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(f"Report: {out_path}")
    print(f"NPS Score: {report['nps']['score']:.1f}/100")
    print(f"Duration accuracy: ±{report['game']['duration_accuracy_pct']:.1f}%")
    print(f"Beta PASSED: {report['beta_passed']}")

    for criterion, passed in report["criteria"].items():
        status = "PASS" if passed else "FAIL"
        print(f"  [{status}] {criterion}")

if __name__ == "__main__":
    main()
```

---

## Phase 9: Commercial Pipeline (15h)

### Task 9.1 — Landing page structure (Astro)

**File:** `web/landing/src/pages/zacus.astro` structure:

```markdown
# Landing Page — lelectronrare.fr/zacus

## Sections:

### Hero
- Headline: "Le Mystère du Professeur Zacus"
- Subheadline: "L'escape room portable, adaptatif, et alimenté par l'IA."
- CTA: "Demander une démonstration" → contact form anchor
- Background: atmospheric lab photo or dark gradient

### Proof (Video embed)
- 2-3 min demo video (teaser edit)
- Highlight: setup in 15 min, Professor voice, puzzle variety

### Puzzle Showcase (3-up cards)
- "7 énigmes physiques" — photo montage of puzzles
- "IA adaptative" — diagram of TECH/NON_TECH/MIXED profiles
- "Voix clonée" — Professor Zacus voice waveform / audio preview

### Pricing Table (3 tiers)

| | Animation | Location | Kit |
|---|---|---|---|
| **Prix** | 800-1500€/séance | 300-500€/week-end | 3000-5000€ |
| **Joueurs** | 4-15 | 4-15 | 4-15 |
| **Durée** | 2-3h sur place | Ven→Lun | Achat définitif |
| **Inclus** | Animateur, débrief | Guide, support tél. | Firmware, 3 scénarios, 1h formation |
| **Idéal pour** | Teambuilding, événements | Associations, anniversaires | Escape rooms, musées |
| **CTA** | Demander un devis | Réserver | Acheter |

### Testimonials
- Quote 1 (tech group beta): "Le Professeur Zacus nous a bluffés. On ne savait pas que c'était une IA."
- Quote 2 (non-tech group): "Parfaitement adapté à notre groupe. Ni trop facile, ni trop difficile."

### FAQ
- "Faut-il du WiFi ?" → Non, fonctionne entièrement en offline.
- "Combien de temps pour installer ?" → 15 minutes avec le guide lamifié.
- "Le Professeur parle-t-il vraiment ?" → Oui, voix clonée par IA (XTTS-v2).
- "Combien de joueurs maximum ?" → 15, minimum 4.
- "Le kit est-il solide pour 50+ sessions ?" → Oui, conçu pour >100 deployments.

### Contact Form
- Fields: Nom, Email, Téléphone, Tier (Animation/Location/Kit), Message
- Submit → Dolibarr webhook → Lead created automatically
```

### Task 9.2 — Demo video script (2h)

**File:** `docs/commercial/demo_video_script.md`

```markdown
# Demo Video Script — Le Mystère du Professeur Zacus (2:30)

## 0:00-0:15 — Hook
VISUAL: Dark lab setting. Phone rings. Dramatic silence.
AUDIO: Professor Zacus voice: "Bienvenue dans mon laboratoire portable…"
TEXT OVERLAY: "Le Mystère du Professeur Zacus"

## 0:15-0:30 — Problem/Context
VISUAL: Time-lapse of 3 suitcases being opened and set up in 15 minutes
AUDIO: Upbeat background music
TEXT OVERLAY: "Setup: 15 minutes. Anywhere."

## 0:30-1:00 — Puzzle Showcase
VISUAL: Quick cuts of each puzzle in action
- P1: Arcade buttons lighting up, melody playing
- P2: Magnetic components snapping into place, LED lights up
- P5: Morse code key tapping, dot-dash rhythm
- P6: NFC tile placed, satisfying beep
- P7: Code entered, servo clicking, chest opening
AUDIO: Ambient lab sounds + player reactions

## 1:00-1:30 — AI Adaptation
VISUAL: Split screen — TECH group (fast, circuit puzzle first)
        vs NON_TECH group (slower, symbols puzzle first)
AUDIO: Professor Zacus voice: "Groupe technique détecté. Excellente nouvelle…"
TEXT OVERLAY: "Adapté à chaque groupe en temps réel"

## 1:30-1:50 — Voice Clone
VISUAL: XTTS waveform + BOX-3 screen showing "XTTS-v2 LIVE"
AUDIO: Professor Zacus: "Vous allez trop vite. J'ai décidé d'ajouter… une petite surprise."
TEXT OVERLAY: "Voix clonée par IA — indiscernable du réel"

## 1:50-2:10 — Victory + NPS
VISUAL: Chest opening + team celebrating + score display on BOX-3
AUDIO: Victory fanfare (AudioCraft)
TEXT OVERLAY: "NPS > 9/10 en bêta"

## 2:10-2:30 — CTA
VISUAL: 3 pricing cards (Animation / Location / Kit)
AUDIO: Simple piano
TEXT OVERLAY: "À partir de 800€ — lelectronrare.fr/zacus"
FINAL FRAME: Logo + contact info
```

### Task 9.3 — One-pager PDF content (1h)

**File:** `docs/commercial/one_pager_content.md`

```markdown
# One-Pager: Le Mystère du Professeur Zacus

## Recto

**[LOGO] L'Electron Rare**

### Le Mystère du Professeur Zacus
*L'escape room portable, adaptatif, et alimenté par l'IA*

---

**7 énigmes physiques** — Séquence sonore, circuit LED, fréquence radio,
code morse, symboles alchimiques, QR treasure, coffre final

**IA adaptative** — Le Professeur profil votre groupe (tech/non-tech) et
ajuste la difficulté en temps réel pour une expérience toujours calibrée

**Voix clonée** — Professeur Zacus parle en direct via XTTS-v2 (RTX 4090)
avec fallback Piper TTS (toujours disponible)

**3 valises** — Déployable en 15 minutes, partout, sans technicien

---

| | Animation | Location | Kit |
|---|---|---|---|
| Prix | 800–1500€/séance | 300–500€/week-end | 3000–5000€ |
| Joueurs | 4–15 | 4–15 | 4–15 |
| Idéal | Teambuilding, événements | Associations, musées | Escape rooms |

---

*"Le Professeur nous a bluffés. On ne savait pas que c'était une IA."*
— Groupe bêta, novembre 2026

---

**Contact:** clement@lelectronrare.fr | lelectronrare.fr/zacus | +33 6 XX XX XX XX

## Verso (Technical sheet for kit buyers)

**Architecture technique:**
- Hub: BOX-3 (ESP32-S3) + RTC_PHONE custom
- Communication: ESP-NOW mesh, 7 noeuds, offline-first
- TTS: XTTS-v2 (GPU) → Piper TTS → MP3 SD (3 niveaux de fallback)
- Audio ambiance: 6 pistes pré-générées AudioCraft MusicGen
- Firmware: ESP-IDF, C, FreeRTOS, PlatformIO (open source)
- Scénarios: YAML → Runtime 3 IR (compilateur Python inclus)

**BOM indicatif:** ~642€ (électronique + boîtiers + valises)
**Marge kit:** ~83% au tarif 3000-5000€

**Durabilité:** Conçu pour >100 déploiements. Boîtiers 3D imprimés ABS.
Composants industriels (Reed switches, NTAG213, SG90).

**Mises à jour firmware:** OTA via WiFi ou USB-C. 1 an inclus dans le kit.

**Support:** 1h de formation incluse. Documentation complète.
Support premium disponible (500€/an).
```

### Task 9.4 — Dolibarr pipeline configuration (2h)

**File:** `docs/commercial/dolibarr_setup.md`

```markdown
# Dolibarr Commercial Pipeline — Zacus V3

## Product Catalog

### Products to create in Dolibarr:

1. **ZACUS-ANIM-1** — Animation Professeur Zacus (demi-journée)
   - Type: Service
   - Prix vente HT: 800€
   - Remise possible: 10% (corporate bulk)

2. **ZACUS-ANIM-2** — Animation Professeur Zacus (journée complète)
   - Type: Service
   - Prix vente HT: 1500€

3. **ZACUS-LOC-WE** — Location Kit Zacus (week-end)
   - Type: Service (location)
   - Prix vente HT: 400€
   - Caution: 1000€ (facturé + remboursé séparé)

4. **ZACUS-KIT-STD** — Kit Zacus Standard (3 valises + firmware + 3 scénarios)
   - Type: Produit
   - Prix vente HT: 3500€
   - Coût de revient: 642€
   - Inclus: 1h formation, 1 an mises à jour firmware

5. **ZACUS-KIT-PRO** — Kit Zacus Pro (+ support premium 1 an)
   - Type: Produit
   - Prix vente HT: 4500€
   - Inclus: tout Standard + support téléphonique, 2 scénarios supplémentaires

6. **ZACUS-SCENARIO** — Scénario supplémentaire
   - Type: Service
   - Prix vente HT: 200€

7. **ZACUS-SUPPORT-AN** — Support premium annuel
   - Type: Service récurrent
   - Prix vente HT: 500€/an

## Pipeline Stages (CRM)

```
NOUVEAU LEAD
    ↓ (48h max)
QUALIFICATION — Quel tier ? Quel contexte ? Budget ?
    ↓ (1 semaine)
DEMO PLANIFIÉE — Vidéo call ou démo en présentiel
    ↓ (1 semaine)
DEVIS ENVOYÉ — Proposé via Dolibarr, signé électroniquement
    ↓ (2 semaines)
COMMANDE CONFIRMÉE — Acompte 30%
    ↓
LIVRAISON / ANIMATION
    ↓
FACTURE FINALE — Solde 70%
    ↓
SUIVI — J+7: NPS, J+30: upsell scénarios, J+365: renouvellement support
```

## Devis Template (Dolibarr)

Objet: Devis — Le Mystère du Professeur Zacus — [Tier] — [Client]

Lignes:
- [Produit] × 1 = [Prix HT]
- Transport et déplacement (animation uniquement) = [forfait]
- Remise éventuelle
- Total HT / TVA 20% / Total TTC

Conditions: Acompte 30% à la commande, solde à la livraison.
Validité: 30 jours.

## Landing Page → Dolibarr Webhook

Contact form on lelectronrare.fr/zacus submits to:
`POST https://erp.saillant.cc/api/index.php/leads`

Payload:
```json
{
  "lastname": "...",
  "email": "...",
  "phone": "...",
  "note_private": "Tier: [Animation/Location/Kit]\nMessage: ...",
  "status": 1,
  "source_id": 3
}
```

Auth: Bearer token from Dolibarr REST API.
```

---

## Verification Commands

```bash
# Phase 1: YAML validation
make content-checks
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v3_complete.yaml
python3 -m pytest tools/scenario/test_v3_compile.py -v

# Phase 3: Firmware build
cd ESP32_ZACUS && pio run -e p1_son
cd ESP32_ZACUS && pio run -e p5_morse
cd ESP32_ZACUS && pio run -e p6_symboles
cd ESP32_ZACUS && pio run -e p7_coffre

# Phase 4: NPC unit tests
make runtime3-test

# Phase 5: XTTS health check
curl http://kxkm-ai:5002/health
python3 tools/tts/generate_xtts_pool.py --dry-run --host kxkm-ai:5002

# Phase 6: AudioCraft
ssh kxkm@kxkm-ai "docker compose -f ~/zacus-tts/docker-compose.audiocraft.yml run audiocraft"

# Phase 8: Analytics
python3 tools/analytics/collect_playtest_metrics.py --session beta_001
```

---

## File Manifest

| File | Phase | Status |
|------|-------|--------|
| `game/scenarios/zacus_v3_complete.yaml` | 1 | To create |
| `game/config/v3_constants.yaml` | 1 | To create |
| `game/scenarios/npc_phrases.yaml` | 1 | Extend existing |
| `tools/scenario/test_v3_compile.py` | 1 | To create |
| `hardware/kicad/p1_son/` | 2 | To create |
| `hardware/kicad/p4_radio/` | 2 | To create |
| `hardware/kicad/p5_morse/` | 2 | To create |
| `hardware/kicad/p6_symboles/` | 2 | To create |
| `hardware/kicad/p7_coffre/` | 2 | To create |
| `hardware/bom/zacus_v3_bom.csv` | 2 | To create |
| `hardware/enclosures/p1_son_box.scad` | 2 | To create |
| `hardware/enclosures/p4_radio_box.scad` | 2 | To create |
| `ESP32_ZACUS/puzzles/common/espnow_slave.h` | 3 | To create |
| `ESP32_ZACUS/puzzles/common/espnow_slave.c` | 3 | To create |
| `ESP32_ZACUS/puzzles/p1_son/main/p1_son_main.c` | 3 | To create |
| `ESP32_ZACUS/puzzles/p5_morse/main/p5_morse_main.c` | 3 | To create |
| `ESP32_ZACUS/puzzles/p6_symboles/main/p6_symboles_main.c` | 3 | To create |
| `ESP32_ZACUS/puzzles/p7_coffre/main/p7_coffre_main.c` | 3 | To create |
| `ESP32_ZACUS/ui_freenove_allinone/src/npc/espnow_master.cpp` | 3 | To create |
| `ESP32_ZACUS/ui_freenove_allinone/include/npc/npc_engine.h` | 4 | Extend existing |
| `ESP32_ZACUS/ui_freenove_allinone/src/npc/npc_engine.cpp` | 4 | Extend existing |
| `ESP32_ZACUS/ui_freenove_allinone/src/npc/game_coordinator.cpp` | 4 | To create |
| `tools/tts/docker-compose.xtts.yml` | 5 | To create |
| `tools/tts/voice_recording_guide.md` | 5 | To create |
| `tools/tts/generate_xtts_pool.py` | 5 | To create |
| `tools/audio/docker-compose.audiocraft.yml` | 6 | To create |
| `tools/audio/generate_tracks.py` | 6 | To create |
| `ESP32_ZACUS/ui_freenove_allinone/src/audio_ble_control.cpp` | 6 | To create |
| `docs/deployment/kit_layout.md` | 7 | To create |
| `docs/deployment/15min_setup_checklist.md` | 7 | To create |
| `docs/playtest/beta_protocol.md` | 8 | To create |
| `tools/analytics/collect_playtest_metrics.py` | 8 | To create |
| `web/landing/src/pages/zacus.astro` | 9 | To create |
| `docs/commercial/demo_video_script.md` | 9 | To create |
| `docs/commercial/one_pager_content.md` | 9 | To create |
| `docs/commercial/dolibarr_setup.md` | 9 | To create |

---

## Success Criteria (from spec)

| Criterion | Target | Measured by |
|-----------|--------|-------------|
| Beta playtest completion | 2 groups complete game | collect_playtest_metrics.py |
| NPS | > 8/10 | NPS survey |
| Setup time | < 15 minutes | Timed during playtest |
| Offline mode | 100% functional | Playtest without WiFi |
| Duration accuracy | ±10% of target | collect_playtest_metrics.py |
| NPC profiling | > 80% correct | Manual observer log |
| Voice quality | > 60% can't distinguish XTTS from human | Blind test survey |
| Kit durability | > 50 deployments | Mechanical design review |
