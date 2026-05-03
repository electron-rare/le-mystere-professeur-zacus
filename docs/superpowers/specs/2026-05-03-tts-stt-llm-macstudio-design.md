# TTS/STT/LLM Migration → MacStudio M3 Ultra

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

---

## Goal

Consolidate the project's three inference workloads — Piper TTS (currently on `Tower:8001`), whisper STT (planned, no production today), and LLM inference for the hints engine + NPC dialogue (currently on `KXKM-AI` RTX 4090) — onto a single Mac Studio M3 Ultra (32 cores, 512 GB unified RAM, 7.3 TB disk). Hard cutover over a single weekend; Tower and KXKM-AI's inference roles are retired; their hosts remain for other duties (Suite Numérique on Tower, fine-tuning on KXKM-AI).

## Non-Goals

- Replacing the **client-side** voice plumbing in `ESP32_ZACUS` (the firmware keeps its WebSocket/HTTP voice bridge, only the upstream URL changes).
- Bringing up Bluetooth A2DP / HFP on PLIP — that lives in the PLIP firmware roadmap, not here.
- Migrating other Tower-hosted services (Suite Numérique, n8n) — out of scope.
- Replacing Cloudflare/Traefik routing for `*.saillant.cc` — those stay.

## Constraints

- **Hard cutover**: one weekend, no parallel run. Rollback path = restart Tower's Piper service + repoint clients back. Documented but never exercised.
- **French**: voice content stays French (NPC = Zacus voice = `tom-medium`, no English fallback).
- **Latency budget**: end-to-end voice command (ESP32 mic → STT → hints LLM → TTS → ESP32 speaker) target ≤ 3 s on M3 Ultra.
- **LAN-local**: clients reach MacStudio over the home LAN (`192.168.0.x`). Tailscale stays available for remote ops but is not the primary path.
- **Open-source bias**: prefer open weights / open-source daemons over Apple closed APIs (AVSpeechSynthesizer is rejected for the NPC voice).

---

## 1. Architecture — "Native daemons + LiteLLM proxy"

```
                 ┌─────────────────────────────────────┐
                 │  MacStudio M3 Ultra (LAN: studio.local) │
                 │                                     │
ESP32 / Atelier ─┤  :4000  LiteLLM proxy (OpenAI API)    │
                 │     │                                │
                 │     ├─→ :11434  ollama (LLM tier)     │
                 │     │       qwen2.5:7b                │
                 │     │       qwen2.5:72b-instruct-q8_0 │
                 │     │                                │
                 │     ├─→ :8200   whisper.cpp server    │
                 │     │       large-v3-turbo (FR multi) │
                 │     │                                │
                 │     └─→ :8001   Piper TTS server      │
                 │             tom-medium / siwis / upmc │
                 │                                     │
                 │  All four services managed by launchd  │
                 └─────────────────────────────────────┘
```

Mirror of the existing `Tower` pattern (LiteLLM in front, Piper underneath) so client contracts stay stable: clients keep talking OpenAI-compatible API on a single endpoint and don't care which model handles a request.

## 2. Model choices

### 2.1 TTS — Piper, status quo

| Voice | Model file | Use |
|-------|------------|-----|
| `tom-medium` | `fr_FR-tom-medium.onnx` | Zacus NPC (canonical) |
| `siwis` | `fr_FR-siwis-medium.onnx` | Auxiliary NPCs |
| `upmc` | `fr_FR-upmc-medium.onnx` | Auxiliary NPCs |

`.onnx` files copied verbatim from `clems@192.168.0.120:/path/to/piper/models/` to MacStudio. No retraining, no swap. Continuity wins.

### 2.2 STT — whisper.cpp `large-v3-turbo`

- Quantization: Q5_K (~1.5 GB on disk, fits the AppleNeural Engine + Metal GPU pipeline).
- French-multilingual model (not the `.en` variant).
- Latency budget: ≤ 400 ms on a 5 s clip on M3 Ultra (verified via `whisper.cpp/main -m large-v3-turbo.bin -f sample.wav -l fr` benchmark).
- Server: `whisper.cpp/server` with HTTP `/inference` endpoint on `:8200`.

### 2.3 LLM — dual-tier Qwen 2.5

| Model | Quant | Disk | Use case | Throughput target |
|-------|-------|------|----------|-------------------|
| `qwen2.5:7b-instruct-q8_0` | Q8 | ~7.5 GB | NPC dialogue, fast turns | ≥ 50 tok/s |
| `qwen2.5:72b-instruct-q8_0` | Q8 | ~75 GB | Hints engine reasoning, anti-cheat scoring | ≥ 12 tok/s |

Both pulled via `ollama pull`. LiteLLM routes on model name: clients ask for `npc-fast` (alias to 7B) or `hints-deep` (alias to 72B).

Both tiers fit comfortably in 512 GB unified RAM with headroom for whisper + Piper + OS (~85 GB total active inference, ~427 GB free).

## 3. Network

- **MacStudio** sits on the home LAN at the IP currently assigned to it (today: `studio.local` resolves via mDNS).
- **Static IP recommended**: pin the MacStudio at e.g. `192.168.0.150` via the home router DHCP reservation, or set a manual static lease. Clients (ESP32, atelier) hardcode the IP rather than relying on mDNS, which can be flaky on ESP32-S3.
- **Tailscale stays installed** for remote ops, not for primary client traffic.
- Existing client URLs:
  - Voice WS: `ws://192.168.0.120:8200/voice/ws` → `ws://192.168.0.150:8200/voice/ws`
  - Piper TTS: `http://192.168.0.120:8001/tts` → `http://192.168.0.150:8001/tts`
  - Hints engine: KXKM-AI URL → `http://192.168.0.150:4000/v1/chat/completions` (LiteLLM proxy)

---

## 4. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P0** | Provision MacStudio (ollama, whisper.cpp, Piper, LiteLLM, launchd plists, static IP) | All four services start on `launchd reload`, respond to `curl localhost:<port>/<probe>`. Models downloaded (~85 GB total). | 3-5 h |
| **P1** | Smoke-test each service in isolation. Compare outputs to Tower / KXKM-AI equivalents on a fixed prompt set | TTS: WAV byte-identical for `tom-medium` ; STT: same transcript on a 10-clip FR set ; LLM: hints engine response within ±10% latency of KXKM-AI baseline | 1-2 h |
| **P2** | Hard cutover (Saturday): repoint all clients to `192.168.0.150`, restart, smoke-test E2E | ESP32_ZACUS Voice WS connects ; atelier hints query resolves ; NPC speaks via TTS ; full puzzle voice loop ≤ 3 s end-to-end | 2-3 h |
| **P3** | Soak (Sunday): 24 h running, watch logs for crashes / OOM / latency creep | No service crash, no OOM, p95 latency stable at < 2 s | 24 h passive |
| **P4** | Decommission Tower Piper service + KXKM-AI ollama service. Update root CLAUDE.md Infrastructure section. Document the rollback path. | Tower's port 8001 returns connection refused ; KXKM-AI's ollama process stopped ; CLAUDE.md mentions MacStudio as primary | 1 h |

**Total wall-clock**: P0 + P1 = pre-cutover prep (5-7 h, can spread across the week). P2 + P3 + P4 = cutover weekend (3-4 h active + 24 h soak).

## 5. Risks + rollback

| Risk | Probability | Mitigation |
|------|-------------|------------|
| MacStudio fan/thermal under sustained 72B inference | Low (M3 Ultra rated for it) | `pmset -g thermlog`, monitor during P3 soak |
| LiteLLM config drift between Tower and MacStudio | Medium | Diff configs side-by-side in P0, document in commit |
| ESP32 firmware caches old IP via mDNS | Medium | Use static IP, update NVS WiFi config explicitly |
| whisper-large-v3-turbo FR accuracy regression vs current solution | Low (no current STT in prod) | This is greenfield — no baseline to regress |
| Network change breaks the VOICE_WS init order on ESP32 | Already fixed this session (network_boot_deferred guard) | Verify the guard remains active when new IP is configured |
| 24 h soak surfaces OOM | Low (85 GB / 512 GB) | macOS Activity Monitor + watch ollama process VSZ |

**Rollback procedure**: stop MacStudio's launchd services, re-enable Tower's Piper service (`systemctl start piper-tts`), re-enable KXKM-AI's ollama service, repoint clients back to `192.168.0.120` IP. Total rollback ≤ 30 min if practiced.

---

## 6. Acceptance gates before retiring Tower / KXKM-AI

- [ ] All four MacStudio daemons green for ≥ 24 h continuous (P3 soak)
- [ ] ESP32_ZACUS Voice WS reconnects after firmware reboot, no panic
- [ ] Atelier `/hints/ask` resolves with response from `qwen2.5:72b` via LiteLLM
- [ ] NPC dialogue test (`tom-medium` voice) byte-identical to Tower output on canonical phrase set
- [ ] Voice loop p95 latency ≤ 3 s (mic → STT → LLM → TTS → speaker)
- [ ] Root `CLAUDE.md` Infrastructure section updated
- [ ] Rollback drill executed once (timed) before declaring cutover final

## 7. Out of scope

- ESP-SR wake-word detection on ESP32 (separate brainstorm — voice pipeline)
- Hints engine prompt design (separate brainstorm — hints engine)
- A2DP / Bluetooth audio path on PLIP (PLIP firmware roadmap)
- Fine-tuning Qwen on Zacus-specific FR escape-room corpus (future enhancement, KXKM-AI hardware kept for that)
- Migrating Suite Numérique / n8n / Grist off Tower (different project)

## 8. Open questions

None at design stage. All decisions resolved during 2026-05-03 brainstorm:
- Architecture: native daemons + LiteLLM ✅
- TTS: Piper status quo (3 voices) ✅
- STT: whisper-large-v3-turbo Q5_K ✅
- LLM: dual-tier Qwen 2.5 7B + 72B Q8 ✅
- Network: LAN, static IP `192.168.0.150` ✅
- Cutover: hard, single weekend ✅
