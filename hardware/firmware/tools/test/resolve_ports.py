#!/usr/bin/env python3
"""Resolve Zacus serial ports with location map + explicit required roles."""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import re
import sys
import time
from pathlib import Path

try:
    from serial.tools import list_ports
except ImportError:
    print("pyserial is missing; run ./tools/dev/bootstrap_local.sh", file=sys.stderr)
    sys.exit(2)

FIRMWARE_ROOT = Path(__file__).resolve().parents[2]
PORTS_MAP_PATH = FIRMWARE_ROOT / "tools" / "dev" / "ports_map.json"
DEFAULT_PORTS_MAP = {
    "location": {
        "20-6.1.1": "esp32",
        "20-6.1.2": "esp8266_usb",
        "20-6.2*": "esp8266_usb",
        "20-6.1*": "esp32",
    },
    "vidpid": {
        "2e8a:0005": "rp2040",
        "2e8a:000a": "rp2040",
    },
}


def normalize_role(role: str) -> str:
    value = (role or "").strip().lower()
    if value in ("esp8266", "esp8266_usb", "ui", "oled"):
        return "esp8266"
    return value


def parse_location(hwid: str) -> str:
    if not hwid:
        return ""
    match = re.search(r"LOCATION=([\w\-.]+)", hwid)
    if not match:
        return ""
    return match.group(1)


def port_vidpid(port) -> str:
    if port.vid is None or port.pid is None:
        return ""
    return f"{port.vid:04x}:{port.pid:04x}"


def port_preference(device: str, prefer_cu: bool) -> int:
    if device.startswith("/dev/cu.SLAB"):
        return 0
    if device.startswith("/dev/cu.usbserial"):
        return 1
    if device.startswith("/dev/cu.usbmodem"):
        return 2
    if prefer_cu and device.startswith("/dev/cu."):
        return 3
    if device.startswith("/dev/tty."):
        return 5
    return 4


def dedupe_ports(raw_ports, prefer_cu: bool):
    by_key = {}
    for port in raw_ports:
        key = parse_location(port.hwid) or port.device
        existing = by_key.get(key)
        if existing is None:
            by_key[key] = port
            continue
        if port_preference(port.device, prefer_cu) < port_preference(existing.device, prefer_cu):
            by_key[key] = port
    return list(by_key.values())


def load_ports_map() -> tuple[list[tuple[str, str]], dict[str, str]]:
    raw = DEFAULT_PORTS_MAP
    try:
        if PORTS_MAP_PATH.exists():
            loaded = json.loads(PORTS_MAP_PATH.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                raw = loaded
    except Exception:
        raw = DEFAULT_PORTS_MAP

    location_map: list[tuple[str, str]] = []
    for pattern, role in (raw.get("location") or {}).items():
        if isinstance(role, dict):
            role = role.get("role", "")
        if not isinstance(role, str):
            continue
        location_map.append((str(pattern), normalize_role(role)))
    location_map.sort(key=lambda item: (-len(item[0]), item[0]))

    vidpid_map: dict[str, str] = {}
    for vidpid, role in (raw.get("vidpid") or {}).items():
        if isinstance(role, dict):
            role = role.get("role", "")
        if not isinstance(role, str):
            continue
        vidpid_map[str(vidpid).lower()] = normalize_role(role)

    return location_map, vidpid_map


def classify_port(port, location_map, vidpid_map) -> tuple[str, str]:
    location = parse_location(port.hwid).lower()
    for pattern, role in location_map:
        if fnmatch.fnmatch(location, pattern.lower()):
            return role, f"location-map:{pattern}"

    vidpid = port_vidpid(port).lower()
    if vidpid and vidpid in vidpid_map:
        return vidpid_map[vidpid], f"vidpid:{vidpid}"

    desc = (getattr(port, "description", "") or "").lower()
    if "rp2040" in desc or "pico" in desc:
        return "rp2040", "description:rp2040"

    return "", ""


def build_manual_overrides(args, snapshot) -> tuple[dict[str, str], dict[str, dict[str, str]], list[str]]:
    overrides: dict[str, str] = {}
    details: dict[str, dict[str, str]] = {}
    notes: list[str] = []
    fields = (
        ("esp32", args.port_esp32 or os.environ.get("ZACUS_PORT_ESP32", "")),
        ("esp8266", args.port_esp8266 or os.environ.get("ZACUS_PORT_ESP8266", "")),
        ("rp2040", args.port_rp2040 or os.environ.get("ZACUS_PORT_RP2040", "")),
    )
    for role, port_value in fields:
        if not port_value:
            continue
        overrides[role] = port_value
        location = ""
        for port in snapshot:
            if port.device == port_value:
                location = parse_location(port.hwid)
                break
        details[role] = {"location": location, "reason": "manual-override"}
        notes.append(f"manual override {role} -> {port_value}")
    return overrides, details, notes


def classify_snapshot(snapshot, prefer_cu: bool):
    location_map, vidpid_map = load_ports_map()
    ports = dedupe_ports(snapshot, prefer_cu)

    found: dict[str, str] = {}
    details: dict[str, dict[str, str]] = {}
    notes: list[str] = []

    for port in ports:
        role, reason = classify_port(port, location_map, vidpid_map)
        role = normalize_role(role)
        if not role:
            continue
        if role in found:
            continue
        found[role] = port.device
        details[role] = {
            "location": parse_location(port.hwid),
            "reason": reason,
        }

    # Fallback CP2102 attribution when map is missing.
    cp2102 = [port for port in ports if (port.vid, port.pid) == (0x10C4, 0xEA60)]
    cp2102.sort(key=lambda port: port_preference(port.device, prefer_cu))
    for port in cp2102:
        if "esp32" not in found:
            found["esp32"] = port.device
            details["esp32"] = {
                "location": parse_location(port.hwid),
                "reason": "cp2102-fallback",
            }
            notes.append(f"cp2102 fallback assigned to esp32: {port.device}")
            continue
        if "esp8266" not in found:
            found["esp8266"] = port.device
            details["esp8266"] = {
                "location": parse_location(port.hwid),
                "reason": "cp2102-fallback",
            }
            notes.append(f"cp2102 fallback assigned to esp8266: {port.device}")

    return found, details, notes


def required_roles(args) -> set[str]:
    roles: set[str] = set()
    if args.need_esp32:
        roles.add("esp32")
    if args.need_esp8266:
        roles.add("esp8266")
    if args.need_rp2040:
        roles.add("rp2040")
    return roles


def resolve_ports(args):
    required = required_roles(args)
    deadline = time.monotonic() + max(0, int(args.wait_port))

    best_found: dict[str, str] = {}
    best_details: dict[str, dict[str, str]] = {}
    notes: list[str] = []

    while True:
        snapshot = list(list_ports.comports())
        found, details, classify_notes = classify_snapshot(snapshot, args.prefer_cu)
        manual, manual_details, manual_notes = build_manual_overrides(args, snapshot)

        found.update(manual)
        details.update(manual_details)
        notes = classify_notes + manual_notes

        # Single-board Freenove setups often expose one usbmodem endpoint that must be treated as ESP32.
        if "esp32" in required and not found.get("esp32"):
            esp8266_port = found.get("esp8266", "")
            if esp8266_port and len(snapshot) == 1:
                found["esp32"] = esp8266_port
                details["esp32"] = {
                    "location": details.get("esp8266", {}).get("location", ""),
                    "reason": "single-port-remap:esp8266->esp32",
                }
                notes.append(f"single-port remap applied for esp32: {esp8266_port}")

        best_found = found
        best_details = details

        missing = [role for role in required if not found.get(role)]
        if not missing:
            break
        if time.monotonic() >= deadline:
            break
        time.sleep(0.4)

    missing = [role for role in required if not best_found.get(role)]
    if not missing:
        status = "pass"
    elif args.allow_no_hardware:
        status = "skip"
        notes.append("missing required hardware: " + ",".join(sorted(missing)))
    else:
        status = "fail"
        notes.append("missing required hardware: " + ",".join(sorted(missing)))

    ports_payload = {
        "esp32": best_found.get("esp32", ""),
        "esp8266": best_found.get("esp8266", ""),
        "rp2040": best_found.get("rp2040", ""),
    }
    details_payload = {
        "esp32": best_details.get("esp32", {"location": "", "reason": ""}),
        "esp8266": best_details.get("esp8266", {"location": "", "reason": ""}),
        "rp2040": best_details.get("rp2040", {"location": "", "reason": ""}),
    }

    return {
        "status": status,
        "ports": ports_payload,
        "reasons": {
            "esp32": details_payload["esp32"]["reason"],
            "esp8266": details_payload["esp8266"]["reason"],
            "rp2040": details_payload["rp2040"]["reason"],
        },
        "details": details_payload,
        "required_roles": sorted(required),
        "notes": sorted(set(notes)),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Resolve Zacus ESP32/ESP8266/RP2040 serial ports")
    parser.add_argument("--wait-port", type=int, default=3, help="seconds to wait for required ports")
    parser.add_argument("--auto-ports", action="store_true", help="scan automatically for ports")
    parser.add_argument("--need-esp32", action="store_true", help="require ESP32 port")
    parser.add_argument("--need-esp8266", action="store_true", help="require ESP8266 port")
    parser.add_argument("--need-rp2040", action="store_true", help="require RP2040 port")
    parser.add_argument("--allow-no-hardware", action="store_true", help="return skip when hardware missing")
    parser.add_argument("--port-esp32", help="explicit ESP32 port")
    parser.add_argument("--port-esp8266", help="explicit ESP8266 port")
    parser.add_argument("--port-rp2040", help="explicit RP2040 port")
    parser.add_argument("--ports-resolve-json", help="write JSON output to this path")
    parser.add_argument("--prefer-cu", action="store_true", help="prefer /dev/cu.* aliases over tty")
    args = parser.parse_args()

    explicit_any = bool(
        args.port_esp32
        or args.port_esp8266
        or args.port_rp2040
        or os.environ.get("ZACUS_PORT_ESP32")
        or os.environ.get("ZACUS_PORT_ESP8266")
        or os.environ.get("ZACUS_PORT_RP2040")
    )
    if not args.auto_ports and not explicit_any:
        parser.error("must provide --auto-ports or explicit port overrides")

    result = resolve_ports(args)
    encoded = json.dumps(result, indent=2, ensure_ascii=False)
    print(encoded)

    if args.ports_resolve_json:
        out = Path(args.ports_resolve_json)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(encoded + "\n", encoding="utf-8")

    return 1 if result["status"] == "fail" else 0


if __name__ == "__main__":
    raise SystemExit(main())
