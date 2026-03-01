#!/usr/bin/env python3
"""Local HTTP gateway for Zacus Story Studio AI generation.

Exposes lightweight endpoints that mirror the frontend contract:
- POST /story_generate
  -> returns {yaml, rationale}
- POST /printables_plan
  -> returns {manifest_yaml, markdown}
- POST /visual_generate or /image_generate
  -> returns {images, count, provider, source}

This lets the frontend call a local AI model (ex: Ollama / vLLM / OpenAI-compatible)
without touching firmware.
"""

from __future__ import annotations

import argparse
import base64
import json
import re
import time
import threading
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any

from yaml import safe_dump, safe_load


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8787
DEFAULT_LLM_URL = "http://127.0.0.1:11434/v1/chat/completions"
DEFAULT_LLM_MODEL = "qwen2.5-coder:14b"
DEFAULT_IMAGE_URL = "http://127.0.0.1:7860/sdapi/v1/txt2img"
DEFAULT_IMAGE_MODEL = "stabilityai/stable-diffusion-xl-base-1.0"
DEFAULT_IMAGE_TIMEOUT = 180


DEFAULT_PRINTABLES_MANIFEST = Path("printables/manifests/zacus_v2_printables.yaml")

IMAGE_PROVIDER_OPENAI = "openai"
IMAGE_PROVIDER_SD_WEBUI = "sd_webui"


BASE_APP_BINDINGS = [
    {"id": "APP_AUDIO", "app": "AUDIO_PACK"},
    {"id": "APP_SCREEN", "app": "SCREEN_SCENE"},
    {"id": "APP_GATE", "app": "MP3_GATE"},
    {"id": "APP_WIFI", "app": "WIFI_STACK"},
    {"id": "APP_ESPNOW", "app": "ESPNOW_STACK"},
    {"id": "APP_QR_UNLOCK", "app": "QR_UNLOCK_APP"},
]

BASE_NODES = [
    {"step_id": "STEP_U_SON_PROTO", "screen": "SCENE_U_SON_PROTO", "audio_pack_id": ""},
    {"step_id": "STEP_LA_DETECTOR", "screen": "SCENE_LA_DETECTOR", "audio_pack_id": ""},
    {"step_id": "STEP_RTC_ESP_ETAPE1", "screen": "SCENE_WIN_ETAPE1", "audio_pack_id": "PACK_WIN1"},
    {"step_id": "STEP_WIN_ETAPE1", "screen": "SCENE_WIN_ETAPE1", "audio_pack_id": "PACK_WIN1"},
    {"step_id": "STEP_WARNING", "screen": "SCENE_WARNING", "audio_pack_id": ""},
    {"step_id": "STEP_LEFOU_DETECTOR", "screen": "SCENE_LEFOU_DETECTOR", "audio_pack_id": ""},
    {"step_id": "STEP_RTC_ESP_ETAPE2", "screen": "SCENE_WIN_ETAPE2", "audio_pack_id": "PACK_WIN2"},
    {"step_id": "STEP_QR_DETECTOR", "screen": "SCENE_QR_DETECTOR", "audio_pack_id": "PACK_QR"},
    {"step_id": "STEP_FINAL_WIN", "screen": "SCENE_FINAL_WIN", "audio_pack_id": "PACK_WIN3"},
    {"step_id": "SCENE_MEDIA_MANAGER", "screen": "SCENE_MEDIA_MANAGER", "audio_pack_id": ""},
]

STEP_TRANSITIONS = {
    "STEP_U_SON_PROTO": [
        {"trigger": "on_event", "event_type": "audio_done", "event_name": "AUDIO_DONE", "target_step_id": "STEP_U_SON_PROTO", "after_ms": 0, "priority": 90},
        {"trigger": "on_event", "event_type": "button", "event_name": "ANY", "target_step_id": "STEP_LA_DETECTOR", "after_ms": 0, "priority": 120},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_ETAPE2", "target_step_id": "STEP_LA_DETECTOR", "after_ms": 0, "priority": 130},
    ],
    "STEP_LA_DETECTOR": [
        {"trigger": "on_event", "event_type": "timer", "event_name": "ETAPE2_DUE", "target_step_id": "STEP_U_SON_PROTO", "after_ms": 0, "priority": 80},
        {"trigger": "on_event", "event_type": "serial", "event_name": "BTN_NEXT", "target_step_id": "STEP_RTC_ESP_ETAPE1", "after_ms": 0, "priority": 110},
        {"trigger": "on_event", "event_type": "unlock", "event_name": "UNLOCK", "target_step_id": "STEP_RTC_ESP_ETAPE1", "after_ms": 0, "priority": 120},
        {"trigger": "on_event", "event_type": "action", "event_name": "ACTION_FORCE_ETAPE2", "target_step_id": "STEP_RTC_ESP_ETAPE1", "after_ms": 0, "priority": 130},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_WIN_ETAPE1", "target_step_id": "STEP_RTC_ESP_ETAPE1", "after_ms": 0, "priority": 140},
    ],
    "STEP_RTC_ESP_ETAPE1": [
        {"trigger": "on_event", "event_type": "esp_now", "event_name": "ACK_WIN1", "target_step_id": "STEP_WIN_ETAPE1", "after_ms": 0, "priority": 130},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_DONE", "target_step_id": "STEP_WIN_ETAPE1", "after_ms": 0, "priority": 125},
    ],
    "STEP_WIN_ETAPE1": [
        {"trigger": "on_event", "event_type": "serial", "event_name": "BTN_NEXT", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 120},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_DONE", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 110},
        {"trigger": "on_event", "event_type": "esp_now", "event_name": "ACK_WARNING", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 125},
    ],
    "STEP_WARNING": [
        {"trigger": "on_event", "event_type": "audio_done", "event_name": "AUDIO_DONE", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 80},
        {"trigger": "on_event", "event_type": "button", "event_name": "ANY", "target_step_id": "STEP_LEFOU_DETECTOR", "after_ms": 0, "priority": 120},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_ETAPE2", "target_step_id": "STEP_LEFOU_DETECTOR", "after_ms": 0, "priority": 130},
    ],
    "STEP_LEFOU_DETECTOR": [
        {"trigger": "on_event", "event_type": "timer", "event_name": "ETAPE2_DUE", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 100},
        {"trigger": "on_event", "event_type": "serial", "event_name": "BTN_NEXT", "target_step_id": "STEP_RTC_ESP_ETAPE2", "after_ms": 0, "priority": 110},
        {"trigger": "on_event", "event_type": "unlock", "event_name": "UNLOCK", "target_step_id": "STEP_RTC_ESP_ETAPE2", "after_ms": 0, "priority": 115},
        {"trigger": "on_event", "event_type": "action", "event_name": "ACTION_FORCE_ETAPE2", "target_step_id": "STEP_RTC_ESP_ETAPE2", "after_ms": 0, "priority": 125},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_WIN_ETAPE2", "target_step_id": "STEP_RTC_ESP_ETAPE2", "after_ms": 0, "priority": 130},
    ],
    "STEP_RTC_ESP_ETAPE2": [
        {"trigger": "on_event", "event_type": "esp_now", "event_name": "ACK_WIN2", "target_step_id": "STEP_QR_DETECTOR", "after_ms": 0, "priority": 130},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_DONE", "target_step_id": "STEP_QR_DETECTOR", "after_ms": 0, "priority": 120},
    ],
    "STEP_QR_DETECTOR": [
        {"trigger": "on_event", "event_type": "timer", "event_name": "ETAPE2_DUE", "target_step_id": "STEP_WARNING", "after_ms": 0, "priority": 100},
        {"trigger": "on_event", "event_type": "serial", "event_name": "BTN_NEXT", "target_step_id": "STEP_FINAL_WIN", "after_ms": 0, "priority": 110},
        {"trigger": "on_event", "event_type": "unlock", "event_name": "UNLOCK_QR", "target_step_id": "STEP_FINAL_WIN", "after_ms": 0, "priority": 140},
        {"trigger": "on_event", "event_type": "action", "event_name": "ACTION_FORCE_ETAPE2", "target_step_id": "STEP_FINAL_WIN", "after_ms": 0, "priority": 125},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_WIN_ETAPE2", "target_step_id": "STEP_FINAL_WIN", "after_ms": 0, "priority": 130},
    ],
    "STEP_FINAL_WIN": [
        {"trigger": "on_event", "event_type": "timer", "event_name": "WIN_DUE", "target_step_id": "SCENE_MEDIA_MANAGER", "after_ms": 0, "priority": 140},
        {"trigger": "on_event", "event_type": "serial", "event_name": "BTN_NEXT", "target_step_id": "SCENE_MEDIA_MANAGER", "after_ms": 0, "priority": 120},
        {"trigger": "on_event", "event_type": "unlock", "event_name": "UNLOCK", "target_step_id": "SCENE_MEDIA_MANAGER", "after_ms": 0, "priority": 115},
        {"trigger": "on_event", "event_type": "action", "event_name": "ACTION_FORCE_ETAPE2", "target_step_id": "SCENE_MEDIA_MANAGER", "after_ms": 0, "priority": 130},
        {"trigger": "on_event", "event_type": "serial", "event_name": "FORCE_WIN_ETAPE2", "target_step_id": "SCENE_MEDIA_MANAGER", "after_ms": 0, "priority": 135},
    ],
}


def sanitize_scenario_id(value: str) -> str:
    raw = (value or "CUSTOM").strip().upper()
    cleaned = re.sub(r"[^A-Z0-9_]", "_", raw)
    cleaned = re.sub(r"_+", "_", cleaned).strip("_")
    return cleaned or "CUSTOM"


def to_int(value: Any, fallback: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return fallback


def call_json_api(url: str, payload: dict[str, Any], timeout: int = 120) -> Any:
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url=url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read().decode("utf-8")
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Endpoint unreachable: {exc}") from exc

    return json.loads(body)


def detect_image_provider(url: str, forced_provider: str) -> str:
    forced = (forced_provider or "").strip().lower()
    if forced in {"auto", IMAGE_PROVIDER_OPENAI, IMAGE_PROVIDER_SD_WEBUI}:
        return forced

    url_lower = (url or "").lower()
    if url_lower.endswith("/v1/images/generations") or "/v1/images" in url_lower:
        return IMAGE_PROVIDER_OPENAI
    if "/sdapi/" in url_lower:
        return IMAGE_PROVIDER_SD_WEBUI
    return IMAGE_PROVIDER_OPENAI


def _ensure_base64(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text:
        return None
    try:
        base64.b64decode(text, validate=True)
        return text
    except Exception:
        return None


def call_llm(url: str, model: str, prompt: str, timeout: int = 120) -> str:
    payload = {
        "model": model,
        "messages": [
            {
                "role": "system",
                "content": (
                    "Tu es un générateur de scénarios Zacus.\n"
                    "Tu dois répondre en YAML strict, sans explication ni markdown.\n"
                    "Le YAML doit rester compatible Story V2 du frontend (id/version/steps/app_bindings).\n"
                    "Conserve les noms d'événements EXACTS quand tu les réécris."
                ),
            },
            {"role": "user", "content": prompt},
        ],
        "temperature": 0.2,
        "max_tokens": 2048,
    }

    parsed = call_json_api(url, payload, timeout)
    if "choices" in parsed and parsed["choices"]:
        first = parsed["choices"][0]
        message = first.get("message", {})
        content = message.get("content")
        if content:
            return str(content).strip()

    # Fallback for non-chat providers (legacy Ollama generate format)
    if "response" in parsed:
        response_text = parsed["response"]
        if isinstance(response_text, str):
            return response_text.strip()

    raise RuntimeError("Réponse LLM invalide (format inattendu)")


def call_image_generation(
    url: str,
    model: str,
    prompt: str,
    negative_prompt: str,
    width: int,
    height: int,
    steps: int,
    cfg_scale: float,
    seed: int,
    count: int,
    timeout: int = DEFAULT_IMAGE_TIMEOUT,
    forced_provider: str = "auto",
) -> list[dict[str, str]]:
    provider = detect_image_provider(url, forced_provider)
    if provider == IMAGE_PROVIDER_OPENAI:
        payload = {
            "model": model,
            "prompt": prompt,
            "n": max(1, count),
            "size": f"{width}x{height}",
            "response_format": "b64_json",
        }
        if seed >= 0:
            payload["seed"] = seed
        parsed = call_json_api(url, payload, timeout)
    else:
        payload = {
            "prompt": prompt,
            "negative_prompt": negative_prompt,
            "steps": steps,
            "cfg_scale": cfg_scale,
            "width": width,
            "height": height,
            "seed": seed,
            "batch_size": max(1, count),
            "n_iter": 1,
        }
        parsed = call_json_api(url, payload, timeout)

    images: list[dict[str, str]] = []

    if isinstance(parsed, dict) and isinstance(parsed.get("data"), list):
        for index, item in enumerate(parsed["data"]):
            if not isinstance(item, dict):
                continue
            data = item.get("b64_json") or item.get("b64") or item.get("image") or item.get("base64")
            if isinstance(data, str) and data.strip():
                b64 = _ensure_base64(data)
                if b64:
                    images.append(
                        {
                            "filename": f"sdxl_{int(time.time())}_{index}.png",
                            "mime": "image/png",
                            "base64": b64,
                        }
                    )
                    continue
            image_url = item.get("url")
            if isinstance(image_url, str) and image_url.strip():
                images.append(
                    {
                        "filename": f"sdxl_{int(time.time())}_{index}.txt",
                        "mime": "text/uri-list",
                        "url": image_url.strip(),
                    }
                )

    if isinstance(parsed, dict) and isinstance(parsed.get("images"), list):
        for index, data in enumerate(parsed["images"]):
            b64 = _ensure_base64(data)
            if b64:
                images.append(
                    {
                        "filename": f"sdxl_{int(time.time())}_{index}.png",
                        "mime": "image/png",
                        "base64": b64,
                    }
                )

    if not images:
        raise RuntimeError("Réponse image invalide (aucune image retournée)")

    return images


def to_float(value: Any, fallback: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def extract_yaml(payload: str) -> str:
    block_match = re.search(r"```ya?ml\\s*\\n([\\s\\S]*?)```", payload, re.IGNORECASE)
    if block_match:
        return block_match.group(1).strip()

    first_brace = payload.find("---")
    if first_brace == 0:
        return payload.strip()
    if first_brace > 0:
        return payload[first_brace:].strip()
    return payload.strip()


def build_fallback_scenario(blueprint: dict[str, Any]) -> str:
    scenario_id = sanitize_scenario_id(str(blueprint.get("scenarioId", "CUSTOM")))
    title = str(blueprint.get("title", "Scenario généré localement")).strip() or "Scenario généré localement"
    duration = to_int(blueprint.get("durationMinutes"), 90)
    min_players = to_int(blueprint.get("minPlayers"), 4)
    max_players = to_int(blueprint.get("maxPlayers"), 12)
    if max_players < min_players:
        max_players = min_players
    include_media = bool(blueprint.get("includeMediaManager", False))

    custom_note = str(blueprint.get("customPrompt", "") or blueprint.get("aiHint", "") or "").strip()

    steps = []
    for index, node in enumerate(BASE_NODES):
        step: dict[str, Any] = {
            "step_id": node["step_id"],
            "screen_scene_id": node["screen"],
            "audio_pack_id": node["audio_pack_id"],
            "actions": ["ACTION_TRACE_STEP"],
            "apps": [binding["id"] for binding in BASE_APP_BINDINGS],
            "mp3_gate_open": node["step_id"] == "STEP_QR_DETECTOR",
            "transitions": [dict(transition) for transition in STEP_TRANSITIONS.get(node["step_id"], [])],
        }
        if index == 0:
            step["is_initial"] = True
        if node["step_id"] == "SCENE_MEDIA_MANAGER" and include_media:
            step["apps"] = [binding["id"] for binding in BASE_APP_BINDINGS] + ["APP_MEDIA"]
        steps.append(step)

    content = {
        "id": scenario_id,
        "version": 2,
        "title": title,
        "duration_minutes": duration,
        "players": {
            "min": min_players,
            "max": max_players,
        },
        "theme": "Scénario généré localement.",
        "difficulty": str(blueprint.get("difficulty", "standard")).strip() or "standard",
        "initial_step": "STEP_U_SON_PROTO",
        "debug_transition_bypass_enabled": False,
        "app_bindings": BASE_APP_BINDINGS,
        "steps": steps,
        "note": custom_note or "Génération locale de secours (fallback).",
    }

    return safe_dump(content, sort_keys=False, allow_unicode=True).strip()


def build_story_prompt(blueprint: dict[str, Any]) -> str:
    normalized = {
        "scenarioId": sanitize_scenario_id(str(blueprint.get("scenarioId", ""))),
        "title": str(blueprint.get("title", "")).strip(),
        "missionSummary": str(blueprint.get("missionSummary", "")).strip(),
        "durationMinutes": to_int(blueprint.get("durationMinutes"), 90),
        "minPlayers": to_int(blueprint.get("minPlayers"), 4),
        "maxPlayers": to_int(blueprint.get("maxPlayers"), 12),
        "difficulty": str(blueprint.get("difficulty", "standard")).strip(),
        "includeMediaManager": bool(blueprint.get("includeMediaManager", False)),
        "customPrompt": str(blueprint.get("customPrompt", "")).strip(),
        "aiHint": str(blueprint.get("aiHint", "")).strip(),
    }
    normalized_json = json.dumps(normalized, ensure_ascii=False, indent=2)

    return (
        "Génère un YAML de scenario Zacus compatible Story V2 (format frontend).\n"
        "Le YAML doit contenir id, version, title, steps, app_bindings, initial_step.\n"
        "Conserver le flow par défaut décrit ci-dessous (noms d'événements inchangés).\n"
        "Si certains champs manquent, utilise les valeurs ci-dessous.\n\n"
        f"{normalized_json}\n\n"
        "Règles techniques:\n"
        "- transitions: trigger, event_type, event_name, target_step_id, after_ms, priority.\n"
        "- event_name respecte exactement: BTN,NEXT,AUDIO_DONE,ACK_*,UNLOCK,FORCE_*,ETAPE2_DUE,QR_TIMEOUT,WIN_DUE.\n"
        "- target_step_id accepte aussi SCENE_MEDIA_MANAGER.\n"
    )


def build_printables_plan(scenario_id: str, title: str, selected: list[str] | None) -> tuple[str, str]:
    manifest_path = DEFAULT_PRINTABLES_MANIFEST
    manifest = safe_load(manifest_path.read_text())
    items = manifest.get("items", [])
    if not isinstance(items, list):
        raise RuntimeError(f"Manifest invalid: {manifest_path}")

    selected_set = set(selected or [])
    filtered = [item for item in items if not selected_set or item.get("id") in selected_set]

    manifest_out = dict(manifest)
    manifest_out["manifest_id"] = f"{sanitize_scenario_id(scenario_id).lower()}_printables"
    manifest_out["scenario_id"] = sanitize_scenario_id(scenario_id)
    manifest_out["title"] = title.strip() or manifest.get("title", "Plan imprimables")
    manifest_out["version"] = manifest_out.get("version", 1)
    manifest_out["items"] = filtered

    yaml_text = safe_dump(manifest_out, sort_keys=False, allow_unicode=True).strip()
    markdown = (
        f"# Pack imprimables — {manifest_out['title']}\\n\\n"
        f"- Scenario: {manifest_out['scenario_id']}\\n"
        f"- Items: {len(filtered)}\\n\\n"
        + "\\n".join(f"- {entry.get('id')} ({entry.get('category')})" for entry in filtered)
        + "\\n\\nGénéré via gateway IA locale.\\n"
    )
    return yaml_text, markdown


def write_json(self: BaseHTTPRequestHandler, status: int, payload: dict[str, Any]) -> None:
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    self.send_response(status)
    self.send_header("Content-Type", "application/json; charset=utf-8")
    self.send_header("Content-Length", str(len(body)))
    self.send_header("Access-Control-Allow-Origin", "*")
    self.send_header("Access-Control-Allow-Headers", "Content-Type")
    self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
    self.end_headers()
    self.wfile.write(body)


class Handler(BaseHTTPRequestHandler):
    llm_url: str
    llm_model: str
    image_url: str
    image_model: str
    image_timeout: int
    image_provider: str
    printable_manifest_default: Path
    fallback: bool

    def do_OPTIONS(self) -> None:
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def _load_body(self) -> dict[str, Any] | None:
        length_header = self.headers.get("Content-Length")
        if not length_header:
            return {}
        length = int(length_header)
        raw = self.rfile.read(length)
        try:
            return json.loads(raw.decode("utf-8"))
        except Exception:
            return None

    def do_POST(self) -> None:
        if self.path in {"/", "/health", "/healthz"}:
            return write_json(self, 200, {"status": "ok", "version": "local-studio-ai-gateway"})

        body = self._load_body()
        if body is None:
            return write_json(self, 400, {"error": "invalid_json"})

        mode = str(body.get("mode", "story_generate")).strip()
        if mode == "story_generate":
            return self.handle_story_generate(body)
        if mode == "printables_plan":
            return self.handle_printables_plan(body)
        if mode in {"visual_generate", "image_generate"}:
            return self.handle_visual_generate(body)

        return write_json(self, 400, {"error": f"mode not supported: {mode}"})

    def do_GET(self) -> None:
        if self.path in {"/", "/health", "/healthz"}:
            return write_json(self, 200, {"status": "ok", "version": "local-studio-ai-gateway"})

        if self.path == "/ping":
            return write_json(self, 200, {"status": "pong"})

        if self.path.startswith("/story_generate") or self.path.startswith("/printables_plan"):
            return write_json(self, 405, {"error": "use POST for this endpoint"})

        return write_json(self, 404, {"error": "not_found"})

    def handle_story_generate(self, body: dict[str, Any]) -> None:
        blueprint = body.get("scenario", {})
        if not isinstance(blueprint, dict):
            write_json(self, 400, {"error": "scenario payload must be an object"})
            return

        prompt = build_story_prompt(blueprint)
        yaml_text = ""
        rationale = "Scénario généré par le gateway local."

        try:
            llm_output = call_llm(self.llm_url, self.llm_model, prompt)
            yaml_text = extract_yaml(llm_output)
            parsed = safe_load(yaml_text)
            if not isinstance(parsed, dict):
                raise ValueError("LLM output is not a YAML object")
            rationale = "Scénario généré via LLM local (vérification YAML OK)."
            write_json(self, 200, {"yaml": yaml_text, "rationale": rationale, "source": "ai"})
            return
        except Exception as exc:  # noqa: BLE001
            if self.fallback:
                yaml_text = build_fallback_scenario(blueprint)
                rationale = f"Fallback local activé: {exc}"
                write_json(self, 200, {"yaml": yaml_text, "rationale": rationale, "source": "local"})
                return
            write_json(self, 502, {"error": str(exc), "source": "ai"})

    def handle_printables_plan(self, body: dict[str, Any]) -> None:
        scenario_id = sanitize_scenario_id(str(body.get("scenarioId", "CUSTOM")))
        title = str(body.get("title", scenario_id))
        selected = body.get("selected")
        selected_ids: list[str] | None
        if selected is None:
            selected_ids = None
        elif isinstance(selected, list):
            selected_ids = [str(item) for item in selected if str(item).strip()]
        else:
            write_json(self, 400, {"error": "selected must be an array"})
            return

        try:
            manifest_yaml, markdown = build_printables_plan(scenario_id, title, selected_ids)
            write_json(
                self,
                200,
                {
                    "manifest_yaml": manifest_yaml,
                    "markdown": markdown,
                    "items": len(markdown.splitlines()),
                    "source": "local",
                },
            )
        except Exception as exc:  # noqa: BLE001
            write_json(self, 500, {"error": str(exc)})

    def handle_visual_generate(self, body: dict[str, Any]) -> None:
        prompt = str(body.get("prompt", "")).strip()
        if not prompt:
            write_json(self, 400, {"error": "prompt is required"})
            return

        negative_prompt = str(body.get("negativePrompt", "")).strip() or str(body.get("negative_prompt", "")).strip()
        width = to_int(body.get("width"), 1024)
        height = to_int(body.get("height"), 1024)
        steps = to_int(body.get("steps"), 25)
        cfg_scale = to_float(body.get("cfgScale", body.get("cfg_scale", 7.5)), 7.5)
        seed = to_int(body.get("seed"), -1)
        count = to_int(body.get("count"), 1)
        model = str(body.get("model", self.image_model)).strip() or self.image_model
        provider = str(body.get("provider", self.image_provider)).strip() or self.image_provider

        width = max(256, min(width, 2048))
        height = max(256, min(height, 2048))
        steps = max(1, min(steps, 150))
        count = max(1, min(count, 4))

        try:
            images = call_image_generation(
                self.image_url,
                model=model,
                prompt=prompt,
                negative_prompt=negative_prompt,
                width=width,
                height=height,
                steps=steps,
                cfg_scale=cfg_scale,
                seed=seed,
                count=count,
                timeout=self.image_timeout,
                forced_provider=provider,
            )
            write_json(
                self,
                200,
                {
                    "images": images,
                    "count": len(images),
                    "provider": provider,
                    "source": "sd",
                },
            )
        except Exception as exc:  # noqa: BLE001
            write_json(self, 500, {"error": str(exc), "source": "image"})

def run_server(
    host: str,
    port: int,
    llm_url: str,
    llm_model: str,
    image_url: str,
    image_model: str,
    image_timeout: int,
    image_provider: str,
    fallback: bool,
) -> None:
    handler = Handler
    handler.llm_url = llm_url
    handler.llm_model = llm_model
    handler.image_url = image_url
    handler.image_model = image_model
    handler.image_timeout = image_timeout
    handler.image_provider = image_provider
    handler.fallback = fallback
    handler.printable_manifest_default = DEFAULT_PRINTABLES_MANIFEST

    server = HTTPServer((host, port), handler)
    print(f"Local Story AI gateway listening on http://{host}:{port}")
    print(f"LLM URL: {llm_url}")
    print(f"IMAGE URL: {image_url} (provider={image_provider})")
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    thread.join()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Local Zacus Studio AI gateway")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--llm-url", default=DEFAULT_LLM_URL)
    parser.add_argument("--llm-model", default=DEFAULT_LLM_MODEL)
    parser.add_argument(
        "--no-fallback",
        action="store_true",
        help="Do not use local fallback when AI is unavailable.",
    )
    parser.add_argument("--image-url", default=DEFAULT_IMAGE_URL)
    parser.add_argument("--image-model", default=DEFAULT_IMAGE_MODEL)
    parser.add_argument(
        "--image-provider",
        choices=["auto", IMAGE_PROVIDER_OPENAI, IMAGE_PROVIDER_SD_WEBUI],
        default="auto",
    )
    parser.add_argument("--image-timeout", type=int, default=DEFAULT_IMAGE_TIMEOUT)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        run_server(
            args.host,
            args.port,
            llm_url=args.llm_url,
            llm_model=args.llm_model,
            fallback=not args.no_fallback,
            image_url=args.image_url,
            image_model=args.image_model,
            image_timeout=args.image_timeout,
            image_provider=args.image_provider,
        )
    except KeyboardInterrupt:
        print("Stop.")
        return 0
    except Exception as exc:
        print(f"Failed to start gateway: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
