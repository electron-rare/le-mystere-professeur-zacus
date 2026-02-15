#!/usr/bin/env python3
"""Resolve ESP32/ESP8266 serial ports deterministically for local hardware runs."""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print(json.dumps({"status": "fail", "notes": ["pyserial missing: pip install pyserial"]}))
    raise SystemExit(2)


REPO_ROOT = Path(__file__).resolve().parents[2]
PORTS_MAP_PATH = REPO_ROOT / "hardware" / "firmware" / "tools" / "dev" / "ports_map.json"

USB_HINTS = ("slab", "usbserial", "wch", "ch340", "cp210", "usbmodem")
ROLE_HINTS = {
    "esp32": ("esp32",),
    "esp8266_usb": ("esp8266", "nodemcu", "ch340", "cp210"),
}
REQUEST_TO_CANONICAL = {"esp32": "esp32", "esp8266": "esp8266_usb"}
CANONICAL_TO_REQUEST = {"esp32": "esp32", "esp8266_usb": "esp8266"}


def is_bluetooth_port(device: str) -> bool:
    lower = device.lower()
    return "bluetooth" in lower or lower.endswith(".blth")


def normalize_role(value: Optional[str]) -> Optional[str]:
    if not value:
        return None
    lower = value.strip().lower()
    if lower in ("ui", "oled", "esp8266", "esp8266_usb", "port-ui"):
        return "esp8266_usb"
    if lower == "esp32":
        return "esp32"
    return None


def parse_location(port) -> str:
    location = getattr(port, "location", None)
    if location:
        return str(location).lower()
    hwid = str(getattr(port, "hwid", "") or "")
    m = re.search(r"LOCATION=([\w\-.]+)", hwid)
    if m:
        return m.group(1).lower()
    return ""


def load_ports_map() -> Dict[str, object]:
    default = {"location": [], "vidpid": {}}
    if not PORTS_MAP_PATH.exists():
        return default
    try:
        raw = json.loads(PORTS_MAP_PATH.read_text(encoding="utf-8"))
    except Exception:
        return default
    location: List[Tuple[str, str]] = []
    for key, value in (raw.get("location") or {}).items():
        role = normalize_role(str(value))
        if role:
            location.append((str(key).lower(), role))
    location.sort(key=lambda item: (-len(item[0]), item[0]))
    vidpid: Dict[str, str] = {}
    for key, value in (raw.get("vidpid") or {}).items():
        role = normalize_role(str(value))
        if role:
            vidpid[str(key).lower()] = role
    return {"location": location, "vidpid": vidpid}


def is_usb_like(port) -> bool:
    text = " ".join(
        [
            str(getattr(port, "device", "") or ""),
            str(getattr(port, "description", "") or ""),
            str(getattr(port, "manufacturer", "") or ""),
            str(getattr(port, "product", "") or ""),
            str(getattr(port, "hwid", "") or ""),
        ]
    ).lower()
    if "bluetooth" in text or ".blth" in text:
        return False
    return any(token in text for token in USB_HINTS)


def is_candidate_port(port) -> bool:
    device = str(getattr(port, "device", "") or "")
    if not device:
        return False
    if is_bluetooth_port(device):
        return False
    if device.startswith("/dev/cu."):
        return True
    return is_usb_like(port)


def score_port(port, prefer_cu: bool) -> int:
    device = str(getattr(port, "device", "") or "")
    score = 50
    if prefer_cu and device.startswith("/dev/cu."):
        score -= 20
    elif device.startswith("/dev/cu."):
        score -= 10
    if "slab" in device.lower() or "usbserial" in device.lower() or "wch" in device.lower():
        score -= 6
    if is_usb_like(port):
        score -= 8
    return score


def role_from_map(port, ports_map: Dict[str, object]) -> Optional[str]:
    location = parse_location(port)
    if location:
        location_patterns = ports_map.get("location") or []
        if isinstance(location_patterns, dict):
            location_patterns = list(location_patterns.items())
        for pattern, mapped_role in location_patterns:
            if fnmatch.fnmatch(location, pattern):
                return mapped_role
    vid = getattr(port, "vid", None)
    pid = getattr(port, "pid", None)
    if vid is not None and pid is not None:
        key = f"{vid:04x}:{pid:04x}".lower()
        if key in ports_map["vidpid"]:
            return ports_map["vidpid"][key]
    return None


def role_from_hint(port) -> Optional[str]:
    text = " ".join(
        [
            str(getattr(port, "description", "") or ""),
            str(getattr(port, "manufacturer", "") or ""),
            str(getattr(port, "product", "") or ""),
            str(getattr(port, "hwid", "") or ""),
        ]
    ).lower()
    for role, hints in ROLE_HINTS.items():
        if any(h in text for h in hints):
            return role
    return None


def fingerprint_port(device: str, timeout: float = 2.0) -> Optional[str]:
    if serial is None:
        return None
    try:
        with serial.Serial(device, 115200, timeout=0.1) as ser:
            ser.dtr = False
            ser.rts = False
            time.sleep(0.05)
            ser.dtr = True
            ser.rts = True
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                text = line.lower()
                if "esp-rom" in text or "rst:" in text:
                    return "esp32"
                if "ets jan" in text or "rst cause" in text:
                    return "esp8266"
    except Exception:
        return None
    return None



def choose_interactive(candidates: List[dict], role: str) -> Optional[str]:
    print(f"Select port for {role}:", file=sys.stderr)
    for idx, item in enumerate(candidates, start=1):
        print(f"  {idx}) {item['device']} [{item['reason']}]", file=sys.stderr)
    try:
        raw = input(f"{role} port number [1-{len(candidates)}]: ").strip()
    except EOFError:
        return None
    if not raw.isdigit():
        return None
    pos = int(raw)
    if pos < 1 or pos > len(candidates):
        return None
    return candidates[pos - 1]["device"]


def gather_ports(wait_port: int) -> List:
    deadline = time.monotonic() + max(1, wait_port)
    while True:
        ports = list(list_ports.comports())
        if ports:
            return ports
        if time.monotonic() >= deadline:
            return []
        time.sleep(0.4)


def classify(ports: List, prefer_cu: bool, ports_map: Dict[str, object], allow_probe: bool) -> Tuple[Dict[str, str], Dict[str, str], Dict[str, str], Dict[str, dict], List[str]]:
    filtered = [p for p in ports if is_candidate_port(p)]
    usb_ports = [p for p in filtered if is_usb_like(p)]
    ports = usb_ports if usb_ports else filtered
    candidates: List[dict] = []
    for port in ports:
        device = str(getattr(port, "device", "") or "")
        if not device:
            continue
        mapped_role = role_from_map(port, ports_map)
        hinted_role = role_from_hint(port)
        role = mapped_role or hinted_role
        location = parse_location(port)
        reason = ""
        if mapped_role:
            reason = f"location-map:{location or 'unknown'}"
        elif hinted_role:
            reason = "usb-hint"
        else:
            reason = "fallback"
        candidates.append(
            {
                "device": device,
                "role": role,
                "location": location,
                "reason": reason,
                "score": score_port(port, prefer_cu),
            }
        )

    candidates.sort(key=lambda x: (x["score"], x["device"]))
    by_role: Dict[str, List[dict]] = {"esp32": [], "esp8266": []}
    fallback: List[dict] = []
    for cand in candidates:
        canonical_role = normalize_role(cand.get("role"))
        requested_role = CANONICAL_TO_REQUEST.get(canonical_role or "", "")
        if requested_role in by_role:
            by_role[requested_role].append(cand)
        else:
            fallback.append(cand)

    selected: Dict[str, str] = {}
    reasons: Dict[str, str] = {}
    probe_baud: Dict[str, str] = {}
    details: Dict[str, dict] = {}
    notes: List[str] = []

    used = set()
    for role in ("esp32", "esp8266"):
        if by_role[role]:
            selected[role] = by_role[role][0]["device"]
            reasons[role] = by_role[role][0]["reason"]
            details[role] = {
                "port": by_role[role][0]["device"],
                "location": by_role[role][0]["location"] or "unknown",
                "role": REQUEST_TO_CANONICAL[role],
                "reason": by_role[role][0]["reason"],
            }
            used.add(by_role[role][0]["device"])

    if allow_probe:
        unresolved = [r for r in ("esp32", "esp8266") if r not in selected]
        for cand in candidates:
            if not unresolved:
                break
            if cand["device"] in used:
                continue
            fingerprint = fingerprint_port(cand["device"])
            if not fingerprint:
                continue
            if fingerprint not in unresolved:
                continue
            selected[fingerprint] = cand["device"]
            reasons[fingerprint] = f"fingerprint:{fingerprint}"
            probe_baud[fingerprint] = "fingerprint"
            details[fingerprint] = {
                "port": cand["device"],
                "location": cand["location"] or "unknown",
                "role": REQUEST_TO_CANONICAL[fingerprint],
                "reason": f"fingerprint:{fingerprint}",
            }
            notes.append(f"fingerprint:{fingerprint}={cand['device']}")
            used.add(cand["device"])
            unresolved = [r for r in ("esp32", "esp8266") if r not in selected]

    used = set(selected.values())
    for role in ("esp32", "esp8266"):
        if role in selected:
            continue
        for cand in candidates:
            if cand["device"] in used:
                continue
            selected[role] = cand["device"]
            reasons[role] = f"deterministic:{cand['reason']}"
            details[role] = {
                "port": cand["device"],
                "location": cand["location"] or "unknown",
                "role": REQUEST_TO_CANONICAL[role],
                "reason": f"deterministic:{cand['reason']}",
            }
            used.add(cand["device"])
            break

    if len(candidates) > 2:
        notes.append(f"multiple candidates: {len(candidates)}")

    return selected, reasons, probe_baud, details, notes


def output_result(result: dict, json_path: str, exit_code: int) -> int:
    if json_path:
        try:
            dest = Path(json_path)
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(json.dumps(result, indent=2), encoding="utf-8")
        except Exception:
            pass
    print(json.dumps(result))
    return exit_code


def main() -> int:
    parser = argparse.ArgumentParser(description="Resolve serial ports for ESP32 + ESP8266")
    parser.add_argument("--port-esp32", default="")
    parser.add_argument("--port-esp8266", default="")
    parser.add_argument("--wait-port", type=int, default=20)
    parser.add_argument("--need-esp32", action="store_true")
    parser.add_argument("--need-esp8266", action="store_true")
    parser.add_argument("--allow-no-hardware", action="store_true")
    parser.add_argument("--auto-ports", dest="auto_ports", action="store_true", default=True)
    parser.add_argument("--no-auto-ports", dest="auto_ports", action="store_false")
    parser.add_argument("--prefer-cu", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--ports-resolve-json", default="")
    args = parser.parse_args()

    prefer_cu = args.prefer_cu or sys.platform == "darwin"
    ports_map = load_ports_map()

    result = {
        "status": "pass",
        "ports": {"esp32": args.port_esp32, "esp8266": args.port_esp8266},
        "reasons": {
            "esp32": "explicit" if args.port_esp32 else "",
            "esp8266": "explicit" if args.port_esp8266 else "",
        },
        "probe_baud": {"esp32": "", "esp8266": ""},
        "details": {
            "esp32": {
                "port": args.port_esp32,
                "location": "explicit" if args.port_esp32 else "",
                "role": "esp32" if args.port_esp32 else "",
                "reason": "explicit" if args.port_esp32 else "",
            },
            "esp8266": {
                "port": args.port_esp8266,
                "location": "explicit" if args.port_esp8266 else "",
                "role": "esp8266_usb" if args.port_esp8266 else "",
                "reason": "explicit" if args.port_esp8266 else "",
            },
        },
        "notes": [],
    }

    missing_roles = []
    if args.need_esp32 and not result["ports"]["esp32"]:
        missing_roles.append("esp32")
    if args.need_esp8266 and not result["ports"]["esp8266"]:
        missing_roles.append("esp8266")

    candidates = []
    if missing_roles and args.auto_ports:
        ports = gather_ports(args.wait_port)
        if not ports:
            if args.allow_no_hardware:
                result["status"] = "skip"
                result["notes"].append("no serial ports detected")
                return output_result(result, args.ports_resolve_json, 0)
            result["status"] = "fail"
            result["notes"].append("no serial ports detected")
            return output_result(result, args.ports_resolve_json, 1)

        selected, reasons, probe_baud, details, notes = classify(
            ports=ports,
            prefer_cu=prefer_cu,
            ports_map=ports_map,
            allow_probe=True,
        )
        result["notes"].extend(notes)

        for role in missing_roles:
            if role in selected:
                result["ports"][role] = selected[role]
                result["reasons"][role] = reasons.get(role, "auto")
                if role in probe_baud:
                    result["probe_baud"][role] = probe_baud[role]
                result["details"][role] = details.get(role, result["details"][role])

        # optional interactive disambiguation when both ports still unresolved or duplicated
        if args.interactive and sys.stdin.isatty():
            if result["ports"].get("esp32") and result["ports"].get("esp8266") and result["ports"]["esp32"] == result["ports"]["esp8266"]:
                candidates = []
                for p in list_ports.comports():
                    candidates.append(
                        {
                            "device": p.device,
                            "reason": f"location={parse_location(p) or 'unknown'}",
                        }
                    )
                candidates.sort(key=lambda x: x["device"])
                if candidates:
                    pick1 = choose_interactive(candidates, "esp32")
                    if pick1:
                        result["ports"]["esp32"] = pick1
                        result["reasons"]["esp32"] = "interactive"
                    remaining = [c for c in candidates if c["device"] != result["ports"]["esp32"]]
                    if remaining:
                        pick2 = choose_interactive(remaining, "esp8266")
                        if pick2:
                            result["ports"]["esp8266"] = pick2
                            result["reasons"]["esp8266"] = "interactive"

    unresolved = []
    if args.need_esp32 and not result["ports"].get("esp32"):
        unresolved.append("esp32")
    if args.need_esp8266 and not result["ports"].get("esp8266"):
        unresolved.append("esp8266")

    if unresolved:
        if args.allow_no_hardware:
            result["status"] = "skip"
            result["notes"].append(f"unresolved roles: {','.join(unresolved)}")
        else:
            result["status"] = "fail"
            result["notes"].append(f"unresolved roles: {','.join(unresolved)}")
            result["notes"].append("check USB data cable, CP210x/CH340 driver, and press BOOT for ESP32 upload")
            return output_result(result, args.ports_resolve_json, 1)

    if result["status"] == "pass":
        if result["ports"].get("esp32") and result["ports"].get("esp8266") and result["ports"]["esp32"] == result["ports"]["esp8266"]:
            result["status"] = "fail"
            result["notes"].append("esp32 and esp8266 resolved to the same device")
            if not args.allow_no_hardware:
                return output_result(result, args.ports_resolve_json, 1)

    return output_result(result, args.ports_resolve_json, 0)


if __name__ == "__main__":
    raise SystemExit(main())
