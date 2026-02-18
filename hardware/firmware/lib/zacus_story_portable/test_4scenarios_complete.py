#!/usr/bin/env python3
"""
Test harness: 4 scenarios + screen validation + disconnect/reconnect + 4h stability
Extends story_screen_smoke.py with multi-scenario testing
"""

import sys
import time
import argparse
from pathlib import Path
from datetime import datetime, timedelta
from collections import OrderedDict
import re

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

def timestamped(msg):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"[{ts}] {msg}")

def open_serial(port, baud):
    """Open serial port"""
    try:
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(0.5)
        ser.flushInput()
        ser.flushOutput()
        return ser
    except Exception as e:
        timestamped(f"✗ Cannot open {port}: {e}")
        return None

def send_cmd(ser, cmd, label=""):
    """Send a serial command"""
    if label:
        timestamped(f"  → {label}: {cmd}")
    try:
        ser.write((cmd + "\n").encode("utf-8"))
        ser.flush()
        time.sleep(0.25)
    except Exception as e:
        timestamped(f"  ✗ Send failed: {e}")

def collect_responses(ser, duration_s=2.0):
    """Collect serial responses for duration_s"""
    responses = []
    deadline = time.time() + duration_s
    while time.time() < deadline:
        try:
            raw = ser.readline()
            if raw:
                line = raw.decode("utf-8", errors="ignore").strip()
                if line:
                    responses.append(line)
        except:
            pass
    return responses

class ScreenTransitionLog:
    """Track screen scene transitions"""
    def __init__(self):
        self.events = OrderedDict()
        self.scenes = set()
        self.steps = set()
        self.transitions = []

    def parse_status(self, line):
        """Extract screen, step from STORY_V2 STATUS line"""
        match = re.search(r'screen=(\w+)', line)
        screen = match.group(1) if match else None
        match = re.search(r'step=(\w+)', line)
        step = match.group(1) if match else None
        return screen, step

    def add_event(self, seq, screen, step):
        """Record an event with scene/step"""
        if screen and step:
            key = (screen, step)
            if key not in self.events:
                self.events[key] = seq
                self.scenes.add(screen)
                self.steps.add(step)
                # Check transition
                if len(self.events) > 1:
                    prev_key = list(self.events.keys())[-2]
                    self.transitions.append((prev_key[0], screen, step))

    def summary(self):
        """Return test summary"""
        return {
            'scenes': sorted(list(self.scenes)),
            'steps': sorted(list(self.steps)),
            'transitions': self.transitions,
            'pass': len(self.transitions) > 0
        }

def main():
    parser = argparse.ArgumentParser(description='Test 4 scenarios + stability')
    parser.add_argument('--port', required=True, help='Serial port (e.g., /dev/cu.SLAB_USBtoUART7)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--mode', choices=['quick', 'disconnect', '4h'], default='quick',
                        help='Test mode: quick (4 scenarios), disconnect (+ reconnect test), 4h (stability)')
    args = parser.parse_args()

    # Verify UI link first
    if not verify_ui_link(args.port, args.baud):
        sys.exit(1)

    scenarios = [
        ('DEFAULT', 'default_unlock_win_etape2'),
        ('EXPRESS', 'example_unlock_express'),
        ('EXPRESS_DONE', 'example_unlock_express_done'),
        ('SPECTRE', 'spectre_radio_lab'),
    ]

    passes = 0
    fails = 0

    # Test all 4 scenarios
    for scenario_id, scenario_name in scenarios:
        if test_scenario(args.port, args.baud, scenario_id, scenario_name):
            passes += 1
        else:
            fails += 1

    # Disconnect/reconnect test
    if args.mode in ['disconnect', '4h']:
        if test_disconnect_reconnect(args.port, args.baud):
            passes += 1
        else:
            fails += 1

    # 4-hour stability test
    if args.mode == '4h':
        if test_stability_4h(args.port, args.baud):
            passes += 1
        else:
            fails += 1

    # Final summary
    timestamped(f"\n{'='*70}")
    timestamped(f"FINAL SUMMARY")
    timestamped(f"{'='*70}")
    timestamped(f"Mode: {args.mode}")
    timestamped(f"Total tests: {passes + fails}")
    timestamped(f"Passed: {passes}")
    timestamped(f"Failed: {fails}")
    timestamped(f"Success rate: {100*passes/(passes+fails) if (passes+fails) > 0 else 0:.1f}%")
    timestamped(f"{'='*70}\n")

    sys.exit(0 if fails == 0 else 1)

if __name__ == '__main__':
    main()
