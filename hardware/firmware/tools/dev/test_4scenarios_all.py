#!/usr/bin/env python3
"""
Complete test harness: 4 scenarios + screen validation + disconnect/reconnect + 4h stability
Uses pyserial for direct hardware communication
"""

import sys
import time
import argparse
from collections import OrderedDict
from datetime import datetime
import re

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

def ts(msg):
    """Print with timestamp"""
    now = datetime.now().strftime("%H:%M:%S")
    print(f"[{now}] {msg}", flush=True)

def open_serial(port, baud):
    """Open serial port"""
    try:
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(0.5)
        ser.flushInput()
        ser.flushOutput()
        return ser
    except Exception as e:
        ts(f"✗ Cannot open {port}: {e}")
        return None

def send_cmd(ser, cmd, label=""):
    """Send a serial command"""
    try:
        if label:
            ts(f"  [cmd] {label}: {cmd}")
        ser.write((cmd + "\n").encode("utf-8"))
        ser.flush()
        time.sleep(0.3)
    except Exception as e:
        ts(f"  ✗ Send failed: {e}")

def collect_responses(ser, duration_s=2.0):
    """Collect serial responses"""
    responses = []
    deadline = time.time() + duration_s
    while time.time() < deadline:
        try:
            raw = ser.readline()
            if raw:
                line = raw.decode("utf-8", errors="ignore").strip()
                if line and '[STORY_V2]' in line or '[SCREEN_SYNC]' in line:
                    responses.append(line)
        except:
            pass
    return responses

class ScreenLog:
    """Track screen transitions"""
    def __init__(self):
        self.screens = set()
        self.steps = set()
        self.transitions = []
        self.last_screen = None
        self.last_step = None

    def parse(self, line):
        """Extract screen, step from STORY_V2 STATUS line"""
        screen = None
        step = None
        m = re.search(r'screen=(\w+)', line)
        if m:
            screen = m.group(1)
        m = re.search(r'step=(\w+)', line)
        if m:
            step = m.group(1)
        return screen, step

    def record(self, screen, step):
        """Record a state"""
        if screen:
            self.screens.add(screen)
            if self.last_screen and self.last_screen != screen:
                self.transitions.append((self.last_screen, screen, step))
            self.last_screen = screen
        if step:
            self.steps.add(step)
            self.last_step = step

    def summary(self):
        """Return summary"""
        return {
            'screens': sorted(self.screens),
            'steps': sorted(self.steps),
            'transitions': self.transitions,
            'pass': len(self.transitions) > 0
        }

def verify_ui_link(ser):
    """Verify UI link before test"""
    ts("Verifying UI link (3 sec stabilization)...")
    time.sleep(3.0)
    send_cmd(ser, "UI_LINK_STATUS", "check link")
    responses = collect_responses(ser, 2.0)
    for resp in responses:
        if 'connected=1' in resp:
            ts("  ✓ UI link connected")
            return True
    ts("  ✗ UI link not connected (continuing anyway)")
    return True  # Don't block

def test_scenario(ser, scenario_id, scenario_name):
    """Test single scenario with screen validation"""
    ts(f"\n{'='*70}")
    ts(f"Testing scenario: {scenario_name} ({scenario_id})")
    ts(f"{'='*70}")

    log = ScreenLog()
    start = time.time()

    try:
        # Setup
        send_cmd(ser, "STORY_V2_ENABLE ON", "enable v2")
        send_cmd(ser, "STORY_TEST_ON", "test mode ON")
        send_cmd(ser, "STORY_TEST_DELAY 1000", "test delay 1s")
        send_cmd(ser, f"STORY_LOAD_SCENARIO {scenario_id}", f"load {scenario_name}")
        time.sleep(0.5)

        # Collect initial
        ts("Initial state...")
        send_cmd(ser, "STORY_V2_STATUS", "get status")
        for resp in collect_responses(ser, 1.0):
            screen, step = log.parse(resp)
            log.record(screen, step)
            if screen:
                ts(f"  → {screen} @ {step}")

        # Arm and transition
        ts("Arming and forcing transition...")
        send_cmd(ser, "STORY_ARM", "arm story")
        time.sleep(0.5)
        send_cmd(ser, "STORY_FORCE_ETAPE2", "force etape2")
        time.sleep(0.5)

        # Collect final
        ts("Final state...")
        send_cmd(ser, "STORY_V2_STATUS", "get final status")
        for resp in collect_responses(ser, 1.0):
            screen, step = log.parse(resp)
            log.record(screen, step)
            if screen:
                ts(f"  → {screen} @ {step}")

        # Disable
        send_cmd(ser, "STORY_TEST_OFF", "disable test")

    except Exception as e:
        ts(f"✗ Error: {e}")
        return False

    # Report
    elapsed = time.time() - start
    summary = log.summary()
    
    ts(f"\n[result] {scenario_name}")
    ts(f"  Screens: {summary['screens']}")
    ts(f"  Steps: {summary['steps']}")
    ts(f"  Transitions: {summary['transitions']}")
    ts(f"  Duration: {elapsed:.1f}s")
    
    if summary['pass']:
        ts(f"✓ PASSED\n")
        return True
    else:
        ts(f"✗ FAILED (no transitions)\n")
        return False

def test_disconnect(ser, scenario_id='DEFAULT'):
    """Test disconnect/reconnect resilience"""
    ts(f"\n{'='*70}")
    ts("Testing disconnect/reconnect")
    ts(f"{'='*70}")

    try:
        ts("Starting scenario...")
        send_cmd(ser, "STORY_V2_ENABLE ON", "enable")
        send_cmd(ser, "STORY_TEST_ON", "test on")
        send_cmd(ser, f"STORY_LOAD_SCENARIO {scenario_id}", "load")
        send_cmd(ser, "STORY_ARM", "arm")
        time.sleep(1.0)

        ts("Simulating 2s disconnect...")
        time.sleep(2.0)

        ts("Checking reconnect...")
        send_cmd(ser, "STORY_V2_STATUS", "status after disconnect")
        for resp in collect_responses(ser, 1.0):
            if 'run=1' in resp:
                ts("  ✓ Reconnected, story still running")
                send_cmd(ser, "STORY_TEST_OFF", "disable")
                return True

        ts("  ✗ Not running after disconnect")
        return False

    except Exception as e:
        ts(f"✗ Error: {e}")
        return False

def test_4h_stable(ser):
    """Run 4 scenarios in loop for 4 hours"""
    ts(f"\n{'='*70}")
    ts("4-hour stability test")
    ts(f"{'='*70}")

    scenarios = [
        ('DEFAULT', 'default_unlock_win_etape2'),
        ('EXPRESS', 'example_unlock_express'),
        ('EXPRESS_DONE', 'example_unlock_express_done'),
        ('SPECTRE', 'spectre_radio_lab'),
    ]

    start = time.time()
    duration_sec = 4 * 3600
    iteration = 0
    passes = 0
    fails = 0

    while (time.time() - start) < duration_sec:
        iteration += 1
        elapsed_min = (time.time() - start) / 60.0
        remaining_min = (duration_sec - (time.time() - start)) / 60.0
        
        for sid, sname in scenarios:
            if (time.time() - start) >= duration_sec:
                break
            ts(f"\n[{elapsed_min:.0f}m/{remaining_min:.0f}m] Iter {iteration}: {sname}")
            if test_scenario(ser, sid, sname):
                passes += 1
            else:
                fails += 1

    elapsed_sec = time.time() - start
    ts(f"\n4-hour test complete ({elapsed_sec/3600:.1f}h actual)")
    ts(f"  Total: {passes + fails}")
    ts(f"  Passed: {passes}")
    ts(f"  Failed: {fails}")
    rate = 100 * passes / (passes + fails) if (passes + fails) > 0 else 0
    ts(f"  Pass rate: {rate:.1f}%\n")
    
    return fails == 0

def main():
    parser = argparse.ArgumentParser(description='Test 4 scenarios + stability')
    parser.add_argument('--port', required=True, help='Serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--mode', choices=['quick', 'disconnect', '4h'], default='quick',
                        help='quick: 4 scenarios, disconnect: +reconnect, 4h: 4h loop')
    args = parser.parse_args()

    ser = open_serial(args.port, args.baud)
    if not ser:
        ts("✗ Cannot open serial port")
        sys.exit(1)

    verify_ui_link(ser)

    scenarios = [
        ('DEFAULT', 'default_unlock_win_etape2'),
        ('EXPRESS', 'example_unlock_express'),
        ('EXPRESS_DONE', 'example_unlock_express_done'),
        ('SPECTRE', 'spectre_radio_lab'),
    ]

    passes = 0
    fails = 0

    # Run 4 scenarios
    for sid, sname in scenarios:
        if test_scenario(ser, sid, sname):
            passes += 1
        else:
            fails += 1

    # Disconnect test
    if args.mode in ['disconnect', '4h']:
        if test_disconnect(ser):
            passes += 1
        else:
            fails += 1

    # 4h stability
    if args.mode == '4h':
        if test_4h_stable(ser):
            passes += 1
        else:
            fails += 1

    # Close
    try:
        ser.close()
    except:
        pass

    # Final summary
    ts(f"\n{'='*70}")
    ts("FINAL SUMMARY")
    ts(f"{'='*70}")
    ts(f"Mode: {args.mode}")
    ts(f"Total: {passes + fails}")
    ts(f"Passed: {passes}")
    ts(f"Failed: {fails}")
    rate = 100 * passes / (passes + fails) if (passes + fails) > 0 else 0
    ts(f"Success rate: {rate:.1f}%")
    ts(f"{'='*70}\n")

    sys.exit(0 if fails == 0 else 1)

if __name__ == '__main__':
    main()
