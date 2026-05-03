# MacStudio inference stack — config sources of truth

Deployment configs for the MacStudio M3 Ultra (`ssh studio`,
`clems@MacStudio-de-MonsieurB.local`, 512 GB) hosting MLX-LM, Ollama,
whisper.cpp, F5-TTS, and the LiteLLM proxy. Spec:
`docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md`.

## Stack live (post-P1 part1-9)

Inference and voice stack runs entirely on studio. Tower no longer participates
in the live Zacus runtime path.

| Service | Port | Backend | LiteLLM alias(es) | Monitoring |
|---------|------|---------|-------------------|------------|
| MLX-LM 7B Q4 | `:8501` | `mlx_lm.server` | `npc-fast` (~12 tok/s warm) | `curl -s http://studio:8501/v1/models` |
| MLX-LM 32B Q4 | `:8500` | `mlx_lm.server` | `hints-deep` (~4.4 tok/s warm) | `curl -s http://studio:8500/v1/models` |
| Ollama | `:11434` | `ollama serve` | `npc-ollama-7b`, `hints-ollama-72b` (legacy A/B) | `curl -s http://studio:11434/api/tags` |
| whisper.cpp | `:8300` | `whisper-server` (`large-v3-turbo-q5_0`) | `whisper-fr`, `stt-fr` | `pgrep -fl whisper-server` |
| F5-TTS-MLX | `:8200` (in-process) | voice-bridge `/tts` + `/voice/{transcribe,intent,ws}` | `tts-zacus-primary` | `curl -s http://studio:8200/healthz` |
| LiteLLM proxy | `:4000` | `litellm` | (master_key `sk-zacus-local-dev-do-not-share`) | `curl -s http://studio:4000/health/readiness` |

## Files

| Local path | Remote path on studio | Purpose |
|------------|-----------------------|---------|
| `litellm-config.yaml` | `~/litellm-config/config.yaml` | LiteLLM proxy (`:4000`) — 6+ model aliases (`npc-fast`, `hints-deep`, `whisper-fr`, `stt-fr`, `tts-zacus-primary`, `npc-ollama-7b`, `hints-ollama-72b`) |
| `voice-bridge/main.py` | `~/voice-bridge/main.py` | FastAPI voice-bridge (`:8200`) — routes `/voice/transcribe` (whisper :8300), `/voice/intent` (LiteLLM `npc-fast`), `/tts` (F5-TTS-MLX in-process), `/voice/ws` |
| `voice-bridge/requirements.txt` | `~/voice-bridge/requirements.txt` | FastAPI deps (fastapi, httpx, uvicorn, f5-tts-mlx) |
| `voice-bridge/watchdog.sh` | `~/voice-bridge/watchdog.sh` | Watchdog: relance voice-bridge si `pgrep` absent |

## Aliases LiteLLM

Model routing exposed via the proxy on `:4000`. Clients hit aliases (never raw
backends) so we can swap MLX ↔ Ollama without code changes.

| Alias | Backend | Use case |
|-------|---------|----------|
| `npc-fast` | MLX-LM 7B Q4 (`:8501`) | NPC dialogue, low-latency intent |
| `hints-deep` | MLX-LM 32B Q4 (`:8500`) | Hint generation, longer reasoning |
| `npc-ollama-7b` | Ollama 7B (`:11434`) | Legacy A/B comparator for `npc-fast` |
| `hints-ollama-72b` | Ollama 72B (`:11434`) | Legacy A/B comparator for `hints-deep` |
| `whisper-fr` / `stt-fr` | whisper.cpp (`:8300`) | French ASR |
| `tts-zacus-primary` | F5-TTS-MLX via voice-bridge (`:8200`) | Zacus voice synthesis |

## Persistence (headless macOS 26 — no launchd)

Studio runs headless; `launchctl` over SSH is broken. All daemons run via
`nohup` for current uptime + `@reboot` crontab for reboot persistence. Plus a
`*/2` watchdog entry that relaunches voice-bridge on crash.

```cron
@reboot /opt/homebrew/bin/node_exporter >> /Users/clems/node_exporter.log 2>&1
@reboot /Users/clems/.local/bin/ollama serve >> /Users/clems/ollama.log 2>&1
@reboot /Users/clems/whisper-install/whisper.cpp/build/bin/whisper-server -m /Users/clems/whisper-install/whisper.cpp/models/ggml-large-v3-turbo-q5_0.bin --port 8300 -t 8 --host 127.0.0.1 -l fr >> /Users/clems/whisper-server.log 2>&1
@reboot /Users/clems/.local/bin/litellm --config /Users/clems/litellm-config/config.yaml --port 4000 --host 0.0.0.0 >> /Users/clems/litellm.log 2>&1
@reboot /Users/clems/.local/bin/mlx_lm.server --model <hints-32b-q4> --port 8500 >> /Users/clems/mlx-32b.log 2>&1
@reboot /Users/clems/.local/bin/mlx_lm.server --model <npc-7b-q4>   --port 8501 >> /Users/clems/mlx-7b.log  2>&1
@reboot /Users/clems/voice-bridge/.venv/bin/uvicorn main:app --host 0.0.0.0 --port 8200 >> /Users/clems/voice-bridge.log 2>&1
*/2 * * * * /Users/clems/voice-bridge/watchdog.sh >> /Users/clems/voice-bridge.watchdog.log 2>&1
```

Edit on studio with `crontab -e`.

## Resilience

- **Watchdog** — `*/2 * * * * /Users/clems/voice-bridge/watchdog.sh` relance
  voice-bridge si `pgrep -f "uvicorn main:app"` est vide (auto-recovery sous 2
  minutes après un crash).
- **Cache TTS disque** — `~/voice-bridge/cache/` indexé par
  `sha256(text + voice_ref + steps)`. Hit cache = retour WAV immédiat (pas de
  ré-synthèse). Purge :
  ```bash
  curl -X DELETE http://studio:8200/tts/cache
  ```
- **Graceful degradation** — `service_down.wav` (hardcoded fallback) servi par
  voice-bridge si F5-TTS lève une exception irrécupérable. Le runtime ESP32
  reçoit toujours un WAV valide, jamais un 5xx silencieux.

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
ssh studio 'curl -s http://localhost:8500/v1/models'                       # mlx 32B (hints-deep)
ssh studio 'curl -s http://localhost:8501/v1/models'                       # mlx 7B  (npc-fast)
ssh studio 'curl -s http://localhost:4000/health/readiness'                # litellm
ssh studio 'curl -s http://localhost:8200/healthz'                         # voice-bridge
ssh studio 'curl -s -F "file=@/Users/clems/whisper-install/whisper.cpp/samples/jfk.wav" http://localhost:8300/inference'  # whisper
ssh studio 'pgrep -fl ollama; pgrep -fl whisper-server; pgrep -fl litellm; pgrep -fl mlx_lm.server; pgrep -fl "uvicorn main:app"'  # daemons
```

## Pré-render NPC pool (F5-TTS)

Generate the full NPC phrase pool against the live voice-bridge (idempotent —
skips already-cached entries). Output: `hotline_tts/f5/<key>.wav` plus
`hotline_tts/f5/manifest.json`.

```bash
python3 tools/tts/generate_npc_pool.py \
  --backend f5 \
  --voice-bridge-url http://studio:8200
```

Cache key derivation must mirror `voice-bridge/main.py::_voice_ref_token`. See
`tools/CLAUDE.md` for backend semantics.

## Known issues / TODO

- **`steps` non borné** côté payload `/tts` — un client malicieux peut envoyer
  `steps=999` et saturer la GPU queue. Ajouter `steps: int = Field(ge=1, le=64)`
  dans le schéma Pydantic de voice-bridge.
- **Default `--voice-bridge-url 192.168.0.150`** dans certains scripts — IP LAN
  studio à confirmer selon le réseau du client. En itinérance préférer le
  Tailscale `100.116.92.12` (ou alias DNS `studio`).
- **`zacus_reference.wav`** à enregistrer (action humaine, prereq P2). Sans ce
  fichier, F5-TTS-MLX tombe sur le checkpoint multilingue de base — qualité
  acceptable mais pas de Zacus voice clone.

## Anti-patterns

- Don't edit configs on studio directly; always edit here and `scp`. Studio
  is the runtime; this directory is the source of truth.
- Don't commit `master_key` rotated values — current value is dev-only.
