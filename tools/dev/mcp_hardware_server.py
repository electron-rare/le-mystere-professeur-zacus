#!/usr/bin/env python3
"""
MCP Hardware Server — Bridge LLM tool calls to Zacus ESP32 hardware

Implements 6 tools per specs/MCP_HARDWARE_SERVER_SPEC.md:
1. puzzle_set_state - Trigger puzzle events (unlock, attempt, hint)
2. audio_play - Play audio file on ESP32 speaker
3. led_set - Control NeoPixel LEDs
4. camera_capture - Trigger camera snapshot
5. scenario_advance - Advance story to next step
6. device_status - Get full device status

Transport: stdio (JSON-RPC 2.0)
Requires: ESP32 reachable at ESP32_URL (default http://192.168.4.1)

Usage:
    python3 mcp_hardware_server.py [--esp32-url http://IP:PORT] [--auth-token TOKEN]

MCP client config (claude_desktop_config.json):
    {
        "mcpServers": {
            "zacus-hardware": {
                "command": "python3",
                "args": ["/path/to/mcp_hardware_server.py", "--esp32-url", "http://192.168.4.1"]
            }
        }
    }
"""

import json
import sys
import os
import base64
import urllib.request
import urllib.error
import argparse
from typing import Any

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_ESP32_URL = os.environ.get("ESP32_URL", "http://192.168.4.1")
DEFAULT_AUTH_TOKEN = os.environ.get("ESP32_AUTH_TOKEN", "")
COMMAND_TIMEOUT = 5  # seconds

SERVER_INFO = {
    "name": "zacus-hardware",
    "version": "1.0.0",
}

SERVER_CAPABILITIES = {
    "tools": {},
}

# ---------------------------------------------------------------------------
# Tool definitions (MCP inputSchema from spec)
# ---------------------------------------------------------------------------

TOOL_DEFINITIONS = [
    {
        "name": "puzzle_set_state",
        "description": (
            "Set the state of a puzzle element (lock, unlock, reset). "
            "Triggers associated LED and audio effects."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
                "puzzle_id": {
                    "type": "string",
                    "description": "Puzzle identifier from scenario runtime",
                    "enum": [
                        "PUZZLE_FIOLE",
                        "PUZZLE_COFFRE",
                        "PUZZLE_MIROIR",
                        "PUZZLE_ENGRENAGE",
                        "PUZZLE_CRYSTAL",
                        "PUZZLE_BOUSSOLE",
                    ],
                },
                "state": {
                    "type": "string",
                    "enum": ["locked", "unlocked", "reset"],
                    "description": "Target state",
                },
                "effects": {
                    "type": "boolean",
                    "default": True,
                    "description": "Play associated LED/audio effects on state change",
                },
            },
            "required": ["device_id", "puzzle_id", "state"],
        },
    },
    {
        "name": "audio_play",
        "description": (
            "Play an audio file or stream on the ESP32 speaker. "
            "Supports local files (LittleFS) and HTTP URLs."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
                "source": {
                    "type": "string",
                    "description": "Audio source: LittleFS path (/audio/hint_01.mp3) or HTTP URL",
                },
                "volume": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 100,
                    "default": 70,
                    "description": "Playback volume (0-100)",
                },
                "loop": {
                    "type": "boolean",
                    "default": False,
                    "description": "Loop playback continuously",
                },
                "action": {
                    "type": "string",
                    "enum": ["play", "stop", "pause", "resume"],
                    "default": "play",
                },
            },
            "required": ["device_id", "source"],
        },
    },
    {
        "name": "led_set",
        "description": (
            "Control LED strips: set color, pattern, brightness. "
            "Supports WS2812B addressable LEDs."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
                "zone": {
                    "type": "string",
                    "description": "LED zone identifier",
                    "enum": ["ambient", "puzzle", "alert", "all"],
                },
                "color": {
                    "type": "string",
                    "description": "Hex color (#RRGGBB) or named color",
                },
                "pattern": {
                    "type": "string",
                    "enum": ["solid", "breathe", "chase", "rainbow", "pulse", "off"],
                    "default": "solid",
                },
                "brightness": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 255,
                    "default": 128,
                },
                "duration_ms": {
                    "type": "integer",
                    "description": "Auto-off after duration (0 = indefinite)",
                    "default": 0,
                },
            },
            "required": ["device_id", "zone", "color"],
        },
    },
    {
        "name": "camera_capture",
        "description": (
            "Capture a JPEG snapshot from the ESP32 camera. "
            "Returns base64-encoded image."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
                "resolution": {
                    "type": "string",
                    "enum": ["QQVGA", "QVGA", "VGA"],
                    "default": "QVGA",
                    "description": "Capture resolution (160x120, 320x240, 640x480)",
                },
                "quality": {
                    "type": "integer",
                    "minimum": 10,
                    "maximum": 63,
                    "default": 20,
                    "description": "JPEG quality (lower = better, 10-63)",
                },
            },
            "required": ["device_id"],
        },
    },
    {
        "name": "scenario_advance",
        "description": (
            "Trigger a Runtime 3 scenario transition. "
            "Used by game masters to manually advance or reset the game."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
                "event_type": {
                    "type": "string",
                    "enum": [
                        "button", "serial", "timer", "audio_done",
                        "unlock", "espnow", "action", "manual",
                    ],
                    "description": "Event type per Runtime 3 transition model",
                },
                "event_name": {
                    "type": "string",
                    "description": "Event name token (e.g., UNLOCK_COFFRE, MANUAL_ADVANCE)",
                },
                "target_step_id": {
                    "type": "string",
                    "description": "Optional: force transition to specific step",
                },
            },
            "required": ["device_id", "event_type", "event_name"],
        },
    },
    {
        "name": "device_status",
        "description": (
            "Get current device status: free memory, current scenario step, "
            "uptime, WiFi RSSI, sensor readings."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "device_id": {
                    "type": "string",
                    "description": "Target ESP32 device identifier",
                },
            },
            "required": ["device_id"],
        },
    },
]

# ---------------------------------------------------------------------------
# Logging helper (always stderr to keep stdout clean for JSON-RPC)
# ---------------------------------------------------------------------------


def log(msg: str) -> None:
    print(f"[mcp-hw] {msg}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# ESP32 HTTP client
# ---------------------------------------------------------------------------


class ESP32Client:
    """Thin HTTP client for ESP32 REST API."""

    def __init__(self, base_url: str, auth_token: str = ""):
        self.base_url = base_url.rstrip("/")
        self.auth_token = auth_token

    def _headers(self) -> dict[str, str]:
        h = {"Content-Type": "application/json"}
        if self.auth_token:
            h["Authorization"] = f"Bearer {self.auth_token}"
        return h

    def request(
        self, method: str, path: str, body: dict | None = None
    ) -> tuple[int, Any]:
        """Send HTTP request to ESP32. Returns (status_code, parsed_json | raw_bytes)."""
        url = f"{self.base_url}{path}"
        data = json.dumps(body).encode() if body else None

        req = urllib.request.Request(
            url,
            data=data,
            headers=self._headers(),
            method=method.upper(),
        )

        try:
            with urllib.request.urlopen(req, timeout=COMMAND_TIMEOUT) as resp:
                raw = resp.read()
                ct = resp.headers.get("Content-Type", "")
                if "image/" in ct:
                    return resp.status, raw  # binary
                try:
                    return resp.status, json.loads(raw)
                except (json.JSONDecodeError, ValueError):
                    return resp.status, raw.decode(errors="replace")
        except urllib.error.HTTPError as exc:
            body_text = ""
            try:
                body_text = exc.read().decode(errors="replace")
            except Exception:
                pass
            return exc.code, {"error": exc.reason, "detail": body_text}
        except urllib.error.URLError as exc:
            raise ConnectionError(f"Device unreachable: {exc.reason}") from exc
        except TimeoutError:
            raise ConnectionError(
                f"Device timeout after {COMMAND_TIMEOUT}s"
            )


# ---------------------------------------------------------------------------
# Tool handlers
# ---------------------------------------------------------------------------


def _validate_required(args: dict, required: list[str]) -> str | None:
    """Return error message if any required param is missing."""
    missing = [k for k in required if k not in args]
    if missing:
        return f"Missing required parameters: {', '.join(missing)}"
    return None


def handle_puzzle_set_state(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id", "puzzle_id", "state"])
    if err:
        raise ValueError(err)

    valid_states = {"locked", "unlocked", "reset"}
    if args["state"] not in valid_states:
        raise ValueError(f"Invalid state '{args['state']}', must be one of {valid_states}")

    body = {
        "id": args["puzzle_id"],
        "state": args["state"],
        "effects": args.get("effects", True),
    }
    status, resp = client.request("POST", "/api/puzzle", body)

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    return {
        "content": [
            {
                "type": "text",
                "text": (
                    f"Puzzle '{args['puzzle_id']}' set to '{args['state']}' "
                    f"on device {args['device_id']} (effects={'on' if args.get('effects', True) else 'off'})"
                ),
            }
        ]
    }


def handle_audio_play(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id", "source"])
    if err:
        raise ValueError(err)

    body = {
        "src": args["source"],
        "vol": args.get("volume", 70),
        "loop": args.get("loop", False),
        "action": args.get("action", "play"),
    }
    status, resp = client.request("POST", "/api/audio", body)

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    return {
        "content": [
            {
                "type": "text",
                "text": (
                    f"Audio '{args['source']}' {body['action']} "
                    f"at volume {body['vol']} on device {args['device_id']}"
                ),
            }
        ]
    }


def handle_led_set(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id", "zone", "color"])
    if err:
        raise ValueError(err)

    body = {
        "zone": args["zone"],
        "color": args["color"],
        "pattern": args.get("pattern", "solid"),
        "bright": args.get("brightness", 128),
    }
    if args.get("duration_ms", 0) > 0:
        body["duration_ms"] = args["duration_ms"]

    status, resp = client.request("POST", "/api/led", body)

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    return {
        "content": [
            {
                "type": "text",
                "text": (
                    f"LED zone '{args['zone']}' set to {args['color']} "
                    f"{body['pattern']} at brightness {body['bright']} "
                    f"on device {args['device_id']}"
                ),
            }
        ]
    }


def handle_camera_capture(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id"])
    if err:
        raise ValueError(err)

    res = args.get("resolution", "QVGA")
    quality = args.get("quality", 20)
    path = f"/api/camera?res={res}&q={quality}"

    status, resp = client.request("GET", path)

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    if isinstance(resp, bytes):
        b64 = base64.b64encode(resp).decode("ascii")
        return {
            "content": [
                {
                    "type": "image",
                    "data": b64,
                    "mimeType": "image/jpeg",
                },
                {
                    "type": "text",
                    "text": (
                        f"Camera capture from {args['device_id']} "
                        f"({res}, quality={quality}, {len(resp)} bytes)"
                    ),
                },
            ]
        }
    else:
        return {
            "content": [
                {
                    "type": "text",
                    "text": f"Camera capture from {args['device_id']}: {resp}",
                }
            ]
        }


def handle_scenario_advance(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id", "event_type", "event_name"])
    if err:
        raise ValueError(err)

    body = {
        "event_type": args["event_type"],
        "event_name": args["event_name"],
    }
    if args.get("target_step_id"):
        body["target"] = args["target_step_id"]

    status, resp = client.request("POST", "/api/scenario/transition", body)

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    return {
        "content": [
            {
                "type": "text",
                "text": (
                    f"Scenario transition '{args['event_name']}' "
                    f"(type={args['event_type']}) sent to {args['device_id']}"
                ),
            }
        ]
    }


def handle_device_status(client: ESP32Client, args: dict) -> dict:
    err = _validate_required(args, ["device_id"])
    if err:
        raise ValueError(err)

    status, resp = client.request("GET", "/api/status")

    if status == 401:
        raise PermissionError("ESP32 auth failed (401)")
    if status >= 400:
        raise RuntimeError(f"ESP32 returned {status}: {resp}")

    if isinstance(resp, dict):
        text = json.dumps(resp, indent=2)
    else:
        text = str(resp)

    return {
        "content": [
            {
                "type": "text",
                "text": f"Device {args['device_id']} status:\n{text}",
            }
        ]
    }


TOOL_HANDLERS = {
    "puzzle_set_state": handle_puzzle_set_state,
    "audio_play": handle_audio_play,
    "led_set": handle_led_set,
    "camera_capture": handle_camera_capture,
    "scenario_advance": handle_scenario_advance,
    "device_status": handle_device_status,
}

# ---------------------------------------------------------------------------
# JSON-RPC 2.0 helpers
# ---------------------------------------------------------------------------

# MCP error codes
PARSE_ERROR = -32700
INVALID_REQUEST = -32600
METHOD_NOT_FOUND = -32601
INVALID_PARAMS = -32602
INTERNAL_ERROR = -32603
DEVICE_UNREACHABLE = -32000
DEVICE_BUSY = -32001
AUTH_FAILED = -32002
PUZZLE_CONFLICT = -32003


def jsonrpc_result(id_: Any, result: Any) -> dict:
    return {"jsonrpc": "2.0", "id": id_, "result": result}


def jsonrpc_error(id_: Any, code: int, message: str, data: Any = None) -> dict:
    err: dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        err["data"] = data
    return {"jsonrpc": "2.0", "id": id_, "error": err}


# ---------------------------------------------------------------------------
# MCP protocol handlers
# ---------------------------------------------------------------------------


def handle_initialize(params: dict | None) -> dict:
    """MCP initialize — return server info and capabilities."""
    return {
        "protocolVersion": "2024-11-05",
        "serverInfo": SERVER_INFO,
        "capabilities": SERVER_CAPABILITIES,
    }


def handle_tools_list(params: dict | None) -> dict:
    """MCP tools/list — return all tool definitions."""
    return {"tools": TOOL_DEFINITIONS}


def handle_tools_call(client: ESP32Client, params: dict) -> dict:
    """MCP tools/call — execute a tool by proxying to ESP32."""
    if not params or "name" not in params:
        raise ValueError("Missing 'name' in tools/call params")

    tool_name = params["name"]
    arguments = params.get("arguments", {})

    handler = TOOL_HANDLERS.get(tool_name)
    if handler is None:
        raise KeyError(f"Unknown tool: {tool_name}")

    return handler(client, arguments)


# ---------------------------------------------------------------------------
# Main server loop
# ---------------------------------------------------------------------------


class MCPServer:
    """MCP server over stdio transport (JSON-RPC 2.0, one message per line)."""

    def __init__(self, esp32_url: str, auth_token: str = ""):
        self.client = ESP32Client(esp32_url, auth_token)
        self.initialized = False
        log(f"Server created, ESP32 target: {esp32_url}")

    def process_message(self, raw: str) -> dict | None:
        """Process a single JSON-RPC message and return a response (or None for notifications)."""
        # Parse JSON
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError as exc:
            return jsonrpc_error(None, PARSE_ERROR, f"Parse error: {exc}")

        # Validate basic structure
        if not isinstance(msg, dict) or msg.get("jsonrpc") != "2.0":
            return jsonrpc_error(
                msg.get("id"), INVALID_REQUEST, "Invalid JSON-RPC 2.0 request"
            )

        method = msg.get("method", "")
        id_ = msg.get("id")
        params = msg.get("params")

        # Notifications (no id) — handle but don't respond
        if id_ is None:
            if method == "notifications/initialized":
                self.initialized = True
                log("Client sent initialized notification")
            elif method == "notifications/cancelled":
                log(f"Request cancelled: {params}")
            else:
                log(f"Unknown notification: {method}")
            return None

        # Route methods
        try:
            if method == "initialize":
                result = handle_initialize(params)
                return jsonrpc_result(id_, result)

            elif method == "tools/list":
                result = handle_tools_list(params)
                return jsonrpc_result(id_, result)

            elif method == "tools/call":
                if not params:
                    return jsonrpc_error(
                        id_, INVALID_PARAMS, "Missing params for tools/call"
                    )
                result = handle_tools_call(self.client, params)
                return jsonrpc_result(id_, result)

            else:
                return jsonrpc_error(
                    id_, METHOD_NOT_FOUND, f"Method not found: {method}"
                )

        except ValueError as exc:
            return jsonrpc_error(id_, INVALID_PARAMS, str(exc))

        except KeyError as exc:
            return jsonrpc_error(id_, METHOD_NOT_FOUND, str(exc))

        except PermissionError as exc:
            return jsonrpc_error(
                id_, AUTH_FAILED, str(exc),
                data={"detail": "ESP32 rejected the auth token"},
            )

        except ConnectionError as exc:
            return jsonrpc_error(
                id_, DEVICE_UNREACHABLE, str(exc),
                data={"detail": f"Could not reach ESP32 at {self.client.base_url}"},
            )

        except Exception as exc:
            log(f"Internal error: {exc}")
            return jsonrpc_error(id_, INTERNAL_ERROR, f"Internal error: {exc}")

    def run(self) -> None:
        """Read JSON-RPC messages from stdin, write responses to stdout."""
        log("MCP Hardware Server starting on stdio transport")
        log(f"ESP32 URL: {self.client.base_url}")

        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue

            log(f"<-- {line[:200]}")

            response = self.process_message(line)
            if response is not None:
                out = json.dumps(response)
                log(f"--> {out[:200]}")
                sys.stdout.write(out + "\n")
                sys.stdout.flush()

        log("stdin closed, shutting down")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="MCP Hardware Server — Bridge LLM tool calls to Zacus ESP32"
    )
    parser.add_argument(
        "--esp32-url",
        default=DEFAULT_ESP32_URL,
        help=f"ESP32 base URL (default: {DEFAULT_ESP32_URL})",
    )
    parser.add_argument(
        "--auth-token",
        default=DEFAULT_AUTH_TOKEN,
        help="Bearer token for ESP32 auth",
    )
    args = parser.parse_args()

    server = MCPServer(args.esp32_url, args.auth_token)
    try:
        server.run()
    except KeyboardInterrupt:
        log("Interrupted, shutting down")
        sys.exit(0)


if __name__ == "__main__":
    main()
