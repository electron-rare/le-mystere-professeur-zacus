# Demo May 1st — Playable Escape Room + AI Professor Zacus

**Date**: 2026-04-02

## Goal

Playable escape room (3 puzzles) with conversational AI Professor Zacus (LLM on KXKM-AI RTX 4090).

## Hardware

- ESP32-S3-BOX-3: lab terminal (touchscreen, mic, speaker, QR camera)
- ESP32 Audio Kit + PSRAM: professor's telephone (speaker, validation button)

## Architecture

```
BOX-3 (Runtime 3, NPC state, QR) ←WiFi→ KXKM-AI (Ollama devstral, WhisperX STT)
                                  ←WiFi→ Tower (Piper TTS :8001, tom-medium)
BOX-3 ←WiFi/ESP-NOW→ Audio Kit (telephone speaker, button GPIO)
```

## Voice Pipeline

Player speaks → BOX-3 mic → ESP-SR wake word → audio → KXKM-AI WhisperX STT → text → Ollama devstral (Zacus system prompt + game state) → response text → Tower Piper TTS → WAV → Audio Kit speaker

Target latency: ~3-4s.

## Fallback

- LLM/TTS down → pre-generated MP3 on SD card (generate_npc_pool.py)
- WiFi down → full offline mode (Runtime 3 local + MP3)

## Game Flow

1. Intro: telephone rings, professor welcomes players
2. Puzzle 1: BOX-3 displays clue, physical props, QR/button validation
3. Puzzle 2: increased difficulty, anti-cheat hints if stuck (3 levels)
4. Puzzle 3: finale
5. Ending: adaptive speech (time, hints used, NPC mood)

## To Implement

1. Zacus system prompt (personality + dynamic game context)
2. STT endpoint on KXKM-AI (WhisperX HTTP)
3. BOX-3 ↔ Audio Kit communication (ESP-NOW or HTTP)
4. 3 puzzle content (props, clues, validation logic)
5. Scenario v3 complete YAML
6. Full MP3 fallback pool
7. End-to-end test run

## Already Working

- NPC engine firmware (mood, TTS client)
- Runtime 3 engine on ESP32
- Piper TTS on Tower:8001
- generate_npc_pool.py
- Hints engine (mascarade /hints/ask)
- Voice bridge hint routing
- Frontend (38 tests passing, backup web mode)
