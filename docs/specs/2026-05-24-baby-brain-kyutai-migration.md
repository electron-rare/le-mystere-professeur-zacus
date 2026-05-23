# Spec — baby-brain STT migration: whisperx → Kyutai (conv path)

**Status** : ready-to-implement (2026-05-24). Lives in the Zacus repo
because the Kyutai infra was validated there; actual code goes into
`~/code/baby-brain/`.

## Goal

Replace `whisperx-server :9500` (macM1) with `moshi-server :8304`
(Studio Kyutai) on baby-brain's **conversational** STT path. Keep
whisperx alive for the **diarisation** path that the WML clustering
identity work needs.

Outcome : sub-second first-partial on the avatar / live-voice loop,
without losing speaker identification for the WML pipeline.

## Why now

- Kyutai is live and proven (Zacus voice-bridge integration, commit
  43b5ddc, 18 partials + 1 final per utterance, validated FR audio)
- whisperx's WS protocol (PCM float32 16 kHz frames) is incompatible
  with batch HTTP voice-bridge — already a known impedance mismatch
- baby-brain conversational loop is bottlenecked at the STT batch
  step; partials would unblock real-time avatar animation

## Current state in baby-brain (verified 2026-05-23)

- `whisperx-server` on macM1 `:9500` (LaunchAgent `cc.ailiance.whisperx`)
  - model `ggml-large-v3-turbo-q5_0.bin` (switched 2026-05-24)
  - WebSocket protocol : binary float32 PCM 16 kHz, emits per-segment
    JSON `{type:"segment", start, end, text, speaker, match_score, compute_ms}`
  - heavy : pyannote embedding pre-loaded (~1.3 GB RAM)
- Consumers identified : `baby_brain/identity/stt_bridge.py`,
  `baby_brain/identity/visual_bridge.py`
- baby-brain runs Python ≥3.11 with `uv` already on macM1 → can
  install `moshi-mlx` package directly if local Kyutai desired

## Two deployment options

| Option | Where Kyutai runs | Latency | RAM impact | Complexity |
|--------|-------------------|---------|------------|------------|
| **A** Local on macM1 | extra `moshi-server` process | minimal (loopback) | +600 MB | medium (new service to manage) |
| **B** Remote Studio :8304 | shared with Zacus | +0.5–3 ms Tailnet | none on macM1 | low (config only) |

**Recommended : option B for v1**, fall back to A if shared load
becomes a problem (Kyutai's `batch_size=4` in `moshi-stt-config.toml`
gives 4 concurrent sessions before queuing).

## File layout changes (baby-brain repo)

```
baby_brain/identity/
  stt_bridge.py           # MODIFY — add Kyutai client, route by mode
  stt_kyutai.py           # NEW — proxy reusing kyutai_stt.py shape
  stt_whisperx.py         # NEW — extract existing whisperx code here
tests/identity/
  test_stt_kyutai.py      # NEW
  test_stt_router.py      # NEW
```

The existing `stt_bridge.py` becomes the **router** : pick the right
backend per call site.

## API surface (baby-brain internal)

```python
class STTBackend(Enum):
    KYUTAI = "kyutai"      # streaming partials, no diarisation
    WHISPERX = "whisperx"  # batch + diarisation

async def transcribe(
    pcm16_16k: bytes,
    backend: STTBackend = STTBackend.KYUTAI,
    on_partial: Callable[[str], Awaitable[None]] | None = None,
    speaker_id_required: bool = False,
) -> TranscriptResult: ...
```

`speaker_id_required=True` forces whisperx no matter what `backend` says
(safety for the WML caller). When False and backend is KYUTAI, partials
are emitted via callback as they arrive.

## Wire the new module — concrete diff sketch

```python
# baby_brain/identity/stt_kyutai.py — adapted from Zacus tools/macstudio/voice-bridge/kyutai_stt.py
import asyncio, os, msgpack, numpy as np, websockets
from scipy.signal import resample_poly

KYUTAI_URL = os.getenv("KYUTAI_STT_URL", "ws://100.116.92.12:8304/api/asr-streaming")
KYUTAI_KEY = os.getenv("KYUTAI_STT_KEY", "babybrain-tailnet")  # new key — add to Kyutai config TOML on Studio

async def kyutai_transcribe(pcm16_16k: bytes, on_partial=None) -> str:
    # Same logic as Zacus kyutai_stt.py — copy verbatim, then evolve.
    ...
```

**Action item before shipping** : extend `/Users/clems/moshi-stt-config.toml`
on Studio to add `"babybrain-tailnet"` to `authorized_ids`. Restart
moshi-server.

## Integration test plan

1. Boot baby-brain with a recorded conversational corpus (the one
   already used for whisperx benchmarks — `tests/identity/fixtures/*.wav`).
2. Run identical transcripts through both backends, log WER side-by-
   side. Target : WER not worse than whisperx-turbo by >5 %.
3. Latency : first-partial < 500 ms p95 over 50 utterances.
4. Diarisation regression : verify the `speaker_id_required=True`
   path still hits whisperx and returns speakers.

## Risks

- **Kyutai 1B model is en_fr only** — if baby-brain ever needs ES/DE/
  other, this regresses. Mitigation : keep whisperx as fallback when
  detected language is non-en_fr (Kyutai exposes language in the
  Marker echo? Verify).
- **Audio format mismatch** — baby-brain's identity bridges produce
  float32 16 kHz today (whisperx native). Kyutai client resamples to
  24 kHz internally, no change needed at call sites.
- **Studio load** — if baby-brain + Zacus both hammer Kyutai, the
  `batch_size=4` ceiling becomes painful. Monitor `lsof` on `:8304`,
  bump to 8 if needed (memory cost ~100 MB per slot).

## Entry point next session

```bash
cd ~/code/baby-brain
git checkout -b kyutai-stt
# 1. Add "babybrain-tailnet" to Studio Kyutai authorized_ids:
ssh electron-server "ssh clems@100.116.92.12 'sed -i.bak \"s/authorized_ids = \\[\\\"zacus-lan-stt\\\"\\]/authorized_ids = [\\\"zacus-lan-stt\\\",\\\"babybrain-tailnet\\\"]/\" /Users/clems/moshi-stt-config.toml ; pkill -f moshi-server ; sleep 2 ; nohup ~/.cargo/bin/moshi-server worker --config /Users/clems/moshi-stt-config.toml -a 0.0.0.0 -p 8304 </dev/null >> ~/moshi-stt/logs/server.log 2>&1 & disown'"
# 2. Copy kyutai_stt.py from the Zacus repo as starting point
curl -sS https://raw.githubusercontent.com/<…>/le-mystere-professeur-zacus/main/tools/macstudio/voice-bridge/kyutai_stt.py > baby_brain/identity/stt_kyutai.py
# 3. Implement router, write tests, A/B against whisperx
```
