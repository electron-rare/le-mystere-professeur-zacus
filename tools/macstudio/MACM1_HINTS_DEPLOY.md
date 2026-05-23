# Hints engine on macM1 — deployment runbook

**Status** : LIVE since 2026-05-23. macM1 (`100.112.121.126` Tailnet) hosts the
hints engine on `:8311`. It calls back to LiteLLM on MacStudio
(`100.116.92.12:4000`) which routes to the MLX-LM `hints-deep` worker on
`localhost:8500` (Studio).

## Why macM1, not Studio

The frontend dashboard defaults to `http://localhost:8311`
(`packages/shared/src/constants.ts::HINTS_DEFAULT_BASE_URL`) — convenient on
the GM laptop, but on macM1 it survives a laptop close and stays reachable to
every other node on the Tailnet. macM1 was sitting at ~58 MB free RAM with
baby-brain daemons squatting it, but the hints engine is a tiny FastAPI +
httpx surface (~150 MB resident) — fits.

## Why **not** LiteLLM on macM1

Was attempted first. Dropped because the MacStudio MLX-LM workers bind to
`127.0.0.1` (`lsof` confirms `127.0.0.1:8500` + `127.0.0.1:8501`) — a LiteLLM
proxy on macM1 cannot reach them across Tailscale. The Studio LiteLLM already
serves on `:4000` Tailnet-wide and routes locally to the MLX workers, so
duplicating it would only add a proxy hop. Hints engine on macM1 talks
straight to Studio `:4000`.

If MLX workers are ever rebound to `0.0.0.0` (Studio plists), the LiteLLM
macM1 idea becomes viable — see "Phase B" in the session log.

## Files on macM1

```
/Users/electron/zacus-backend/
  tools/hints/server.py            # rsync'd from repo tools/hints/server.py
  tools/hints/__init__.py
  game/scenarios/hints_safety.yaml
  game/scenarios/hints_adaptive.yaml
  game/scenarios/npc_phrases.yaml
  hints.log / hints.err            # uvicorn stdout/stderr (LaunchAgent)

/Users/electron/Library/LaunchAgents/
  cc.zacus.hints.plist             # RunAtLoad + KeepAlive on crash
```

Toolchain : `uv 0.11.9` already at `~/.local/bin/uv`. Python 3.11.15 fetched
on demand by `uv run --python 3.11`. Deps installed in `uv` cache, not in a
project venv (`--no-project --with ...`).

## LaunchAgent

`cc.zacus.hints.plist` runs :

```bash
cd /Users/electron/zacus-backend && \
~/.local/bin/uv run --no-project --python 3.11 \
  --with 'fastapi>=0.115' --with 'uvicorn[standard]>=0.30' \
  --with 'httpx>=0.27' --with 'pyyaml>=6' \
  --with 'sse-starlette>=2' --with 'pydantic>=2' \
  uvicorn tools.hints.server:app --host 0.0.0.0 --port 8311
```

Env :
- `LITELLM_URL=http://100.116.92.12:4000`
- `LITELLM_MASTER_KEY=sk-zacus-local-dev-do-not-share` (placeholder, fine on closed LAN)
- `HINTS_LLM_MODEL=hints-deep`

## Operate

```bash
# Status
ssh macm1 'launchctl list | grep zacus'
ssh macm1 'lsof -nP -iTCP:8311 -sTCP:LISTEN'

# Restart
ssh macm1 'launchctl kickstart -k gui/$(id -u)/cc.zacus.hints'

# Logs
ssh macm1 'tail -50 /Users/electron/zacus-backend/hints.{log,err}'

# Update code (from repo root)
rsync -avz tools/hints/server.py macm1:/Users/electron/zacus-backend/tools/hints/
rsync -avz game/scenarios/hints_safety.yaml game/scenarios/hints_adaptive.yaml \
  game/scenarios/npc_phrases.yaml \
  macm1:/Users/electron/zacus-backend/game/scenarios/
ssh macm1 'launchctl kickstart -k gui/$(id -u)/cc.zacus.hints'
```

## Smoke tests (from any Tailnet node)

```bash
# Liveness
curl -sS http://100.112.121.126:8311/hints/sessions

# Full chain: macM1 → Studio LiteLLM → Studio MLX hints-deep
curl -sS -X POST http://100.112.121.126:8311/hints/ask \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"smoke","puzzle_id":"P1_SON","level":1}'
```

The second call should return both `hint_static` (from `npc_phrases.yaml`) and
`hint_rewritten` (LLM-paraphrased in Professeur Zacus tone). If
`hint_rewritten` is null and `error` is set, check :
1. `curl http://100.116.92.12:4000/v1/models` — Studio LiteLLM reachable
2. `ssh electron-server 'ssh clems@100.116.92.12 lsof -nP -iTCP:8500 -sTCP:LISTEN'` — MLX hints-deep alive
3. `tail -50 /Users/electron/zacus-backend/hints.err` on macM1

## Frontend wiring

Dashboard default points at `localhost:8311`. To use the macM1 instance from
another machine, set in `frontend-v3/apps/dashboard/.env.local` :

```
VITE_HINTS_BASE_URL=http://100.112.121.126:8311
```

(Or whatever mDNS name resolves to macM1 on your LAN.)

## Bug fixed during deploy

`tools/hints/server.py:1035-1038` was calling `log.warning(...)` — should be
`LOG.warning(...)` (the module-level logger is named `LOG`). Pure crash on
startup as soon as `LITELLM_MASTER_KEY` was the default. Fixed and committed
in this session.
