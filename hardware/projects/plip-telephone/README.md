# PLIP Téléphone — Zacus Escape Room

## Description

Custom PCB: ESP32-WROOM-32E + Si3210 SLIC for driving a retro telephone
handset in the Zacus escape room. Bluetooth Classic A2DP/HFP support.

## Features

- Si3210 SLIC: ring generation (-72V), off-hook detect, PCM audio codec
- Bluetooth Classic: A2DP sink + HFP hands-free
- WiFi: commands + Piper TTS streaming from Tower:8001
- Micro SD: local audio storage (MP3, WAV)
- UART: UI link to ESP8266/RP2040 display
- RJ9 4P4C: standard telephone handset connector
- USB-C: power + programming via CP2102N bridge

## PCB Specs

- Dimensions: < 60 x 40 mm
- Layers: 2 (HASL lead-free)
- Assembly: JLCPCB SMT top side

## BOM — Confirmed LCSC Parts

| # | Component | LCSC | Price | Status |
|---|-----------|------|-------|--------|
| 1 | ESP32-WROOM-32E-N16 | C701341 | ~$2.80 | Confirmed |
| 2 | Si3210-E-FM (QFN-38) | C6295850 | $2.70 | Confirmed |
| 3 | CP2102N-A02-GQFN24 | C6568 | ~$1.50 | Confirmed |
| 4 | ME6211C33M5G-N (LDO 3.3V) | C82942 | $0.10 | Confirmed |
| 5 | USB-C 16pin SMD | C2765186 | $0.15 | Confirmed |
| 6 | Micro SD slot push-push | C585353 | $0.20 | Confirmed |
| 7 | JST-SH 4pin SMD | C265021 | $0.10 | Confirmed |
| 8 | USBLC6-2SC6 (ESD) | C7519 | $0.10 | Confirmed |
| 9 | 100nF 0402 (x10) | C307331 | $0.01 | Confirmed |
| 10 | 10µF 0805 (x4) | C19702 | $0.02 | Confirmed |
| 11 | 10kΩ 0402 (x6) | C25744 | $0.01 | Confirmed |
| 12 | 1kΩ 0402 (x2) | C25512 | $0.01 | Confirmed |
| 13 | 22Ω 0402 (x2) | C25092 | $0.01 | Confirmed |
| 14 | LED green 0402 (x2) | C2286 | $0.02 | Confirmed |
| 15 | 600Ω:600Ω transformer | TBD | ~$0.50 | Source via MCP |
| 16 | RJ9 4P4C female | TBD | ~$0.30 | Source via MCP |
| 17 | 100µH inductor (DC-DC) | TBD | ~$0.20 | Source via MCP |
| 18 | 5.1kΩ 0402 (x2, USB CC) | C25905 | $0.01 | Confirmed |

**Total estimated: ~$9-10**

Items 15-17 require MCP sourcing (restart Claude Code with new .mcp.json to use jlcmcp-remote/jlcpcb-search).

## Design Spec

See `../../docs/superpowers/specs/2026-04-08-plip-telephone-design.md`
