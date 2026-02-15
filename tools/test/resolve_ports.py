#!/usr/bin/env python3
"""Resolve ESP32/ESP8266 serial ports deterministically for local hardware runs."""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
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
LEARNED_MAP_PATH = REPO_ROOT / "hardware" / "firmware" / ".local" / "ports_map.learned.json"
ESP32_SIGNATURE = re.compile(r"(BOOT_PROTO|U-SON|U_LOCK|STORY_V2|MP3_DBG)", re.IGNORECASE)
ESP8266_SIGNATURE = re.compile(r"(ets Jan|Exception \(|Stack smashing|\[SCREEN\])", re.IGNORECASE)
FINGERPRINT_BAUDS = (115200, 19200)
FINGERPRINT_TIMEOUT = 2.0


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


def load_map_file(path: Path) -> Dict[str, Dict[str, str]]:
    data = {"location": {}, "vidpid": {}}
    if not path.exists():
        return data
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return data
    for key, value in (raw.get("location") or {}).items():
        role = normalize_role(str(value))
        if role:
            data["location"][str(key).lower()] = role
    for key, value in (raw.get("vidpid") or {}).items():
        role = normalize_role(str(value))
        if role:
            data["vidpid"][str(key).lower()] = role
    return data


def sort_patterns(patterns: List[dict]) -> List[dict]:
    return sorted(
        patterns,
        key=lambda entry: (
            -len(entry["pattern"]),
            0 if entry["source"] == "learned-map" else 1,
            entry["pattern"],
        ),
    )


def load_ports_map() -> Dict[str, object]:
    base = load_map_file(PORTS_MAP_PATH)
    learned = load_map_file(LEARNED_MAP_PATH)
    patterns = []
    for pattern, role in base["location"].items():
        patterns.append({"pattern": pattern, "role": role, "source": "location-map"})
    for pattern, role in learned["location"].items():
        patterns.append({"pattern": pattern, "role": role, "source": "learned-map"})
    patterns = sort_patterns(patterns)
    vidpid: Dict[str, str] = {}
    vidpid.update(learned["vidpid"])
    vidpid.update(base["vidpid"])
    return {"patterns": patterns, "vidpid": vidpid, "learned": learned}


def record_learned_mapping(ports_map: Dict[str, object], location: str, role: str) -> bool:
    if not location:
        return False
    normalized = location.lower()
    learned = ports_map.get("learned") or {"location": {}, "vidpid": {}}
    location_map = learned.setdefault("location", {})
    canonical_role = normalize_role(role)
    if not canonical_role:
        return False
    if location_map.get(normalized) == canonical_role:
        return False
    location_map[normalized] = canonical_role
    patterns = [
        entry
        for entry in ports_map["patterns"]
        if not (entry["pattern"] == normalized and entry["source"] == "learned-map")
    ]
    patterns.append({"pattern": normalized, "role": canonical_role, "source": "learned-map"})
    ports_map["patterns"] = sort_patterns(patterns)
    ports_map["learned"] = learned
    return True


def persist_learned_map(learned_map: Dict[str, Dict[str, str]]) -> None:
    if not learned_map:
        return
    try:
        LEARNED_MAP_PATH.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "location": {k: learned_map["location"][k] for k in sorted(learned_map.get("location", {}))},
            "vidpid": {k: learned_map["vidpid"][k] for k in sorted(learned_map.get("vidpid", {}))},
        }
        LEARNED_MAP_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    except Exception:
        pass


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


def role_from_map(port, ports_map: Dict[str, object]) -> Tuple[Optional[str], str, str]:
    location = parse_location(port)
    for entry in ports_map.get("patterns", []):
        if location and fnmatch.fnmatch(location, entry["pattern"]):
            return entry["role"], entry["source"], entry["pattern"]
    vid = getattr(port, "vid", None)
    pid = getattr(port, "pid", None)
    if vid is not None and pid is not None:
        key = f"{vid:04x}:{pid:04x}".lower()
        vidpid_map = ports_map.get("vidpid") or {}
        if key in vidpid_map:
            return vidpid_map[key], "vidpid-map", ""
    return None, "", ""


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


def fingerprint_port(
    device: str,
    bauds: Tuple[int, ...] = FINGERPRINT_BAUDS,
    timeout: float = FINGERPRINT_TIMEOUT,
) -> Tuple[Optional[str], Optional[int]]:
    if serial is None:
        return None, None
    for baud in bauds:
        try:
            with serial.Serial(device, baud, timeout=0.1) as ser:
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
                    text = line
                    if ESP32_SIGNATURE.search(text):
                        return "esp32", baud
                    if ESP8266_SIGNATURE.search(text):
                        return "esp8266", baud
        except Exception:
            continue
    return None, None



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


def classify(ports: List, prefer_cu: bool, ports_map: Dict[str, object], allow_probe: bool) -> Tuple[Dict[str, str], Dict[str, str], Dict[str, str], Dict[str, dict], List[str], bool]:
    filtered = [p for p in ports if is_candidate_port(p)]
    usb_ports = [p for p in filtered if is_usb_like(p)]
    ports = usb_ports if usb_ports else filtered
    candidates: List[dict] = []
    for port in ports:
        device = str(getattr(port, "device", "") or "")
        if not device:
            continue
        mapped_role, mapped_source, _ = role_from_map(port, ports_map)
        hinted_role = role_from_hint(port)
        role = mapped_role or hinted_role
        location = parse_location(port)
        reason = ""
        if mapped_role:
            reason = f"{mapped_source}:{location or 'unknown'}"
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
    learned_dirty = False

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
            fingerprint_role, fingerprint_baud = fingerprint_port(cand["device"])
            if not fingerprint_role:
                continue
            if fingerprint_role not in unresolved:
                continue
            selected[fingerprint_role] = cand["device"]
            reasons[fingerprint_role] = f"fingerprint:{fingerprint_baud}"
            probe_baud[fingerprint_role] = str(fingerprint_baud)
            details[fingerprint_role] = {
                "port": cand["device"],
                "location": cand["location"] or "unknown",
                "role": REQUEST_TO_CANONICAL[fingerprint_role],
                "reason": f"fingerprint:{fingerprint_baud}",
            }
            notes.append(f"fingerprint:{fingerprint_role}={cand['device']}@{fingerprint_baud}")
            learned_dirty = learned_dirty or record_learned_mapping(
                ports_map, cand["location"], fingerprint_role
            )
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

    return selected, reasons, probe_baud, details, notes, learned_dirty


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

    env_override = {
        "esp32": False,
        "esp8266": False,
    }
    env_esp32 = os.environ.get("ZACUS_ESP32_PORT", "").strip()
    env_esp8266 = os.environ.get("ZACUS_ESP8266_PORT", "").strip()
    if env_esp32 and not args.port_esp32:
        args.port_esp32 = env_esp32
        env_override["esp32"] = True
    if env_esp8266 and not args.port_esp8266:
        args.port_esp8266 = env_esp8266
        env_override["esp8266"] = True

    ports_map = load_ports_map()

    initial_ports = {"esp32": args.port_esp32, "esp8266": args.port_esp8266}

    def reason_label(role: str) -> str:
        if env_override.get(role):
            return "env"
        return "explicit" if initial_ports.get(role) else ""

    def make_detail(role: str, port: str, reason: str) -> Dict[str, str]:
        return {
            "port": port,
            "location": "explicit" if port else "",
            "role": REQUEST_TO_CANONICAL[role],
            "reason": reason,
        }

    result = {
        "status": "pass",
        "ports": initial_ports.copy(),
        "reasons": {role: reason_label(role) for role in ("esp32", "esp8266")},
        "probe_baud": {"esp32": "", "esp8266": ""},
        "details": {
            "esp32": make_detail("esp32", initial_ports["esp32"], reason_label("esp32")),
            "esp8266": make_detail("esp8266", initial_ports["esp8266"], reason_label("esp8266")),
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

        selected, reasons, probe_baud, details, notes, learned_dirty = classify(
            ports=ports,
            prefer_cu=prefer_cu,
            ports_map=ports_map,
            allow_probe=True,
        )
        result["notes"].extend(notes)

        if learned_dirty:
            persist_learned_map(ports_map.get("learned", {}))

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
