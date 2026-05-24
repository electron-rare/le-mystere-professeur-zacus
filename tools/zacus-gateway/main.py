"""Zacus Hub gateway — thin aggregator for the SwiftUI hub app.

Sprint 1+ scope: real backend probes, WS live state, scenario write/validate,
multipart audio transcribe proxy. See docs/specs/2026-05-24-zacus-hub-app.md.
"""
from __future__ import annotations

import asyncio
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import AsyncIterator, Literal

import httpx
import yaml
from fastapi import Depends, FastAPI, HTTPException, Request, UploadFile, File, WebSocket, WebSocketDisconnect, status
from fastapi.responses import JSONResponse, PlainTextResponse, Response
from pydantic import BaseModel
from pydantic_settings import BaseSettings, SettingsConfigDict

from blocks_to_runtime3 import compile_blocks, CompileError


def _load_boards_registry() -> dict[str, dict]:
    path = Path(__file__).parent / "boards.yaml"
    if not path.is_file():
        return {}
    raw = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    return raw.get("boards") or {}


BOARDS = _load_boards_registry()


REPO_ROOT = Path(__file__).resolve().parents[2]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="ZACUS_HUB_", env_file=".env", extra="ignore")

    token: str = "change-me-run-gen_token.py"
    voice_bridge_url: str = "http://studio:8200"
    hints_url: str = "http://macm1:8311"
    esp32_url: str | None = None
    scenarios_dir: str = ""  # empty → REPO_ROOT/game/scenarios
    request_timeout: float = 10.0
    probe_interval: float = 2.0


settings = Settings()
SCENARIOS_DIR = Path(settings.scenarios_dir) if settings.scenarios_dir else REPO_ROOT / "game" / "scenarios"


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    app.state.http = httpx.AsyncClient(timeout=settings.request_timeout)
    app.state.health_cache = HealthCache(app.state.http)
    try:
        yield
    finally:
        await app.state.http.aclose()


app = FastAPI(title="Zacus Hub Gateway", version="0.2.0", lifespan=lifespan)


def require_token(request: Request) -> None:
    auth = request.headers.get("authorization", "")
    if not auth.lower().startswith("bearer "):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "missing bearer token")
    if auth.split(" ", 1)[1].strip() != settings.token:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "invalid token")


# ---------- Backend health ----------

BackendName = Literal["voice_bridge", "hints", "esp32"]


class BackendHealth(BaseModel):
    name: BackendName
    url: str | None
    online: bool
    latency_ms: float | None = None
    detail: str | None = None


class HealthCache:
    """Probes upstreams at most every `probe_interval` seconds."""

    def __init__(self, client: httpx.AsyncClient) -> None:
        self.client = client
        self._cache: dict[BackendName, BackendHealth] = {}
        self._stamps: dict[BackendName, float] = {}

    def targets(self) -> dict[BackendName, str | None]:
        return {
            "voice_bridge": settings.voice_bridge_url,
            "hints": settings.hints_url,
            "esp32": settings.esp32_url,
        }

    async def probe(self, name: BackendName, url: str | None) -> BackendHealth:
        if not url:
            return BackendHealth(name=name, url=None, online=False, detail="not configured")
        start = time.perf_counter()
        try:
            resp = await self.client.get(f"{url.rstrip('/')}/healthz", timeout=3.0)
            elapsed = (time.perf_counter() - start) * 1000
            return BackendHealth(
                name=name,
                url=url,
                online=200 <= resp.status_code < 500,
                latency_ms=round(elapsed, 1),
                detail=f"HTTP {resp.status_code}",
            )
        except Exception as exc:
            elapsed = (time.perf_counter() - start) * 1000
            return BackendHealth(name=name, url=url, online=False, latency_ms=round(elapsed, 1), detail=str(exc)[:120])

    async def get(self, force: bool = False) -> list[BackendHealth]:
        now = time.monotonic()
        results: list[BackendHealth] = []
        for name, url in self.targets().items():
            if not force and (now - self._stamps.get(name, 0)) < settings.probe_interval and name in self._cache:
                results.append(self._cache[name])
                continue
            health = await self.probe(name, url)
            self._cache[name] = health
            self._stamps[name] = now
            results.append(health)
        return results


# ---------- Routes: meta ----------

@app.get("/healthz", response_class=PlainTextResponse)
async def healthz() -> str:
    return "ok"


@app.get("/v1/auth/ping")
async def auth_ping(_: None = Depends(require_token)) -> dict[str, str]:
    return {"status": "authenticated"}


@app.get("/v1/backends/health", response_model=list[BackendHealth])
async def backends_health(request: Request, _: None = Depends(require_token)) -> list[BackendHealth]:
    return await request.app.state.health_cache.get(force=True)


# ---------- Game master state ----------

class GameState(BaseModel):
    scene_id: str | None = None
    scene_index: int = 0
    last_hint: str | None = None
    voice_session: str | None = None
    backends: list[BackendHealth] = []


async def build_state(request: Request) -> GameState:
    health = await request.app.state.health_cache.get()
    # TODO: pull real scene/voice_session from ESP32 + voice-bridge when those
    # endpoints expose state. For now we report what we can verify.
    return GameState(backends=health)


@app.get("/v1/state", response_model=GameState)
async def get_state(request: Request, _: None = Depends(require_token)) -> GameState:
    return await build_state(request)


@app.websocket("/v1/state/ws")
async def state_ws(ws: WebSocket) -> None:
    token = ws.query_params.get("token", "")
    if token != settings.token:
        await ws.close(code=1008)
        return
    await ws.accept()
    try:
        while True:
            state = await build_state(ws)  # type: ignore[arg-type]
            await ws.send_json(state.model_dump())
            await asyncio.sleep(settings.probe_interval)
    except WebSocketDisconnect:
        return


# ---------- Game master actions (sprint 2 stubs) ----------

class HintTrigger(BaseModel):
    scene: str
    level: int = 1


@app.post("/v1/gm/hint")
async def gm_hint(request: Request, body: HintTrigger, _: None = Depends(require_token)) -> JSONResponse:
    """Forward a hint request to the hints engine on behalf of the GM."""
    upstream = await request.app.state.http.post(
        f"{settings.hints_url}/hints/ask",
        json={"scene": body.scene, "level": body.level, "source": "gm"},
    )
    return JSONResponse(content=upstream.json(), status_code=upstream.status_code)


# ---------- Companion: voice proxies ----------

@app.post("/v1/companion/voice/intent")
async def voice_intent(request: Request, _: None = Depends(require_token)) -> JSONResponse:
    body = await request.body()
    upstream = await request.app.state.http.post(
        f"{settings.voice_bridge_url}/voice/intent",
        content=body,
        headers={"content-type": request.headers.get("content-type", "application/json")},
    )
    return JSONResponse(content=upstream.json(), status_code=upstream.status_code)


@app.post("/v1/companion/voice/transcribe")
async def voice_transcribe(
    request: Request,
    audio: UploadFile = File(...),
    _: None = Depends(require_token),
) -> JSONResponse:
    """Multipart audio → voice-bridge /voice/transcribe (which fans out to whisper.cpp)."""
    data = await audio.read()
    files = {"audio": (audio.filename or "clip.wav", data, audio.content_type or "audio/wav")}
    upstream = await request.app.state.http.post(
        f"{settings.voice_bridge_url}/voice/transcribe",
        files=files,
        timeout=30.0,
    )
    try:
        payload = upstream.json()
    except Exception:
        payload = {"raw": upstream.text[:500]}
    return JSONResponse(content=payload, status_code=upstream.status_code)


@app.post("/v1/companion/voice/tts")
async def voice_tts(request: Request, _: None = Depends(require_token)) -> Response:
    """Proxy → voice-bridge /tts. Returns raw audio bytes."""
    body = await request.body()
    upstream = await request.app.state.http.post(
        f"{settings.voice_bridge_url}/tts",
        content=body,
        headers={"content-type": request.headers.get("content-type", "application/json")},
        timeout=60.0,
    )
    return Response(
        content=upstream.content,
        status_code=upstream.status_code,
        media_type=upstream.headers.get("content-type", "audio/wav"),
    )


@app.post("/v1/companion/hint")
async def hint_ask(request: Request, _: None = Depends(require_token)) -> JSONResponse:
    body = await request.json()
    upstream = await request.app.state.http.post(f"{settings.hints_url}/hints/ask", json=body)
    return JSONResponse(content=upstream.json(), status_code=upstream.status_code)


# ---------- Studio ----------

class ScenarioMeta(BaseModel):
    name: str
    path: str
    size: int
    modified: float


class ScenarioWrite(BaseModel):
    yaml: str


class ValidationResult(BaseModel):
    ok: bool
    errors: list[str] = []
    warnings: list[str] = []
    top_level_keys: list[str] = []


def _resolve_scenario(name: str) -> Path:
    target = (SCENARIOS_DIR / name).resolve()
    base = SCENARIOS_DIR.resolve()
    if base != target.parent and base not in target.parents:
        raise HTTPException(404, "scenario not found")
    return target


@app.get("/v1/studio/scenarios", response_model=list[ScenarioMeta])
async def list_scenarios(_: None = Depends(require_token)) -> list[ScenarioMeta]:
    if not SCENARIOS_DIR.exists():
        return []
    return [
        ScenarioMeta(
            name=p.name,
            path=str(p.relative_to(SCENARIOS_DIR.parent.parent)) if SCENARIOS_DIR.parent.parent in p.parents else p.name,
            size=p.stat().st_size,
            modified=p.stat().st_mtime,
        )
        for p in sorted(SCENARIOS_DIR.glob("*.yaml"))
    ]


@app.get("/v1/studio/scenario/{name}")
async def get_scenario(name: str, _: None = Depends(require_token)) -> dict:
    target = _resolve_scenario(name)
    if not target.is_file():
        raise HTTPException(404, "scenario not found")
    text = target.read_text(encoding="utf-8")
    return {"name": name, "yaml": text, "modified": target.stat().st_mtime}


@app.put("/v1/studio/scenario/{name}", response_model=ValidationResult)
async def write_scenario(name: str, body: ScenarioWrite, _: None = Depends(require_token)) -> ValidationResult:
    target = _resolve_scenario(name)
    validation = _validate_yaml(body.yaml)
    if not validation.ok:
        raise HTTPException(422, detail=validation.model_dump())
    # backup first
    if target.exists():
        backup = target.with_suffix(target.suffix + f".bak.{int(time.time())}")
        backup.write_bytes(target.read_bytes())
    target.write_text(body.yaml, encoding="utf-8")
    return validation


class BoardInfo(BaseModel):
    name: str
    label: str
    type: str
    ip: str | None
    mdns: str | None
    hot_endpoint: str | None
    cold_data_dir: str | None
    espnow_relay_peers: list[str] = []


@app.get("/v1/flash/boards", response_model=list[BoardInfo])
async def list_boards(_: None = Depends(require_token)) -> list[BoardInfo]:
    out = []
    for name, cfg in BOARDS.items():
        out.append(BoardInfo(
            name=name,
            label=cfg.get("label", name),
            type=cfg.get("type", "unknown"),
            ip=cfg.get("ip"),
            mdns=cfg.get("mdns"),
            hot_endpoint=cfg.get("hot_endpoint"),
            cold_data_dir=cfg.get("cold_data_dir"),
            espnow_relay_peers=cfg.get("espnow_relay_peers") or [],
        ))
    return out


class FlashRequest(BaseModel):
    scenario: str            # YAML filename (the blocks_studio v2 source)
    strategy: str = "auto"   # "auto" | "hot" | "cold"


class FlashStep(BaseModel):
    label: str
    status: str              # "ok" | "warn" | "skip" | "error"
    detail: str = ""


class FlashResult(BaseModel):
    board: str
    strategy_used: str
    ok: bool
    steps: list[FlashStep]
    ir_path: str | None = None
    cold_command: str | None = None
    relayed_to: list[str] = []


@app.post("/v1/flash/{board_name}", response_model=FlashResult)
async def flash_board(board_name: str, body: FlashRequest, request: Request, _: None = Depends(require_token)) -> FlashResult:
    if board_name not in BOARDS:
        raise HTTPException(404, f"unknown board '{board_name}' — declare it in boards.yaml")
    board = BOARDS[board_name]

    # 1. compile current YAML to IR
    yaml_path = _resolve_scenario(body.scenario)
    if not yaml_path.is_file():
        raise HTTPException(404, f"scenario '{body.scenario}' not found")
    try:
        ir = compile_blocks(yaml_path.read_text(encoding="utf-8"),
                            scenario_id=body.scenario.replace(".yaml", "").upper())
    except CompileError as exc:
        raise HTTPException(422, f"compile: {exc}") from exc
    import json
    ir_text = json.dumps(ir, indent=2, ensure_ascii=False)
    ir_path = yaml_path.with_suffix(".ir.json")
    ir_path.write_text(ir_text, encoding="utf-8")

    steps: list[FlashStep] = [FlashStep(label="compile IR", status="ok",
                                         detail=f"{len(ir['steps'])} steps → {ir_path.name}")]

    strategy = body.strategy
    if strategy == "auto":
        strategy = "hot" if board.get("ip") else "cold"

    cold_command: str | None = None
    relayed_to: list[str] = []
    ok = True

    if strategy == "hot":
        # Try POST to firmware /game/scenario
        ip = board.get("ip")
        hot_endpoint = board.get("hot_endpoint") or "/game/scenario"
        if not ip:
            steps.append(FlashStep(label="hot POST", status="error",
                                   detail=f"no IP for board '{board_name}' — set ip in boards.yaml or use ESP-NOW relay via master"))
            ok = False
        else:
            url = f"http://{ip}{hot_endpoint}"
            try:
                resp = await request.app.state.http.post(url, content=ir_text,
                                                          headers={"content-type": "application/json"},
                                                          timeout=20.0)
                if 200 <= resp.status_code < 300:
                    steps.append(FlashStep(label="hot POST", status="ok",
                                           detail=f"{resp.status_code} {resp.text[:120]}"))
                elif resp.status_code == 404:
                    steps.append(FlashStep(label="hot POST", status="error",
                                           detail=f"404 — firmware doesn't expose {hot_endpoint} yet. Apply firmware Phase 2 spec, or use strategy=cold."))
                    ok = False
                else:
                    steps.append(FlashStep(label="hot POST", status="error",
                                           detail=f"{resp.status_code} {resp.text[:200]}"))
                    ok = False
            except Exception as exc:
                steps.append(FlashStep(label="hot POST", status="error", detail=str(exc)[:200]))
                ok = False

        # If master and ok, relay via ESP-NOW to peers
        if ok and board.get("espnow_relay_peers"):
            relay_url = f"http://{ip}/game/scenario/relay"
            try:
                resp = await request.app.state.http.post(
                    relay_url,
                    json={"peers": board["espnow_relay_peers"], "ir": ir},
                    timeout=20.0,
                )
                if 200 <= resp.status_code < 300:
                    relayed_to = list(board["espnow_relay_peers"])
                    steps.append(FlashStep(label="ESP-NOW relay", status="ok",
                                           detail=f"forwarded to {','.join(relayed_to)}"))
                else:
                    steps.append(FlashStep(label="ESP-NOW relay", status="warn",
                                           detail=f"{resp.status_code} — relay endpoint not ready (Phase 2)"))
            except Exception as exc:
                steps.append(FlashStep(label="ESP-NOW relay", status="warn", detail=f"skipped: {exc}"[:200]))

    elif strategy == "cold":
        cold_dir = board.get("cold_data_dir")
        if not cold_dir:
            steps.append(FlashStep(label="cold stage", status="error",
                                   detail=f"no cold_data_dir for '{board_name}'"))
            ok = False
        else:
            # Stage the IR into the firmware data dir on the gateway host
            # (assumes the repo is cloned alongside the gateway; otherwise just
            # report the file path back to the developer).
            stage_dir = REPO_ROOT / cold_dir
            if stage_dir.is_dir():
                target = stage_dir / "scenario.json"
                target.write_text(ir_text, encoding="utf-8")
                steps.append(FlashStep(label="cold stage", status="ok", detail=f"wrote {target}"))
                cold_command = _cold_command_for(board.get("type", ""), cold_dir)
            else:
                steps.append(FlashStep(label="cold stage", status="warn",
                                       detail=f"directory {stage_dir} not present on gateway — copy IR manually from {ir_path}"))
                cold_command = _cold_command_for(board.get("type", ""), cold_dir)
    else:
        raise HTTPException(400, f"unknown strategy '{strategy}' (auto|hot|cold)")

    return FlashResult(
        board=board_name,
        strategy_used=strategy,
        ok=ok,
        steps=steps,
        ir_path=str(ir_path.relative_to(REPO_ROOT)) if REPO_ROOT in ir_path.parents else str(ir_path),
        cold_command=cold_command,
        relayed_to=relayed_to,
    )


def _cold_command_for(board_type: str, cold_dir: str) -> str:
    if board_type in ("idf_zacus", "box3_voice", "puzzle"):
        proj_dir = cold_dir.rsplit("/data", 1)[0] if cold_dir.endswith("/data") else cold_dir
        return f"cd {proj_dir} && idf.py -p $(ls /dev/cu.usbserial* /dev/cu.SLAB* 2>/dev/null | head -1) flash monitor"
    if board_type == "plip_firmware":
        return "cd PLIP_FIRMWARE && pio run --target upload"
    return f"# manual flash needed — IR staged in {cold_dir}/scenario.json"


class CompileSummary(BaseModel):
    ok: bool
    steps_count: int
    entry_step_id: str | None
    errors: list[str] = []
    warnings: list[str] = []


@app.post("/v1/studio/scenario/{name}/compile", response_model=CompileSummary)
async def compile_scenario(name: str, _: None = Depends(require_token)) -> CompileSummary:
    """Read the scenario YAML, run blocks→Runtime 3 IR, write the IR next to it."""
    target = _resolve_scenario(name)
    if not target.is_file():
        raise HTTPException(404, "scenario not found")
    text = target.read_text(encoding="utf-8")
    try:
        ir = compile_blocks(text, scenario_id=name.replace(".yaml", "").upper())
    except CompileError as exc:
        raise HTTPException(422, detail=str(exc)) from exc
    ir_path = target.with_suffix(".ir.json")
    import json
    ir_path.write_text(json.dumps(ir, indent=2, ensure_ascii=False), encoding="utf-8")
    return CompileSummary(
        ok=True,
        steps_count=len(ir["steps"]),
        entry_step_id=ir["scenario"].get("entry_step_id") or None,
        errors=ir["metadata"]["errors"],
        warnings=ir["metadata"]["warnings"],
    )


@app.post("/v1/studio/scenario/{name}/validate", response_model=ValidationResult)
async def validate_scenario(name: str, body: ScenarioWrite | None = None, _: None = Depends(require_token)) -> ValidationResult:
    if body is not None:
        return _validate_yaml(body.yaml)
    target = _resolve_scenario(name)
    if not target.is_file():
        raise HTTPException(404, "scenario not found")
    return _validate_yaml(target.read_text(encoding="utf-8"))


def _validate_yaml(text: str) -> ValidationResult:
    errors: list[str] = []
    warnings: list[str] = []
    keys: list[str] = []
    try:
        parsed = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        return ValidationResult(ok=False, errors=[f"YAML parse: {exc}"])
    if not isinstance(parsed, dict):
        return ValidationResult(ok=False, errors=["top-level must be a mapping"])
    keys = sorted(parsed.keys())
    # Soft schema hints aligned with zacus_v3.yaml — we don't enforce strictly here.
    for required in ("id", "version"):
        if required not in parsed:
            warnings.append(f"missing recommended top-level key '{required}'")
    if "acts" not in parsed and "steps_narrative" not in parsed and "scenes" not in parsed:
        warnings.append("no 'acts' / 'steps_narrative' / 'scenes' found — scenario looks empty")
    return ValidationResult(ok=True, errors=errors, warnings=warnings, top_level_keys=keys)
