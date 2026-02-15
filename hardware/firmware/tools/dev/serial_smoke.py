#!/usr/bin/env python3
"""Quick USB serial smoke for ESP32/ESP8266/RP2040 with LOCATION-based ROLE detection."""

import argparse
import json
import os
import platform
import re
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
READY_PATTERN = re.compile(r"\[SCREEN\]\s*Ready\.", re.IGNORECASE)
FATAL_PATTERN = re.compile(
    r"(User exception|Exception|panic|abort|assert|rst cause|stack smashing|Guru Meditation|Fatal)",
    re.IGNORECASE,
)
REBOOT_PATTERN = re.compile(r"(ets Jan|rst:|boot mode|load:0x|entry 0x)", re.IGNORECASE)

ROOT = Path(__file__).resolve().parents[2]
PORTS_MAP_PATH = ROOT / "tools" / "dev" / "ports_map.json"
DEFAULT_PORTS_MAP = {
    "location": {
        "20-6.1.1": "esp32",
        "20-6.1.2": "esp8266_usb",
    },
    "vidpid": {
        "2e8a:0005": "rp2040",
        "2e8a:000a": "rp2040",
    },
}
ROLE_PRIORITY = ["esp32", "esp8266_usb", "rp2040"]


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
    if not PORTS_MAP_PATH.exists():
        PORTS_MAP_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(PORTS_MAP_PATH, "w", encoding="utf-8") as fp:
            json.dump(DEFAULT_PORTS_MAP, fp, indent=2)
    with open(PORTS_MAP_PATH, encoding="utf-8") as fp:
        raw_map = json.load(fp)
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
    location_role = ports_map.get("location", {}).get((location or "").lower())
    if location_role:
        return location_role
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
        if role:
            roles.setdefault(role, {
                "device": canonical.device,
                "hwid": canonical.hwid,
                "location": location or "unknown",
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
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if line:
            yield line


def run_smoke(device, baud, timeout):
    print(f"[smoke] running on {device} (baud={baud})")
    try:
        with Serial(device, baud, timeout=0.2) as ser:
            time.sleep(0.15)
            ser.reset_input_buffer()
            handshake_hits = 0
            post_handshake = False
            handshake_deadline = time.time() + max(timeout * 2, 1.2)

            while time.time() < handshake_deadline and handshake_hits < 2:
                print(f"[tx] {PING_COMMAND}")
                ser.write((PING_COMMAND + "\n").encode("ascii"))
                for line in read_lines(ser, max(0.7, timeout)):
                    print(f"[rx] {line}")
                    if FATAL_PATTERN.search(line):
                        print(f"[fail] fatal marker detected: {line}", file=sys.stderr)
                        return False
                    if REBOOT_PATTERN.search(line):
                        # Boot noise can happen before a stable handshake.
                        continue
                    if PING_OK_PATTERN.search(line):
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
                for line in read_lines(ser, 0.35):
                    print(f"[rx] {line}")
                    if FATAL_PATTERN.search(line):
                        print(f"[fail] fatal marker after handshake: {line}", file=sys.stderr)
                        return False
                    if REBOOT_PATTERN.search(line):
                        print(f"[fail] reboot marker after handshake: {line}", file=sys.stderr)
                        return False
            print(f"[ok] {PING_COMMAND} stable")
            return True
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def run_monitor_smoke(device, baud):
    print(f"[smoke] monitor-only on {device} (baud={baud})")
    try:
        with Serial(device, baud, timeout=0.2) as ser:
            time.sleep(0.15)
            ser.reset_input_buffer()
            ready_deadline = time.time() + 4.0
            saw_ready = False
            while time.time() < ready_deadline:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                print(f"[rx] {line}")
                if FATAL_PATTERN.search(line):
                    print(f"[fail] fatal marker detected: {line}", file=sys.stderr)
                    return False
                if REBOOT_PATTERN.search(line):
                    print(f"[fail] reboot marker detected: {line}", file=sys.stderr)
                    return False
                if READY_PATTERN.search(line):
                    saw_ready = True
                    break
            if not saw_ready:
                print("[fail] ready marker missing", file=sys.stderr)
                return False

            stable_until = time.time() + 3.0
            while time.time() < stable_until:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                print(f"[rx] {line}")
                if FATAL_PATTERN.search(line):
                    print(f"[fail] fatal marker after ready: {line}", file=sys.stderr)
                    return False
                if REBOOT_PATTERN.search(line):
                    print(f"[fail] reboot marker after ready: {line}", file=sys.stderr)
                    return False
            print("[ok] monitor stable")
            return True
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def main() -> int:
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
    args = parser.parse_args()
    args.role = normalize_role(args.role)
    allow_no_hardware = args.allow_no_hardware or os.environ.get("ZACUS_ALLOW_NO_HW") == "1"

    ports_map = load_ports_map()
    detection = {}

    if args.port:
        port = find_port_by_name(args.port, args.wait_port)
        if port is None:
            print(f"[error] port {args.port} not found after waiting {args.wait_port}s", file=sys.stderr)
            return 1
        detection = detect_roles([port], args.prefer_cu, ports_map)
        if not detection and args.role == "auto":
            print("[error] failed to classify the explicit port", file=sys.stderr)
            return 1
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
                return exit_no_hw(
                    args.wait_port, allow_no_hardware, "failed to classify baseline ports"
                )
        else:
            baseline_devices = {p.device for p in baseline_ports}
            new_ports = wait_for_new_ports(baseline_devices, args.wait_port)
            if not new_ports:
                return exit_no_hw(
                    args.wait_port, allow_no_hardware, "no new serial port detected"
                )
            detection = detect_roles(filter_detectable_ports(new_ports), args.prefer_cu, ports_map)
            if not detection:
                return exit_no_hw(
                    args.wait_port, allow_no_hardware, "failed to classify detected ports"
                )

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
        return 1

    overall_ok = True
    for entry in targets:
        role = entry["role"]
        device = entry["device"]
        location = entry["location"]
        baud = args.baud or (115200 if role in ("esp32", "esp8266_usb") else 19200)
        print(f"[detect] role={role} device={device} location={location}")
        if role == "esp8266_usb":
            ok = run_monitor_smoke(device, baud)
        else:
            ok = run_smoke(device, baud, args.timeout)
        overall_ok = overall_ok and ok

    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
