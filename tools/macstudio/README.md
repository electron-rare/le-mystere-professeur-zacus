# MacStudio inference stack — config sources of truth

Deployment configs for the MacStudio M3 Ultra (`ssh studio`) hosting Ollama,
whisper.cpp, F5-TTS, and the LiteLLM proxy. Spec:
`docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md`.

## Files

| Local path | Remote path on studio | Purpose |
|------------|-----------------------|---------|
| `litellm-config.yaml` | `~/litellm-config/config.yaml` | LiteLLM proxy (`:4000`) — 6 model aliases (`npc-fast`, `hints-deep`, `whisper-fr`, `stt-fr`, `tts-zacus-primary`, `tts-zacus-fallback`) |
| `voice-bridge/main.py` | `~/voice-bridge/main.py` | FastAPI voice-bridge scaffold (`:8200`) — routes `/voice/transcribe` (whisper :8300), `/voice/intent` (LiteLLM `npc-fast`), `/tts` (F5 primary, Piper Tower:8001 fallback) |
| `voice-bridge/requirements.txt` | `~/voice-bridge/requirements.txt` | FastAPI deps (fastapi, httpx, uvicorn) |

## Persistence (headless macOS — no launchd)

Studio runs headless; `launchctl` over SSH is broken. All daemons run via
`nohup` for current uptime + `@reboot` crontab for reboot persistence.

```cron
@reboot /Users/clems/.local/bin/ollama serve >> /Users/clems/ollama.log 2>&1
@reboot /Users/clems/whisper-install/whisper.cpp/build/bin/whisper-server -m /Users/clems/whisper-install/whisper.cpp/models/ggml-large-v3-turbo-q5_0.bin --port 8300 -t 8 --host 127.0.0.1 -l fr >> /Users/clems/whisper-server.log 2>&1
@reboot /Users/clems/.local/bin/litellm --config /Users/clems/litellm-config/config.yaml --port 4000 --host 0.0.0.0 >> /Users/clems/litellm.log 2>&1
```

Edit on studio with `crontab -e`.

## Sync local → remote

After editing here, push to studio:

```bash
scp tools/macstudio/litellm-config.yaml     studio:~/litellm-config/config.yaml
scp tools/macstudio/voice-bridge/main.py    studio:~/voice-bridge/main.py
scp tools/macstudio/voice-bridge/requirements.txt studio:~/voice-bridge/requirements.txt

ssh studio 'pkill -HUP -f "litellm --config" || pgrep -fl litellm'   # reload LiteLLM
```

## Health checks

```bash
ssh studio 'curl -s http://localhost:11434/api/tags | jq .models[].name'   # ollama models
ssh studio 'curl -s http://localhost:4000/health/readiness'                # litellm
ssh studio 'curl -s -F "file=@/Users/clems/whisper-install/whisper.cpp/samples/jfk.wav" http://localhost:8300/inference'  # whisper
ssh studio 'pgrep -fl ollama; pgrep -fl whisper-server; pgrep -fl litellm'  # daemons
```

## Known limitations (2026-05-03)

- `qwen2.5:72b-instruct-q8_0` via Ollama gives ~2 tok/s on M3 Ultra. Switch
  to MLX-LM backend or Q4_K_M quant if hints latency unacceptable (cible
  spec: <8 s for 50 tokens, currently ~25 s).
- F5-TTS uses base multilingual checkpoint (no FR-specific). Quality
  acceptable but no Zacus voice clone yet — needs `zacus_reference.wav` to
  be recorded (P2).
- Voice-bridge scaffold is **not running** as daemon yet. Activate when
  F5-TTS in-process integration is wired.

## Anti-patterns

- Don't edit configs on studio directly; always edit here and `scp`. Studio
  is the runtime; this directory is the source of truth.
- Don't commit `master_key` rotated values — current value is dev-only.
