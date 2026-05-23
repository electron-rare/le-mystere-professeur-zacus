# Voice pipeline roadmap — 2026-05-24

Snapshot of the voice stack after the macM1 hints + Kyutai STT + WS
fallback session. Lives here so the next session (this repo or others)
can pick up the right context without re-discovering every gotcha.

## Current state

```
ESP32 firmware (ESP-IDF feat/idf-migration)
   │
   │ WebSocket /voice/ws  (PCM16 16 kHz, json control frames)
   ▼
voice-bridge :8200  (FastAPI, F5-TTS-MLX in-process, MacStudio)
   │   STT_BACKEND = kyutai (default since commit 43b5ddc)
   │
   ├── /voice/ws STT     → kyutai_stt.py → ws://localhost:8304
   │                       (msgpack, /api/asr-streaming, partials forwarded)
   │                       ↳ fallback whisper.cpp on KyutaiSttError
   │
   ├── /voice/ws intent  → LiteLLM :4000 → npc-fast (MLX Qwen2.5-7B Q4 :8501)
   │
   ├── /voice/ws TTS     → cache → F5 → Piper Tower :8001 (commit a5b00aa)
   │                       (Piper EN-only today, plugs the silence)
   │
   └── /usage/stats      → polled by dashboard useVoiceUsage (5 s)

hints engine :8311  (FastAPI, macM1)
   └── /hints/ask       → LiteLLM Studio :4000 → hints-deep (MLX 32B Q4 :8500)

Studio crontab @reboot: whisper.cpp :8300, MLX :8500/:8501, moshi-server :8304
macM1 LaunchAgents:    cc.zacus.hints (:8311), cc.ailiance.whisperx (:9500)
```

Runbooks :
- `tools/macstudio/MACM1_HINTS_DEPLOY.md`
- `tools/macstudio/MOSHI_STT_DEPLOY.md`

## Open chantiers

Ordered by ROI / effort, not by priority — pick what matters next.

### ⭐ Glossary biasing for Kyutai STT

**Goal** : fix `U-SON → eu son nez`, `Cherchez → chercher`, all the
metier vocabulary holes the generic 1B model can't know about.

**Constraint discovered this session** : `moshi-server 0.6.4` Rust does
**not** expose any `TextPrompt` / `InitialPrompt` message via the WS
(`rust/moshi-server/src/asr.rs:17` enum `InMsg = Init|Marker|Audio|OggOpus`
only). The PyTorch prompt mechanism uses `on_text_logits_hook` which is
in-process only.

**Three viable alternatives** :

| Approach | Latency | Maintenance | Notes |
|----------|---------|-------------|-------|
| Post-correction LLM (`npc-fast` "rewrite this transcript knowing ...") | +500 ms | nil | use the same model already in the chain |
| Audio prompt pre-pended (cf. `stt_from_file_with_prompt_pytorch.py`) | +2-3 s | regen TTS cache | dose pre-roll = glossary spoken aloud, skip N first Word events |
| Fuzzy alias dictionary (firmware `voice_dispatcher` already has one) | 0 | manual | extend per-puzzle; **smallest scope, highest ROI** |

**Recommended** : start with the fuzzy alias dictionary extension. The
firmware already runs this on the intent path (see core mem 20793 +
`voice_dispatcher fuzzy alias matching for Whisper transcription
errors`). Adding per-puzzle metier terms there is ~30 lines + a small
YAML. Reserve post-correction LLM for the day fuzzy aliases plateau.

### #3 Server-side end-of-utterance

**Status** : skipped 2026-05-24. The firmware already sends
`{"type":"end"}` via its own VAD AFE (`voice_pipeline_ws.c:332`).
Adding Silero on the server = double VAD + ~500 MB torch dep, almost
no gain.

**If reopened** : the cleanest path is to consume the **Step events**
already emitted by `moshi-server` (`{type:"Step", prs:[f32],
buffered_pcm}`) — Kyutai has a built-in semantic VAD. `prs` semantics
are undocumented (it's a projection `p[0]` of the Mimi LM logits),
needs empirical reverse-engineering. Budget: 1-2 h of test sessions
with a printed-Step client, then a tiny `auto-marker on N consecutive
low-pr frames` rule in `kyutai_stt.py`.

### #4 Kokoro / TTS fallback

**Status** : Piper fallback wired into `/voice/ws` (commit a5b00aa).
Kokoro evaluated and rejected for Zacus :

- 1 FR voice only (`af` + a handful), maintainer flags G2P FR as weak
- Mediocre quality on metier terms
- Piper already in place as fallback infrastructure

**Real follow-up** : deploy a **Piper FR model** on Tower (or on
Studio next to whisper.cpp). The Tower instance is EN-only today, so
the fallback currently produces an English pronunciation of French
input — better than silence, but worth fixing. ~30 min :

```bash
# On Tower (or wherever you want the FR fallback):
mkdir -p /opt/piper-fr/models
cd /opt/piper-fr/models
curl -L -o fr_FR-siwis-medium.onnx \
  https://huggingface.co/rhasspy/piper-voices/resolve/main/fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx
curl -L -o fr_FR-siwis-medium.onnx.json \
  https://huggingface.co/rhasspy/piper-voices/resolve/main/fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx.json
# Wire a separate piper-server (OpenAI-compatible) on :8002 then
# set PIPER_URL=http://192.168.0.120:8002 in voice-bridge env.
```

### Plus loin : ailiance OpenAI-Realtime wrap

**Goal** : expose Kyutai (and later Moshi end-to-end) behind an
OpenAI-Realtime-compatible API endpoint inside the `ailiance` gateway.
This is a *product differentiator* — ailiance becomes "sovereign EU
Realtime API" vs OpenAI's hosted offering.

**Scope** :
- ailiance gateway already runs on `electron-server:9300` (FastAPI,
  source in `ailiance/ailiance` private repo, see `/home/electron/ailiance/`)
- Add a `/v1/realtime` WebSocket endpoint that speaks OpenAI's Realtime
  protocol (session events, input_audio_buffer, response.create, etc.)
- Internally adapt:
  - incoming `input_audio_buffer.append` → `moshi-server :8304` Audio frames
  - moshi-server Word/EndWord → emit OpenAI `conversation.item.input_audio_transcription.completed`
  - response generation: call LiteLLM `npc-fast` (or Helium when Moshi
    full-duplex lands), stream tokens, generate TTS via F5 or Kokoro/Piper
- ~400-600 lines Python, ~3 days

**Pre-requisites** :
- moshi-server STT live on Studio (done)
- A spec mapping OpenAI Realtime events ↔ moshi-server msgpack events
  (write this first, it's the hard part)

**Next session entry point** : `cd ailiance/ailiance && code .` ; the
adapter goes in `ailiance/realtime/` as a sub-router of the gateway.

### Plus loin : baby-brain migration whisperx → Kyutai

**Goal** : replace `whisperx-server :9500` on macM1 with Kyutai STT
for the conversational path. Keep whisperx for diarisation/speaker ID
since Kyutai doesn't do that.

**Scope** :
- Two STT endpoints in baby-brain: one streaming (Kyutai) for the live
  avatar/voice agent, one batched-with-diarisation (whisperx) for the
  WML clustering path that needs speaker labels
- Touch points : `baby_brain/identity/stt_bridge.py` (the WS client),
  `baby_brain/identity/visual_bridge.py` (avatar mood from voice)
- Decision needed : run a 2nd `moshi-server` on macM1 (local low
  latency, ~600 MB extra RAM in an already-tight env), or point at the
  Studio :8304 instance (1 RTT extra)
- ~1 day

**Pre-requisites** :
- baby-brain repo cleanup (last session was at PR/refactor stage)
- benchmark whisperx vs Kyutai on baby-brain's actual conversational
  corpus (which is closer to human voice than the Zacus F5 WAVs)

**Next session entry point** : `cd ~/code/baby-brain && code .` ; talk
to the `stt_bridge.py` author about whether they prefer the WSc proxy
approach or a direct in-process Python client (the Python `moshi-mlx`
package would work on macM1 since baby-brain already runs Python).

## Cross-cutting things learned this session

- **MLX workers on Studio bind to 127.0.0.1** (see `lsof` proof in
  `MACM1_HINTS_DEPLOY.md`). Anything Tailnet-reachable on Studio has
  to be explicitly `0.0.0.0` in its launch args. Today the only
  exceptions are the gateway-routed services (`:9301`, `:9303`, `:9327`
  etc.) and the new `:8304` Kyutai.
- **Non-interactive SSH on Studio doesn't see Homebrew** (cmake, etc.).
  Always prepend `PATH=/opt/homebrew/bin:$PATH` in cargo/make invocations
  shipped over `ssh ...`.
- **pyo3 0.23.5 + Python 3.14+** needs `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1`
  at build time. Bit anyone building Rust packages with Python bindings.
- **Voice-bridge venv has no pip** (uv-style minimal venv). Always use
  `uv pip install --python /Users/clems/voice-bridge/.venv/bin/python …`
  to add deps.
- **WS `/voice/ws` is already implemented end-to-end** (server +
  ESP-IDF client) — don't reinvent the protocol, just add backends to
  the existing branches.
