#!/usr/bin/env python3
"""Quick USB serial smoke for ESP32/ESP8266/RP2040 with LOCATION-based ROLE detection."""

import argparse
import json
import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path

try:
    from serial import Serial
    from serial.tools import list_ports
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

PING_COMMAND = "PING"
PING_OK_PATTERN = re.compile(r"\b(PONG|ACK|OK|UNKNOWN|HELLO|STAT|STATUS)\b", re.IGNORECASE)
READY_PATTERN = re.compile(r"(\[SCREEN\]\s*Ready\.|\[SCREEN\]\s*oled=OK)", re.IGNORECASE)
FATAL_PATTERN = re.compile(
    r"(User exception|Exception|panic|abort|assert|rst cause|stack smashing|Guru Meditation|Fatal)",
    re.IGNORECASE,
)
REBOOT_PATTERN = re.compile(r"(ets Jan|rst:|boot mode|load:0x|entry 0x)", re.IGNORECASE)
BASIC_WIFI_PATTERN = re.compile(
    r"(wifi|wlan|ssid|rssi|disconnect|reconnect|dhcp|ip=|got ip|sta|ap)",
    re.IGNORECASE,
)
BINARY_JUNK_RATIO_THRESHOLD = 0.35
BINARY_JUNK_BURST_LIMIT = 2
BINARY_JUNK_MIN_BYTES = 8

ROOT = Path(__file__).resolve().parents[2]
PORTS_MAP_PATH = ROOT / "tools" / "dev" / "ports_map.json"
DEFAULT_PORTS_MAP = {
    "location": {
        "20-6.1.1": "esp32",
        "20-6.1.2": "esp8266_usb",
        "20-6.4.1": "esp8266_usb",
        "20-6.4.2": "esp32",
    },
    "vidpid": {
        "2e8a:0005": "rp2040",
        "2e8a:000a": "rp2040",
    },
}
ROLE_PRIORITY = ["esp32", "esp8266_usb", "rp2040"]

FW_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = FW_ROOT.parent.parent
PHASE = "serial_smoke"


def init_evidence(outdir: str) -> dict:
    stamp = time.strftime("%Y%m%d-%H%M%S", time.gmtime())
    if outdir:
        path = Path(outdir)
        if not path.is_absolute():
            path = FW_ROOT / path
    else:
        path = FW_ROOT / "artifacts" / PHASE / stamp
    path.mkdir(parents=True, exist_ok=True)
    return {
        "dir": path,
        "meta": path / "meta.json",
        "git": path / "git.txt",
        "commands": path / "commands.txt",
        "summary": path / "summary.md",
    }


def write_git_info(dest: Path) -> None:
    lines = []
    try:
        branch = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "--abbrev-ref",
            "HEAD",
        ], text=True).strip()
    except Exception:
        branch = "n/a"
    try:
        commit = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "HEAD",
        ], text=True).strip()
    except Exception:
        commit = "n/a"
    lines.append(f"branch: {branch}")
    lines.append(f"commit: {commit}")
    lines.append("status:")
    try:
        status = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "status",
            "--porcelain",
        ], text=True).strip()
    except Exception:
        status = ""
    if status:
        lines.append(status)
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_meta_json(dest: Path, command: str) -> None:
    payload = {
        "timestamp": time.strftime("%Y%m%d-%H%M%S", time.gmtime()),
        "phase": PHASE,
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "command": command,
        "cwd": str(Path.cwd()),
        "repo_root": str(REPO_ROOT),
        "fw_root": str(FW_ROOT),
    }
    dest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def record_command(commands_path: Path, cmd: str) -> None:
    with commands_path.open("a", encoding="utf-8") as fp:
        fp.write(cmd + "\n")


def write_summary(dest: Path, result: str, notes: list[str]) -> None:
    lines = ["# Serial smoke summary", "", f"- Result: **{result}**"]
    for note in notes:
        lines.append(f"- {note}")
    # Ajout du mapping chip/port
    try:
        from serial.tools import list_ports
        ports = list_ports.comports()
        for port in ports:
            chip = "unknown"
            hwid = port.hwid
            if hwid:
                if "VID:PID=10C4:EA60" in hwid:
                    chip = "CP2102"
                elif "VID:PID=1A86:7523" in hwid:
                    chip = "CH340"
                elif "VID:PID=2E8A:000A" in hwid or "VID:PID=2E8A:0005" in hwid:
                    chip = "RP2040"
                elif "CH340" in (port.description or "") or "CH341" in (port.description or ""):
                    chip = "CH340"
                elif "Pico" in (port.description or ""):
                    chip = "RP2040"
            lines.append(f"- Port: {port.device} | Chip: {chip} | Desc: {port.description}")
    except Exception:
        lines.append("- [chip detection error]")
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def normalize_role(role: str) -> str:
    value = (role or "").strip().lower()
    if value in ("esp8266", "esp8266_usb", "ui", "oled"):
        return "esp8266_usb"
    return value


def exit_no_hw(wait_port, allow, reason=None):
    if not allow:
        if reason:
            print(f"[error] {reason}", file=sys.stderr)
        print(
            f"[error] no suitable hardware detected after waiting {wait_port}s",
            file=sys.stderr,
        )
        return 2
    print("SKIP: no hardware detected")
    return 0


def load_ports_map():
    raw_map = DEFAULT_PORTS_MAP
    try:
        if PORTS_MAP_PATH.exists():
            loaded = json.loads(PORTS_MAP_PATH.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                raw_map = loaded
    except Exception:
        raw_map = DEFAULT_PORTS_MAP
    return normalize_ports_map(raw_map)


def normalize_ports_map(raw_map):
    location = {}
    vidpid = {}
    if not isinstance(raw_map, dict):
        return {"location": location, "vidpid": vidpid}

    if "location" in raw_map or "vidpid" in raw_map:
        raw_location = raw_map.get("location", {})
        if isinstance(raw_location, dict):
            for key, value in raw_location.items():
                if isinstance(value, str):
                    location[str(key).lower()] = normalize_role(value)
                elif isinstance(value, dict) and isinstance(value.get("role"), str):
                    location[str(key).lower()] = normalize_role(value["role"])
        raw_vidpid = raw_map.get("vidpid", {})
        if isinstance(raw_vidpid, dict):
            for key, value in raw_vidpid.items():
                if isinstance(value, str):
                    vidpid[str(key).lower()] = normalize_role(value)
                elif isinstance(value, dict) and isinstance(value.get("role"), str):
                    vidpid[str(key).lower()] = normalize_role(value["role"])
        return {"location": location, "vidpid": vidpid}

    # Backward compatibility with the previous shape:
    # { "macos": { "20-6.1.1": {"role":"esp32"} } }
    for platform_key in ("macos", "linux", "windows"):
        platform_map = raw_map.get(platform_key, {})
        if not isinstance(platform_map, dict):
            continue
        for key, value in platform_map.items():
            if isinstance(value, dict) and isinstance(value.get("role"), str):
                location[str(key).lower()] = normalize_role(value["role"])
            elif isinstance(value, str):
                location[str(key).lower()] = normalize_role(value)
    return {"location": location, "vidpid": vidpid}


def parse_location(hwid: str):
    if not hwid:
        return None
    match = re.search(r"LOCATION=([\w\-.]+)", hwid)
    if match:
        return match.group(1).lower()
    return None


def port_vidpid(port):
    if port.vid is None or port.pid is None:
        return None
    return f"{port.vid:04x}:{port.pid:04x}"


def canonical_device(group, prefer_cu):
    def score(port):
        name = port.device
        if name.startswith("/dev/cu.SLAB"):
            return 0
        if name.startswith("/dev/cu.usbserial"):
            return 1
        if name.startswith("/dev/cu.usbmodem"):
            return 2
        if prefer_cu and name.startswith("/dev/cu."):
            return 3
        if name.startswith("/dev/cu."):
            return 4
        return 5

    return min(group, key=score)


def determine_role(port, location, ports_map):
    # Recherche exacte
    loc_map = ports_map.get("location", {})
    loc_key = (location or "").lower()
    location_role = loc_map.get(loc_key)
    if location_role:
        return location_role
    # Recherche wildcard
    for key, value in loc_map.items():
        if "*" in key:
            prefix = key.rstrip("*")
            if loc_key.startswith(prefix):
                return value
    vidpid = port_vidpid(port)
    if vidpid:
        mapped = ports_map.get("vidpid", {}).get(vidpid)
        if mapped:
            return mapped
    product = (port.product or "").lower()
    if port.vid in (0x2E8A,) or "rp2040" in product or "pico" in product or "usbmodem" in port.device:
        return "rp2040"
    return None


def port_preference(port):
    name = port.device or ""
    if name.startswith("/dev/cu.SLAB"):
        return 0
    if name.startswith("/dev/cu.usbserial"):
        return 1
    if name.startswith("/dev/cu."):
        return 2
    return 3


def filter_detectable_ports(ports):
    by_location = {}
    for port in ports:
        if port.vid is None or port.pid is None:
            continue
        location = parse_location(port.hwid) or port.device
        existing = by_location.get(location)
        if existing is None or port_preference(port) < port_preference(existing):
            by_location[location] = port
    return list(by_location.values())


def detect_roles(ports, prefer_cu, ports_map):
    groups = {}
    for port in ports:
        key = parse_location(port.hwid) or port.device
        groups.setdefault(key, []).append(port)

    roles = {}
    for group_ports in groups.values():
        canonical = canonical_device(group_ports, prefer_cu)
        location = parse_location(canonical.hwid)
        role = determine_role(canonical, location, ports_map)
        chip = None
        hwid = canonical.hwid
        if hwid:
            if "VID:PID=10C4:EA60" in hwid:
                chip = "CP2102"
            elif "VID:PID=1A86:7523" in hwid:
                chip = "CH340"
            elif "VID:PID=2E8A:000A" in hwid or "VID:PID=2E8A:0005" in hwid:
                chip = "RP2040"
            elif "CH340" in (getattr(canonical, 'description', '') or "") or "CH341" in (getattr(canonical, 'description', '') or ""):
                chip = "CH340"
            elif "Pico" in (getattr(canonical, 'description', '') or ""):
                chip = "RP2040"
        if role:
            roles.setdefault(role, {
                "device": canonical.device,
                "hwid": hwid,
                "location": location or "unknown",
                "chip": chip or "unknown",
                "description": getattr(canonical, 'description', '') or "",
            })
    return roles


def wait_for_new_ports(existing_devices, wait_port):
    deadline = time.monotonic() + wait_port
    while time.monotonic() < deadline:
        ports = list(list_ports.comports())
        new_ports = [p for p in ports if p.device not in existing_devices]
        if new_ports:
            return new_ports
        time.sleep(0.5)
    return []


def find_port_by_name(name, wait_port):
    deadline = time.monotonic() + wait_port
    while time.monotonic() < deadline:
        for port in list(list_ports.comports()):
            if port.device == name:
                return port
        time.sleep(0.5)
    return None


def read_lines(ser: Serial, timeout_s: float):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        yield raw, line


def non_printable_ratio(raw: bytes) -> float:
    if not raw:
        return 0.0
    bad = 0
    for byte in raw:
        if byte in (9, 10, 13):
            continue
        if 32 <= byte <= 126:
            continue
        bad += 1
    return bad / max(1, len(raw))


def update_junk_burst(raw: bytes, burst: int) -> int:
    if len(raw) >= BINARY_JUNK_MIN_BYTES and non_printable_ratio(raw) >= BINARY_JUNK_RATIO_THRESHOLD:
        burst += 1
        print(f"[warn] binary-junk line detected ({burst}/{BINARY_JUNK_BURST_LIMIT})")
        return burst
    return 0


def run_smoke(device, baud, timeout):
    print(f"[smoke] running on {device} (baud={baud})")
    try:
        with Serial(device, baud, timeout=0.2) as ser:
            time.sleep(0.15)
            ser.reset_input_buffer()
            handshake_hits = 0
            post_handshake = False
            handshake_deadline = time.time() + max(timeout * 2, 1.2)
            junk_burst = 0

            while time.time() < handshake_deadline and handshake_hits < 2:
                print(f"[tx] {PING_COMMAND}")
                ser.write((PING_COMMAND + "\n").encode("ascii"))
                for raw, line in read_lines(ser, max(0.7, timeout)):
                    if line:
                        print(f"[rx] {line}")
                    else:
                        print(f"[rx-bin] {raw[:24].hex()}")
                    junk_burst = update_junk_burst(raw, junk_burst)
                    if junk_burst >= BINARY_JUNK_BURST_LIMIT:
                        print("[fail] binary junk detected", file=sys.stderr)
                        return False
                    if line and FATAL_PATTERN.search(line):
                        print(f"[fail] fatal marker detected: {line}", file=sys.stderr)
                        return False
                    if line and REBOOT_PATTERN.search(line):
                        print(f"[fail] reboot marker detected: {line}", file=sys.stderr)
                        return False
                    if line and PING_OK_PATTERN.search(line):
                        handshake_hits += 1
                        print(f"[ok] handshake {handshake_hits}/2")
                        if handshake_hits >= 2:
                            post_handshake = True
                            break
                if handshake_hits < 2:
                    time.sleep(0.15)

            if not post_handshake:
                print(f"[fail] handshake incomplete ({handshake_hits}/2)", file=sys.stderr)
                return False

            stable_until = time.time() + 3.0
            while time.time() < stable_until:
                for raw, line in read_lines(ser, 0.35):
                    if line:
                        print(f"[rx] {line}")
                    else:
                        print(f"[rx-bin] {raw[:24].hex()}")
                    junk_burst = update_junk_burst(raw, junk_burst)
                    if junk_burst >= BINARY_JUNK_BURST_LIMIT:
                        print("[fail] binary junk detected after handshake", file=sys.stderr)
                        return False
                    if line and FATAL_PATTERN.search(line):
                        print(f"[fail] fatal marker after handshake: {line}", file=sys.stderr)
                        return False
                    if line and REBOOT_PATTERN.search(line):
                        print(f"[fail] reboot marker after handshake: {line}", file=sys.stderr)
                        return False
            print(f"[ok] {PING_COMMAND} stable")
            return True
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def run_monitor_smoke(device, baud, ready_timeout):
    print(f"[smoke] monitor-only on {device} (baud={baud})")
    try:
        with Serial(device, baud, timeout=0.2) as ser:
            time.sleep(0.15)
            ready_deadline = time.time() + ready_timeout
            junk_burst = 0
            ui_link_log = []
            verdict_connected = False
            verdict_screen_ok = False
            verdict_error = False
            verdict_debug = False
            log_path = f"ui_link_monitor_{int(time.time())}.log"
            while time.time() < ready_deadline:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="ignore").strip()
                ui_link_log.append(line)
                if line.startswith("[UI_LINK] STATUS: connected=1"):
                    verdict_connected = True
                if line.startswith("[UI_LINK] SCREEN:"):
                    verdict_screen_ok = True
                if line.startswith("[UI_LINK] ERROR:"):
                    verdict_error = True
                if line.startswith("[UI_LINK] DEBUG:"):
                    verdict_debug = True
                junk_burst = update_junk_burst(raw, junk_burst)
                if junk_burst >= BINARY_JUNK_BURST_LIMIT:
                    print("[fail] binary junk detected", file=sys.stderr)
                    return False
                if line and FATAL_PATTERN.search(line):
                    print(f"[fail] fatal marker detected: {line}", file=sys.stderr)
                    return False
                if line and REBOOT_PATTERN.search(line):
                    print(f"[fail] reboot marker detected: {line}", file=sys.stderr)
                    return False
            # Sauvegarde du log UI_LINK
            with open(log_path, "w", encoding="utf-8") as fp:
                for l in ui_link_log:
                    fp.write(l + "\n")
            # Validation automatique
            if not verdict_connected:
                print("[fail] UI_LINK_STATUS connected=1 absent", file=sys.stderr)
                return False
            if not verdict_screen_ok:
                print("[fail] UI_LINK SCREEN absent", file=sys.stderr)
                return False
            print(f"[ok] UI_LINK monitor stable (log: {log_path})")
            return True
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def run_wifi_debug(device, baud, duration_s, wifi_regex):
    print(f"[wifi-debug] listening on {device} (baud={baud}, duration={duration_s}s)")
    matches = []
    saw_reboot = False
    try:
        with Serial(device, baud, timeout=0.3) as ser:
            time.sleep(0.15)
            ser.reset_input_buffer()
            deadline = time.time() + max(1.0, duration_s)
            while time.time() < deadline:
                for raw, line in read_lines(ser, 0.35):
                    if line:
                        print(f"[rx] {line}")
                    else:
                        print(f"[rx-bin] {raw[:24].hex()}")
                    if line and wifi_regex.search(line):
                        matches.append(line)
                    if line and (FATAL_PATTERN.search(line) or REBOOT_PATTERN.search(line)):
                        saw_reboot = True
                        break
                if saw_reboot:
                    break
        print("[wifi-debug] === WIFI FILTER ===")
        print(f"[wifi-debug] regex={wifi_regex.pattern}")
        if matches:
            for line in matches:
                print(f"[wifi] {line}")
        else:
            print("[wifi] (no matches)")
        if saw_reboot:
            print("[fail] reboot or fatal marker detected during wifi debug", file=sys.stderr)
            return False
        return True
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def parse_args(args=None):
    parser = argparse.ArgumentParser(description="Run quick serial smoke with auto port detection.")
    parser.add_argument("--port", help="Explicit serial port to use (optional)")
    parser.add_argument("--wait-port", type=int, default=30, help="Seconds to wait for a port (default 30)")
    parser.add_argument("--role", choices=["auto", "all", "esp32", "esp8266", "esp8266_usb", "rp2040"], default="auto")
    parser.add_argument("--all", action="store_true", help="Run smoke on all detected roles (when not auto)")
    parser.add_argument("--baud", type=int, default=0, help="Serial baud override (role default when omitted)")
    parser.add_argument("--timeout", type=float, default=1.0, help="Per-command timeout (seconds)")
    parser.add_argument(
        "--allow-no-hardware",
        action="store_true",
        help="Exit cleanly when no hardware is detected instead of failing.",
    )
    parser.add_argument("--prefer-cu", action="store_true", default=platform.system() == "Darwin", help="Prefer /dev/cu.* on macOS")
    parser.add_argument("--outdir", default="", help="Evidence output directory")
    parser.add_argument("--no-evidence", action="store_true", help="Disable evidence output files")
    parser.add_argument("--wifi-debug", action="store_true", help="Run WiFi serial debug monitor")
    parser.add_argument("--wifi-debug-seconds", type=int, default=600, help="WiFi debug duration in seconds")
    parser.add_argument("--wifi-debug-regex", default="", help="Regex for WiFi debug filter")
    parsed = parser.parse_args(args)
    parsed.role = normalize_role(parsed.role)
    return parsed

def main() -> int:
    args = parse_args()
    allow_no_hardware = args.allow_no_hardware or os.environ.get("ZACUS_ALLOW_NO_HW") == "1"

    evidence = None
    if not args.no_evidence:
        outdir = args.outdir or os.environ.get("ZACUS_OUTDIR", "")
        evidence = init_evidence(outdir)
        write_git_info(evidence["git"])
        evidence["commands"].write_text("# Commands\n", encoding="utf-8")
        record_command(evidence["commands"], " ".join(sys.argv))
        write_meta_json(evidence["meta"], " ".join(sys.argv))

    def finalize(exit_code: int, notes: list[str]) -> int:
        result = "PASS" if exit_code == 0 else "FAIL"
        if evidence is not None:
            write_summary(evidence["summary"], result, notes)
        print(f"RESULT={result}")
        return exit_code

    ports_map = load_ports_map()
    detection = {}

    if args.port:
        port = find_port_by_name(args.port, args.wait_port)
        if port is None:
            print(f"[error] port {args.port} not found after waiting {args.wait_port}s", file=sys.stderr)
            return finalize(1, [f"Port {args.port} not found"])
        detection = detect_roles([port], args.prefer_cu, ports_map)
        if not detection and args.role == "auto":
            print("[error] failed to classify the explicit port", file=sys.stderr)
            return finalize(1, ["Failed to classify explicit port"])
        if args.role in ("esp32", "esp8266_usb", "rp2040") and args.role not in detection:
            location = parse_location(getattr(port, "hwid", "")) or "unknown"
            detection[args.role] = {
                "device": port.device,
                "hwid": getattr(port, "hwid", ""),
                "location": location,
            }
    else:
        baseline_ports = list(list_ports.comports())
        if baseline_ports:
            print("Using existing ports (already connected).")
            detection = detect_roles(
                filter_detectable_ports(baseline_ports), args.prefer_cu, ports_map
            )
            if not detection:
                reason = "failed to classify baseline ports"
                code = exit_no_hw(args.wait_port, allow_no_hardware, reason)
                return finalize(code, [reason])
        else:
            baseline_devices = {p.device for p in baseline_ports}
            new_ports = wait_for_new_ports(baseline_devices, args.wait_port)
            if not new_ports:
                reason = "no new serial port detected"
                code = exit_no_hw(args.wait_port, allow_no_hardware, reason)
                return finalize(code, [reason])
            detection = detect_roles(filter_detectable_ports(new_ports), args.prefer_cu, ports_map)
            if not detection:
                reason = "failed to classify detected ports"
                code = exit_no_hw(args.wait_port, allow_no_hardware, reason)
                return finalize(code, [reason])

    if args.role == "auto":
        run_roles = [role for role in ROLE_PRIORITY if role in detection]
    elif args.role == "all":
        run_roles = [role for role in ROLE_PRIORITY if role in detection]
        run_roles += sorted(role for role in detection if role not in ROLE_PRIORITY)
    else:
        run_roles = [args.role]
        if args.all:
            run_roles = sorted(set(run_roles + list(detection.keys())))

    targets = []
    for role in run_roles:
        info = detection.get(role)
        if info:
            targets.append({"role": role, **info})
        elif args.role != "auto":
            print(f"[warn] requested role {role} not detected", file=sys.stderr)
            if args.port:
                targets.append(
                    {
                        "role": role,
                        "device": args.port,
                        "hwid": "",
                        "location": "explicit",
                    }
                )

    if not targets:
        return finalize(1, ["No targets detected"])

    overall_ok = True
    wifi_regex = BASIC_WIFI_PATTERN
    if args.wifi_debug_regex:
        try:
            wifi_regex = re.compile(args.wifi_debug_regex, re.IGNORECASE)
        except re.error as exc:
            print(f"[error] invalid wifi regex: {exc}", file=sys.stderr)
            return finalize(1, ["Invalid wifi debug regex"])
    for entry in targets:
        role = entry["role"]
        device = entry["device"]
        location = entry["location"]
        baud = args.baud or (115200 if role in ("esp32", "esp8266_usb") else 19200)
        print(f"[detect] role={role} device={device} location={location}")
        if args.wifi_debug:
            ok = run_wifi_debug(device, baud, args.wifi_debug_seconds, wifi_regex)
        elif role == "esp8266_usb":
            try:
                ready_timeout = float(os.environ.get("ZACUS_READY_TIMEOUT", "7.0"))
            except ValueError:
                ready_timeout = 7.0
            ok = run_monitor_smoke(device, baud, ready_timeout)
        else:
            ok = run_smoke(device, baud, args.timeout)
        overall_ok = overall_ok and ok

    mode_note = "wifi-debug" if args.wifi_debug else "serial-smoke"
    return finalize(0 if overall_ok else 1, [f"Targets: {len(targets)}", f"Mode: {mode_note}"])


if __name__ == "__main__":
    raise SystemExit(main())
