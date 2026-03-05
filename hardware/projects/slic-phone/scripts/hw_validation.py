#!/usr/bin/env python3
"""A252-only hardware validation runner (without bench controller)."""

from __future__ import annotations

import argparse
import glob
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib import error, parse, request

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None


VALID_STATES = {"PASS", "FAIL", "MANUAL_PASS", "MANUAL_FAIL", "MANUAL_SKIP"}
OPTIONAL_SERIAL_COMMANDS = {"WIFI_SCAN", "ESPNOW_STATUS"}
EXPECTED_FIRMWARE_CONTRACT_VERSION = "A252_AUDIO_CHAIN_V4"
SUPPORTED_INPUT_BITS = {8, 16, 24, 32}
SUPPORTED_OUTPUT_BITS = {16}
SUPPORTED_CHANNELS = {1, 2}
SUPPORTED_OUTPUT_SAMPLE_RATES = {8000, 16000, 22050, 32000, 44100, 48000}
REQUIRED_FIRMWARE_KEYS = ("build_id", "git_sha", "contract_version")
REQUIRED_AUDIO_STATUS_KEYS = (
    "tone_route_active",
    "tone_rendering",
    "playback_input_sample_rate",
    "playback_input_bits_per_sample",
    "playback_input_channels",
    "playback_output_sample_rate",
    "playback_output_bits_per_sample",
    "playback_output_channels",
    "playback_resampler_active",
    "playback_channel_upmix_active",
    "playback_loudness_auto",
    "playback_loudness_gain_db",
    "playback_limiter_active",
    "playback_rate_fallback",
    "playback_copy_source_bytes",
    "playback_copy_accepted_bytes",
    "playback_copy_loss_bytes",
    "playback_copy_loss_events",
    "playback_last_error",
)
REQUIRED_AUDIO_PROBE_KEYS = (
    "input_sample_rate",
    "input_bits_per_sample",
    "input_channels",
    "output_sample_rate",
    "output_bits_per_sample",
    "output_channels",
    "resampler_active",
    "channel_upmix_active",
    "loudness_auto",
    "loudness_gain_db",
    "limiter_active",
    "rate_fallback",
    "data_size_bytes",
    "duration_ms",
)


@dataclass
class ScenarioResult:
    name: str
    state: str
    details: Dict[str, Any]


def run_cmd(cmd: List[str]) -> None:
    print(f"[hw_validation] $ {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _extract_audio_status(status_payload: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(status_payload, dict):
        return {}
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return {}
    return audio


def _parse_json_from_line(line: str) -> Optional[Any]:
    candidates: List[str] = []
    stripped = line.strip()
    if stripped:
        candidates.append(stripped)
    if "{" in line and "}" in line:
        start = line.find("{")
        end = line.rfind("}")
        if 0 <= start < end:
            candidates.append(line[start : end + 1])
    if "[" in line and "]" in line:
        start = line.find("[")
        end = line.rfind("]")
        if 0 <= start < end:
            candidates.append(line[start : end + 1])

    for candidate in candidates:
        text = candidate.strip()
        if not text:
            continue
        if text[0] not in ("{", "["):
            continue
        if text[-1] not in ("}", "]"):
            continue
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            continue
    return None


class SerialEndpoint:
    def __init__(self, port: str, baud: int, timeout_s: float = 0.5) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required (pip install pyserial)")
        self.port = port
        self.baud = baud
        self.timeout_s = timeout_s
        self._ser: Optional[serial.Serial] = None

    def __enter__(self) -> "SerialEndpoint":
        self._ser = serial.Serial(self.port, self.baud, timeout=self.timeout_s)
        time.sleep(0.8)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def command(self, cmd: str, timeout_s: float = 6.0, expect: str = "any") -> Dict[str, Any]:
        if not self._ser or not self._ser.is_open:
            raise RuntimeError("serial port not open")
        self._ser.reset_input_buffer()
        self._ser.write((cmd + "\r\n").encode())
        self._ser.flush()

        deadline = time.time() + timeout_s
        last_line = ""
        while time.time() < deadline:
            raw = self._ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            last_line = line
            print(f"[{self.port}] {line}")
            if expect in {"any", "json"}:
                parsed = _parse_json_from_line(line)
                if parsed is not None:
                    return parsed
            ok_pos = line.find("OK ")
            err_pos = line.find("ERR ")
            ack_pos = -1
            ack_ok = False
            if ok_pos >= 0 and (err_pos < 0 or ok_pos <= err_pos):
                ack_pos = ok_pos
                ack_ok = True
            elif err_pos >= 0:
                ack_pos = err_pos
                ack_ok = False
            if ack_pos >= 0:
                if expect in {"any", "ack"}:
                    return {"ok": ack_ok, "line": line[ack_pos:]}
                continue
            if line == "PONG" or "PONG" in line:
                if expect in {"any", "pong", "ack"}:
                    return {"ok": True, "result": "PONG"}
                continue
        raise RuntimeError(f"timeout on command '{cmd}' last='{last_line}'")

    def sync(self, retries: int = 6) -> None:
        last_error = ""
        for _ in range(retries):
            try:
                self.command("PING", timeout_s=2.0, expect="pong")
                return
            except Exception as exc:  # pragma: no cover - hardware timing
                last_error = str(exc)
                time.sleep(0.5)
        raise RuntimeError(f"serial sync failed: {last_error}")


def resolve_a252_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port

    preferred_patterns = (
        "/dev/cu.usbserial*",
        "/dev/tty.usbserial*",
    )

    for pattern in preferred_patterns:
        candidates = sorted(glob.glob(pattern))
        if candidates:
            print(f"[hw_validation] A252 locked to USB-Serial port: {candidates[0]}")
            return candidates[0]

    raise RuntimeError(
        "No USB-Serial port found for A252. Expected /dev/tty.usbserial* or /dev/cu.usbserial*."
    )


def fetch_json(url: str) -> Dict[str, Any]:
    req = request.Request(url, method="GET")
    with request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))


def post_json(url: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    raw = json.dumps(payload)
    req = request.Request(
        url,
        method="POST",
        data=raw.encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    try:
        with request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except error.HTTPError as exc:
        if exc.code != 400:
            raise
        # Fallback for endpoints implemented with AsyncWebServer "plain" body extraction.
        fallback = request.Request(
            url,
            method="POST",
            data=parse.urlencode({"plain": raw}).encode("utf-8"),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        with request.urlopen(fallback, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))


def scenario_serial_smoke(
    dev: SerialEndpoint,
    strict_serial_smoke: bool,
    allow_capture_fail_when_disabled: bool,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["ping"] = dev.command("PING", expect="pong")
        details["status"] = dev.command("STATUS", expect="json")
        details["capture_start"] = dev.command("CAPTURE_START", expect="ack")
        details["capture_stop"] = dev.command("CAPTURE_STOP", expect="ack")
        details["reset_metrics"] = dev.command("RESET_METRICS", expect="ack")
        state, required_checks, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=strict_serial_smoke,
            allow_capture_fail_when_disabled=allow_capture_fail_when_disabled,
        )
        details["required_checks"] = required_checks
        details["failed_checks"] = failed_checks
        details["warnings"] = warnings
        return ScenarioResult("serial_smoke", state, details)
    except Exception as exc:
        return ScenarioResult("serial_smoke", "FAIL", {"error": str(exc)})


def scenario_serial_firmware_contract(
    dev: SerialEndpoint,
    *,
    required_contract_version: str,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["status"] = dev.command("STATUS", expect="json")
        firmware = details["status"].get("firmware", {}) if isinstance(details["status"], dict) else {}
        audio = _extract_audio_status(details["status"] if isinstance(details["status"], dict) else {})

        missing_firmware = [key for key in REQUIRED_FIRMWARE_KEYS if key not in firmware]
        missing_audio = [key for key in REQUIRED_AUDIO_STATUS_KEYS if key not in audio]

        contract_value = ""
        if isinstance(firmware, dict):
            contract_value = str(firmware.get("contract_version", "")).strip()

        checks = {
            "firmware_block_present": isinstance(firmware, dict),
            "firmware_required_keys_present": not missing_firmware,
            "audio_required_keys_present": not missing_audio,
            "contract_version_matches": (
                not required_contract_version
                or contract_value == required_contract_version
            ),
        }

        details["checks"] = checks
        details["missing_firmware_keys"] = missing_firmware
        details["missing_audio_keys"] = missing_audio
        details["contract_version"] = contract_value
        details["required_contract_version"] = required_contract_version
        return ScenarioResult(
            "serial_firmware_contract",
            "PASS" if all(checks.values()) else "FAIL",
            details,
        )
    except Exception as exc:
        return ScenarioResult("serial_firmware_contract", "FAIL", {"error": str(exc), **details})


def _is_success_response(resp: Dict[str, Any]) -> bool:
    if "line" in resp:
        line = str(resp.get("line")).strip().upper()
        if line.startswith("OK "):
            return True
        if line.startswith("ERR "):
            return False
        return False
    if "ok" in resp:
        return bool(resp.get("ok"))
    return True


def _has_espnow_capability(resp: Dict[str, Any]) -> bool:
    if not isinstance(resp, dict):
        return False
    if not bool(resp.get("ready")):
        return False
    peers = resp.get("peer_count")
    return isinstance(peers, int) and peers > 0


def _normalize_command(command: str) -> str:
    return command.strip().upper()


def _is_soft_unsupported(command: str, resp: Dict[str, Any]) -> bool:
    normalized_command = _normalize_command(command)
    if normalized_command not in OPTIONAL_SERIAL_COMMANDS:
        return False

    if not isinstance(resp, dict):
        return False

    if isinstance(resp.get("line"), str):
        line = resp["line"].strip().upper()
        return line.startswith("ERR UNSUPPORTED_COMMAND") or "UNSUPPORTED" in line

    if isinstance(resp.get("code"), str):
        code = resp["code"].strip().upper()
        return "UNSUPPORTED" in code

    return False


def _is_acceptable_response(command: str, resp: Dict[str, Any], required: bool) -> bool:
    if required:
        return _is_success_response(resp)
    if _is_soft_unsupported(command, resp):
        return True
    return _is_success_response(resp)


def _extract_capture_enabled(status_payload: Dict[str, Any]) -> bool:
    if not isinstance(status_payload, dict):
        return True
    config = status_payload.get("config")
    if not isinstance(config, dict):
        return True
    audio = config.get("audio")
    if not isinstance(audio, dict):
        return True
    value = audio.get("enable_capture")
    if isinstance(value, bool):
        return value
    return True


def evaluate_serial_smoke_contract(
    details: Dict[str, Any],
    *,
    strict_serial_smoke: bool,
    allow_capture_fail_when_disabled: bool,
) -> tuple[str, List[str], List[str], List[str]]:
    warnings: List[str] = []
    failed_checks: List[str] = []

    ping_ok = bool(details.get("ping", {}).get("ok"))
    status_payload = details.get("status", {})
    status_ok = isinstance(status_payload, dict) and "telephony" in status_payload
    capture_start_ok = _is_success_response(details.get("capture_start", {}))
    capture_stop_ok = _is_success_response(details.get("capture_stop", {}))
    reset_metrics_ok = _is_success_response(details.get("reset_metrics", {}))

    capture_enabled = _extract_capture_enabled(status_payload if isinstance(status_payload, dict) else {})
    capture_start_required = capture_enabled or (not allow_capture_fail_when_disabled)

    required_checks: List[str] = ["PING", "STATUS", "CAPTURE_STOP", "RESET_METRICS"]
    if capture_start_required:
        required_checks.append("CAPTURE_START")

    if not ping_ok:
        failed_checks.append("PING")
    if not status_ok:
        failed_checks.append("STATUS")
    if not capture_stop_ok:
        failed_checks.append("CAPTURE_STOP")
    if not reset_metrics_ok:
        failed_checks.append("RESET_METRICS")
    if capture_start_required and not capture_start_ok:
        failed_checks.append("CAPTURE_START")

    if not capture_enabled and not capture_start_ok:
        if allow_capture_fail_when_disabled:
            warnings.append("capture_start_failed_capture_disabled")
        else:
            warnings.append("capture_start_required_even_when_capture_disabled")

    if strict_serial_smoke:
        return ("PASS" if not failed_checks else "FAIL", required_checks, failed_checks, warnings)

    minimum_failures = [check for check in failed_checks if check in {"PING", "STATUS"}]
    if failed_checks and not minimum_failures:
        warnings.append("strict_serial_smoke_disabled")
    return ("PASS" if not minimum_failures else "FAIL", required_checks, failed_checks, warnings)


def _quote_arg(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _command_with_retry(
    dev: SerialEndpoint,
    command: str,
    *,
    expect: str = "any",
    timeout_s: float = 6.0,
    attempts: int = 1,
    retry_delay_s: float = 0.5,
) -> Dict[str, Any]:
    last_error: Optional[Exception] = None
    effective_attempts = max(1, attempts)
    for attempt in range(effective_attempts):
        try:
            return dev.command(command, timeout_s=timeout_s, expect=expect)
        except Exception as exc:
            last_error = exc
            if attempt + 1 >= effective_attempts:
                break
            time.sleep(retry_delay_s)
    raise RuntimeError(str(last_error) if last_error else f"command failed: {command}")


def scenario_serial_network(dev: SerialEndpoint, wifi_ssid: str, wifi_password: str) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["wifi_status_before"] = dev.command("WIFI_STATUS", expect="json")
        # Wi-Fi scan can be slow right after boot/flash; retry with a longer timeout.
        details["wifi_scan"] = _command_with_retry(
            dev,
            "WIFI_SCAN",
            expect="any",
            timeout_s=12.0,
            attempts=2,
            retry_delay_s=1.0,
        )
        if wifi_ssid:
            already_connected = (
                bool(details["wifi_status_before"].get("connected"))
                and str(details["wifi_status_before"].get("ssid", "")) == wifi_ssid
            )
            if already_connected:
                details["wifi_connect"] = {"ok": True, "line": "SKIP WIFI_CONNECT already_connected"}
                details["wifi_status_after"] = details["wifi_status_before"]
            else:
                details["wifi_connect"] = dev.command(
                    f"WIFI_CONNECT {_quote_arg(wifi_ssid)} {_quote_arg(wifi_password)}",
                    timeout_s=20.0,
                    expect="ack",
                )
                time.sleep(2.0)
                details["wifi_status_after"] = dev.command("WIFI_STATUS", expect="json")
        # Re-sync dispatcher before ESPNOW_STATUS because telephony logs can interleave and
        # occasionally delay a response right after ring/tone scenarios.
        details["ping_before_espnow"] = _command_with_retry(
            dev,
            "PING",
            expect="pong",
            timeout_s=3.0,
            attempts=3,
            retry_delay_s=0.4,
        )
        details["espnow_status"] = _command_with_retry(
            dev,
            "ESPNOW_STATUS",
            expect="any",
            timeout_s=10.0,
            attempts=4,
            retry_delay_s=1.0,
        )

        checks = [
            ("WIFI_STATUS", details["wifi_status_before"], True),
            ("WIFI_SCAN", details["wifi_scan"], False),
            ("ESPNOW_STATUS", details["espnow_status"], True),
        ]
        if wifi_ssid and "wifi_status_after" in details:
            if "wifi_connect" in details:
                checks.append(("WIFI_CONNECT", details["wifi_connect"], True))
            checks.append(("WIFI_STATUS", details["wifi_status_after"], True))

        ok = True
        for command, value, required in checks:
            if command == "ESPNOW_STATUS" and not _has_espnow_capability(value):
                ok = False
                break
            if not _is_acceptable_response(command, value, required):
                ok = False
                break

        if ok and wifi_ssid and "wifi_status_after" in details:
            wifi_after = details["wifi_status_after"]
            if not (
                isinstance(wifi_after, dict)
                and bool(wifi_after.get("connected"))
                and str(wifi_after.get("ssid", "")) == wifi_ssid
            ):
                ok = False
        return ScenarioResult("serial_network_stack", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_network_stack", "FAIL", {"error": str(exc), **details})


def _extract_hook_state(status_payload: Dict[str, Any]) -> str:
    if not isinstance(status_payload, dict):
        return ""
    telephony = status_payload.get("telephony")
    if not isinstance(telephony, dict):
        return ""
    hook = telephony.get("hook")
    if not isinstance(hook, str):
        return ""
    value = hook.strip().upper()
    if value in {"ON_HOOK", "OFF_HOOK"}:
        return value
    return ""


def _extract_dial_tone_active(status_payload: Dict[str, Any]) -> Optional[bool]:
    if not isinstance(status_payload, dict):
        return None
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return None
    value = audio.get("dial_tone_active")
    if isinstance(value, bool):
        return value
    return None


def _extract_tone_active(status_payload: Dict[str, Any]) -> Optional[bool]:
    if not isinstance(status_payload, dict):
        return None
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return None
    value = audio.get("tone_active")
    if isinstance(value, bool):
        return value
    value = audio.get("tone_rendering")
    if isinstance(value, bool):
        return value
    value = audio.get("tone_route_active")
    if isinstance(value, bool):
        return value
    return _extract_dial_tone_active(status_payload)


def _extract_tone_route_active(status_payload: Dict[str, Any]) -> Optional[bool]:
    if not isinstance(status_payload, dict):
        return None
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return None
    value = audio.get("tone_route_active")
    if isinstance(value, bool):
        return value
    return _extract_tone_active(status_payload)


def _extract_tone_rendering(status_payload: Dict[str, Any]) -> Optional[bool]:
    if not isinstance(status_payload, dict):
        return None
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return None
    value = audio.get("tone_rendering")
    if isinstance(value, bool):
        return value
    return _extract_tone_active(status_payload)


def _is_supported_tone_bits(value: Any) -> bool:
    if not isinstance(value, int):
        return False
    return value in {16, 24, 32}


def _extract_tone_event(status_payload: Dict[str, Any]) -> str:
    if not isinstance(status_payload, dict):
        return ""
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return ""
    value = audio.get("tone_event")
    if not isinstance(value, str):
        return ""
    return value.strip().lower()


def scenario_serial_hook_ring_audio(
    dev: SerialEndpoint,
    *,
    require_hook_toggle: bool,
    hook_observe_seconds: int,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    observed_hooks: List[str] = []
    warnings: List[str] = []

    try:
        details["status_before"] = dev.command("STATUS", expect="json")
        initial_hook = _extract_hook_state(details["status_before"])
        if initial_hook:
            observed_hooks.append(initial_hook)

        details["tone_on"] = dev.command("TONE_ON", expect="ack")
        time.sleep(0.4)
        details["status_tone_on"] = dev.command("STATUS", expect="json")
        hook_after_tone_on = _extract_hook_state(details["status_tone_on"])
        if hook_after_tone_on:
            observed_hooks.append(hook_after_tone_on)

        details["tone_off"] = dev.command("TONE_OFF", expect="ack")
        time.sleep(0.3)
        details["status_tone_off"] = dev.command("STATUS", expect="json")
        hook_after_tone_off = _extract_hook_state(details["status_tone_off"])
        if hook_after_tone_off:
            observed_hooks.append(hook_after_tone_off)

        observe_seconds = max(0, hook_observe_seconds)
        poll_deadline = time.time() + float(observe_seconds)
        while time.time() < poll_deadline:
            poll_status = dev.command("STATUS", expect="json")
            hook = _extract_hook_state(poll_status)
            if hook:
                observed_hooks.append(hook)
            time.sleep(0.8)

        unique_hooks = sorted(set(observed_hooks))
        details["hook_values_observed"] = unique_hooks
        details["hook_observe_seconds"] = observe_seconds

        tone_on_ok = _is_success_response(details["tone_on"])
        tone_off_ok = _is_success_response(details["tone_off"])
        tone_route_active_after_on = _extract_tone_route_active(details["status_tone_on"]) is True
        tone_rendering_after_on = _extract_tone_rendering(details["status_tone_on"]) is True
        tone_event_after_on = _extract_tone_event(details["status_tone_on"]) == "dial"
        tone_route_inactive_after_off = _extract_tone_route_active(details["status_tone_off"]) is False
        tone_rendering_after_off = _extract_tone_rendering(details["status_tone_off"])

        if require_hook_toggle:
            required_hooks = ["ON_HOOK", "OFF_HOOK"]
            hook_ok = all(state in unique_hooks for state in required_hooks)
            details["required_hook_states"] = required_hooks
        else:
            hook_ok = True
            required_hooks = ["BYPASSED"]
            details["required_hook_states"] = required_hooks
            details["hook_validation_mode"] = "BYPASSED_NON_PRESENTIEL"
            warnings.append("hook checks skipped because --no-require-hook-toggle is enabled")

        details["checks"] = {
            "tone_on_ok": tone_on_ok,
            "tone_route_active_after_on": tone_route_active_after_on,
            "tone_rendering_after_on": tone_rendering_after_on,
            "tone_event_after_on": tone_event_after_on,
            "tone_off_ok": tone_off_ok,
            "tone_route_inactive_after_off": tone_route_inactive_after_off,
            "hook_ok": hook_ok,
        }
        if warnings:
            details["warnings"] = warnings
        details["tone_rendering_after_off"] = tone_rendering_after_off

        ok = (
            tone_on_ok
            and tone_route_active_after_on
            and tone_rendering_after_on
            and tone_event_after_on
            and tone_off_ok
            and tone_route_inactive_after_off
            and hook_ok
        )
        return ScenarioResult("serial_hook_ring_audio", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_hook_ring_audio", "FAIL", {"error": str(exc), **details})


def scenario_serial_media_routing(dev: SerialEndpoint) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        dial_payload = '{"0123456789":{"kind":"tone","profile":"FR_FR","event":"ringback"}}'
        espnow_payload = '{"LA_OK":{"kind":"tone","profile":"FR_FR","event":"busy"}}'
        espnow_legacy_payload = '{"LA_BUSY":"/assets/wav/FR_FR/busy.wav"}'

        details["dial_media_map_set"] = dev.command(f"DIAL_MEDIA_MAP_SET_VOLATILE {dial_payload}", expect="ack")
        details["dial_media_map_get"] = dev.command("DIAL_MEDIA_MAP_GET", expect="json")
        details["espnow_call_map_set"] = dev.command(f"ESPNOW_CALL_MAP_SET_VOLATILE {espnow_payload}", expect="ack")
        details["espnow_call_map_set_legacy_tone"] = dev.command(
            f"ESPNOW_CALL_MAP_SET_VOLATILE {espnow_legacy_payload}",
            expect="ack",
        )
        details["espnow_call_map_get"] = dev.command("ESPNOW_CALL_MAP_GET", expect="json")
        details["play_legacy_tone"] = dev.command("PLAY /assets/wav/FR_FR/dial.wav", expect="ack")
        details["tone_play"] = dev.command("TONE_PLAY FR_FR busy", expect="ack")
        time.sleep(0.4)
        details["status_after_tone_play"] = _command_with_retry(
            dev,
            "STATUS",
            expect="json",
            timeout_s=12.0,
            attempts=4,
            retry_delay_s=0.6,
        )
        details["tone_stop"] = dev.command("TONE_STOP", expect="ack")
        time.sleep(0.2)
        details["status_after_tone_stop"] = dev.command("STATUS", expect="json")
        details["dial_media_map_reset_volatile"] = dev.command("DIAL_MEDIA_MAP_RESET_VOLATILE", expect="ack")
        details["espnow_call_map_reset_volatile"] = dev.command("ESPNOW_CALL_MAP_RESET_VOLATILE", expect="ack")

        status_audio_play = (
            details["status_after_tone_play"].get("audio", {}) if isinstance(details["status_after_tone_play"], dict) else {}
        )
        status_audio_stop = (
            details["status_after_tone_stop"].get("audio", {}) if isinstance(details["status_after_tone_stop"], dict) else {}
        )
        status_config = (
            details["status_after_tone_play"].get("config", {}) if isinstance(details["status_after_tone_play"], dict) else {}
        )
        status_audio_cfg = status_config.get("audio", {}) if isinstance(status_config, dict) else {}
        dial_map = status_config.get("dial_media_map", {}) if isinstance(status_config, dict) else {}
        espnow_map = status_config.get("espnow_call_map", {}) if isinstance(status_config, dict) else {}

        playback_sample_rate = status_audio_play.get("playback_sample_rate")
        playback_bits_per_sample = status_audio_play.get("playback_bits_per_sample")
        playback_channels = status_audio_play.get("playback_channels")
        playback_format_overridden = status_audio_play.get("playback_format_overridden")
        config_sample_rate = status_audio_cfg.get("sample_rate") if isinstance(status_audio_cfg, dict) else None
        config_bits_per_sample = status_audio_cfg.get("bits_per_sample") if isinstance(status_audio_cfg, dict) else None

        tone_route_active_after_play = _extract_tone_route_active(details["status_after_tone_play"])
        tone_rendering_after_play = _extract_tone_rendering(details["status_after_tone_play"])
        tone_route_after_stop = _extract_tone_route_active(details["status_after_tone_stop"])

        legacy_line = str(details["play_legacy_tone"].get("line", "")).upper()
        legacy_rejected = (not _is_success_response(details["play_legacy_tone"])) and (
            "TONE_WAV_DEPRECATED_USE_TONE_PLAY" in legacy_line
        )
        legacy_map_line = str(details["espnow_call_map_set_legacy_tone"].get("line", "")).upper()
        legacy_map_rejected = (not _is_success_response(details["espnow_call_map_set_legacy_tone"])) and (
            "TONE_WAV_DEPRECATED_USE_KIND_TONE" in legacy_map_line
        )

        checks = {
            "dial_media_map_set_ok": _is_success_response(details["dial_media_map_set"]),
            "espnow_call_map_set_ok": _is_success_response(details["espnow_call_map_set"]),
            "espnow_call_map_set_legacy_tone_rejected": legacy_map_rejected,
            "dial_media_map_reset_volatile_ok": _is_success_response(details["dial_media_map_reset_volatile"]),
            "espnow_call_map_reset_volatile_ok": _is_success_response(details["espnow_call_map_reset_volatile"]),
            "dial_media_map_status_present": isinstance(dial_map, dict),
            "espnow_call_map_status_present": isinstance(espnow_map, dict),
            "play_legacy_tone_rejected": legacy_rejected,
            "tone_play_ok": _is_success_response(details["tone_play"]),
            "tone_route_active_after_play": tone_route_active_after_play is True,
            "tone_rendering_after_play": tone_rendering_after_play is True,
            "tone_event_after_play": _extract_tone_event(details["status_after_tone_play"]) == "busy",
            "tone_stop_ok": _is_success_response(details["tone_stop"]),
            "tone_route_inactive_after_stop": tone_route_after_stop is False,
            "tone_rendering_present": "tone_rendering" in status_audio_play,
            "tone_route_present": "tone_route_active" in status_audio_play,
            "playback_sample_rate_present": isinstance(playback_sample_rate, int),
            "playback_bits_present": isinstance(playback_bits_per_sample, int),
            "playback_channels_present": isinstance(playback_channels, int),
            "playback_bits_supported": _is_supported_tone_bits(playback_bits_per_sample),
            "playback_channels_supported": playback_channels in (1, 2),
            "playback_format_overridden_bool": isinstance(playback_format_overridden, bool),
            "playback_format_consistent_with_config": (
                config_sample_rate is None
                or config_bits_per_sample is None
                or playback_format_overridden
                or (
                    isinstance(config_sample_rate, int)
                    and isinstance(config_bits_per_sample, int)
                    and playback_sample_rate == config_sample_rate
                    and playback_bits_per_sample == config_bits_per_sample
                )
            ),
            "storage_fields_present": all(
                key in status_audio_play for key in ("storage_default_policy", "storage_last_source", "storage_last_path")
            ),
            "tone_fields_present": all(key in status_audio_stop for key in ("tone_active", "tone_profile", "tone_event", "tone_engine")),
        }
        details["checks"] = checks
        ok = all(checks.values())
        return ScenarioResult("serial_media_routing", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_media_routing", "FAIL", {"error": str(exc), **details})


def _extract_route_file_path(route_payload: Any) -> str:
    if isinstance(route_payload, str):
        return route_payload.strip()
    if not isinstance(route_payload, dict):
        return ""
    path = route_payload.get("path")
    if isinstance(path, str):
        return path.strip()
    return ""


def scenario_serial_hotline_defaults(dev: SerialEndpoint) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        expected = {
            "1": "/welcome.wav",
            "2": "/souffle.wav",
            "3": "/radio.wav",
        }
        details["dial_media_map_get"] = dev.command("DIAL_MEDIA_MAP_GET", expect="json")
        dial_map = details["dial_media_map_get"] if isinstance(details["dial_media_map_get"], dict) else {}
        found_paths = {digit: _extract_route_file_path(dial_map.get(digit)) for digit in expected}
        details["expected"] = expected
        details["found_paths"] = found_paths

        checks = {
            "dial_media_map_is_object": isinstance(dial_map, dict),
            "dial_media_map_exact_keys": isinstance(dial_map, dict) and set(dial_map.keys()) == set(expected.keys()),
            "dial_1_welcome": found_paths.get("1") == expected["1"],
            "dial_2_souffle": found_paths.get("2") == expected["2"],
            "dial_3_radio": found_paths.get("3") == expected["3"],
        }
        details["checks"] = checks
        return ScenarioResult("serial_hotline_defaults", "PASS" if all(checks.values()) else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_hotline_defaults", "FAIL", {"error": str(exc), **details})


def _candidate_probe_paths(preferred_path: str) -> List[str]:
    candidates: List[str] = []
    raw_candidates = [preferred_path, "/welcome.wav", "/musique.wav"]
    for raw in raw_candidates:
        value = str(raw or "").strip()
        if not value:
            continue
        if value not in candidates:
            candidates.append(value)
    return candidates


def _select_audio_probe(
    dev: SerialEndpoint,
    preferred_path: str,
) -> tuple[Optional[Dict[str, Any]], str, List[Dict[str, Any]]]:
    attempts: List[Dict[str, Any]] = []
    for path in _candidate_probe_paths(preferred_path):
        probe_cmd = f"AUDIO_PROBE {_quote_arg(path)}"
        response = _command_with_retry(
            dev,
            probe_cmd,
            expect="any",
            timeout_s=8.0,
            attempts=2,
            retry_delay_s=0.3,
        )
        attempts.append({"path": path, "response": response})
        if isinstance(response, dict) and all(key in response for key in REQUIRED_AUDIO_PROBE_KEYS):
            return response, path, attempts
    return None, "", attempts


def scenario_serial_audio_format_chain(dev: SerialEndpoint, audio_probe_path: str) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["audio_policy_get"] = _command_with_retry(
            dev,
            "AUDIO_POLICY_GET",
            expect="json",
            timeout_s=5.0,
            attempts=2,
            retry_delay_s=0.3,
        )

        probe, selected_path, attempts = _select_audio_probe(dev, audio_probe_path)
        details["audio_probe_attempts"] = attempts
        if probe is None:
            details["checks"] = {
                "audio_policy_get_is_json": isinstance(details.get("audio_policy_get"), dict),
                "audio_probe_found_supported_file": False,
            }
            return ScenarioResult("serial_audio_format_chain", "FAIL", details)

        details["audio_probe"] = probe
        details["audio_probe_selected_path"] = selected_path

        play_cmd = f"PLAY {_quote_arg(selected_path)}"
        details["play"] = _command_with_retry(
            dev,
            play_cmd,
            expect="ack",
            timeout_s=8.0,
            attempts=2,
            retry_delay_s=0.3,
        )

        status_polls: List[Dict[str, Any]] = []
        playback_observed_after_play = False
        observed_status: Dict[str, Any] = {}
        observed_status_playing: Dict[str, Any] = {}
        terminal_status: Dict[str, Any] = {}
        probe_duration_ms = probe.get("duration_ms") if isinstance(probe.get("duration_ms"), int) else 0
        poll_window_s = 4.0
        if probe_duration_ms > 0:
            poll_window_s = min(20.0, max(4.0, (probe_duration_ms / 1000.0) * 2.2 + 1.5))
        max_polls = max(12, min(20, int(poll_window_s / 0.10)))
        poll_interval_s = 0.10

        playback_started_at: Optional[float] = None
        playback_last_true_at: Optional[float] = None
        poll_start = time.monotonic()
        while (time.monotonic() - poll_start) < poll_window_s and len(status_polls) < max_polls:
            polled_status = _command_with_retry(
                dev,
                "STATUS",
                expect="json",
                timeout_s=12.0,
                attempts=2,
                retry_delay_s=0.2,
            )
            status_polls.append(polled_status)
            poll_audio = _extract_audio_status(polled_status if isinstance(polled_status, dict) else {})
            now = time.monotonic()
            playing_value = poll_audio.get("playing")
            if isinstance(playing_value, bool):
                if playing_value:
                    if playback_started_at is None:
                        playback_started_at = now
                    playback_last_true_at = now
                    playback_observed_after_play = True
                    observed_status_playing = polled_status
                    observed_status = polled_status
                elif playback_started_at is not None:
                    terminal_status = polled_status
                    break
            if not observed_status and isinstance(poll_audio.get("playback_input_sample_rate"), int):
                if poll_audio.get("playback_input_sample_rate", 0) > 0:
                    observed_status = polled_status
            time.sleep(poll_interval_s)
        if observed_status_playing:
            observed_status = observed_status_playing
        if not observed_status and status_polls:
            observed_status = status_polls[-1]
        if not terminal_status and status_polls:
            terminal_status = status_polls[-1]

        observed_playback_duration_ms = 0
        if playback_started_at is not None and playback_last_true_at is not None and playback_last_true_at >= playback_started_at:
            observed_seconds = (playback_last_true_at - playback_started_at) + poll_interval_s
            observed_playback_duration_ms = max(0, int(round(observed_seconds * 1000.0)))

        playback_duration_lower_ms = int(round(float(probe_duration_ms) * 0.7)) if probe_duration_ms > 0 else 0
        playback_duration_upper_ms = int(round(float(probe_duration_ms) * 1.4)) if probe_duration_ms > 0 else 0
        playback_duration_within_tolerance = (
            probe_duration_ms > 0
            and observed_playback_duration_ms > 0
            and playback_duration_lower_ms <= observed_playback_duration_ms <= playback_duration_upper_ms
        )
        details["status_after_play_polls"] = status_polls
        details["status_after_play"] = observed_status
        details["status_after_play_terminal"] = terminal_status
        details["probe_duration_ms"] = probe_duration_ms
        details["observed_playback_duration_ms"] = observed_playback_duration_ms
        details["playback_duration_tolerance_ms"] = {
            "lower": playback_duration_lower_ms,
            "upper": playback_duration_upper_ms,
        }

        policy = details["audio_policy_get"] if isinstance(details["audio_policy_get"], dict) else {}
        status_audio = _extract_audio_status(
            details["status_after_play"] if isinstance(details["status_after_play"], dict) else {}
        )
        status_missing = [key for key in REQUIRED_AUDIO_STATUS_KEYS if key not in status_audio]
        status_fallback = status_audio.get("playback_rate_fallback")
        probe_fallback = probe.get("rate_fallback")
        probe_loudness_auto = probe.get("loudness_auto")
        status_loudness_auto = status_audio.get("playback_loudness_auto")
        expected_loudness_auto = str(policy.get("wav_loudness_policy", "")).upper() == "AUTO_NORMALIZE_LIMITER"
        probe_gain = probe.get("loudness_gain_db")
        status_gain = status_audio.get("playback_loudness_gain_db")
        probe_limiter = probe.get("limiter_active")
        status_limiter = status_audio.get("playback_limiter_active")

        probe_input_rate = probe.get("input_sample_rate")
        probe_output_rate = probe.get("output_sample_rate")
        adaptive_rate_expected = False
        if isinstance(probe_input_rate, int) and isinstance(probe_output_rate, int):
            if probe_input_rate in SUPPORTED_OUTPUT_SAMPLE_RATES:
                adaptive_rate_expected = (probe_output_rate == probe_input_rate) and (probe_fallback == 0)
            else:
                adaptive_rate_expected = (
                    probe_output_rate in SUPPORTED_OUTPUT_SAMPLE_RATES
                    and probe_output_rate != probe_input_rate
                    and probe_fallback == probe_output_rate
                )

        status_matches_probe_rate = status_audio.get("playback_output_sample_rate") == probe.get("output_sample_rate")
        status_matches_probe_bits = status_audio.get("playback_output_bits_per_sample") == probe.get("output_bits_per_sample")
        status_matches_probe_channels = status_audio.get("playback_output_channels") == probe.get("output_channels")
        status_matches_probe_fallback = status_fallback == probe_fallback

        checks = {
            "audio_policy_get_is_json": isinstance(policy, dict),
            "audio_policy_clock_hybrid_telco": str(policy.get("clock_policy", "")).upper() == "HYBRID_TELCO",
            "audio_policy_loudness_valid": str(policy.get("wav_loudness_policy", "")).upper()
            in {"AUTO_NORMALIZE_LIMITER", "FIXED_GAIN_ONLY"},
            "audio_probe_has_required_fields": all(key in probe for key in REQUIRED_AUDIO_PROBE_KEYS),
            "audio_probe_input_bits_supported": probe.get("input_bits_per_sample") in SUPPORTED_INPUT_BITS,
            "audio_probe_input_channels_supported": probe.get("input_channels") in SUPPORTED_CHANNELS,
            "audio_probe_output_rate_supported": probe.get("output_sample_rate") in SUPPORTED_OUTPUT_SAMPLE_RATES,
            "audio_probe_output_bits_supported": probe.get("output_bits_per_sample") in SUPPORTED_OUTPUT_BITS,
            "audio_probe_output_channels_supported": probe.get("output_channels") in SUPPORTED_CHANNELS,
            "audio_probe_resampler_flag_bool": isinstance(probe.get("resampler_active"), bool),
            "audio_probe_upmix_flag_bool": isinstance(probe.get("channel_upmix_active"), bool),
            "audio_probe_loudness_auto_bool": isinstance(probe_loudness_auto, bool),
            "audio_probe_loudness_number": _is_number(probe.get("loudness_gain_db")),
            "audio_probe_limiter_bool": isinstance(probe.get("limiter_active"), bool),
            "audio_probe_rate_fallback_number": isinstance(probe_fallback, int) and probe_fallback >= 0,
            "audio_probe_data_size_bytes_number": isinstance(probe.get("data_size_bytes"), int)
            and probe.get("data_size_bytes") >= 0,
            "audio_probe_duration_ms_positive": isinstance(probe.get("duration_ms"), int) and probe.get("duration_ms") > 0,
            "audio_probe_adaptive_rate_expected": adaptive_rate_expected,
            "play_ok": _is_success_response(details["play"]),
            "status_playback_window_observed": playback_observed_after_play,
            "status_playback_duration_within_tolerance": playback_duration_within_tolerance,
            "status_audio_fields_present": not status_missing,
            "status_output_rate_supported": status_audio.get("playback_output_sample_rate") in SUPPORTED_OUTPUT_SAMPLE_RATES,
            "status_output_bits_supported": status_audio.get("playback_output_bits_per_sample") in SUPPORTED_OUTPUT_BITS,
            "status_output_channels_supported": status_audio.get("playback_output_channels") in SUPPORTED_CHANNELS,
            "status_resampler_bool": isinstance(status_audio.get("playback_resampler_active"), bool),
            "status_upmix_bool": isinstance(status_audio.get("playback_channel_upmix_active"), bool),
            "status_loudness_auto_bool": isinstance(status_loudness_auto, bool),
            "status_loudness_number": _is_number(status_audio.get("playback_loudness_gain_db")),
            "status_limiter_bool": isinstance(status_audio.get("playback_limiter_active"), bool),
            "status_fallback_number": isinstance(status_fallback, int) and status_fallback >= 0,
            "status_copy_source_number": isinstance(status_audio.get("playback_copy_source_bytes"), int)
            and status_audio.get("playback_copy_source_bytes") >= 0,
            "status_copy_accepted_number": isinstance(status_audio.get("playback_copy_accepted_bytes"), int)
            and status_audio.get("playback_copy_accepted_bytes") >= 0,
            "status_copy_loss_bytes_number": isinstance(status_audio.get("playback_copy_loss_bytes"), int)
            and status_audio.get("playback_copy_loss_bytes") >= 0,
            "status_copy_loss_events_number": isinstance(status_audio.get("playback_copy_loss_events"), int)
            and status_audio.get("playback_copy_loss_events") >= 0,
            "status_copy_loss_bytes_zero": status_audio.get("playback_copy_loss_bytes") == 0,
            "status_copy_loss_events_zero": status_audio.get("playback_copy_loss_events") == 0,
            "status_copy_accounting_monotonic": isinstance(status_audio.get("playback_copy_source_bytes"), int)
            and isinstance(status_audio.get("playback_copy_accepted_bytes"), int)
            and status_audio.get("playback_copy_accepted_bytes") <= status_audio.get("playback_copy_source_bytes"),
            "probe_loudness_matches_policy": isinstance(probe_loudness_auto, bool)
            and probe_loudness_auto == expected_loudness_auto,
            "status_loudness_matches_policy": isinstance(status_loudness_auto, bool)
            and status_loudness_auto == expected_loudness_auto,
            "probe_fixed_gain_consistent": expected_loudness_auto
            or (
                _is_number(probe_gain)
                and abs(float(probe_gain)) <= 0.05
                and probe_limiter is False
            ),
            "status_fixed_gain_consistent": expected_loudness_auto
            or (
                _is_number(status_gain)
                and abs(float(status_gain)) <= 0.05
                and status_limiter is False
            ),
            "status_matches_probe_rate": status_matches_probe_rate,
            "status_matches_probe_bits": status_matches_probe_bits,
            "status_matches_probe_channels": status_matches_probe_channels,
            "status_matches_probe_fallback": status_matches_probe_fallback,
        }
        details["missing_status_audio_keys"] = status_missing
        details["playback_observed_after_play"] = playback_observed_after_play
        details["checks"] = checks
        return ScenarioResult("serial_audio_format_chain", "PASS" if all(checks.values()) else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_audio_format_chain", "FAIL", {"error": str(exc), **details})


def scenario_http(base_url: str) -> ScenarioResult:
    details: Dict[str, Any] = {"base_url": base_url}
    try:
        details["status"] = fetch_json(base_url.rstrip("/") + "/api/status")
        details["wifi"] = fetch_json(base_url.rstrip("/") + "/api/network/wifi")
        details["espnow"] = fetch_json(base_url.rstrip("/") + "/api/network/espnow")
        details["control_ping"] = post_json(base_url.rstrip("/") + "/api/control", {"action": "PING"})
        return ScenarioResult("http_endpoints", "PASS", details)
    except error.HTTPError as exc:
        return ScenarioResult("http_endpoints", "FAIL", {"error": f"HTTP {exc.code}", **details})
    except Exception as exc:
        return ScenarioResult("http_endpoints", "FAIL", {"error": str(exc), **details})


def scenario_manual(name: str, state: str, note: str) -> ScenarioResult:
    if state not in VALID_STATES:
        state = "MANUAL_SKIP"
    return ScenarioResult(name, state, {"note": note})


def write_reports(results: List[ScenarioResult], report_json: Path, report_md: Path) -> None:
    ensure_parent(report_json)
    ensure_parent(report_md)

    overall_passed = all(item.state not in {"FAIL", "MANUAL_FAIL"} for item in results)
    payload = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "overall_passed": overall_passed,
        "results": [{"name": x.name, "state": x.state, "details": x.details} for x in results],
    }
    report_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = [
        "# Rapport validation HW (A252)",
        "",
        f"- Date UTC: {payload['timestamp_utc']}",
        f"- Verdict global: {'PASS' if overall_passed else 'FAIL'}",
        "",
        "| Scénario | État | Détails |",
        "|---|---|---|",
    ]
    for item in results:
        details = json.dumps(item.details, ensure_ascii=False)
        lines.append(f"| {item.name} | {item.state} | `{details}` |")
    report_md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="RTC_BL_PHONE A252 validation runner")
    parser.add_argument(
        "--port-a252",
        default="",
        help=(
            "serial port for A252 target. If omitted, auto-detects first /dev/*usbserial* on macOS/"
            "Linux."
        ),
    )
    parser.add_argument("--baud", type=int, default=115200, help="serial baudrate")
    parser.add_argument("--flash", action="store_true", help="build and upload firmware before tests")
    parser.add_argument("--base-url", default="", help="optional A252 base URL (http://ip)")
    parser.add_argument("--wifi-ssid", default="", help="optional SSID for WIFI_CONNECT test")
    parser.add_argument("--wifi-password", default="", help="optional password for WIFI_CONNECT test")
    parser.add_argument("--report-json", default="docs/rapport_hw.json", help="output JSON report path")
    parser.add_argument(
        "--report-md", default="docs/rapport_tests_fonctionnels.md", help="output Markdown report path"
    )
    parser.add_argument("--manual-hook", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-ring", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-audio", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-hfp", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-note", default="", help="optional shared note for manual checks")
    parser.add_argument(
        "--strict-serial-smoke",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="if enabled, fail serial_smoke when required command checks fail (default: enabled)",
    )
    parser.add_argument(
        "--allow-capture-fail-when-disabled",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "if enabled, CAPTURE_START failure is tolerated when STATUS reports audio.enable_capture=false "
            "(default: enabled)"
        ),
    )
    parser.add_argument(
        "--require-hook-toggle",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="if enabled, serial_hook_ring_audio requires both ON_HOOK and OFF_HOOK states",
    )
    parser.add_argument(
        "--hook-observe-seconds",
        type=int,
        default=45,
        help="hook observation window in seconds for serial_hook_ring_audio (default: 45)",
    )
    parser.add_argument(
        "--require-contract-version",
        default=EXPECTED_FIRMWARE_CONTRACT_VERSION,
        help=(
            "required firmware contract_version in STATUS.firmware. "
            "Set empty string to disable strict version match."
        ),
    )
    parser.add_argument(
        "--audio-probe-path",
        default="/welcome.wav",
        help="preferred file path used by AUDIO_PROBE/PLAY checks (fallbacks: /welcome.wav, /musique.wav)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    resolved_port = resolve_a252_port(args.port_a252.strip() or None)

    if args.flash:
        run_cmd(["pio", "run", "-e", "esp32dev"])
        run_cmd(["pio", "run", "-e", "esp32dev", "-t", "upload", "--upload-port", resolved_port])

    results: List[ScenarioResult] = []
    network_result: Optional[ScenarioResult] = None

    try:
        with SerialEndpoint(resolved_port, args.baud) as dev:
            dev.sync()
            results.append(
                scenario_serial_smoke(
                    dev,
                    strict_serial_smoke=args.strict_serial_smoke,
                    allow_capture_fail_when_disabled=args.allow_capture_fail_when_disabled,
                )
            )
            results.append(
                scenario_serial_firmware_contract(
                    dev,
                    required_contract_version=(args.require_contract_version or "").strip(),
                )
            )
            results.append(
                scenario_serial_hook_ring_audio(
                    dev,
                    require_hook_toggle=args.require_hook_toggle,
                    hook_observe_seconds=args.hook_observe_seconds,
                )
            )
            results.append(scenario_serial_hotline_defaults(dev))
            results.append(scenario_serial_media_routing(dev))
            results.append(scenario_serial_audio_format_chain(dev, args.audio_probe_path))
            network_result = scenario_serial_network(dev, args.wifi_ssid, args.wifi_password)
            results.append(network_result)
    except Exception as exc:
        results.append(ScenarioResult("serial_runner", "FAIL", {"error": str(exc)}))

    runtime_base_url = args.base_url.strip()
    if network_result and isinstance(network_result.details, dict):
        wifi_after = network_result.details.get("wifi_status_after")
        if isinstance(wifi_after, dict) and wifi_after.get("connected") and wifi_after.get("ip"):
            runtime_base_url = f"http://{wifi_after['ip']}"

    if runtime_base_url:
        results.append(scenario_http(runtime_base_url))
    else:
        results.append(ScenarioResult("http_endpoints", "MANUAL_SKIP", {"note": "base URL not provided"}))

    note = args.manual_note or "validated manually"
    results.append(scenario_manual("manual_hook_transition", args.manual_hook, note))
    results.append(scenario_manual("manual_ring_behavior", args.manual_ring, note))
    results.append(scenario_manual("manual_audio_path", args.manual_audio, note))
    results.append(scenario_manual("manual_hfp_pairing", args.manual_hfp, note))

    write_reports(results, Path(args.report_json), Path(args.report_md))
    return 0 if all(item.state not in {"FAIL", "MANUAL_FAIL"} for item in results) else 1


if __name__ == "__main__":
    sys.exit(main())
