# Kyutai STT (moshi-server Rust) on MacStudio — deployment runbook

**Status** : LIVE since 2026-05-23. Streams `/api/asr-streaming` (WebSocket
msgpack) on `100.116.92.12:8304`. Backbone : `moshi-server 0.6.4` (crates.io)
+ model `kyutai/stt-1b-en_fr-candle` (~600 MB, Candle/Metal, EN+FR).

Validated on a clean French sample (`say -v Thomas`) — 14/17 mots verbatim,
3 substitutions cosmétiques. **Wired into `voice-bridge` /voice/ws since
2026-05-24** — see `kyutai_stt.py` next to `main.py` and the `STT_BACKEND`
env switch in `main.py:92`. Per-word partials forwarded as
`{"type":"stt","final":false,"text":...}`; fallback to whisper.cpp on
`KyutaiSttError`.

## Why this instead of whisper.cpp / whisperx

- **Vrai streaming token-par-token** with timestamps : the WS emits one
  `Word`/`EndWord` message per word, not a single `text` blob after EOS.
  Whisper.cpp `/inference` and `whisperx-server` are batch endpoints.
- **Sub-200 ms first partial** on 7 s of FR audio (vs ~1.5 s wall for
  whisper-large-v3-turbo batch).
- **Native streaming protocol** designed for VAD/turn-taking — natural fit
  for `voice-bridge` `/voice/ws` to forward partials to the firmware as
  they arrive.
- Same `large-v3-turbo`-class accuracy on clean audio (see compare table
  below) but with a real interactive loop.

## Install (already done — for reference)

```bash
# On MacStudio, via bastion `electron-server`:
ssh electron-server "ssh clems@100.116.92.12 'export PATH=/opt/homebrew/bin:\$PATH ; \
  export PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 ; \
  cargo install moshi-server --version 0.6.4 --features metal'"
```

Two non-obvious flags :
- `PATH=/opt/homebrew/bin:$PATH` — non-interactive SSH doesn't see brew's
  `cmake` otherwise (cf. `~/.claude/.../memory/studio_provisioning_gotchas.md`)
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1` — Studio runs Python >= 3.14
  which one of the indirect deps (`pyo3 0.23.5`) doesn't yet support
  without the forward-compat ABI flag.

Build time : ~6-8 min, output binary ~22 MB at `~/.cargo/bin/moshi-server`.

## Files on Studio

```
/Users/clems/
  moshi-stt-config.toml         # TOML config, BatchedAsr module
  moshi-stt/static/             # (empty, only used if HTTP UI enabled)
  moshi-stt/logs/server.log     # tracing-subscriber output
  .cargo/bin/moshi-server       # binary (22 MB)
```

Model + tokenizer + Mimi codec are auto-downloaded by the `hf://` URIs in
the config TOML on first run, into `~/.cache/huggingface/`. Total cache
footprint : ~600 MB.

## Run

Persistent via crontab `@reboot` (matches the other Studio services —
whisper-server :8300, mlx-llm :8500, :8501) :

```cron
@reboot PATH=/opt/homebrew/bin:/Users/clems/.cargo/bin:/usr/bin:/bin \
  /Users/clems/.cargo/bin/moshi-server worker \
  --config /Users/clems/moshi-stt-config.toml \
  -a 0.0.0.0 -p 8304 \
  >> /Users/clems/moshi-stt/logs/server.log 2>&1
```

Manual restart (no reboot) :

```bash
ssh electron-server "ssh clems@100.116.92.12 'pkill -f \"moshi-server worker\" ; \
  sleep 2 ; nohup ~/.cargo/bin/moshi-server worker --config /Users/clems/moshi-stt-config.toml \
  -a 0.0.0.0 -p 8304 </dev/null >> ~/moshi-stt/logs/server.log 2>&1 & disown'"
```

## Operate

```bash
# Status
ssh electron-server "ssh clems@100.116.92.12 'lsof -nP -iTCP:8304 -sTCP:LISTEN'"

# Tail logs
ssh electron-server "ssh clems@100.116.92.12 'tail -30 ~/moshi-stt/logs/server.log'"

# HTTP probe (should return 405 Method Not Allowed on the WS endpoint)
curl -I http://100.116.92.12:8304/api/asr-streaming
```

## Smoke test (any Tailnet host with uv)

```bash
# Get the official client + run on a 24 kHz mono WAV
curl -sS -o /tmp/kyutai_stt_client.py \
  https://raw.githubusercontent.com/kyutai-labs/delayed-streams-modeling/main/scripts/stt_from_file_rust_server.py

uv run --no-project --python 3.12 \
  --with msgpack --with numpy --with sphn --with websockets -- \
  python /tmp/kyutai_stt_client.py /path/to/audio.wav \
  --url ws://100.116.92.12:8304 \
  --api-key zacus-lan-stt \
  --rtf 2.0
```

The client streams `Word` / `EndWord` events to stdout in real time. RTF=2
means "send audio at 2× real-time" — useful for batch eval.

## Protocol cheatsheet

WebSocket on `/api/asr-streaming`, auth via header `kyutai-api-key:
zacus-lan-stt` *or* query string `?auth_id=zacus-lan-stt`. Both directions
use msgpack-encoded JSON.

Client → server :

| `type` | Payload |
|--------|---------|
| `Audio` | `pcm: [f32]` — mono 24 kHz, max 1920 samples/frame recommended (80 ms) |
| `Marker` | `id: int` — signal end-of-utterance, server echoes once flushed |

Server → client :

| `type` | Payload | Meaning |
|--------|---------|---------|
| `Step` | (semantic VAD signal, ignorable) | how much audio has been consumed |
| `Word` | `text: str`, `start_time: float` | a new word is committed |
| `EndWord` | `stop_time: float` | the last `Word` is now complete |
| `Marker` | `id: int` | echo of client marker → safe to close |

Pre-roll : send 1 s of zeros before the real audio (the 1B model can warm
up on the very first frames otherwise). Post-roll : send a few seconds of
zeros after the last real audio so the model emits the trailing words and
the marker.

## Comparative results (same Zacus WAV, then clean `say -v Thomas`)

| Backend / Setup | F5-synth WAV | `say -v Thomas` WAV |
|---|---|---|
| whisper.cpp `large-v3-turbo` ANE (Studio) | hallucinations sévères | not benchmarked |
| whisperx `large-v3-turbo` MPS (macM1) | hallucinations sévères | not benchmarked |
| **Kyutai STT 1B en_fr Candle/Metal (Studio)** | hallucinations en EN (mauvaise détection langue) | **14/17 mots verbatim, 3 substitutions cosmétiques** |

Take-away : F5-synthesized audio is **not** a useful STT corpus — all
three STT engines hallucinate on it identically. The win is on human-grade
audio, where Kyutai gives a transcription usable as-is by `npc-fast` in
the existing `/voice/intent` chain.

## Next chantiers

1. ✅ **Voice-bridge integration** (done 2026-05-24) — `kyutai_stt.py`
   replaces the batch path. STT_BACKEND=kyutai is the default; flip to
   whispercpp for A/B or during Kyutai upgrade. Auto-fallback on error.
2. **Glossary biasing** — Kyutai's STT supports an `initial_prompt`
   mechanism via the conditioning embedding. Wire the active-puzzle
   glossary from `game/scenarios/zacus_v2.yaml` to fix the `U-SON →
   eu son nez` substitution and similar metier terms.
3. **Persona-aware system prompt** — pass the active puzzle ID into
   `/voice/intent`'s system prompt so `npc-fast` can interpret a
   sub-optimal transcript with contextual prior (this is what saves us
   even when STT misses 1-2 words).
4. **Silero VAD server-side** — auto-detect end-of-utterance instead of
   waiting for the firmware to send `{"type":"end"}`. Reduces fixed
   buffer-wait latency.

## Known issues

- Default language auto-detection picks **EN** on noisy / dégradé audio
  even though the model is named `en_fr`. On clean FR audio it picks FR
  correctly. Forcing the language requires a per-request setting we have
  not yet exposed in the client; the upstream Python script
  `stt_from_file_rust_server.py` doesn't expose it either.
- `instance_name = "kyutai-stt-en_fr"` in the config is informational
  only (used for log lines), not a routing key.
- The model file URIs `hf://` are pinned to specific commit hashes
  (`mimi-pytorch-e351c8d8@125.safetensors`). When upstream updates, our
  config must be bumped in lockstep.
