# Kokoro-FR HTTP server — runbook

**Status** : LIVE on MacStudio :8002 since 2026-05-24. Drop-in
third-tier TTS fallback for `voice-bridge` after F5 and Piper.

Why : Piper Tower (`192.168.0.120:8001`) is English-only as of 2026-05
and produces wrong-language pronunciation on French Zacus utterances.
Kokoro's `ff_siwis` is the only open-source FR voice that fits the
"small + fast + acceptable quality" niche today (B- grade on the
authors' scale, <11 h training data).

Validated 2026-05-24: a Kokoro-FR synthesis fed back into Kyutai STT
roundtrips cleanly (`USON est sensible, vous savez, il réagit à
certaines fréquences précises, cherchez bien autour de vous.`).

## Files on Studio

```
/Users/clems/kokoro-fr/
  server.py             # FastAPI app, /health + /synthesize
  requirements.txt
  .venv/                # uv-managed, ~1.5 GB (mlx-audio + misaki + spacy)
  logs/server.log
```

The HF model (`prince-canuma/Kokoro-82M`) is auto-downloaded into
`~/.cache/huggingface/` on first request (~150 MB).

## API

```
GET  /health                              → {status, model, default_voice, lang, sample_rate}
POST /synthesize   {"text": "...", "speaker_id": "ff_siwis"?}
                                          → audio/wav PCM16 mono 24 kHz
                                            X-Kokoro-Latency-Ms + X-Kokoro-Voice headers
```

Field name `speaker_id` is intentional Piper-compat — lets
`voice-bridge._piper_fallback` shape be reused verbatim by
`_kokoro_fallback`.

## Run

Persistent via crontab `@reboot` :

```cron
@reboot cd /Users/clems/kokoro-fr && \
  /Users/clems/kokoro-fr/.venv/bin/python -m uvicorn server:app \
  --host 0.0.0.0 --port 8002 --app-dir /Users/clems/kokoro-fr \
  >> /Users/clems/kokoro-fr/logs/server.log 2>&1
```

Manual restart :

```bash
ssh electron-server "ssh clems@100.116.92.12 'pkill -f \"uvicorn server:app\" ; \
  sleep 2 ; cd /Users/clems/kokoro-fr ; \
  nohup .venv/bin/python -m uvicorn server:app --host 0.0.0.0 --port 8002 \
  --app-dir /Users/clems/kokoro-fr </dev/null >> logs/server.log 2>&1 & disown'"
```

## Smoke test

```bash
curl -sS http://100.116.92.12:8002/health
curl -sS -X POST http://100.116.92.12:8002/synthesize \
  -H 'Content-Type: application/json' \
  -d '{"text":"Bonjour, ceci est un test."}' \
  -o /tmp/test.wav
file /tmp/test.wav   # → WAVE audio, 16 bit, mono 24000 Hz
afplay /tmp/test.wav # macOS playback
```

## Latency

| Scenario | Wall time |
|----------|-----------|
| Cold (first call after boot, model load) | ~9 s |
| Warm, short utterance (~5 words) | ~1.4 s |
| Warm, medium utterance (~20 words) | ~3 s |

Far from F5 quality (no voice cloning) but **faster on warm path**
and FR-native. Wired as the 2nd fallback in `voice-bridge` `/voice/ws`
(after F5, before Piper) since the WS path is FR-dominant.

## Voice catalog

Only `ff_siwis` is FR. Other useful voices on this model :

| Voice | Lang | Notes |
|-------|------|-------|
| `ff_siwis` | FR | Female, SIWIS dataset, only FR voice |
| `af_heart` | EN | Female US, default English voice |
| `am_michael` | EN | Male US |

To change the default: `KOKORO_VOICE=<id>` env var before launching.

## Known limitations

- ~1.5 GB venv (mlx-audio + spacy + transformers + misaki). Future
  Kokoro releases may slim this down.
- First call after boot is slow (~9 s) — model + tokenizer load. The
  `@reboot` crontab does NOT pre-warm; consider a curl probe in
  a post-boot script if cold-start ever matters for a demo.
- Single voice for FR means we can't differentiate NPCs the way F5
  voice cloning would. Kokoro is for "anonymous system voice" /
  fallback, not for persona work.

## Roadmap

- Streaming first-chunk delivery (`mlx_audio` exposes `stream=True`
  but our HTTP wrapper still does end-to-end synth + return). Worth
  doing the day we want sub-500 ms first-byte from the fallback path.
- Tune `cfg_scale` / `temperature` against playtest recordings.
