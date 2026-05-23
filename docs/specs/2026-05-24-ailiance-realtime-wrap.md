# Spec — ailiance Realtime API wrap around Kyutai + LiteLLM + TTS

**Status** : ready-to-implement (2026-05-24). All upstream pieces are
live and validated by the Zacus voice-bridge integration. This spec
lives in the Zacus repo because that's where the validating infrastructure
is — the actual code goes into the private `ailiance/ailiance` repo on
electron-server.

## Goal

Expose an **OpenAI Realtime API-compatible** WebSocket endpoint inside
the ailiance gateway so any client built for OpenAI's `/v1/realtime`
can transparently target ailiance and get a sovereign EU answer.

Differentiator : ailiance becomes the only open EU drop-in for OpenAI
Realtime, backed by Kyutai STT + LiteLLM-routed MLX LLM + F5 / Kokoro TTS,
all running on MacStudio hardware that ailiance already operates.

## Upstream surface (already live, validated)

| Component | URL | Protocol | Notes |
|-----------|-----|----------|-------|
| Kyutai STT | `ws://100.116.92.12:8304/api/asr-streaming` | WebSocket + msgpack | streaming Word events, header `kyutai-api-key` |
| LiteLLM | `http://100.116.92.12:4000/v1/chat/completions` | HTTP OpenAI-compat | routes to MLX npc-fast / hints-deep / others |
| F5 TTS | (in-process to voice-bridge :8200, no standalone HTTP) | — | wrap voice-bridge `/tts` instead of calling F5 direct |
| Kokoro FR | `http://100.116.92.12:8002/synthesize` | HTTP `{text, speaker_id}` → WAV | fast warm path (~1.4 s / 5 words) |
| voice-bridge `/tts` | `http://100.116.92.12:8200/tts` | HTTP `{text, voice_ref}` → WAV | already chains cache→F5→Kokoro→Piper |

The Zacus repo references for each (commits, runbooks) :

- `tools/macstudio/MOSHI_STT_DEPLOY.md` (commit 85f42a5)
- `tools/macstudio/MACM1_HINTS_DEPLOY.md` (commit b2988c6) — for the
  hints model class which ailiance may also want to expose
- `tools/macstudio/kokoro-fr/README.md` (commit 95e4bf6)
- `tools/macstudio/voice-bridge/main.py` (commits 43b5ddc, a5b00aa,
  966ff73, 6d5edc7, 95e4bf6) — reference impl of the cascade

## OpenAI Realtime API events to support

Minimal viable subset for v1 (~80 % of clients use only these) :

| Direction | Event | Maps to |
|-----------|-------|---------|
| client → | `session.update` | configure voice, instructions, modalities |
| client → | `input_audio_buffer.append` | forward PCM to Kyutai |
| client → | `input_audio_buffer.commit` | send Kyutai Marker → wait for echo |
| client → | `response.create` | trigger LiteLLM call + TTS chain |
| server → | `conversation.item.input_audio_transcription.completed` | from Kyutai final transcript |
| server → | `response.audio.delta` | TTS chunks (b64 PCM16) |
| server → | `response.audio.done` | end of TTS stream |
| server → | `response.audio_transcript.delta` / `.done` | LLM tokens echoed for caption |
| server → | `error` | upstream failure |

Out of scope v1 : function calling, multi-modal, persistent sessions,
multiple concurrent items in queue, full instructions audio (system
prompts injected during session.update only).

## Architecture in ailiance gateway

```
/v1/realtime (WS)
   │
   ├── session FSM        (idle → listening → thinking → speaking)
   │
   ├── stt_adapter.py     (proxy Kyutai WS, translate events)
   │
   ├── llm_adapter.py     (httpx call to LiteLLM, stream choices)
   │
   └── tts_adapter.py     (httpx call to voice-bridge /tts or kokoro)
```

Single Python process. Each `/v1/realtime` connection opens **one**
upstream Kyutai WS (not pooled — Kyutai sessions are stateful and
cheap to create). LLM and TTS are stateless HTTP per turn.

## File layout (target — in ailiance/ailiance repo)

```
ailiance/realtime/
  __init__.py
  router.py          # FastAPI sub-router mounted at /v1/realtime
  session.py         # Per-connection FSM + event dispatch
  protocol.py        # OpenAI Realtime event schemas (pydantic)
  stt_adapter.py     # Kyutai WS bridge
  llm_adapter.py     # LiteLLM streaming HTTP client
  tts_adapter.py     # voice-bridge /tts client
tests/realtime/
  test_protocol.py
  test_session_fsm.py
  test_end_to_end.py # spins ailiance + hits live upstreams
```

Mount in the ailiance gateway main app :

```python
from ailiance.realtime.router import router as realtime_router
app.include_router(realtime_router)
```

## Implementation order (single session of ~3 days)

1. **protocol.py** — pydantic models for the 9 events. Lift schemas
   from OpenAI Realtime docs verbatim. ~150 lines, 1 h.
2. **stt_adapter.py** — wrap `kyutai_stt.py` from Zacus
   `tools/macstudio/voice-bridge/` (it already does PCM16-16k → 24k
   resample + msgpack framing). Yield `Word` events as
   `input_audio_transcription.delta`. ~120 lines, 2 h.
3. **tts_adapter.py** — POST text to `voice-bridge :8200/tts`, slice
   the returned WAV into ~4 KB PCM chunks, yield as
   `response.audio.delta` (b64-encoded). ~80 lines, 1 h.
4. **llm_adapter.py** — httpx streaming POST to LiteLLM
   `/v1/chat/completions`. ~100 lines, 1 h.
5. **session.py** — FSM, ties the three adapters together, manages
   the OpenAI event bookkeeping (response_id, item_id sequencing).
   ~400 lines, 1 day.
6. **router.py** — minimal FastAPI WebSocket endpoint
   `@router.websocket("/v1/realtime")`, instantiates one session
   per connection. ~50 lines, 30 min.
7. **tests** — unit tests for protocol + FSM (mocked adapters), one
   integration test against the live MacStudio stack. ~half-day.

## Bench targets (validate before ship)

- First STT delta < 400 ms after first audio chunk arrives
- First audio delta < 1.5 s after `response.create` (cache miss,
  cold LLM path through F5)
- First audio delta < 500 ms when LLM reply is in voice-bridge cache
- Full bidirectional `say-Thomas` E2E < 4 s wall

## Decisions / open questions

- **Authentication** : reuse ailiance gateway's existing API key
  scheme. The OpenAI `Authorization: Bearer …` header pattern works
  unchanged. No new mechanism needed.
- **Voice catalog** : expose Kokoro voices verbatim (`ff_siwis`,
  `af_heart`, …) as OpenAI voice IDs. Map `alloy` → `ff_siwis` for
  the default FR experience.
- **Concurrent sessions** : start single-process, single-session-per-
  connection. MacStudio handles ~5 concurrent Kyutai easily today.
  Move to per-session limit + queue when load testing shows need.
- **F5 voice cloning exposure** : skip for v1. Persona work is a
  Zacus-specific feature, not a general Realtime API concern.

## Risks

- **LiteLLM streaming buffering** : verify httpx + LiteLLM proxy do
  not coalesce tokens before yielding. Confirm before locking the
  llm_adapter design.
- **Kyutai session lifetime** : `moshi-server` may have an idle
  timeout we haven't hit yet. Add reconnect logic if a long Realtime
  session sees it.
- **TTS chunking** : OpenAI Realtime expects audio at 24 kHz PCM16
  little-endian, b64-encoded. voice-bridge returns WAV (24 kHz PCM16
  match) — just strip the 44-byte RIFF header and chunk.

## Entry point next session

```bash
cd ~/code/ailiance/ailiance       # or wherever the repo lives locally
git checkout -b realtime-wrap
# Start from protocol.py, run mypy/tests after each step.
# Live upstream: STT ws://100.116.92.12:8304, LLM http://100.116.92.12:4000,
# TTS http://100.116.92.12:8200, Kokoro http://100.116.92.12:8002
```
