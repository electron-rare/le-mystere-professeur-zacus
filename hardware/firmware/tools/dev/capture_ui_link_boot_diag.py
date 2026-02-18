#!/usr/bin/env python3
"""Capture a cross-monitor boot session for ESP32<->ESP8266 UI link diagnostics."""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path
from queue import Empty, Queue

try:
    from serial import Serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

FW_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = FW_ROOT.parent.parent
PHASE = "ui_link_diag"


@dataclass
class EvidencePaths:
    base: Path
    meta: Path
    git: Path
    commands: Path
    summary: Path
    merged: Path
    esp32_log: Path
    esp8266_log: Path
    ports_json: Path


def init_evidence(outdir: str) -> EvidencePaths:
    stamp = datetime.now(UTC).strftime("%Y%m%d-%H%M%S")
    if outdir:
        base = Path(outdir)
        if not base.is_absolute():
            base = FW_ROOT / outdir
    else:
        base = FW_ROOT / "artifacts" / PHASE / stamp
    base.mkdir(parents=True, exist_ok=True)
    return EvidencePaths(
        base=base,
        meta=base / "meta.json",
        git=base / "git.txt",
        commands=base / "commands.txt",
        summary=base / "summary.md",
        merged=base / "merged.log",
        esp32_log=base / "esp32.log",
        esp8266_log=base / "esp8266.log",
        ports_json=base / "ports_resolve.json",
    )


def write_git_info(dest: Path) -> None:
    lines: list[str] = []
    for label, args in (
        ("branch", ["rev-parse", "--abbrev-ref", "HEAD"]),
        ("commit", ["rev-parse", "HEAD"]),
    ):
        try:
            value = subprocess.check_output(["git", "-C", str(REPO_ROOT), *args], text=True).strip()
        except Exception:
            value = "n/a"
        lines.append(f"{label}: {value}")
    lines.append("status:")
    try:
        status = subprocess.check_output(
            ["git", "-C", str(REPO_ROOT), "status", "--porcelain"], text=True
        ).strip()
    except Exception:
        status = ""
    if status:
        lines.append(status)
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_meta(dest: Path, command: str, args: argparse.Namespace, esp32: str, esp8266: str) -> None:
    payload = {
        "timestamp": datetime.now(UTC).strftime("%Y%m%d-%H%M%S"),
        "phase": PHASE,
        "utc": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "command": command,
        "cwd": str(Path.cwd()),
        "repo_root": str(REPO_ROOT),
        "fw_root": str(FW_ROOT),
        "esp32_port": esp32,
        "esp8266_port": esp8266,
        "duration_s": args.seconds,
        "baud_esp32": args.baud_esp32,
        "baud_esp8266": args.baud_esp8266,
    }
    dest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def resolve_ports(args: argparse.Namespace, ports_json: Path) -> tuple[str, str]:
    if args.esp32_port and args.esp8266_port:
        return args.esp32_port, args.esp8266_port

    cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "test" / "resolve_ports.py"),
        "--wait-port",
        str(args.wait_port),
        "--auto-ports",
        "--need-esp32",
        "--need-esp8266",
        "--ports-resolve-json",
        str(ports_json),
    ]
    if platform.system() == "Darwin":
        cmd.append("--prefer-cu")
    if args.allow_no_hardware:
        cmd.append("--allow-no-hardware")
    if args.esp32_port:
        cmd += ["--port-esp32", args.esp32_port]
    if args.esp8266_port:
        cmd += ["--port-esp8266", args.esp8266_port]

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        if args.allow_no_hardware:
            raise RuntimeError("SKIP: no hardware detected")
        raise RuntimeError("port resolution failed")

    data = json.loads(ports_json.read_text(encoding="utf-8"))
    ports = data.get("ports", {})
    esp32 = ports.get("esp32", "")
    esp8266 = ports.get("esp8266", ports.get("esp8266_usb", ""))
    if not esp32 or not esp8266:
        if args.allow_no_hardware:
            raise RuntimeError("SKIP: resolved ports missing")
        raise RuntimeError("resolved ports missing")
    return esp32, esp8266


def reader(role: str, ser: Serial, queue: Queue[tuple[float, str, str]], stop_evt: threading.Event) -> None:
    while not stop_evt.is_set():
        try:
            raw = ser.readline()
        except Exception as exc:
            queue.put((time.time(), role, f"[READ_ERROR] {exc}"))
            return
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if line:
            queue.put((time.time(), role, line))


def reset_edge(ser: Serial) -> None:
    try:
        ser.dtr = False
        time.sleep(0.08)
        ser.dtr = True
    except Exception:
        return


def send_cmd(ser: Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Cross-monitor ESP32 + ESP8266 UI link boot diagnostics.")
    parser.add_argument("--esp32-port", default="", help="ESP32 serial port")
    parser.add_argument("--esp8266-port", default="", help="ESP8266 serial port")
    parser.add_argument("--baud-esp32", type=int, default=115200, help="ESP32 monitor baud")
    parser.add_argument("--baud-esp8266", type=int, default=115200, help="ESP8266 monitor baud")
    parser.add_argument("--seconds", type=float, default=20.0, help="Capture duration in seconds")
    parser.add_argument("--wait-port", type=int, default=3, help="Resolver wait window")
    parser.add_argument("--outdir", default="", help="Evidence output directory")
    parser.add_argument(
        "--allow-no-hardware",
        action="store_true",
        help="Return SKIP instead of failing when hardware is absent.",
    )
    parser.add_argument(
        "--no-reset-edge",
        action="store_true",
        help="Do not pulse DTR after opening ports.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    evidence = init_evidence(args.outdir)
    write_git_info(evidence.git)
    evidence.commands.write_text("# Commands\n", encoding="utf-8")
    with evidence.commands.open("a", encoding="utf-8") as fp:
        fp.write(" ".join(sys.argv) + "\n")

    try:
        esp32_port, esp8266_port = resolve_ports(args, evidence.ports_json)
    except RuntimeError as exc:
        if str(exc).startswith("SKIP"):
            evidence.summary.write_text("# UI link diag summary\n\n- Result: **SKIP**\n- Reason: no hardware\n", encoding="utf-8")
            print("RESULT=SKIP")
            return 0
        print(f"[error] {exc}", file=sys.stderr)
        return 2

    write_meta(evidence.meta, " ".join(sys.argv), args, esp32_port, esp8266_port)

    counters: dict[str, int] = {
        "esp32_diag_tx": 0,
        "esp32_diag_rx": 0,
        "esp32_hello": 0,
        "esp32_connected_1": 0,
        "esp32_connected_0": 0,
        "esp8266_diag_tx": 0,
        "esp8266_diag_rx": 0,
        "esp8266_diag_sum": 0,
        "esp8266_connected_1": 0,
        "esp8266_connected_0": 0,
    }

    queue: Queue[tuple[float, str, str]] = Queue()
    stop_evt = threading.Event()
    threads: list[threading.Thread] = []

    start_monotonic = time.monotonic()
    start_epoch = time.time()
    end_monotonic = start_monotonic + max(1.0, args.seconds)
    ui_status_sent = 0

    try:
        with Serial(esp32_port, args.baud_esp32, timeout=0.1) as ser_esp32, Serial(
            esp8266_port, args.baud_esp8266, timeout=0.1
        ) as ser_esp8266, evidence.esp32_log.open("w", encoding="utf-8") as fp_esp32, evidence.esp8266_log.open(
            "w", encoding="utf-8"
        ) as fp_esp8266, evidence.merged.open("w", encoding="utf-8") as fp_merged:
            if not args.no_reset_edge:
                reset_edge(ser_esp32)
                reset_edge(ser_esp8266)
                time.sleep(0.2)
            ser_esp32.reset_input_buffer()
            ser_esp8266.reset_input_buffer()

            threads = [
                threading.Thread(target=reader, args=("esp32", ser_esp32, queue, stop_evt), daemon=True),
                threading.Thread(target=reader, args=("esp8266", ser_esp8266, queue, stop_evt), daemon=True),
            ]
            for th in threads:
                th.start()

            while time.monotonic() < end_monotonic:
                now = time.monotonic()
                elapsed = now - start_monotonic
                if ui_status_sent == 0 and elapsed >= 3.0:
                    send_cmd(ser_esp32, "UI_LINK_STATUS")
                    ui_status_sent = 1
                elif ui_status_sent == 1 and elapsed >= max(6.0, args.seconds - 4.0):
                    send_cmd(ser_esp32, "UI_LINK_STATUS")
                    ui_status_sent = 2

                try:
                    ts_epoch, role, line = queue.get(timeout=0.2)
                except Empty:
                    continue

                rel_ms = int((ts_epoch - start_epoch) * 1000.0)
                merged_line = f"[{rel_ms:06d}ms][{role}] {line}\n"
                fp_merged.write(merged_line)
                if role == "esp32":
                    fp_esp32.write(line + "\n")
                    if "[UI_DIAG][ESP32][TX]" in line:
                        counters["esp32_diag_tx"] += 1
                    if "[UI_DIAG][ESP32][RX]" in line:
                        counters["esp32_diag_rx"] += 1
                    if "[UI_DIAG][ESP32][HELLO]" in line:
                        counters["esp32_hello"] += 1
                    if "UI_LINK_STATUS" in line and "connected=1" in line:
                        counters["esp32_connected_1"] += 1
                    if "UI_LINK_STATUS" in line and "connected=0" in line:
                        counters["esp32_connected_0"] += 1
                else:
                    fp_esp8266.write(line + "\n")
                    if "[UI_DIAG][ESP8266][TX]" in line:
                        counters["esp8266_diag_tx"] += 1
                    if "[UI_DIAG][ESP8266][RX]" in line:
                        counters["esp8266_diag_rx"] += 1
                    if "[UI_DIAG][ESP8266][SUM]" in line:
                        counters["esp8266_diag_sum"] += 1
                    if "[UI_LINK] STATUS: connected=1" in line:
                        counters["esp8266_connected_1"] += 1
                    if "[UI_LINK] STATUS: connected=0" in line:
                        counters["esp8266_connected_0"] += 1
    finally:
        stop_evt.set()
        for th in threads:
            th.join(timeout=0.5)

    esp32_to_esp8266 = counters["esp8266_diag_rx"] > 0
    esp8266_to_esp32 = (counters["esp32_diag_rx"] > 0) or (counters["esp32_hello"] > 0)
    connected_seen = (counters["esp32_connected_1"] > 0) or (counters["esp8266_connected_1"] > 0)

    if esp32_to_esp8266 and esp8266_to_esp32 and connected_seen:
        result = "PASS"
        rc = 0
        verdict = "bidirectional UART + connected=1 observed"
    elif esp32_to_esp8266 or esp8266_to_esp32:
        result = "WARN"
        rc = 1
        verdict = "partial UART traffic observed, handshake incomplete"
    else:
        result = "FAIL"
        rc = 2
        verdict = "no discriminant UART traffic observed in either direction"

    summary_lines = [
        "# UI link diag summary",
        "",
        f"- Result: **{result}**",
        f"- Verdict: {verdict}",
        f"- ESP32 port: `{esp32_port}`",
        f"- ESP8266 port: `{esp8266_port}`",
        f"- Duration: {args.seconds:.1f}s",
        f"- ESP32 diag tx/rx: `{counters['esp32_diag_tx']}` / `{counters['esp32_diag_rx']}`",
        f"- ESP8266 diag tx/rx: `{counters['esp8266_diag_tx']}` / `{counters['esp8266_diag_rx']}`",
        f"- ESP32 connected=1 hits: `{counters['esp32_connected_1']}`",
        f"- ESP8266 connected=1 hits: `{counters['esp8266_connected_1']}`",
        f"- Logs: `{evidence.base}`",
    ]
    evidence.summary.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    print(f"RESULT={result}")
    print(f"[diag] evidence={evidence.base}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
