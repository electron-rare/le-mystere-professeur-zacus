#!/usr/bin/env python3
"""Drive Story V3 via serial JSON-lines and validate screen sync activity."""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from collections import OrderedDict

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

SCENE_SYNC_RE = re.compile(r"\[SCREEN_SYNC\] seq=(\d+)")


class ScreenTransitionLog:
    def __init__(self) -> None:
        self.events = OrderedDict()
        self.steps = set()
        self.sync_seq = []
        self.last_seq = None
        self.sync_transitions = 0

    def record(self, ts: float, step: str | None, seq: int | None) -> None:
        if step:
            self.steps.add(step)
        if seq is not None:
            self.sync_seq.append(seq)
            if self.last_seq is not None and seq != self.last_seq:
                self.sync_transitions += 1
            self.last_seq = seq
        self.events[ts] = (step or "-", "" if seq is None else str(seq))

    def summary(self) -> dict[str, object]:
        return {
            "steps": sorted(self.steps),
            "step_count": len(self.steps),
            "sync_samples": len(self.sync_seq),
            "sync_transitions": self.sync_transitions,
        }


def send_cmd(ser, cmd: str, label: str = "") -> None:
    if label:
        print(f"  [cmd] {label}: {cmd}")
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.25)


def send_json_cmd(ser, cmd: str, data: dict[str, object] | None = None, label: str = "") -> None:
    payload = {"cmd": cmd}
    if data:
        payload["data"] = data
    send_cmd(ser, json.dumps(payload, separators=(",", ":")), label or cmd)


def parse_json_line(line: str) -> tuple[str | None, str | None]:
    if not line.startswith("{"):
        return None, None
    try:
        payload = json.loads(line)
    except json.JSONDecodeError:
        return None, None
    if not isinstance(payload, dict):
        return None, None
    data = payload.get("data")
    if not isinstance(data, dict):
        return None, None
    step = data.get("step")
    scenario = data.get("scenario")
    if isinstance(step, str) and isinstance(scenario, str):
        return scenario, step
    if isinstance(step, str):
        return None, step
    return None, None


def collect_and_parse(ser, duration_s: float, log: ScreenTransitionLog, label: str = "") -> int:
    deadline = time.time() + duration_s
    start_time = time.time()
    line_count = 0
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue

        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        line_count += 1

        ts = time.time() - start_time
        seq = None
        step = None

        match = SCENE_SYNC_RE.search(line)
        if match:
            seq = int(match.group(1))

        _, json_step = parse_json_line(line)
        if json_step:
            step = json_step

        if seq is not None or step is not None:
            log.record(ts, step, seq)

        if "[SCREEN_SYNC]" in line or line.startswith("{"):
            prefix = "  [rx]" if label else "[rx]"
            print(f"{prefix} {line[:120]}")
    return line_count


def verify_ui_link(ser) -> bool:
    print("[test] Verifying UI link (stabilization 2s)...")
    time.sleep(2.0)
    send_cmd(ser, "UI_LINK_STATUS", "check link")
    deadline = time.time() + 5.0
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        if "[UI_LINK_STATUS]" in line:
            print(f"  [rx] {line[:120]}")
            if "connected=1" in line:
                print("  [ok] UI link connected")
                return True
    print("[fail] UI link not connected", file=sys.stderr)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Story V3 screen sync validation.")
    parser.add_argument("--port", required=True, help="Serial port (ESP32)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--min-steps", type=int, default=1, help="Minimum distinct steps")
    parser.add_argument("--min-sync-transitions", type=int, default=1, help="Minimum SCREEN_SYNC seq transitions")
    args = parser.parse_args()

    log = ScreenTransitionLog()
    try:
        with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
            time.sleep(1.2)
            ser.reset_input_buffer()

            if not verify_ui_link(ser):
                return 3

            print("[test] Enabling story test mode...")
            send_cmd(ser, "STORY_TEST_ON", "test mode ON")
            send_cmd(ser, "STORY_TEST_DELAY 1000", "test delay")

            print("[test] Initial story status...")
            send_json_cmd(ser, "story.status", label="story.status")
            collect_and_parse(ser, 2.0, log, "phase=init")

            print("[test] Arming story...")
            send_cmd(ser, "STORY_ARM", "arm")
            send_json_cmd(ser, "story.status", label="story.status")
            collect_and_parse(ser, 2.0, log, "phase=armed")

            print("[test] Forcing ETAPE2 transition...")
            send_cmd(ser, "STORY_FORCE_ETAPE2", "force etape2")
            send_json_cmd(ser, "story.status", label="story.status")
            collect_and_parse(ser, 3.0, log, "phase=etape2")

            print("[test] Final status...")
            send_json_cmd(ser, "story.status", label="story.status")
            send_json_cmd(ser, "story.validate", label="story.validate")
            collect_and_parse(ser, 2.0, log, "phase=final")

            send_cmd(ser, "STORY_TEST_OFF", "test mode OFF")
    except Exception as exc:
        print(f"[error] serial failure: {exc}", file=sys.stderr)
        return 2

    summary = log.summary()
    print("\n[summary]")
    print(f"  Steps observed: {summary['step_count']} -> {summary['steps']}")
    print(f"  SCREEN_SYNC samples: {summary['sync_samples']}")
    print(f"  SCREEN_SYNC transitions: {summary['sync_transitions']}")

    fail_reasons = []
    if int(summary["step_count"]) < args.min_steps:
        fail_reasons.append(f"steps={summary['step_count']} < min={args.min_steps}")
    if int(summary["sync_transitions"]) < args.min_sync_transitions:
        fail_reasons.append(
            f"sync_transitions={summary['sync_transitions']} < min={args.min_sync_transitions}"
        )

    if fail_reasons:
        print(f"\n[fail] {'; '.join(fail_reasons)}", file=sys.stderr)
        return 4

    print("\n[ok] screen validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
