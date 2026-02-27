#!/usr/bin/env python3
"""Deep runtime transition verification for all story scenarios.

The script combines:
- static selectability checks (priority/event matching)
- source-step reachability analysis
- runtime replay against an attached board using SC_LOAD/SC_EVENT/STATUS
"""

from __future__ import annotations

import argparse
import glob
import json
import re
import time
from collections import defaultdict, deque
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable

import serial

STATUS_RE = re.compile(
    r"\bSTATUS\b.*\bscenario=([A-Z0-9_]+)\b.*\bstep=([A-Z0-9_]+)\b.*\bscreen=([A-Z0-9_]+)\b"
)


@dataclass
class TransitionCase:
    scenario_id: str
    source_step: str
    index: int
    transition_id: str
    trigger: str
    event_type: str
    event_name: str
    after_ms: int
    priority: int
    target_step: str
    select_name: str | None = None
    selectable: bool = False


def event_name_matches(expected: str, actual: str) -> bool:
    if expected == "":
        return True
    return expected == actual


def selected_on_event(
    step_transitions: list[TransitionCase], event_type: str, actual_name: str
) -> TransitionCase | None:
    selected = None
    for transition in step_transitions:
        if transition.trigger != "on_event":
            continue
        if transition.event_type != event_type:
            continue
        if not event_name_matches(transition.event_name, actual_name):
            continue
        if selected is None or transition.priority > selected.priority:
            selected = transition
    return selected


def selected_after_ms(
    step_transitions: list[TransitionCase], elapsed_ms: int
) -> TransitionCase | None:
    selected = None
    for transition in step_transitions:
        if transition.trigger != "after_ms":
            continue
        if elapsed_ms < transition.after_ms:
            continue
        if selected is None or transition.priority > selected.priority:
            selected = transition
    return selected


def read_lines(ser: serial.Serial, timeout_s: float) -> list[str]:
    lines: list[str] = []
    end = time.time() + timeout_s
    while time.time() < end:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if line:
            lines.append(line)
    return lines


def send_cmd(ser: serial.Serial, cmd: str, timeout_s: float = 1.0) -> list[str]:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.16)
    return read_lines(ser, timeout_s)


def fetch_status(
    ser: serial.Serial, retries: int = 8, timeout_s: float = 0.7
) -> tuple[str, str, str, list[str]] | None:
    for _ in range(retries):
        lines = send_cmd(ser, "STATUS", timeout_s)
        for line in reversed(lines):
            match = STATUS_RE.search(line)
            if match:
                return match.group(1), match.group(2), match.group(3), lines
    return None


def build_model(
    scenario_path: Path,
) -> tuple[
    str,
    str,
    dict[str, dict],
    dict[str, list[TransitionCase]],
    list[TransitionCase],
    dict[str, list[TransitionCase]],
]:
    payload = json.loads(scenario_path.read_text(encoding="utf-8"))
    scenario_id = payload["id"]
    initial_step = payload["initial_step"]
    steps = {step["step_id"]: step for step in payload["steps"]}

    transitions_by_step: dict[str, list[TransitionCase]] = defaultdict(list)
    all_transitions: list[TransitionCase] = []

    for step in payload["steps"]:
        source_step = step["step_id"]
        for index, tr in enumerate(step.get("transitions", [])):
            transition = TransitionCase(
                scenario_id=scenario_id,
                source_step=source_step,
                index=index,
                transition_id=tr.get("id", f"{source_step}:{index}"),
                trigger=tr.get("trigger", ""),
                event_type=tr.get("event_type", ""),
                event_name=(tr.get("event_name") or ""),
                after_ms=int(tr.get("after_ms", 0) or 0),
                priority=int(tr.get("priority", 0) or 0),
                target_step=tr.get("target_step_id", ""),
            )
            transitions_by_step[source_step].append(transition)
            all_transitions.append(transition)

    for transitions in transitions_by_step.values():
        names_by_type: dict[str, set[str]] = defaultdict(set)
        for transition in transitions:
            if transition.trigger == "on_event" and transition.event_name:
                names_by_type[transition.event_type].add(transition.event_name)

        for transition in transitions:
            if transition.trigger == "on_event":
                candidates = list(names_by_type[transition.event_type])
                if transition.event_name:
                    candidates.insert(0, transition.event_name)
                candidates.extend(
                    [
                        "__AUTO_EVT__",
                        "BTN_NEXT",
                        "UNLOCK",
                        "UNLOCK_QR",
                        "AUDIO_DONE",
                        "FORCE_DONE",
                        "ACK_WIN1",
                        "ACK_WIN2",
                        "ETAPE2_DUE",
                        "LA_TIMEOUT",
                        "ANY",
                    ]
                )
                deduped: list[str] = []
                for candidate in candidates:
                    if candidate not in deduped:
                        deduped.append(candidate)
                for actual_name in deduped:
                    selected = selected_on_event(
                        transitions, transition.event_type, actual_name
                    )
                    if selected is transition:
                        transition.selectable = True
                        transition.select_name = actual_name
                        break
            elif transition.trigger == "after_ms":
                selected = selected_after_ms(transitions, transition.after_ms)
                if selected is transition:
                    transition.selectable = True

    edges: dict[str, list[TransitionCase]] = defaultdict(list)
    for transition in all_transitions:
        if transition.selectable:
            edges[transition.source_step].append(transition)

    paths: dict[str, list[TransitionCase]] = {initial_step: []}
    queue: deque[str] = deque([initial_step])
    while queue:
        current = queue.popleft()
        for transition in edges.get(current, []):
            target = transition.target_step
            if not target or target in paths:
                continue
            paths[target] = paths[current] + [transition]
            queue.append(target)

    return (
        scenario_id,
        initial_step,
        steps,
        transitions_by_step,
        all_transitions,
        paths,
    )


def _event_cmd(event_type: str, event_name: str | None) -> str:
    event_name_text = event_name if event_name else ""
    if event_type:
        return f"SC_EVENT {event_type} {event_name_text}".strip()
    return f"SC_EVENT_RAW {event_name_text}".strip()


def verify(port: str, baud: int, scenario_files: Iterable[Path], log_path: Path) -> int:
    models = [build_model(path) for path in scenario_files]
    if not models:
        print("No scenarios found to verify", flush=True)
        return 1

    log_path.parent.mkdir(parents=True, exist_ok=True)

    blocked: list[TransitionCase] = []
    unreachable: list[TransitionCase] = []
    runtime_results: list[tuple[bool, str, str, str]] = []

    ser = serial.Serial(port, baud, timeout=0.2)
    time.sleep(1.0)
    ser.reset_input_buffer()

    with log_path.open("w", encoding="utf-8") as log:
        def log_line(text: str) -> None:
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()

        for (
            scenario_id,
            initial_step,
            _steps,
            _transitions_by_step,
            all_transitions,
            paths,
        ) in models:
            log_line(
                f"=== {scenario_id} initial={initial_step} transitions={len(all_transitions)} ==="
            )

            for transition in all_transitions:
                if not transition.selectable:
                    blocked.append(transition)
                    log_line(
                        f"STATIC_BLOCKED {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} trigger={transition.trigger} "
                        f"type={transition.event_type} name={transition.event_name!r} "
                        f"prio={transition.priority} target={transition.target_step}"
                    )

            testable = [transition for transition in all_transitions if transition.selectable]
            for idx, transition in enumerate(testable, start=1):
                if transition.source_step not in paths:
                    unreachable.append(transition)
                    log_line(
                        f"STATIC_UNREACHABLE_SOURCE {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step}"
                    )
                    continue

                log_line(f"[test] {scenario_id} {idx}/{len(testable)} {transition.transition_id}")

                send_cmd(ser, "RESET", 1.0)
                load_lines = send_cmd(ser, f"SC_LOAD {scenario_id}", 1.4)
                if not any(
                    f"ACK SC_LOAD id={scenario_id} ok=1" in line for line in load_lines
                ):
                    runtime_results.append(
                        (False, scenario_id, transition.transition_id, "load_failed")
                    )
                    log_line(f"FAIL {scenario_id} {transition.transition_id} load_failed")
                    continue

                status = fetch_status(ser)
                if status is None:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            "status_missing_after_load",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} status_missing_after_load"
                    )
                    continue
                status_scenario, _, _, _ = status
                if status_scenario != scenario_id:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"scenario_mismatch:{status_scenario}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} scenario_mismatch={status_scenario}"
                    )
                    continue

                path_edges = paths[transition.source_step]
                path_ok = True
                path_error = ""
                for edge in path_edges:
                    if edge.trigger == "on_event":
                        send_cmd(ser, _event_cmd(edge.event_type, edge.select_name), 1.2)
                        edge_status = fetch_status(ser, retries=8, timeout_s=0.6)
                        if edge_status is None:
                            path_ok = False
                            path_error = f"path_status_missing:{edge.transition_id}"
                            break
                        _, edge_step, _, _ = edge_status
                        if edge_step != edge.target_step:
                            path_ok = False
                            path_error = (
                                f"path_step_mismatch:{edge.transition_id}:"
                                f"{edge_step}->{edge.target_step}"
                            )
                            break
                    elif edge.trigger == "after_ms":
                        deadline = time.time() + (edge.after_ms / 1000.0) + 3.0
                        hit = False
                        while time.time() < deadline:
                            edge_status = fetch_status(ser, retries=1, timeout_s=0.7)
                            if edge_status is None:
                                continue
                            _, edge_step, _, _ = edge_status
                            if edge_step == edge.target_step:
                                hit = True
                                break
                        if not hit:
                            path_ok = False
                            path_error = f"path_after_ms_timeout:{edge.transition_id}"
                            break
                    else:
                        path_ok = False
                        path_error = f"unsupported_path_trigger:{edge.trigger}"
                        break

                if not path_ok:
                    runtime_results.append(
                        (False, scenario_id, transition.transition_id, path_error)
                    )
                    log_line(f"FAIL {scenario_id} {transition.transition_id} {path_error}")
                    continue

                source_status = fetch_status(ser, retries=5, timeout_s=0.6)
                if source_status is None:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            "status_missing_before_transition",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} status_missing_before_transition"
                    )
                    continue
                _, current_step, _, _ = source_status
                if current_step != transition.source_step:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"source_step_mismatch:{current_step}->{transition.source_step}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} "
                        f"source_step_mismatch={current_step}->{transition.source_step}"
                    )
                    continue

                if transition.trigger == "on_event":
                    send_cmd(
                        ser, _event_cmd(transition.event_type, transition.select_name), 1.2
                    )
                    target_status = fetch_status(ser, retries=8, timeout_s=0.6)
                    if target_status is None:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                "status_missing_after_transition",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} status_missing_after_transition"
                        )
                        continue
                    _, new_step, _, _ = target_status
                    if new_step != transition.target_step:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                f"target_mismatch:{new_step}->{transition.target_step}",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} "
                            f"target_mismatch={new_step}->{transition.target_step}"
                        )
                        continue
                    runtime_results.append((True, scenario_id, transition.transition_id, "ok"))
                    log_line(
                        f"PASS {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} target={transition.target_step} "
                        f"via={transition.event_type}:{transition.select_name}"
                    )
                elif transition.trigger == "after_ms":
                    deadline = time.time() + (transition.after_ms / 1000.0) + 4.0
                    hit = False
                    last_step = current_step
                    while time.time() < deadline:
                        target_status = fetch_status(ser, retries=1, timeout_s=0.7)
                        if target_status is None:
                            continue
                        _, new_step, _, _ = target_status
                        last_step = new_step
                        if new_step == transition.target_step:
                            hit = True
                            break
                    if not hit:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                f"after_ms_timeout:last_step={last_step}",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} "
                            f"after_ms_timeout expected={transition.target_step} got={last_step}"
                        )
                        continue
                    runtime_results.append((True, scenario_id, transition.transition_id, "ok"))
                    log_line(
                        f"PASS {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} target={transition.target_step} "
                        f"via=after_ms:{transition.after_ms}"
                    )
                else:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"unsupported_trigger:{transition.trigger}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} unsupported_trigger={transition.trigger}"
                    )

    ser.close()

    pass_count = sum(1 for status, *_ in runtime_results if status)
    fail_results = [result for result in runtime_results if not result[0]]

    print("--- SUMMARY ---", flush=True)
    print(f"log={log_path}", flush=True)
    print(
        f"runtime_tested={len(runtime_results)} pass={pass_count} fail={len(fail_results)} "
        f"static_blocked={len(blocked)} static_unreachable_source={len(unreachable)}",
        flush=True,
    )

    if blocked:
        print("blocked transitions:", flush=True)
        for transition in blocked:
            print(
                f"  {transition.scenario_id}:{transition.transition_id} source={transition.source_step} "
                f"type={transition.event_type} name={transition.event_name!r} prio={transition.priority}",
                flush=True,
            )

    if unreachable:
        print("unreachable source steps:", flush=True)
        for transition in unreachable:
            print(
                f"  {transition.scenario_id}:{transition.transition_id} source={transition.source_step}",
                flush=True,
            )

    if fail_results:
        print("runtime failures:", flush=True)
        for _, scenario_id, transition_id, reason in fail_results:
            print(f"  {scenario_id}:{transition_id} reason={reason}", flush=True)
        return 1

    if blocked or unreachable:
        return 2

    return 0


def resolve_first_usbmodem_port() -> str | None:
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not candidates:
        return None
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Deep runtime transition verification for all story scenarios"
    )
    parser.add_argument("--port", default="", help="Serial port (default: first /dev/cu.usbmodem*)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument(
        "--scenarios-glob",
        default="data/story/scenarios/*.json",
        help="Glob for scenario JSON files",
    )
    parser.add_argument("--log", default="", help="Optional log path")
    args = parser.parse_args()

    port = args.port or resolve_first_usbmodem_port()
    if not port:
        print("No serial port found. Use --port /dev/cu.usbmodemXXX", flush=True)
        return 1

    scenario_files = [Path(path) for path in sorted(glob.glob(args.scenarios_glob))]
    if args.log:
        log_path = Path(args.log)
    else:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        log_path = Path(f"artifacts/rc_live/deep_transition_verify_{stamp}.log")
    return verify(port, args.baud, scenario_files, log_path)


if __name__ == "__main__":
    raise SystemExit(main())
