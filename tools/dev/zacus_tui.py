#!/usr/bin/env python3
"""
ZACUS - Orchestration TUI v1.0
Text User Interface for the Le Mystere du Professeur Zacus project.
No external dependencies — stdlib only.
"""

import argparse
import glob
import itertools
import os
import subprocess
import sys
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent  # tools/dev -> tools -> repo root
LOG_DIR = SCRIPT_DIR / "logs"
LOG_RETENTION_DAYS = 7

# ---------------------------------------------------------------------------
# ANSI colours
# ---------------------------------------------------------------------------
RST = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
CYAN = "\033[96m"
WHITE = "\033[97m"
BG_BLUE = "\033[44m"


def supports_color() -> bool:
    if os.environ.get("NO_COLOR"):
        return False
    return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


USE_COLOR = supports_color()


def c(code: str, text: str) -> str:
    return f"{code}{text}{RST}" if USE_COLOR else text


# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
_log_file = None


def init_log():
    global _log_file
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    path = LOG_DIR / f"zacus_tui_{ts}.log"
    _log_file = open(path, "w", encoding="utf-8")
    log(f"Session started — log: {path}")
    return path


def log(msg: str):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] {msg}"
    if _log_file:
        _log_file.write(line + "\n")
        _log_file.flush()


def close_log():
    if _log_file:
        log("Session ended.")
        _log_file.close()


# ---------------------------------------------------------------------------
# Spinner
# ---------------------------------------------------------------------------
class Spinner:
    FRAMES = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

    def __init__(self, label: str):
        self.label = label
        self._stop = threading.Event()
        self._thread = None

    def _spin(self):
        for frame in itertools.cycle(self.FRAMES):
            if self._stop.is_set():
                break
            sys.stdout.write(f"\r  {c(CYAN, frame)} {self.label}")
            sys.stdout.flush()
            time.sleep(0.08)
        sys.stdout.write("\r" + " " * (len(self.label) + 10) + "\r")
        sys.stdout.flush()

    def __enter__(self):
        if USE_COLOR:
            self._thread = threading.Thread(target=self._spin, daemon=True)
            self._thread.start()
        else:
            print(f"  ... {self.label}")
        return self

    def __exit__(self, *_):
        self._stop.set()
        if self._thread:
            self._thread.join()


# ---------------------------------------------------------------------------
# Command runner
# ---------------------------------------------------------------------------
def run_command(label: str, cmd: str, cwd: str | None = None) -> bool:
    work_dir = cwd or str(REPO_ROOT)
    print(c(BOLD + BLUE, f"\n{'─' * 50}"))
    print(c(BOLD + WHITE, f"  ▶ {label}"))
    print(c(DIM, f"    $ {cmd}"))
    print(c(DIM, f"    cwd: {work_dir}"))
    print(c(BOLD + BLUE, f"{'─' * 50}"))
    log(f"ACTION: {label}")
    log(f"  CMD: {cmd}")
    log(f"  CWD: {work_dir}")

    t0 = time.monotonic()
    try:
        proc = subprocess.Popen(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=work_dir,
            text=True,
            bufsize=1,
        )
        for line in proc.stdout:
            sys.stdout.write("    " + line)
            log("  | " + line.rstrip())
        proc.wait()
        rc = proc.returncode
    except FileNotFoundError as exc:
        rc = 127
        msg = f"Command not found: {exc}"
        print(c(RED, f"    {msg}"))
        log(f"  ERROR: {msg}")
    except Exception as exc:
        rc = 1
        msg = f"Exception: {exc}"
        print(c(RED, f"    {msg}"))
        log(f"  ERROR: {msg}")

    elapsed = time.monotonic() - t0
    if rc == 0:
        status = c(GREEN + BOLD, "SUCCESS")
        log(f"  STATUS: SUCCESS ({elapsed:.1f}s)")
    else:
        status = c(RED + BOLD, f"FAILED (exit {rc})")
        log(f"  STATUS: FAILED rc={rc} ({elapsed:.1f}s)")

    print(f"\n  {status}  {c(DIM, f'[{elapsed:.1f}s]')}\n")
    return rc == 0


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------
def action_validate_content():
    return run_command(
        "Validate All Content",
        "bash tools/test/run_content_checks.sh",
    )


def action_compile_runtime3():
    return run_command(
        "Compile Runtime 3",
        "python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml",
    )


def action_simulate_runtime3():
    return run_command(
        "Simulate Runtime 3",
        "python3 tools/scenario/simulate_runtime3.py game/scenarios/zacus_v2.yaml",
    )


def action_python_tests():
    return run_command(
        "Run Python Tests",
        "python3 -m pytest tests/runtime3/ -v",
    )


def action_frontend_tests():
    return run_command(
        "Run Frontend Tests",
        "npx vitest run",
        cwd=str(REPO_ROOT / "frontend-scratch-v2"),
    )


def action_build_frontend():
    return run_command(
        "Build Frontend",
        "npm run build",
        cwd=str(REPO_ROOT / "frontend-scratch-v2"),
    )


def action_build_docs():
    return run_command(
        "Build Docs (MkDocs)",
        "python3 -m mkdocs build --strict",
    )


def action_full_ci():
    steps = [
        ("1/7 Validate Content", action_validate_content),
        ("2/7 Compile Runtime 3", action_compile_runtime3),
        ("3/7 Simulate Runtime 3", action_simulate_runtime3),
        ("4/7 Python Tests", action_python_tests),
        ("5/7 Frontend Tests", action_frontend_tests),
        ("6/7 Build Frontend", action_build_frontend),
        ("7/7 Build Docs", action_build_docs),
    ]
    print(c(BOLD + CYAN, "\n  Full CI Gate — running all validations\n"))
    log("FULL CI GATE START")
    t0 = time.monotonic()
    for label, fn in steps:
        print(c(YELLOW, f"  [{label}]"))
        ok = fn()
        if not ok:
            elapsed = time.monotonic() - t0
            print(c(RED + BOLD, f"\n  CI GATE FAILED at {label} [{elapsed:.1f}s total]\n"))
            log(f"CI GATE FAILED at {label} [{elapsed:.1f}s]")
            return False
    elapsed = time.monotonic() - t0
    print(c(GREEN + BOLD, f"\n  CI GATE PASSED — all 7 steps OK [{elapsed:.1f}s total]\n"))
    log(f"CI GATE PASSED [{elapsed:.1f}s]")
    return True


def action_flash_firmware():
    return run_command(
        "Flash Firmware (PlatformIO)",
        "pio run -e freenove_esp32s3_full_with_ui --target upload",
        cwd=str(REPO_ROOT / "ESP32_ZACUS"),
    )


def action_deploy_content():
    return run_command(
        "Deploy Content (LittleFS)",
        "pio run -e freenove_esp32s3_full_with_ui --target uploadfs",
        cwd=str(REPO_ROOT / "ESP32_ZACUS"),
    )


def action_view_logs():
    print(c(BOLD + BLUE, f"\n{'─' * 50}"))
    print(c(BOLD + WHITE, "  Recent Logs"))
    print(c(BOLD + BLUE, f"{'─' * 50}"))
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    logs = sorted(LOG_DIR.glob("zacus_tui_*.log"), reverse=True)
    if not logs:
        print(c(YELLOW, "  No log files found.\n"))
        return True
    for lf in logs[:20]:
        size = lf.stat().st_size
        mtime = datetime.fromtimestamp(lf.stat().st_mtime).strftime("%Y-%m-%d %H:%M")
        if size > 1024 * 1024:
            sz = f"{size / (1024*1024):.1f} MB"
        elif size > 1024:
            sz = f"{size / 1024:.1f} KB"
        else:
            sz = f"{size} B"
        # Check for FAILED in log
        content = lf.read_text(errors="replace")
        if "FAILED" in content:
            status_icon = c(RED, "FAIL")
        elif "SUCCESS" in content or "PASSED" in content:
            status_icon = c(GREEN, " OK ")
        else:
            status_icon = c(DIM, " -- ")
        print(f"  [{status_icon}] {c(CYAN, lf.name)}  {c(DIM, sz):>12}  {c(DIM, mtime)}")
    print()
    return True


def action_clean_logs():
    print(c(BOLD + BLUE, f"\n{'─' * 50}"))
    print(c(BOLD + WHITE, f"  Cleaning logs older than {LOG_RETENTION_DAYS} days"))
    print(c(BOLD + BLUE, f"{'─' * 50}"))
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    cutoff = datetime.now() - timedelta(days=LOG_RETENTION_DAYS)
    removed = 0
    for lf in LOG_DIR.glob("zacus_tui_*.log"):
        mtime = datetime.fromtimestamp(lf.stat().st_mtime)
        if mtime < cutoff:
            lf.unlink()
            print(c(DIM, f"  Deleted: {lf.name}"))
            log(f"Deleted old log: {lf.name}")
            removed += 1
    if removed:
        print(c(GREEN, f"\n  Removed {removed} old log file(s).\n"))
    else:
        print(c(YELLOW, "  No old logs to remove.\n"))
    log(f"Log cleanup: removed {removed} files")
    return True


# ---------------------------------------------------------------------------
# Menu mapping
# ---------------------------------------------------------------------------
ACTIONS = {
    1: ("Validate All Content", action_validate_content),
    2: ("Compile Runtime 3", action_compile_runtime3),
    3: ("Simulate Runtime 3", action_simulate_runtime3),
    4: ("Run Python Tests", action_python_tests),
    5: ("Run Frontend Tests", action_frontend_tests),
    6: ("Build Frontend", action_build_frontend),
    7: ("Build Docs (MkDocs)", action_build_docs),
    8: ("Full CI Gate (all validations)", action_full_ci),
    9: ("Flash Firmware (PlatformIO)", action_flash_firmware),
    10: ("Deploy Content (LittleFS)", action_deploy_content),
    11: ("View Recent Logs", action_view_logs),
    12: ("Clean Logs (>7 days)", action_clean_logs),
}


# ---------------------------------------------------------------------------
# Menu display
# ---------------------------------------------------------------------------
def print_banner():
    print()
    print(c(CYAN, "  ╔══════════════════════════════════════════════╗"))
    print(c(CYAN, "  ║") + c(BOLD + WHITE, "       ZACUS - Orchestration TUI v1.0        ") + c(CYAN, "║"))
    print(c(CYAN, "  ╠══════════════════════════════════════════════╣"))
    for num, (label, _) in ACTIONS.items():
        n = f"{num:>2}"
        pad = 42 - len(label)
        print(c(CYAN, "  ║") + f"  {c(YELLOW, n)}. {label}{' ' * pad}" + c(CYAN, "║"))
    print(c(CYAN, "  ║") + f"  {c(YELLOW, ' 0')}. Exit{' ' * 38}" + c(CYAN, "║"))
    print(c(CYAN, "  ╚══════════════════════════════════════════════╝"))
    print()


# ---------------------------------------------------------------------------
# Main loops
# ---------------------------------------------------------------------------
def interactive_loop():
    while True:
        print_banner()
        try:
            raw = input(c(WHITE + BOLD, "  Select option: ")).strip()
        except (KeyboardInterrupt, EOFError):
            print(c(YELLOW, "\n  Interrupted. Bye!\n"))
            break

        if not raw:
            continue
        try:
            choice = int(raw)
        except ValueError:
            print(c(RED, "  Invalid input. Enter a number 0-12."))
            continue

        if choice == 0:
            print(c(GREEN, "  Au revoir!\n"))
            break

        if choice not in ACTIONS:
            print(c(RED, f"  Unknown option: {choice}"))
            continue

        label, fn = ACTIONS[choice]
        log(f"User selected: [{choice}] {label}")
        fn()


def run_single(action_num: int) -> bool:
    if action_num not in ACTIONS:
        print(c(RED, f"Unknown action: {action_num}. Valid: 1-12"))
        return False
    label, fn = ACTIONS[action_num]
    log(f"Non-interactive: [{action_num}] {label}")
    return fn()


def run_all() -> bool:
    log("Non-interactive: running full CI gate")
    return action_full_ci()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="ZACUS Orchestration TUI — Le Mystere du Professeur Zacus"
    )
    parser.add_argument(
        "--non-interactive",
        action="store_true",
        help="Run full CI gate (actions 1-7) and exit",
    )
    parser.add_argument(
        "--action",
        type=int,
        metavar="N",
        help="Run a specific action (1-12) and exit",
    )
    args = parser.parse_args()

    log_path = init_log()

    try:
        if args.action is not None:
            ok = run_single(args.action)
            close_log()
            sys.exit(0 if ok else 1)
        elif args.non_interactive:
            ok = run_all()
            close_log()
            sys.exit(0 if ok else 1)
        else:
            interactive_loop()
            close_log()
    except KeyboardInterrupt:
        print(c(YELLOW, "\n  Interrupted. Bye!\n"))
        close_log()
        sys.exit(130)


if __name__ == "__main__":
    main()
