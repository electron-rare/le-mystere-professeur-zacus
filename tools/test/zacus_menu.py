#!/usr/bin/env python3
"""Terminal test menu for Zacus tooling (curses + fallback)."""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PYTHON = sys.executable or "python3"
CONTENT_RUNNER = ROOT / "tools" / "test" / "run_content_checks.sh"
SERIAL_SUITE_RUNNER = ROOT / "tools" / "test" / "run_serial_suite.py"
UI_LINK_SIM = ROOT / "tools" / "test" / "ui_link_sim.py"
SERIAL_SMOKE = ROOT / "hardware" / "firmware" / "tools" / "dev" / "serial_smoke.py"
BUILD_AND_SMOKE = ROOT / "hardware" / "firmware" / "tools" / "dev" / "run_matrix_and_smoke.sh"
SUITE_FILE = ROOT / "tools" / "test" / "serial_suites.json"

ACTION_ITEMS = [
    ("content", "Content checks (cheap)"),
    ("build", "Build matrix + smoke (long)"),
    ("smoke", "Smoke USB rapide"),
    ("suite", "Lancer une suite serie USB"),
    ("ui_link", "UI Link sim"),
    ("console", "Console serie mini-REPL"),
    ("quit", "Quitter"),
]

REPL_HINTS = [
    "BOOT_STATUS",
    "STORY_STATUS",
    "STORY_V2_STATUS",
    "STORY_V2_HEALTH",
    "STORY_V2_METRICS",
    "MP3_STATUS",
    "MP3_UI_STATUS",
    "MP3_LIST",
    "MP3_PLAY 1",
    "MP3_SCAN STATUS",
    "MP3_FX WIN 800",
    "MP3_FX_STOP",
    "UI_LINK_STATUS",
]


def run_command(cmd: list[str]) -> int:
    print(f"[run] {shlex.join(cmd)}")
    completed = subprocess.run(cmd, cwd=str(ROOT), check=False)
    return completed.returncode


def read_suite_names() -> list[str]:
    try:
        with SUITE_FILE.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except Exception:
        return []
    suites = data.get("suites", {}) if isinstance(data, dict) else {}
    if not isinstance(suites, dict):
        return []
    return sorted(suites.keys())


def confirm(prompt: str) -> bool:
    answer = input(f"{prompt} [y/N]: ").strip().lower()
    return answer in {"y", "yes"}


def choose_with_numbers(title: str, options: list[tuple[str, str]]) -> str:
    print(f"\n{title}")
    for idx, (_, label) in enumerate(options, start=1):
        print(f"  {idx}) {label}")
    while True:
        raw = input("Select: ").strip()
        if not raw.isdigit():
            print("Invalid choice.")
            continue
        value = int(raw)
        if 1 <= value <= len(options):
            return options[value - 1][0]
        print("Invalid choice.")


def choose_action_curses() -> str:
    import curses

    def _inner(stdscr):
        curses.curs_set(0)
        index = 0
        while True:
            stdscr.erase()
            stdscr.addstr(0, 0, "Zacus test menu")
            stdscr.addstr(1, 0, "Use arrows + Enter (q to quit)")
            for row, (_, label) in enumerate(ACTION_ITEMS, start=3):
                mode = curses.A_REVERSE if (row - 3) == index else curses.A_NORMAL
                stdscr.addstr(row, 2, label, mode)
            stdscr.refresh()

            key = stdscr.getch()
            if key in (ord("q"), 27):
                return "quit"
            if key in (curses.KEY_UP, ord("k")):
                index = (index - 1) % len(ACTION_ITEMS)
            elif key in (curses.KEY_DOWN, ord("j")):
                index = (index + 1) % len(ACTION_ITEMS)
            elif key in (10, 13):
                return ACTION_ITEMS[index][0]

    return curses.wrapper(_inner)


def choose_action(no_curses: bool) -> str:
    if not no_curses and sys.stdin.isatty() and sys.stdout.isatty():
        try:
            return choose_action_curses()
        except Exception:
            pass
    return choose_with_numbers("Zacus test menu", ACTION_ITEMS)


def choose_suite_interactive() -> str | None:
    names = read_suite_names()
    if not names:
        print("[fail] unable to read suites from tools/test/serial_suites.json")
        return None
    options = [(name, name) for name in names]
    return choose_with_numbers("Serial suites", options)


def command_for_content(args) -> list[str]:
    cmd = ["bash", str(CONTENT_RUNNER)]
    if args.check_clean_git:
        cmd.append("--check-clean-git")
    return cmd


def command_for_build() -> list[str]:
    return ["bash", str(BUILD_AND_SMOKE)]


def command_for_smoke(args) -> list[str]:
    cmd = [
        PYTHON,
        str(SERIAL_SMOKE),
        "--role",
        args.role,
        "--baud",
        str(args.baud),
        "--wait-port",
        str(args.wait_port),
    ]
    if args.port:
        cmd.extend(["--port", args.port])
    if args.allow_no_hardware:
        cmd.append("--allow-no-hardware")
    return cmd


def command_for_suite(args) -> list[str]:
    cmd = [
        PYTHON,
        str(SERIAL_SUITE_RUNNER),
        "--suite",
        args.suite,
        "--role",
        args.role,
        "--baud",
        str(args.baud),
        "--wait-port",
        str(args.wait_port),
        "--timeout",
        str(args.timeout),
    ]
    if args.port:
        cmd.extend(["--port", args.port])
    if args.allow_no_hardware:
        cmd.append("--allow-no-hardware")
    return cmd


def command_for_ui_link(args) -> list[str]:
    cmd = [
        PYTHON,
        str(UI_LINK_SIM),
        "--baud",
        str(args.baud),
        "--wait-port",
        str(args.wait_port),
        "--ui-type",
        args.ui_type,
        "--ui-id",
        args.ui_id,
        "--fw",
        args.fw,
        "--caps",
        args.caps,
        "--script-delay-ms",
        str(args.script_delay_ms),
    ]
    if args.port:
        cmd.extend(["--port", args.port])
    if args.script:
        cmd.extend(["--script", args.script])
    if args.allow_no_hardware:
        cmd.append("--allow-no-hardware")
    return cmd


def ensure_pyserial_for_console():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("[fail] missing dependency: pip install pyserial")
        return None, None
    return serial, list_ports


def select_console_port(args, list_ports_module):
    ports = list(list_ports_module.comports())
    if args.port:
        for port in ports:
            if port.device == args.port:
                return args.port
        print(f"[fail] explicit port not found: {args.port}")
        return None

    if not ports:
        if args.allow_no_hardware:
            print("SKIP: no serial port detected")
            return ""
        print("[fail] no serial port detected")
        return None

    if len(ports) == 1 or not sys.stdin.isatty():
        return ports[0].device

    print("\nDetected serial ports:")
    for idx, port in enumerate(ports, start=1):
        print(f"  {idx}) {port.device} ({port.description})")

    while True:
        raw = input("Select port number: ").strip()
        if not raw.isdigit():
            print("Invalid choice.")
            continue
        value = int(raw)
        if 1 <= value <= len(ports):
            return ports[value - 1].device
        print("Invalid choice.")


def setup_readline_completion():
    try:
        import readline
    except Exception:
        return

    candidates = sorted(REPL_HINTS)

    def completer(text, state):
        matches = [item for item in candidates if item.startswith(text.upper())]
        if state < len(matches):
            return matches[state]
        return None

    readline.set_completer(completer)
    readline.parse_and_bind("tab: complete")


def run_console(args) -> int:
    serial_module, list_ports_module = ensure_pyserial_for_console()
    if serial_module is None:
        return 3

    port = select_console_port(args, list_ports_module)
    if port is None:
        return 2
    if port == "":
        return 0

    setup_readline_completion()
    print(f"[info] console connected on {port} @ {args.baud}")
    print("[info] type /quit to exit")

    try:
        with serial_module.Serial(port, args.baud, timeout=0.2) as ser:
            time.sleep(0.2)
            while True:
                try:
                    user_line = input("zacus> ").strip()
                except EOFError:
                    print()
                    return 0
                except KeyboardInterrupt:
                    print()
                    return 0

                if not user_line:
                    continue
                if user_line.lower() in {"/quit", "quit", "exit"}:
                    return 0

                ser.reset_input_buffer()
                ser.write((user_line + "\n").encode("ascii", errors="ignore"))

                deadline = time.monotonic() + max(args.timeout, 0.5)
                got_line = False
                while time.monotonic() < deadline:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    got_line = True
                    print(f"[rx] {line}")
                if not got_line:
                    print("[info] no response within timeout")
    except Exception as exc:
        print(f"[fail] console serial error: {exc}")
        return 1


def execute_action(action: str, args, interactive: bool) -> int:
    if action == "content":
        return run_command(command_for_content(args))

    if action == "build":
        if interactive:
            if not args.yes and not confirm("Build matrix + smoke may take a long time. Continue?"):
                print("[ok] build cancelled")
                return 0
        elif not args.yes:
            print("[fail] --action build requires --yes confirmation")
            return 2
        return run_command(command_for_build())

    if action == "smoke":
        return run_command(command_for_smoke(args))

    if action == "suite":
        if not args.suite:
            print("[fail] --suite is required for action 'suite'")
            return 2
        return run_command(command_for_suite(args))

    if action == "ui_link":
        return run_command(command_for_ui_link(args))

    if action == "console":
        return run_console(args)

    if action == "quit":
        return 0

    print(f"[fail] unknown action: {action}")
    return 2


def interactive_loop(args) -> int:
    while True:
        action = choose_action(args.no_curses)
        if action == "quit":
            return 0

        runtime_args = argparse.Namespace(**vars(args))
        if action == "suite" and not runtime_args.suite:
            suite_name = choose_suite_interactive()
            if suite_name is None:
                continue
            runtime_args.suite = suite_name

        rc = execute_action(action, runtime_args, interactive=True)
        print(f"[info] action '{action}' exit={rc}")
        if sys.stdin.isatty():
            input("Press Enter to continue...")


def main() -> int:
    parser = argparse.ArgumentParser(description="Zacus test menu")
    parser.add_argument("--action", choices=["content", "build", "smoke", "suite", "ui_link", "console"])
    parser.add_argument("--suite", help="Suite name for --action suite")
    parser.add_argument("--port", help="Explicit serial port")
    parser.add_argument("--role", choices=["auto", "esp32", "esp8266", "rp2040"], default="auto")
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--timeout", type=float, default=1.5)
    parser.add_argument("--wait-port", type=int, default=3)
    parser.add_argument("--allow-no-hardware", action="store_true")
    parser.add_argument("--check-clean-git", action="store_true", help="Forwarded to content checks runner")
    parser.add_argument("--yes", action="store_true", help="Bypass build confirmation")
    parser.add_argument("--script", default="", help="Forwarded to UI Link simulator")
    parser.add_argument("--script-delay-ms", type=int, default=300)
    parser.add_argument("--ui-type", default="TFT")
    parser.add_argument("--ui-id", default="zacus-cli")
    parser.add_argument("--fw", default="dev")
    parser.add_argument("--caps", default="btn:1;touch:0;display:cli")
    parser.add_argument("--no-curses", action="store_true", help="Force fallback numbered menu")
    args = parser.parse_args()

    if args.action:
        return execute_action(args.action, args, interactive=False)
    return interactive_loop(args)


if __name__ == "__main__":
    raise SystemExit(main())
