#!/usr/bin/env python3
"""Send reliable ESP-NOW control commands from controller board to HOTLINE_PHONE."""

from __future__ import annotations

import argparse
import glob
import json
import sys
import time
from typing import Any, Dict, Optional

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None


def parse_json_line(line: str) -> Optional[Any]:
    line = line.strip()
    if not line:
        return None
    if line[0] not in ("{", "[") or line[-1] not in ("}", "]"):
        return None
    try:
        return json.loads(line)
    except json.JSONDecodeError:
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

    def command(self, cmd: str, timeout_s: float = 4.0, expect: str = "any") -> Dict[str, Any]:
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
            parsed = parse_json_line(line)
            if parsed is not None:
                if expect in {"any", "json"} and isinstance(parsed, dict):
                    return parsed
                continue
            if line.startswith("OK ") or line.startswith("ERR "):
                if expect in {"any", "ack"}:
                    return {"ok": line.startswith("OK "), "line": line}
                continue
            if line == "PONG":
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
                time.sleep(0.4)
        raise RuntimeError(f"serial sync failed: {last_error}")


def resolve_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port
    for pattern in ("/dev/cu.usbserial*", "/dev/tty.usbserial*"):
        candidates = sorted(glob.glob(pattern))
        if candidates:
            return candidates[0]
    raise RuntimeError("No serial port found. Provide --port explicitly.")


def build_command(action: str, custom_cmd: str) -> str:
    action = action.lower()
    if action == "ring":
        return "RING"
    if action == "status":
        return "STATUS"
    if action == "hotline1":
        return "HOTLINE_TRIGGER 1 pulse"
    if action == "hotline2":
        return "HOTLINE_TRIGGER 2 pulse"
    if action == "hotline3":
        return "HOTLINE_TRIGGER 3 pulse"
    if action == "discover":
        return "ESPNOW_DEVICE_NAME_GET"
    if action == "custom":
        cmd = custom_cmd.strip()
        if not cmd:
            raise ValueError("--cmd is required when --action custom")
        return cmd
    raise ValueError(f"unsupported action: {action}")


def make_envelope(command_line: str, msg_id: str, seq: int) -> Dict[str, Any]:
    return {
        "msg_id": msg_id,
        "seq": seq,
        "type": "command",
        "ack": True,
        "payload": command_line,
    }


def parse_ack(last_rx_payload: str, expected_msg_id: str, expected_seq: int) -> Optional[Dict[str, Any]]:
    try:
        obj = json.loads(last_rx_payload)
    except json.JSONDecodeError:
        return None
    if not isinstance(obj, dict):
        return None
    if str(obj.get("type", "")).lower() != "ack":
        return None
    if str(obj.get("msg_id", "")) != expected_msg_id:
        return None
    try:
        seq = int(obj.get("seq", -1))
    except (TypeError, ValueError):
        return None
    if seq != expected_seq:
        return None
    payload = obj.get("payload")
    return payload if isinstance(payload, dict) else None


def extract_device_name_from_ack_payload(ack_payload: Dict[str, Any]) -> str:
    data = ack_payload.get("data")
    if not isinstance(data, dict):
        return ""
    device_name = str(data.get("device_name", "")).strip()
    return device_name


def send_with_retry(
    dev: SerialEndpoint,
    target: str,
    command_line: str,
    retries: int,
    ack_timeout_s: float,
    poll_interval_s: float,
) -> Dict[str, Any]:
    seq = int(time.time()) & 0xFFFFFFFF
    last_error = "ack_timeout"
    last_send_line = ""
    for attempt in range(1, retries + 1):
        status_before = dev.command("ESPNOW_STATUS", expect="json", timeout_s=3.0)
        rx_before = int(status_before.get("rx_count", 0))
        msg_id = f"host-{int(time.time() * 1000)}-{attempt}"
        envelope = make_envelope(command_line, msg_id, seq)
        wire = json.dumps(envelope, separators=(",", ":"))

        send_ack = dev.command(f"ESPNOW_SEND {target} {wire}", expect="ack", timeout_s=4.0)
        if not send_ack.get("ok", False):
            last_send_line = str(send_ack.get("line", ""))
            last_error = "send_failed"
            print(f"[espnow] send failed attempt {attempt}/{retries}: {last_send_line}")
            continue

        last_error = "ack_timeout"
        print(f"[espnow] sent attempt {attempt}/{retries}, waiting ack msg_id={msg_id} seq={seq}")
        deadline = time.monotonic() + ack_timeout_s
        while time.monotonic() < deadline:
            status_now = dev.command("ESPNOW_STATUS", expect="json", timeout_s=2.5)
            rx_now = int(status_now.get("rx_count", 0))
            if rx_now <= rx_before:
                time.sleep(poll_interval_s)
                continue
            ack_payload = parse_ack(str(status_now.get("last_rx_payload", "")), msg_id, seq)
            if ack_payload is not None:
                return {
                    "ok": bool(ack_payload.get("ok", False)),
                    "attempt": attempt,
                    "msg_id": msg_id,
                    "seq": seq,
                    "target": target,
                    "command": command_line,
                    "source_mac": str(status_now.get("last_rx_mac", "")),
                    "ack_payload": ack_payload,
                }
            rx_before = rx_now
            time.sleep(poll_interval_s)

        print(f"[espnow] ack timeout attempt {attempt}/{retries}")

    return {
        "ok": False,
        "attempt": retries,
        "msg_id": "",
        "seq": seq,
        "target": target,
        "command": command_line,
        "error": last_error,
        "send_line": last_send_line,
    }


def discover_target_mac(
    dev: SerialEndpoint,
    target_name: str,
    retries: int,
    rounds: int,
    broadcast_window_s: float,
    ack_timeout_s: float,
    poll_interval_s: float,
) -> Dict[str, Any]:
    normalized_target_name = target_name.strip().upper()
    discovery: Dict[str, Any] = {
        "mode": "broadcast+discovery",
        "target_name": target_name,
        "broadcast_rounds": [],
        "known_peers": [],
        "peer_probes": [],
        "candidates": [],
        "matches": [],
        "resolved_mac": "",
    }

    candidates_by_mac: Dict[str, Dict[str, Any]] = {}

    status = dev.command("ESPNOW_STATUS", expect="json", timeout_s=3.0)
    rx_cursor = int(status.get("rx_count", 0))
    peers = status.get("peers", [])
    known_peers = [str(item).strip().upper() for item in peers if str(item).strip()]
    discovery["known_peers"] = known_peers

    for round_index in range(1, max(1, rounds) + 1):
        seq = (int(time.time()) + round_index) & 0xFFFFFFFF
        msg_id = f"discover-{int(time.time() * 1000)}-{round_index}"
        envelope = make_envelope("ESPNOW_DEVICE_NAME_GET", msg_id, seq)
        wire = json.dumps(envelope, separators=(",", ":"))

        send_ack = dev.command(f"ESPNOW_SEND broadcast {wire}", expect="ack", timeout_s=4.0)
        round_info: Dict[str, Any] = {
            "round": round_index,
            "ok": bool(send_ack.get("ok", False)),
            "line": str(send_ack.get("line", "")),
            "msg_id": msg_id,
            "seq": seq,
            "hits": [],
        }
        discovery["broadcast_rounds"].append(round_info)
        if not send_ack.get("ok", False):
            continue

        deadline = time.monotonic() + max(0.4, broadcast_window_s)
        while time.monotonic() < deadline:
            status_now = dev.command("ESPNOW_STATUS", expect="json", timeout_s=2.5)
            rx_now = int(status_now.get("rx_count", 0))
            if rx_now <= rx_cursor:
                time.sleep(poll_interval_s)
                continue

            rx_cursor = rx_now
            ack_payload = parse_ack(str(status_now.get("last_rx_payload", "")), msg_id, seq)
            if ack_payload is None:
                time.sleep(poll_interval_s)
                continue

            source_mac = str(status_now.get("last_rx_mac", "")).strip().upper()
            device_name = extract_device_name_from_ack_payload(ack_payload)
            hit = {
                "mac": source_mac,
                "device_name": device_name,
                "ok": bool(ack_payload.get("ok", False)),
                "code": str(ack_payload.get("code", "")),
            }
            round_info["hits"].append(hit)

            if source_mac:
                candidates_by_mac[source_mac] = {
                    "mac": source_mac,
                    "device_name": device_name,
                    "path": "broadcast",
                    "ok": True,
                }
            time.sleep(poll_interval_s)

    for peer in known_peers:
        probe = send_with_retry(
            dev=dev,
            target=peer,
            command_line="ESPNOW_DEVICE_NAME_GET",
            retries=max(1, retries),
            ack_timeout_s=ack_timeout_s,
            poll_interval_s=poll_interval_s,
        )
        probe["peer"] = peer
        discovery["peer_probes"].append(probe)

        if not probe.get("ok", False):
            continue

        source_mac = str(probe.get("source_mac", "")).strip().upper() or peer
        ack_payload = probe.get("ack_payload")
        device_name = ""
        if isinstance(ack_payload, dict):
            device_name = extract_device_name_from_ack_payload(ack_payload)

        candidates_by_mac[source_mac] = {
            "mac": source_mac,
            "device_name": device_name,
            "path": "peer_scan",
            "ok": True,
        }

    candidates = sorted(candidates_by_mac.values(), key=lambda item: str(item.get("mac", "")))
    discovery["candidates"] = candidates

    matches = []
    for candidate in candidates:
        device_name = str(candidate.get("device_name", "")).strip().upper()
        if not normalized_target_name:
            matches.append(candidate)
            continue
        if device_name == normalized_target_name:
            matches.append(candidate)
    discovery["matches"] = matches

    if matches:
        discovery["resolved_mac"] = str(matches[0].get("mac", ""))
    elif len(candidates) == 1:
        discovery["resolved_mac"] = str(candidates[0].get("mac", ""))
        discovery["resolved_mode"] = "single_candidate_fallback"
    else:
        discovery["resolved_mac"] = ""

    return discovery


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Controller script for ESP-NOW -> HOTLINE_PHONE")
    parser.add_argument("--port", default="", help="controller serial port (default: first /dev/*usbserial*)")
    parser.add_argument("--baud", type=int, default=115200, help="serial baudrate")
    parser.add_argument(
        "--target",
        default="broadcast",
        help="ESP-NOW target MAC, 'broadcast', or 'broadcast+discovery' (default: broadcast)",
    )
    parser.add_argument(
        "--target-name",
        default="HOTLINE_PHONE",
        help="expected remote logical name for operator context only (default: HOTLINE_PHONE)",
    )
    parser.add_argument(
        "--action",
        default="ring",
        choices=("ring", "status", "hotline1", "hotline2", "hotline3", "discover", "custom"),
        help="high-level action (default: ring)",
    )
    parser.add_argument("--cmd", default="", help="raw command line used with --action custom")
    parser.add_argument("--retries", type=int, default=3, help="send retries on ack timeout/failure")
    parser.add_argument("--ack-timeout", type=float, default=3.0, help="seconds to wait for each ACK")
    parser.add_argument("--poll-ms", type=int, default=120, help="ACK polling interval in ms")
    parser.add_argument(
        "--discover",
        action="store_true",
        help="resolve target MAC by device_name before sending command",
    )
    parser.add_argument(
        "--discover-retries",
        type=int,
        default=1,
        help="retry count per peer during discovery (default: 1)",
    )
    parser.add_argument(
        "--discover-rounds",
        type=int,
        default=3,
        help="number of broadcast discovery rounds (default: 3)",
    )
    parser.add_argument(
        "--ensure-peer",
        action="store_true",
        help="add target MAC as peer before send (ignored for broadcast)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    port = resolve_port(args.port.strip() or None)
    target = args.target.strip()
    poll_interval_s = max(0.05, float(args.poll_ms) / 1000.0)
    target_lower = target.lower()

    try:
        command_line = build_command(args.action, args.cmd)
    except ValueError as exc:
        print(f"[espnow] FAIL: {exc}")
        return 2

    print(
        f"[espnow] controller_port={port} target={target} target_name={args.target_name} "
        f"action={args.action} command='{command_line}'"
    )

    try:
        with SerialEndpoint(port, args.baud) as dev:
            dev.sync()
            dev.command("ESPNOW_ON", expect="ack", timeout_s=3.0)

            local_name_resp = dev.command("ESPNOW_DEVICE_NAME_GET", expect="json", timeout_s=3.0)
            local_name = str(local_name_resp.get("device_name", ""))
            print(f"[espnow] local device_name={local_name}")

            wants_discovery = bool(args.discover or args.action == "discover" or target_lower == "broadcast+discovery")
            discovery: Dict[str, Any] = {}
            if wants_discovery:
                discovery = discover_target_mac(
                    dev=dev,
                    target_name=str(args.target_name),
                    retries=max(1, int(args.discover_retries)),
                    rounds=max(1, int(args.discover_rounds)),
                    broadcast_window_s=max(0.4, float(args.ack_timeout)),
                    ack_timeout_s=max(0.5, float(args.ack_timeout)),
                    poll_interval_s=poll_interval_s,
                )
                print(f"[espnow] discovery={json.dumps(discovery, ensure_ascii=True)}")
                resolved_mac = str(discovery.get("resolved_mac", "")).strip()
                if args.action == "discover":
                    if discovery.get("candidates"):
                        print(json.dumps({"ok": True, "discovery": discovery}, ensure_ascii=True))
                        return 0
                    print(json.dumps({"ok": False, "discovery": discovery}, ensure_ascii=True))
                    return 1
                if not resolved_mac:
                    print(json.dumps({"ok": False, "error": "discovery_failed", "discovery": discovery}, ensure_ascii=True))
                    return 1
                target = resolved_mac
                print(f"[espnow] resolved target={target} for target_name={args.target_name}")

            if args.ensure_peer and target.lower() != "broadcast":
                peer_res = dev.command(f"ESPNOW_PEER_ADD {target}", expect="ack", timeout_s=3.0)
                print(f"[espnow] ensure_peer -> {peer_res.get('line', '')}")

            result = send_with_retry(
                dev=dev,
                target=target,
                command_line=command_line,
                retries=max(1, int(args.retries)),
                ack_timeout_s=max(0.5, float(args.ack_timeout)),
                poll_interval_s=poll_interval_s,
            )
            if wants_discovery:
                result["target_name"] = args.target_name
    except Exception as exc:
        print(f"[espnow] FAIL: {exc}")
        return 1

    print(json.dumps(result, ensure_ascii=True))
    if not result.get("ok", False):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
