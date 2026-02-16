#!/usr/bin/env python3
"""Drive Story V2 via serial and validate screen scene transitions."""

import argparse
import re
import sys
import time
from collections import OrderedDict

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

# Regex patterns to extract state from serial output
STATUS_RE = re.compile(r"\[STORY_V2\] STATUS .*?screen=([^\s]+)")
STEP_RE = re.compile(r"\[STORY_V2\] STATUS .*?step=([^\s]+)")
SCENE_SYNC_RE = re.compile(r"\[SCREEN_SYNC\] seq=(\d+)")
SCREEN_LINK_RE = re.compile(r"\[UI_LINK_STATUS\].*?connected=(\d)")


class ScreenTransitionLog:
    """Track screen changes over time."""
    def __init__(self):
        self.events = OrderedDict()  # timestamp -> (screen, step, seq)
        self.screens = set()
        self.steps = set()
        self.last_screen = None
        self.last_step = None
        self.transitions = []  # List of (from_screen, to_screen, step)

    def record(self, ts, screen, step, seq):
        if screen:
            self.screens.add(screen)
            if self.last_screen and self.last_screen != screen:
                self.transitions.append((self.last_screen, screen, step))
            self.last_screen = screen
        if step:
            self.steps.add(step)
            self.last_step = step
        self.events[ts] = (screen or "-", step or "-", seq or "")

    def summary(self):
        return {
            "scenes": sorted(self.screens),
            "steps": sorted(self.steps),
            "scene_count": len(self.screens),
            "step_count": len(self.steps),
            "transitions": self.transitions,
            "transition_count": len(self.transitions),
        }

    def print_log(self):
        print("\n[screen] Observed transitions:")
        if not self.transitions:
            print("  (none)")
        else:
            for from_s, to_s, step in self.transitions:
                print(f"  {from_s} -> {to_s} (step={step})")


def send_cmd(ser, cmd, label=""):
    """Send a serial command and wait briefly."""
    if label:
        print(f"  [cmd] {label}: {cmd}")
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.25)


def collect_and_parse(ser, duration_s, log, label=""):
    """Collect serial output and parse screen/step transitions."""
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

        # Extract screen and step from STORY_V2 STATUS
        screen = None
        step = None
        seq = None

        m_screen = STATUS_RE.search(line)
        if m_screen:
            screen = m_screen.group(1)

        m_step = STEP_RE.search(line)
        if m_step:
            step = m_step.group(1)

        m_seq = SCENE_SYNC_RE.search(line)
        if m_seq:
            seq = m_seq.group(1)

        # Record if we found state info
        if screen or step or seq:
            log.record(ts, screen, step, seq)

        # Print key lines
        if "[STORY_V2] STATUS" in line or "[SCREEN_SYNC]" in line or "[UI_LINK_STATUS]" in line:
            prefix = "  [rx]" if label else "[rx]"
            print(f"{prefix} {line[:100]}")

    return line_count


def main():
    parser = argparse.ArgumentParser(description="Story V2 screen scene validation.")
    parser.add_argument("--port", required=True, help="Serial port (ESP32)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--min-scenes", type=int, default=2, help="Minimum distinct scenes")
    parser.add_argument("--min-transitions", type=int, default=1, help="Minimum scene transitions")
    args = parser.parse_args()

    log = ScreenTransitionLog()
    deadline_total = time.time() + 20.0

    try:
        with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
            time.sleep(1.5)
            ser.reset_input_buffer()

            # 1. Verify UI link is up
            print("[test] Verifying UI link (3 sec wait for stabilization)...")
            time.sleep(2.0)  # Let UI link stabilize
            
            send_cmd(ser, "UI_LINK_STATUS", "check link")
            linked = False
            responses = []
            check_until = time.time() + 5.0
            
            while time.time() < check_until:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                responses.append(line)
                
                if "[UI_LINK_STATUS]" in line:
                    # Look for connected=1 anywhere in the line
                    if "connected=1" in line:
                        linked = True
                        print(f"  [ok] UI link connected")
                        break
                    elif "connected=" in line:
                        print(f"  [warn] UI link status: {line[:100]}")
            
            if not linked:
                print(f"[fail] UI link not connected after {len(responses)} lines", file=sys.stderr)
                if responses:
                    print(f"  Last responses: {responses[-3:]}", file=sys.stderr)
                return 3

            # 2. Enable Story V2 and test mode
            print("[test] Enabling Story V2 test mode...")
            send_cmd(ser, "STORY_V2_ENABLE ON", "enable v2")
            send_cmd(ser, "STORY_TEST_ON", "test mode ON")
            send_cmd(ser, "STORY_TEST_DELAY 1000", "test delay")
            time.sleep(0.3)

            # 3. Initial status snapshot
            print("[test] Collecting initial state...")
            send_cmd(ser, "STORY_V2_STATUS", "status")
            collect_and_parse(ser, 2.0, log, "phase=init")

            # 4. Arm and drive transitions
            print("[test] Arming story...")
            send_cmd(ser, "STORY_ARM", "arm")
            collect_and_parse(ser, 2.0, log, "phase=armed")

            print("[test] Forcing ETAPE2 transition...")
            send_cmd(ser, "STORY_FORCE_ETAPE2", "force etape2")
            collect_and_parse(ser, 3.0, log, "phase=etape2")

            # 5. Final status
            print("[test] Final status...")
            send_cmd(ser, "STORY_V2_STATUS", "final")
            collect_and_parse(ser, 2.0, log, "phase=final")

            send_cmd(ser, "STORY_TEST_OFF", "test mode OFF")

    except Exception as exc:
        print(f"[error] serial failure: {exc}", file=sys.stderr)
        return 2

    # Analyze results
    summary = log.summary()
    print("\n[summary]")
    print(f"  Scenes observed: {summary['scene_count']} -> {summary['scenes']}")
    print(f"  Steps observed: {summary['step_count']} -> {summary['steps']}")
    print(f"  Transitions: {summary['transition_count']}")
    log.print_log()

    # Validation
    fail_reasons = []
    if summary['scene_count'] < args.min_scenes:
        fail_reasons.append(f"scenes={summary['scene_count']} < min={args.min_scenes}")
    if summary['transition_count'] < args.min_transitions:
        fail_reasons.append(f"transitions={summary['transition_count']} < min={args.min_transitions}")
    if not summary['steps']:
        fail_reasons.append("no steps recorded")

    if fail_reasons:
        print(f"\n[fail] {'; '.join(fail_reasons)}", file=sys.stderr)
        return 4

    print(f"\n[ok] screen validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
