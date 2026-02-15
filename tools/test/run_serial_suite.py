#!/usr/bin/env python3
"""Run serial command suites against Zacus firmware with minimal dependencies."""

from __future__ import annotations

import argparse
import importlib.util
import json
import platform
import re
import sys
import time
from pathlib import Path
from typing import Any


ROLE_FALLBACK_PRIORITY = ("esp32", "esp8266", "rp2040")


def load_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except FileNotFoundError:
        print(f"[fail] suite file not found: {path}", file=sys.stderr)
        raise SystemExit(2)
    except json.JSONDecodeError as exc:
        print(f"[fail] invalid JSON in {path}: {exc}", file=sys.stderr)
        raise SystemExit(2)

    if not isinstance(data, dict):
        print(f"[fail] invalid suite format in {path}", file=sys.stderr)
        raise SystemExit(2)
    return data


def print_suites(suites: dict[str, Any]) -> None:
    print("Available suites:")
    for name in sorted(suites):
        description = ""
        value = suites.get(name, {})
        if isinstance(value, dict):
            description = str(value.get("description", "")).strip()
        suffix = f" - {description}" if description else ""
        print(f"- {name}{suffix}")


def load_serial_smoke_module(repo_root: Path):
    module_path = repo_root / "hardware" / "firmware" / "tools" / "dev" / "serial_smoke.py"
    if not module_path.exists():
        print(f"[fail] missing serial smoke module: {module_path}", file=sys.stderr)
        raise SystemExit(2)

    spec = importlib.util.spec_from_file_location("zacus_serial_smoke", module_path)
    if spec is None or spec.loader is None:
        print("[fail] unable to load serial smoke module", file=sys.stderr)
        raise SystemExit(2)

    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except SystemExit as exc:
        print("[fail] serial smoke module initialization failed", file=sys.stderr)
        raise SystemExit(int(exc.code) if isinstance(exc.code, int) else 2)
    return module


def ensure_pyserial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("[fail] missing dependency: pip install pyserial", file=sys.stderr)
        raise SystemExit(3)
    return serial, list_ports


def pick_target_role(detection: dict[str, Any], smoke_module) -> dict[str, str] | None:
    priority = tuple(getattr(smoke_module, "ROLE_PRIORITY", ROLE_FALLBACK_PRIORITY))
    for role in priority:
        if role in detection:
            info = detection[role]
            return {
                "role": role,
                "device": str(info.get("device", "")),
                "hwid": str(info.get("hwid", "")),
                "location": str(info.get("location", "unknown")),
            }
    for role in sorted(detection):
        info = detection[role]
        return {
            "role": role,
            "device": str(info.get("device", "")),
            "hwid": str(info.get("hwid", "")),
            "location": str(info.get("location", "unknown")),
        }
    return None


def select_target(args, smoke_module, list_ports_module) -> tuple[dict[str, str] | None, str | None]:
    prefer_cu = platform.system() == "Darwin"
    ports_map = smoke_module.load_ports_map()

    if args.port:
        found = smoke_module.find_port_by_name(args.port, args.wait_port)
        if found is None:
            return None, f"explicit port not found after {args.wait_port}s: {args.port}"

        detection = smoke_module.detect_roles([found], prefer_cu, ports_map)
        if args.role == "auto":
            target = pick_target_role(detection, smoke_module)
            if target is not None:
                return target, None
            return None, "failed to classify explicit port"

        if args.role in detection:
            info = detection[args.role]
            return {
                "role": args.role,
                "device": str(info.get("device", found.device)),
                "hwid": str(info.get("hwid", getattr(found, "hwid", ""))),
                "location": str(info.get("location", "unknown")),
            }, None

        location = smoke_module.parse_location(getattr(found, "hwid", "")) or "unknown"
        return {
            "role": args.role,
            "device": str(found.device),
            "hwid": str(getattr(found, "hwid", "")),
            "location": location,
        }, None

    baseline_ports = list(list_ports_module.comports())
    detection: dict[str, Any]
    if baseline_ports:
        print("Using existing ports (already connected).")
        filtered = smoke_module.filter_detectable_ports(baseline_ports)
        detection = smoke_module.detect_roles(filtered, prefer_cu, ports_map)
    else:
        new_ports = smoke_module.wait_for_new_ports(set(), args.wait_port)
        if not new_ports:
            return None, f"no serial port detected after waiting {args.wait_port}s"
        detection = smoke_module.detect_roles(smoke_module.filter_detectable_ports(new_ports), prefer_cu, ports_map)

    if not detection:
        return None, "failed to classify detected ports"

    if args.role == "auto":
        target = pick_target_role(detection, smoke_module)
        if target is None:
            return None, "no suitable role detected"
        return target, None

    info = detection.get(args.role)
    if info is None:
        return None, f"requested role not detected: {args.role}"

    return {
        "role": args.role,
        "device": str(info.get("device", "")),
        "hwid": str(info.get("hwid", "")),
        "location": str(info.get("location", "unknown")),
    }, None


def compile_patterns(raw_patterns: list[str]) -> list[tuple[str, re.Pattern[str]]]:
    compiled: list[tuple[str, re.Pattern[str]]] = []
    for raw in raw_patterns:
        compiled.append((raw, re.compile(raw, re.IGNORECASE)))
    return compiled


def parse_timeout(value: Any, fallback: float) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        return fallback
    if parsed <= 0:
        return fallback
    return parsed


def run_test(ser, test: dict[str, Any], default_timeout: float) -> bool:
    name = str(test.get("name", "unnamed"))
    command = str(test.get("command", "")).strip()
    timeout = parse_timeout(test.get("timeout"), default_timeout)

    if not command:
        print(f"[fail] {name} (empty command)")
        return False

    expect = compile_patterns(list(test.get("expect", [])))
    expect_any = compile_patterns(list(test.get("expect_any", [])))

    expect_hits = {raw: False for raw, _ in expect}
    expect_any_hit = len(expect_any) == 0

    print(f"[tx] {command}")
    try:
        ser.reset_input_buffer()
    except Exception:
        pass
    ser.write((command + "\n").encode("ascii", errors="ignore"))

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(f"[rx] {line}")

        for source, pattern in expect:
            if not expect_hits[source] and pattern.search(line):
                expect_hits[source] = True

        if not expect_any_hit:
            for _, pattern in expect_any:
                if pattern.search(line):
                    expect_any_hit = True
                    break

        if all(expect_hits.values()) and expect_any_hit:
            break

    passed = all(expect_hits.values()) and expect_any_hit
    if passed:
        print(f"[pass] {name}")
        return True

    print(f"[fail] {name}")
    missing_expect = [source for source, hit in expect_hits.items() if not hit]
    if missing_expect:
        print(f"[fail] missing expect patterns: {missing_expect}")
    if not expect_any_hit and expect_any:
        print(f"[fail] missing expect_any match: {[source for source, _ in expect_any]}")

    tip = str(test.get("tip", "")).strip()
    if tip:
        print(f"[tip] {tip}")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Run serial command suites for Zacus firmware.")
    parser.add_argument("--port", help="Explicit serial port (optional)")
    parser.add_argument("--role", choices=["auto", "esp32", "esp8266", "rp2040"], default="auto")
    parser.add_argument("--suite", help="Suite name to run")
    parser.add_argument("--baud", type=int, default=None, help="Serial baud (default from suite config)")
    parser.add_argument("--timeout", type=float, default=None, help="Default per-test timeout in seconds")
    parser.add_argument("--wait-port", type=int, default=None, help="Seconds to wait for serial port detection")
    parser.add_argument("--read-timeout", type=float, default=None, help="Serial read timeout per line")
    parser.add_argument("--allow-no-hardware", action="store_true", help="Return SKIP (exit 0) if hardware is missing")
    parser.add_argument("--list-suites", action="store_true", help="List available suite names and exit")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    suites_path = Path(__file__).resolve().with_name("serial_suites.json")
    config = load_json(suites_path)
    suites = config.get("suites", {})
    if not isinstance(suites, dict):
        print("[fail] invalid suites section", file=sys.stderr)
        return 2

    if args.list_suites:
        print_suites(suites)
        return 0

    if not args.suite:
        print("[fail] --suite is required unless --list-suites", file=sys.stderr)
        return 2

    suite = suites.get(args.suite)
    if not isinstance(suite, dict):
        print(f"[fail] unknown suite: {args.suite}", file=sys.stderr)
        return 2

    tests = suite.get("tests", [])
    if not isinstance(tests, list) or not tests:
        print(f"[fail] suite has no tests: {args.suite}", file=sys.stderr)
        return 2

    defaults = config.get("defaults", {}) if isinstance(config.get("defaults", {}), dict) else {}
    baud = args.baud if args.baud is not None else int(defaults.get("baud", 19200))
    wait_port = args.wait_port if args.wait_port is not None else int(defaults.get("wait_port", 3))
    read_timeout = args.read_timeout if args.read_timeout is not None else float(defaults.get("read_timeout", 0.2))
    default_timeout = args.timeout if args.timeout is not None else float(defaults.get("timeout", 1.5))

    args.wait_port = wait_port

    serial_module, list_ports_module = ensure_pyserial()
    smoke_module = load_serial_smoke_module(repo_root)

    target, reason = select_target(args, smoke_module, list_ports_module)
    if target is None:
        if args.allow_no_hardware:
            print(f"SKIP: {reason}")
            return 0
        print(f"[fail] {reason}", file=sys.stderr)
        return 2

    device = target.get("device", "")
    role = target.get("role", "unknown")
    location = target.get("location", "unknown")
    print(f"[detect] suite={args.suite} role={role} device={device} location={location}")

    passed = 0
    failed = 0
    try:
        with serial_module.Serial(device, baud, timeout=read_timeout) as ser:
            time.sleep(0.2)
            for raw_test in tests:
                if not isinstance(raw_test, dict):
                    print("[fail] invalid test entry (not an object)")
                    failed += 1
                    continue
                if run_test(ser, raw_test, default_timeout):
                    passed += 1
                else:
                    failed += 1
    except Exception as exc:
        print(f"[fail] serial session error on {device}: {exc}", file=sys.stderr)
        return 1

    total = passed + failed
    print("\n=== Suite summary ===")
    print(f"Suite : {args.suite}")
    print(f"Device: {device} ({role})")
    print(f"Result: passed={passed} failed={failed} total={total}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
