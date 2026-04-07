# Zacus PLIP Téléphone — Design Spec

## Overview

PCB custom compact pour le module téléphone de l'escape room "Le Mystère du Professeur Zacus". Remplace le dev kit ESP32 Audio Kit V2.2 (A252) par une carte intégrée ESP32 + SLIC Si3210, pilotant un combiné téléphone rétro via RJ9. Utilise un ESP32 classique (pas S3) pour le support Bluetooth Classic (A2DP audio streaming + HFP hands-free).

## Goals

- Piloter un combiné téléphone rétro (sonnerie, audio bidirectionnel, détection décrochage)
- Recevoir des commandes et du stream audio TTS via WiFi (Piper TTS sur Tower:8001)
- Bluetooth Classic A2DP (recevoir audio streaming depuis téléphone/tablette) et HFP (hands-free profile)
- Stocker des fichiers audio locaux sur SD card
- Communiquer avec l'UI ESP8266/RP2040 via UART
- Format compact, fabricable chez JLCPCB avec composants LCSC
- Premier projet du pipeline EDA MCP complet (sourcing → schéma → PCB → review → JLCPCB)

## Non-goals

- Pas de haut-parleur externe (audio uniquement via le combiné)
- Pas d'amplificateur de puissance séparé
- Pas de codec audio externe (le Si3210 a un codec intégré)

## Hardware Architecture

```
                    USB-C
                      │
                   [ESD + LDO 3.3V]
                      │
    ┌─────────────────┼─────────────────┐
    │                 │                 │
[ESP32]      [Si3210 SLIC]     [SD Card]
  SPI master ──── SPI slave        SPI slave
  PCM/I2S  ────── PCM codec
  GPIO ─────────── RESET, INT
  UART TX/RX ─── → JST UI link
  WiFi ──────── → Tower/Piper TTS
                      │
                 [Transfo 600Ω]
                      │
                   [RJ9 4P4C]
                      │
                 [Combiné rétro]
```

### Blocs fonctionnels

1. **ESP32-WROOM-32E** — MCU principal, WiFi, Bluetooth Classic (A2DP + HFP), SPI master, I2S
2. **Si3210-E-FM** — SLIC, codec PCM intégré, génération sonnerie -72V, détection off-hook, contrôle SPI
3. **CP2102N** — USB-UART bridge pour programmation et debug série
4. **Alimentation** — USB-C (5V) + LDO ME6211C33 (3.3V). Le Si3210 intègre son propre DC-DC pour la ring voltage.
5. **Micro SD** — Slot micro SD en SPI, stockage audio (MP3, WAV, OPUS)
6. **UART UI link** — JST-SH 4 pins (TX, RX, 3.3V, GND) vers UI ESP8266 OLED ou RP2040 TFT
7. **Connecteur combiné** — RJ9 4P4C standard téléphone + transfo 600Ω:600Ω d'isolation

## Pin Mapping ESP32

| Fonction | GPIO | Notes |
|----------|------|-------|
| SPI SCLK (SLIC + SD) | GPIO18 | VSPI CLK, bus partagé |
| SPI MOSI | GPIO23 | VSPI MOSI |
| SPI MISO | GPIO19 | VSPI MISO |
| SLIC CS | GPIO5 | Si3210 chip select |
| SD CS | GPIO15 | SD card chip select |
| SLIC INT | GPIO4 | Si3210 interrupt (off-hook, ring done) |
| SLIC RESET | GPIO2 | Si3210 hardware reset |
| I2S BCLK | GPIO26 | I2S bit clock vers Si3210 PCM |
| I2S LRCLK | GPIO25 | I2S word select / PCM FSYNC |
| I2S DOUT | GPIO22 | ESP32 → Si3210 (audio vers combiné) |
| I2S DIN | GPIO35 | Si3210 → ESP32 (audio depuis micro, input-only) |
| UART TX (UI) | GPIO17 | UART2 TX → UI link |
| UART RX (UI) | GPIO16 | UART2 RX ← UI link |
| UART TX (debug) | GPIO1 | UART0 TX (USB-UART bridge CP2102/CH340) |
| UART RX (debug) | GPIO3 | UART0 RX |
| LED status | GPIO13 | LED power/status |

Note: L'ESP32 classique n'a pas d'USB natif. Le USB-C utilise un bridge UART (CP2102 ou CH340) pour la programmation et le debug série.

## BOM

| # | Composant | Ref LCSC | Qté | Prix unit | Rôle |
|---|-----------|----------|-----|-----------|------|
| 1 | ESP32-WROOM-32E-N16 | C701341 | 1 | ~$2.80 | MCU + WiFi + BT Classic |
| 2 | Si3210-E-FM | C6295850 | 1 | $2.70 | SLIC + codec |
| 3 | Transfo audio 600Ω:600Ω | TBD | 1 | ~$0.50 | Isolation ligne téléphone |
| 4 | ME6211C33M5G-N | C82942 | 1 | $0.10 | LDO 3.3V 500mA |
| 5 | USB-C 16pin SMD | C2765186 | 1 | $0.15 | Alimentation + UART debug |
| 5b | CP2102N (USB-UART bridge) | C6568 | 1 | $1.50 | Programmation + debug série |
| 6 | Micro SD slot push-push | C585353 | 1 | $0.20 | Stockage audio |
| 7 | RJ9 4P4C female | TBD | 1 | ~$0.30 | Connecteur combiné |
| 8 | JST-SH 4pin SMD | C265021 | 1 | $0.10 | UART UI link |
| 9 | Capas 100nF 0402 | C307331 | 10 | $0.01 | Découplage |
| 10 | Capas 10µF 0805 | C19702 | 4 | $0.02 | Bulk bypass |
| 11 | Résistances 10k 0402 | C25744 | 4 | $0.01 | Pull-ups SPI |
| 12 | LED status 0402 | C2286 | 2 | $0.02 | Power + status |
| 13 | ESD protection USB | C7519 | 1 | $0.10 | TVS diode array |
| | **Total estimé** | | | **~$8-9** | |

Note: Le transfo 600Ω et le RJ9 doivent être sourcés via les MCPs JLCPCB (`jlcmcp-remote` ou `jlcpcb-search`). Ce seront les premières recherches du pipeline EDA.

## PCB Specs

| Paramètre | Valeur |
|-----------|--------|
| Dimensions | < 60 x 40 mm |
| Couches | 2 (top + bottom) |
| Épaisseur | 1.6mm standard |
| Finish | HASL lead-free |
| Cuivre | 1oz |
| Min trace/space | 0.15mm / 0.15mm |
| Min via | 0.3mm drill |
| Assemblage | JLCPCB SMT (top side) |

### Layout guidelines

- ESP32 en haut, antenne WiFi dégagée vers le bord du PCB (pas de cuivre sous l'antenne)
- Si3210 au centre, proche du transfo et du RJ9
- USB-C sur un bord court
- SD card sur un bord accessible
- JST UI link sur un bord latéral
- Plan de masse continu sur bottom layer

## Firmware

Le firmware existant `slic-phone-esp32` sera adapté :

1. **Driver Si3210** — remplace le driver ES8388. SPI init, registre config, ring control, off-hook interrupt.
2. **Audio path** — PCM bidirectionnel via I2S. Lecture MP3/WAV depuis SD → PCM out. Micro combiné → PCM in.
3. **WiFi** — REST endpoint pour commandes (ring, play, stop, volume). WebSocket pour stream Piper TTS.
4. **UART UI link** — protocole v2 inchangé (57600 baud).
5. **Scénarios** — Runtime 3 IR, déclenchement sur off-hook, enchaînement audio.

## Fonctionnement

1. **Sonnerie** — Système Zacus envoie commande WiFi `POST /ring` → ESP32 commande Si3210 via SPI → Si3210 génère -72V ring → combiné sonne
2. **Décrochage** — Si3210 détecte off-hook → INT GPIO4 → ESP32 notifie le système + lance le scénario audio
3. **Audio sortie** — ESP32 lit MP3 depuis SD, stream Piper TTS via WiFi, ou reçoit A2DP Bluetooth → décode → PCM vers Si3210 → écouteur combiné
4. **Audio entrée** — Micro combiné → Si3210 codec → PCM vers ESP32 (voice activity detection, enregistrement optionnel, HFP uplink)
5. **Bluetooth A2DP** — ESP32 en sink A2DP : reçoit audio streaming depuis téléphone/tablette du game master → route vers combiné
6. **Bluetooth HFP** — ESP32 en hands-free : le combiné rétro devient un kit mains-libres BT (appel entrant → sonnerie Si3210 → audio bidirectionnel)
7. **Raccrochage** — Si3210 détecte on-hook → INT → ESP32 arrête l'audio, notifie le système

## Pipeline EDA

Ce projet sert de **premier test complet** du pipeline EDA MCP :

| Étape | MCP | Action |
|-------|-----|--------|
| 1. Sourcing | `jlcmcp-remote`, `jlcpcb-search` | Trouver transfo 600Ω, RJ9, vérifier stock Si3210 |
| 2. Schéma | `kicad-sch` | Créer le schéma dans KiCad |
| 3. PCB | `kicad-design` | Placement, routing, design rules |
| 4. Autoroute | `kicad-design` (Freerouting) | Autoroute 2 layers |
| 5. Review | kicad-happy skills | DFM, EMC, thermique |
| 6. Export | `kicad-design` | Gerbers, BOM LCSC, CPL |
| 7. Commande | JLCPCB | Upload + commande assemblée |

## Success Criteria

- [ ] Schéma validé (ERC clean)
- [ ] PCB routé (DRC clean)
- [ ] DFM review passé (kicad-happy)
- [ ] BOM 100% sourcée LCSC
- [ ] Gerbers exportés, CPL avec rotations corrigées
- [ ] Commande JLCPCB passée
- [ ] PCB reçu, combiné sonne et audio fonctionne
