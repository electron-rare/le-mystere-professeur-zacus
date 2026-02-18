print("[DEBUG] script started")
#!/usr/bin/env python3
"""Resolve Zacus serial ports with glob mapping, fingerprint fallback, and a learned cache."""
import argparse
import fnmatch
import json
import os
import re
import sys
import time
from pathlib import Path

try:
    from serial import Serial, SerialException
    from serial.tools import list_ports
except ImportError:
    print("pyserial is missing; run ./tools/dev/bootstrap_local.sh", file=sys.stderr)
    sys.exit(2)

FIRMWARE_ROOT = Path(__file__).resolve().parents[2]
PORTS_MAP_PATH = FIRMWARE_ROOT / "tools" / "dev" / "ports_map.json"
LEARNED_MAP_PATH = FIRMWARE_ROOT / ".local" / "ports_map.learned.json"
DEFAULT_PORTS_MAP = {
    "location": {
        "20-6.1.1": "esp32",
        "20-6.1.2": "esp8266_usb",
        "20-6.2*": "esp8266_usb",
        "20-6.1*": "esp32",
        "20-6.4.1": "esp8266_usb",
        "20-6.4.2": "esp32",
    },
    "vidpid": {
        "2e8a:0005": "rp2040",
        "2e8a:000a": "rp2040",
    },
}

FINGERPRINT_SCANS = [
    {"baud": 115200, "duration": 2.0},
    {"baud": 19200, "duration": 2.0},
]
FINGERPRINT_PATTERNS = {
    "esp32": [
        re.compile(r"UI_LINK_STATUS", re.IGNORECASE),
        re.compile(r"\[SYS", re.IGNORECASE),
        re.compile(r"UI_LINK", re.IGNORECASE),
    ],
    "esp8266": [
        re.compile(r"\[SCREEN\]", re.IGNORECASE),
        re.compile(r"HELLO,proto=\d+,ui_type=OLED", re.IGNORECASE),
        re.compile(r"OLED", re.IGNORECASE),
    ],
}


def normalize_role(role: str) -> str:
    value = (role or "").strip().lower()
    if value in ("esp8266", "esp8266_usb", "ui", "oled"):
        return "esp8266"
    return value


def parse_location(hwid: str):
    if not hwid:
        return None
    match = re.search(r"LOCATION=([\w\-.]+)", hwid)
    if match:
        return match.group(1)
    return None


def port_vidpid(port):
    if port.vid is None or port.pid is None:
        return None
    return f"{port.vid:04x}:{port.pid:04x}"


def port_preference(port):
    name = port.device or ""
    if name.startswith("/dev/cu.SLAB"):
        return 0
    if name.startswith("/dev/cu.usbserial"):
        return 1
    if name.startswith("/dev/cu.usbmodem"):
        return 2
    if name.startswith("/dev/cu."):
        return 3
    return 4


def filter_detectable_ports(ports):
    by_location = {}
    for port in ports:
        location = parse_location(port.hwid) or port.device
        existing = by_location.get(location)
        if existing is None or port_preference(port) < port_preference(existing):
            by_location[location] = port
    return list(by_location.values())


def fingerprint_port(device):
    for scan in FINGERPRINT_SCANS:
        try:
            with Serial(device, scan["baud"], timeout=0.4) as ser:
                start = time.monotonic()
                while time.monotonic() - start < scan["duration"]:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    for role, patterns in FINGERPRINT_PATTERNS.items():
                        for pattern in patterns:
                            if pattern.search(line):
                                reason = f"fingerprint:{role}@{scan['baud']}:{pattern.pattern}"
                                return role, reason
        except (SerialException, OSError):
            continue
    return None, None


def load_json(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def build_ports_map():
    raw_map = load_json(PORTS_MAP_PATH)
    if not isinstance(raw_map, dict):
        raw_map = DEFAULT_PORTS_MAP
    location_entries = []
    for key, value in raw_map.get("location", {}).items():
        pattern = str(key)
        if isinstance(value, str):
            role = normalize_role(value)
        elif isinstance(value, dict) and isinstance(value.get("role"), str):
            role = normalize_role(value["role"])
        else:
            continue
        location_entries.append({"pattern": pattern, "role": role, "source": "map"})

    learned = load_json(LEARNED_MAP_PATH).get("location", {})
    if isinstance(learned, dict):
        for key, value in learned.items():
            pattern = str(key)
            role = normalize_role(value)
            location_entries.append({"pattern": pattern, "role": role, "source": "learned"})

    location_entries.sort(key=lambda entry: (-len(entry["pattern"]), entry["pattern"]))

    vidpid_map = {}
    for key, value in raw_map.get("vidpid", {}).items():
        vidpid = str(key).lower()
        if isinstance(value, str):
            vidpid_map[vidpid] = normalize_role(value)
        elif isinstance(value, dict) and isinstance(value.get("role"), str):
            vidpid_map[vidpid] = normalize_role(value["role"])
    return location_entries, vidpid_map


def learn_location(location: str, role: str):
    if not location:
        return False
    cache = load_json(LEARNED_MAP_PATH)
    if not isinstance(cache, dict):
        cache = {}
    entries = cache.get("location")
    if not isinstance(entries, dict):
        entries = {}
    if entries.get(location) == role:
        return False
    entries[location] = role
    cache["location"] = entries
    LEARNED_MAP_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(LEARNED_MAP_PATH, "w", encoding="utf-8") as fh:
        json.dump(cache, fh, indent=2)
    return True


def match_location_entry(entries, location):
    if not location:
        return None
    normalized = location.lower()
    for entry in entries:
        if fnmatch.fnmatch(normalized, entry["pattern"].lower()):
            return entry
    return None


def classify_port(port, location, entries, vidpid_map):
    entry = match_location_entry(entries, location)
    if entry:
        fp_role, fp_reason = fingerprint_port(port.device)
        if fp_role and fp_role != entry["role"]:
            return fp_role, f"fingerprint-override:{entry['pattern']}:{fp_reason}"
        return entry["role"], f"location-map:{entry['pattern']}"
    vidpid = port_vidpid(port)
    if vidpid and vidpid.lower() in vidpid_map:
        return vidpid_map[vidpid.lower()], f"vidpid:{vidpid}"
    return fingerprint_port(port.device)


def classify_ports(ports, entries, vidpid_map):
    found = {}
    details = {}
    notes = []
    cp2102_ports = []
    # DEBUG: log complet des ports détectés dans un fichier
    import json as _json
    debug_ports = []
    for port in ports:
        attrs = {k: getattr(port, k, None) for k in dir(port) if not k.startswith('__') and not callable(getattr(port, k, None))}
        debug_ports.append(attrs)
    debug_log_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), 'logs', 'ports_debug.json')
    try:
        os.makedirs(os.path.dirname(debug_log_path), exist_ok=True)
        with open(debug_log_path, "w", encoding="utf-8") as dbg:
            _json.dump(debug_ports, dbg, indent=2, ensure_ascii=False)
    except Exception as e:
        print(f"[WARN] Could not write debug log: {e}")
    for port in ports:
        location = parse_location(port.hwid) or ""
        role, reason = classify_port(port, location, entries, vidpid_map)
        norm_role = normalize_role(role)
        if norm_role and norm_role not in found:
            found[norm_role] = port.device
            details[norm_role] = {"location": location, "reason": reason}
            if reason and reason.startswith("fingerprint") and location:
                if learn_location(location, norm_role):
                    notes.append(f"learned {location} -> {norm_role}")
            if reason and reason.startswith("fingerprint"):
                notes.append(f"fingerprint {norm_role} on {port.device}")
        # Collecter les ports CP2102 non mappés
        if (getattr(port, 'vid', None) == 0x10C4 and getattr(port, 'pid', None) == 0xEA60):
            cp2102_ports.append(port)
    # DEBUG : print mapping final des rôles trouvés
    import sys
    print("[DEBUG][classify_ports] found:", found)
    sys.stdout.flush()
    # Fallback : assigner les CP2102 restants
    # Mode mono-port : si un seul port CP2102 détecté dans tous les ports, il sert pour les deux rôles
    cp2102_total = [p for p in ports if getattr(p, 'vid', None) == 0x10C4 and getattr(p, 'pid', None) == 0xEA60]
    if len(cp2102_total) == 1:
        port = cp2102_total[0]
        # N'assigne que si non déjà trouvé par mapping/fingerprint
        if "esp32" not in found:
            found["esp32"] = port.device
            details["esp32"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-monop-fallback"}
        if "esp8266" not in found:
            found["esp8266"] = port.device
            details["esp8266"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-monop-fallback"}
        notes.append(f"cp2102 mono-port fallback assigned to esp32+esp8266: {port.device}")
    else:
        for port in cp2102_ports:
            if "esp32" not in found:
                found["esp32"] = port.device
                details["esp32"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-fallback"}
                notes.append(f"cp2102 fallback assigned to esp32: {port.device}")
            elif "esp8266" not in found:
                found["esp8266"] = port.device
                details["esp8266"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-fallback"}
                notes.append(f"cp2102 fallback assigned to esp8266: {port.device}")
    return found, details, notes


def build_manual_assignments(args, snapshot):
    overrides = {}
    details = {}
    notes = []
    for role, attr, env in (
        ("esp32", "port_esp32", "ZACUS_PORT_ESP32"),
        ("esp8266", "port_esp8266", "ZACUS_PORT_ESP8266"),
        ("rp2040", "port_rp2040", "ZACUS_PORT_RP2040"),
    ):
        port_value = getattr(args, attr)
        if not port_value:
            port_value = os.environ.get(env)
        if not port_value:
            continue
        overrides[role] = port_value
        location = ""
        for port in snapshot:
            if port.device == port_value:
                location = parse_location(port.hwid) or ""
                break
        details[role] = {"location": location, "reason": "manual-override"}
        notes.append(f"manual override {role} -> {port_value}")
    return overrides, details, notes


def dedupe_notes(notes):
    seen = set()
    out = []
    for note in notes:
        if note and note not in seen:
            seen.add(note)
            out.append(note)
    return out


def resolve_ports(args):
    # Initialisation des variables AVANT toute utilisation
    details_payload = {}
    best_found = {}
    best_details = {}
    notes = []
    entries, vidpid_map = build_ports_map()
    snapshot = list(list_ports.comports())

    # 1. Attribution automatique par mapping/fingerprint/fallback
    auto_found, auto_details, auto_notes = classify_ports(snapshot, entries, vidpid_map)
    best_found.update(auto_found)
    best_details.update(auto_details)
    notes.extend(auto_notes)

    # 2. Overrides manuels (args/env) : prioritaire si défini
    manual_ports, manual_details, manual_notes = build_manual_assignments(args, snapshot)
    best_found.update(manual_ports)
    best_details.update(manual_details)
    notes.extend(manual_notes)

    # 3. Correction mono-port CP2102 (si un seul port CP2102 détecté)
    cp2102_total = [p for p in snapshot if getattr(p, 'vid', None) == 0x10C4 and getattr(p, 'pid', None) == 0xEA60]
    if len(cp2102_total) == 1:
        port = cp2102_total[0]
        if "esp32" not in best_found:
            best_found["esp32"] = port.device
            best_details["esp32"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-monop-fallback"}
        if "esp8266" not in best_found:
            best_found["esp8266"] = port.device
            best_details["esp8266"] = {"location": parse_location(port.hwid) or "", "reason": "cp2102-monop-fallback"}
        notes.append(f"cp2102 mono-port fallback (resolve_ports) assigned to esp32+esp8266: {port.device}")

    # 4. Statut et payloads
    required_roles = set()
    if args.need_esp32:
        required_roles.add("esp32")
    if args.need_esp8266:
        required_roles.add("esp8266")

    roles_found = set(best_found.keys())
    has_esp32 = "esp32" in roles_found
    has_esp8266 = "esp8266" in roles_found
    has_rp2040 = "rp2040" in roles_found
    if has_esp32 and (has_esp8266 or has_rp2040):
        status = "pass"
    elif has_esp32:
        status = "skip" if args.allow_no_hardware else "fail"
    else:
        status = "skip" if args.allow_no_hardware else "fail"

    details_payload["esp32"] = best_details.get("esp32", {"location": "", "reason": ""})
    details_payload["esp8266"] = best_details.get("esp8266", {"location": "", "reason": ""})

    ports_payload = {
        "esp32": best_found.get("esp32", ""),
        "esp8266": best_found.get("esp8266", ""),
        "rp2040": best_found.get("rp2040", ""),
    }

    # Correction : swap si fingerprint inversé
    if ports_payload["esp32"] and ports_payload["esp8266"]:
        fp_esp32, fp_reason32 = fingerprint_port(ports_payload["esp32"])
        fp_esp8266, fp_reason8266 = fingerprint_port(ports_payload["esp8266"])
        if fp_esp32 == "esp8266" and fp_esp8266 == "esp32":
            ports_payload["esp32"], ports_payload["esp8266"] = ports_payload["esp8266"], ports_payload["esp32"]
            details_payload["esp32"]["reason"] = f"fingerprint-swap:{fp_reason32}"
            details_payload["esp8266"]["reason"] = f"fingerprint-swap:{fp_reason8266}"
            notes.append("fingerprint swap applied")

    # Affichage debug si demandé (après attribution)
    if hasattr(args, 'debug') and args.debug:
        print("[DEBUG] best_found:", best_found)
        print("[DEBUG] best_details:", best_details)
        print("[DEBUG] details_payload:", details_payload)
        print("[DEBUG] notes:", notes)

    result = {
        "status": status,
        "ports": ports_payload,
        "reasons": {role: details_payload[role]["reason"] for role in ("esp32", "esp8266")},
        "details": details_payload,
        "notes": dedupe_notes(notes),
    }
    # Affichage debug juste avant le return
    if hasattr(args, 'debug') and args.debug:
        print("[DEBUG] best_found:", best_found)
        print("[DEBUG] best_details:", best_details)
        print("[DEBUG] details_payload:", details_payload)
        print("[DEBUG] notes:", notes)
        print("[DEBUG] result:", result)
    return result


def main():
    parser = argparse.ArgumentParser(description="Resolve Zacus ESP32/ESP8266/RP2040 serial ports")
    parser.add_argument("--debug", action="store_true", help="affiche le mapping et les détails pour debug")
    parser.add_argument("--wait-port", type=int, default=3, help="seconds to wait for ports")
    parser.add_argument("--auto-ports", action="store_true", help="scan automatically for ports")
    parser.add_argument("--need-esp32", action="store_true", help="require ESP32 port")
    parser.add_argument("--need-esp8266", action="store_true", help="require ESP8266 port")
    parser.add_argument("--need-rp2040", action="store_true", help="require RP2040 port")
    parser.add_argument("--allow-no-hardware", action="store_true", help="return skip when hardware missing")
    parser.add_argument("--port-esp32", help="explicit ESP32 port")
    parser.add_argument("--port-esp8266", help="explicit ESP8266 port")
    parser.add_argument("--port-rp2040", help="explicit RP2040 port")
    parser.add_argument("--ports-resolve-json", help="write JSON output to this path")
    args = parser.parse_args()

    if not args.auto_ports and not args.port_esp32 and not args.port_esp8266 and not os.environ.get("ZACUS_PORT_ESP32") and not os.environ.get("ZACUS_PORT_ESP8266"):
        parser.error("must provide --auto-ports or explicit port overrides")

    result = resolve_ports(args)
    # Affichage du résultat JSON pour debug plug-and-play
    print(json.dumps(result, indent=2, ensure_ascii=False))
    sys.stdout.flush()
    if result["status"] == "fail":
        print("[FAIL] Attribution automatique échouée :", result)
        sys.stdout.flush()
    sys.exit(1 if result["status"] == "fail" else 0)
