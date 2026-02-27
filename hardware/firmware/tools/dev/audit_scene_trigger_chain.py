#!/usr/bin/env python3
"""Audit trigger parity and safety checks across YAML/JSON/embedded scenario sources."""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
from pathlib import Path
from typing import Any

try:
    import yaml  # type: ignore
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"[fail] missing dependency PyYAML: {exc}")

STEP_ALIASES = {
    "U_SON": "SCENE_U_SON_PROTO",
    "LA": "SCENE_LA_DETECTOR",
    "WIN_ETAPE": "RTC_ESP_ETAPE1",
    "WIN_ETAPE1": "WIN_ETAPE1",
}

Transition = dict[str, Any]


def normalize_step_id(step: dict[str, Any]) -> str:
    return str(step.get("step_id") or step.get("id") or "")


def normalized_transition(raw: dict[str, Any]) -> Transition:
    return {
        "id": str(raw.get("id") or ""),
        "trigger": str(raw.get("trigger") or "").strip(),
        "event_type": str(raw.get("event_type") or "").strip(),
        "event_name": str(raw.get("event_name") or "").strip(),
        "target_step_id": str(raw.get("target_step_id") or "").strip(),
        "after_ms": int(raw.get("after_ms") or 0),
        "priority": int(raw.get("priority") or 0),
        "debug_only": bool(raw.get("debug_only", False)),
    }


def canonical_transition_key(tr: Transition) -> tuple[Any, ...]:
    return (
        tr["trigger"],
        tr["event_type"],
        tr["event_name"],
        tr["target_step_id"],
        tr["after_ms"],
        tr["priority"],
        tr["debug_only"],
    )


def extract_step_map(steps: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    out: dict[str, dict[str, Any]] = {}
    for step in steps:
        step_id = normalize_step_id(step)
        if step_id:
            out[step_id] = step
    return out


def parse_yaml_steps(path: Path) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    payload = yaml.safe_load(path.read_text())
    if not isinstance(payload, dict):
        raise ValueError(f"YAML root must be an object: {path}")
    steps = payload.get("steps") or []
    if not isinstance(steps, list):
        raise ValueError(f"YAML steps must be a list: {path}")
    return extract_step_map(steps), payload


def parse_json_steps(path: Path) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    payload = json.loads(path.read_text())
    steps = payload.get("steps") or []
    if not isinstance(steps, list):
        raise ValueError(f"JSON steps must be a list: {path}")
    return extract_step_map(steps), payload


def parse_embedded_default_steps(path: Path) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    text = path.read_text()
    match = re.search(r'/story/scenarios/DEFAULT\.json",\s*R"JSON\((.*?)\)JSON"\},', text, re.DOTALL)
    if not match:
        raise ValueError(f"Embedded DEFAULT scenario not found in: {path}")
    payload = json.loads(match.group(1))
    steps = payload.get("steps") or []
    if not isinstance(steps, list):
        raise ValueError("Embedded DEFAULT steps must be a list")
    return extract_step_map(steps), payload


def resolve_focus_steps(focus_text: str, all_step_ids: list[str]) -> list[str]:
    tokens = [token.strip().upper() for token in focus_text.split(",") if token.strip()]
    if not tokens:
        tokens = ["U_SON", "LA", "WIN_ETAPE", "WIN_ETAPE1"]
    if len(tokens) == 1 and tokens[0] == "ALL_DEFAULT":
        return all_step_ids

    steps: list[str] = []
    for token in tokens:
        step_id = STEP_ALIASES.get(token, token)
        if step_id not in all_step_ids:
            raise ValueError(f"unsupported focus token: {token}")
        if step_id not in steps:
            steps.append(step_id)
    return steps


def step_transition_list(step: dict[str, Any]) -> list[Transition]:
    transitions = step.get("transitions") or []
    if not isinstance(transitions, list):
        return []
    out: list[Transition] = []
    for item in transitions:
        if isinstance(item, dict):
            out.append(normalized_transition(item))
    return out


def find_orphan_targets(step_map: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    known_ids = set(step_map.keys())
    issues: list[dict[str, Any]] = []
    for step_id, step in step_map.items():
        for tr in step_transition_list(step):
            target = tr["target_step_id"]
            if target and target not in known_ids:
                issues.append({"step_id": step_id, "transition_id": tr["id"], "target_step_id": target})
    return issues


def find_ambiguous_priorities(step_map: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    for step_id, step in step_map.items():
        groups: dict[tuple[Any, ...], list[Transition]] = {}
        for tr in step_transition_list(step):
            key = (tr["trigger"], tr["event_type"], tr["event_name"], tr["after_ms"], tr["debug_only"])
            groups.setdefault(key, []).append(tr)
        for key, trs in groups.items():
            if len(trs) <= 1:
                continue
            # Ambiguous when same match key+priority points to multiple targets.
            by_priority: dict[int, set[str]] = {}
            for tr in trs:
                by_priority.setdefault(tr["priority"], set()).add(tr["target_step_id"])
            for priority, targets in by_priority.items():
                if len(targets) > 1:
                    issues.append(
                        {
                            "step_id": step_id,
                            "key": {
                                "trigger": key[0],
                                "event_type": key[1],
                                "event_name": key[2],
                                "after_ms": key[3],
                                "debug_only": key[4],
                            },
                            "priority": priority,
                            "targets": sorted(targets),
                        }
                    )
    return issues


def source_report(
    source_name: str,
    step_map: dict[str, dict[str, Any]],
    payload_root: dict[str, Any],
    focused_steps: list[str],
) -> dict[str, Any]:
    report: dict[str, Any] = {
        "source": source_name,
        "step_checks": [],
        "debug_transition_bypass_enabled": bool(payload_root.get("debug_transition_bypass_enabled", False)),
        "orphans": find_orphan_targets(step_map),
        "ambiguous_priorities": find_ambiguous_priorities(step_map),
        "ok": True,
    }

    for step_id in focused_steps:
        step = step_map.get(step_id)
        if step is None:
            report["step_checks"].append({"step_id": step_id, "ok": False, "reason": "missing_step"})
            report["ok"] = False
            continue
        transitions = step_transition_list(step)
        report["step_checks"].append(
            {
                "step_id": step_id,
                "ok": True,
                "transition_count": len(transitions),
                "transitions": sorted(canonical_transition_key(tr) for tr in transitions),
            }
        )

    if report["orphans"] or report["ambiguous_priorities"]:
        report["ok"] = False

    return report


def parity_report(reports: list[dict[str, Any]], focused_steps: list[str]) -> dict[str, Any]:
    out = {"ok": True, "steps": []}
    if not reports:
        out["ok"] = False
        return out

    baseline = reports[0]
    baseline_steps = {
        item["step_id"]: set(item.get("transitions", []))
        for item in baseline.get("step_checks", [])
        if item.get("ok")
    }
    baseline_debug_flag = baseline.get("debug_transition_bypass_enabled", False)

    for step_id in focused_steps:
        step_entry = {"step_id": step_id, "ok": True, "sources": []}
        ref_set = baseline_steps.get(step_id)
        if ref_set is None:
            step_entry["ok"] = False
            step_entry["reason"] = f"missing in {baseline['source']}"
            out["ok"] = False
            out["steps"].append(step_entry)
            continue

        for report in reports:
            checks = {item["step_id"]: item for item in report.get("step_checks", [])}
            current = checks.get(step_id)
            if current is None or not current.get("ok"):
                step_entry["ok"] = False
                step_entry["sources"].append({"source": report["source"], "ok": False, "reason": "missing_step"})
                continue
            current_set = set(current.get("transitions", []))
            same = current_set == ref_set
            step_entry["sources"].append({"source": report["source"], "ok": same})
            if not same:
                step_entry["ok"] = False
        if report.get("debug_transition_bypass_enabled", False) != baseline_debug_flag:
            step_entry["ok"] = False
        if not step_entry["ok"]:
            out["ok"] = False
        out["steps"].append(step_entry)

    for report in reports:
        if report.get("debug_transition_bypass_enabled", False) != baseline_debug_flag:
            out["ok"] = False

    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument("--focus", default="U_SON,LA,WIN_ETAPE,WIN_ETAPE1")
    parser.add_argument("--out", type=Path, default=None, help="Optional explicit output report path")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    fw_root = repo_root / "hardware" / "firmware"

    yaml_path = repo_root / "game" / "scenarios" / "default_unlock_win_etape2.yaml"
    json_path = fw_root / "data" / "story" / "scenarios" / "DEFAULT.json"
    fallback_candidates = [
        fw_root / "ui_freenove_allinone" / "src" / "storage_manager.cpp",
        fw_root / "hardware" / "firmware" / "ui_freenove_allinone" / "src" / "storage_manager.cpp",
    ]
    fallback_path = next((path for path in fallback_candidates if path.exists()), fallback_candidates[0])

    yaml_steps, yaml_root = parse_yaml_steps(yaml_path)
    json_steps, json_root = parse_json_steps(json_path)
    embedded_steps, embedded_root = parse_embedded_default_steps(fallback_path)

    all_step_ids = sorted(set(yaml_steps.keys()) | set(json_steps.keys()) | set(embedded_steps.keys()))
    focus_steps = resolve_focus_steps(args.focus, all_step_ids)

    reports = [
        source_report("canonical_yaml", yaml_steps, yaml_root, focus_steps),
        source_report("runtime_json", json_steps, json_root, focus_steps),
        source_report("embedded_fallback", embedded_steps, embedded_root, focus_steps),
    ]

    parity = parity_report(reports, focus_steps)
    overall_ok = parity["ok"] and all(report["ok"] for report in reports)

    final_report = {
        "focus_steps": focus_steps,
        "overall_ok": overall_ok,
        "parity": parity,
        "reports": reports,
    }

    if args.out is not None:
        out_path = args.out.resolve()
    else:
        ts = _dt.datetime.now(_dt.UTC).strftime("%Y%m%dT%H%M%SZ")
        out_path = fw_root / "artifacts" / "rc_live" / f"audit_scene_trigger_chain_{ts}.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(final_report, ensure_ascii=False, indent=2) + "\n")

    if overall_ok:
        print(f"[ok] trigger audit aligned -> {out_path}")
        return 0

    print(f"[fail] trigger audit divergence -> {out_path}")
    for report in reports:
        if report["ok"]:
            continue
        print(f"  - source={report['source']} issues")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
