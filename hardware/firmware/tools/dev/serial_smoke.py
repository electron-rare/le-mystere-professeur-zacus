#!/usr/bin/env python3
"""Quick USB serial smoke for ESP32/ESP8266/RP2040 with LOCATION-based ROLE detection."""

import argparse
import json
import os
import platform
import re
import sys
import time

try:
    from serial import Serial
    from serial.tools import list_ports
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

CHECKS = [
    ("BOOT_STATUS", re.compile(r"BOOT|U_LOCK|STATUS", re.IGNORECASE)),
    ("STORY_STATUS", re.compile(r"STORY", re.IGNORECASE)),
    ("MP3_STATUS", re.compile(r"MP3", re.IGNORECASE)),
]

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORTS_MAP_PATH = os.path.join(ROOT, "tools", "dev", "ports_map.json")
DEFAULT_PORTS_MAP = {
    "macos": {
        "20-6.1.1": {"role": "esp32"},
        "20-6.1.2": {"role": "esp8266"},
    }
}
ROLE_PRIORITY = ["esp32", "esp8266", "rp2040"]


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
    if not os.path.exists(PORTS_MAP_PATH):
        os.makedirs(os.path.dirname(PORTS_MAP_PATH), exist_ok=True)
        with open(PORTS_MAP_PATH, "w", encoding="utf-8") as fp:
            json.dump(DEFAULT_PORTS_MAP, fp, indent=2)
    with open(PORTS_MAP_PATH, encoding="utf-8") as fp:
        return json.load(fp)


def parse_location(hwid: str):
    if not hwid:
        return None
    match = re.search(r"LOCATION=([\w\-.]+)", hwid)
    if match:
        return match.group(1).lower()
    return None


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
    product = (port.product or "").lower()
    if port.vid in (0x2E8A,) or "rp2040" in product or "pico" in product or "usbmodem" in port.device:
        return "rp2040"
    if port.vid == 0x10C4 and port.pid == 0xEA60:
        entry = ports_map.get("macos", {}).get(location or "")
        if entry:
            return entry.get("role")
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


def read_until_match(ser: Serial, pattern: re.Pattern, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(f"[rx] {line}")
        if pattern.search(line):
            return True
    return False


def run_smoke(device, baud, timeout):
    print(f"[smoke] running on {device} (baud={baud})")
    try:
        with Serial(device, baud, timeout=0.2) as ser:
            time.sleep(0.2)
            ser.reset_input_buffer()
            ok = True
            for command, pattern in CHECKS:
                print(f"[tx] {command}")
                ser.write((command + "\n").encode("ascii"))
                if not read_until_match(ser, pattern, timeout):
                    print(f"[fail] no expected response for {command}", file=sys.stderr)
                    ok = False
                else:
                    print(f"[ok] {command}")
            return ok
    except Exception as exc:
        print(f"[error] serial failure on {device}: {exc}", file=sys.stderr)
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Run quick serial smoke with auto port detection.")
    parser.add_argument("--port", help="Explicit serial port to use (optional)")
    parser.add_argument("--wait-port", type=int, default=180, help="Seconds to wait for a port (default 180)")
    parser.add_argument("--role", choices=["auto", "esp32", "esp8266", "rp2040"], default="auto")
    parser.add_argument("--all", action="store_true", help="Run smoke on all detected roles (when not auto)")
    parser.add_argument("--baud", type=int, default=19200, help="Serial baud (default 19200)")
    parser.add_argument("--timeout", type=float, default=2.0, help="Per-command timeout (seconds)")
    parser.add_argument(
        "--allow-no-hardware",
        action="store_true",
        help="Exit cleanly when no hardware is detected instead of failing.",
    )
    parser.add_argument("--prefer-cu", action="store_true", default=platform.system() == "Darwin", help="Prefer /dev/cu.* on macOS")
    args = parser.parse_args()
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

    if not targets:
        return 1

    overall_ok = True
    for entry in targets:
        role = entry["role"]
        device = entry["device"]
        location = entry["location"]
        print(f"[detect] role={role} device={device} location={location}")
        ok = run_smoke(device, args.baud, args.timeout)
        overall_ok = overall_ok and ok

    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
