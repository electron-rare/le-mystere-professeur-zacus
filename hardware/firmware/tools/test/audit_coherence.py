#!/usr/bin/env python3
"""Audit coherence between cockpit registry, docs, and scripts."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

FW_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = FW_ROOT.parent.parent
PHASE = "audit"

REGISTRY_PATH = FW_ROOT / "tools" / "dev" / "cockpit_commands.yaml"
DOCS_TO_SCAN = [
    FW_ROOT / "README.md",
    FW_ROOT / "docs" / "QUICKSTART.md",
    FW_ROOT / "docs" / "RC_FINAL_BOARD.md",
    FW_ROOT / "docs" / "RC_FINAL_REPORT_TEMPLATE.md",
    FW_ROOT / "docs" / "TEST_SCRIPT_COORDINATOR.md",
]


def init_evidence(outdir: Optional[str]) -> Dict[str, Path]:
    stamp = datetime.utcnow().strftime("%Y%m%d-%H%M%S")
    if outdir:
        path = Path(outdir)
        if not path.is_absolute():
            path = FW_ROOT / path
    else:
        path = FW_ROOT / "artifacts" / PHASE / stamp
    path.mkdir(parents=True, exist_ok=True)
    return {
        "dir": path,
        "meta": path / "meta.json",
        "git": path / "git.txt",
        "commands": path / "commands.txt",
        "summary": path / "summary.md",
    }


def write_git_info(dest: Path) -> None:
    lines = []
    try:
        branch = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "--abbrev-ref",
            "HEAD",
        ], text=True).strip()
    except Exception:
        branch = "n/a"
    try:
        commit = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "HEAD",
        ], text=True).strip()
    except Exception:
        commit = "n/a"
    lines.append(f"branch: {branch}")
    lines.append(f"commit: {commit}")
    lines.append("status:")
    try:
        status = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "status",
            "--porcelain",
        ], text=True).strip()
    except Exception:
        status = ""
    if status:
        lines.append(status)
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_meta_json(dest: Path, command: str) -> None:
    payload = {
        "timestamp": datetime.utcnow().strftime("%Y%m%d-%H%M%S"),
        "phase": PHASE,
        "utc": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
        "command": command,
        "cwd": str(Path.cwd()),
        "repo_root": str(REPO_ROOT),
        "fw_root": str(FW_ROOT),
    }
    dest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def record_command(commands_path: Path, cmd: str) -> None:
    with commands_path.open("a", encoding="utf-8") as fp:
        fp.write(cmd + "\n")


def write_summary(dest: Path, result: str, issues: List[str]) -> None:
    lines = ["# Coherence audit summary", "", f"- Result: **{result}**"]
    if issues:
        lines.append("- Issues:")
        lines.extend([f"  - {issue}" for issue in issues])
    else:
        lines.append("- Issues: none")
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_registry() -> Dict[str, object]:
    sys.path.insert(0, str(FW_ROOT / "tools" / "dev"))
    from cockpit_registry import load_registry as load  # type: ignore

    return load(REGISTRY_PATH)


def extract_cockpit_commands(text: str) -> List[str]:
    matches = re.findall(r"cockpit\.sh\s+([a-zA-Z0-9_-]+)", text)
    return sorted(set(matches))


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit cockpit/doc coherence")
    parser.add_argument("--outdir", default="", help="Evidence output directory")
    args = parser.parse_args()

    outdir = args.outdir or os.environ.get("ZACUS_OUTDIR", "") or None
    evidence = init_evidence(outdir)
    write_git_info(evidence["git"])
    evidence["commands"].write_text("# Commands\n", encoding="utf-8")
    record_command(evidence["commands"], " ".join(sys.argv))
    write_meta_json(evidence["meta"], " ".join(sys.argv))

    issues: List[str] = []
    registry = load_registry()
    commands = registry.get("commands", []) if isinstance(registry, dict) else []
    ids = {str(cmd.get("id", "")) for cmd in commands if isinstance(cmd, dict)}

    for doc_path in DOCS_TO_SCAN:
        if not doc_path.exists():
            issues.append(f"Missing doc: {doc_path.relative_to(FW_ROOT)}")
            continue
        text = doc_path.read_text(encoding="utf-8")
        for cmd in extract_cockpit_commands(text):
            if cmd not in ids:
                issues.append(f"Doc references unknown cockpit command: {cmd} ({doc_path.relative_to(FW_ROOT)})")

    for cmd in commands:
        if not isinstance(cmd, dict):
            continue
        cmd_id = str(cmd.get("id", ""))
        entrypoint = str(cmd.get("entrypoint", ""))
        runbook_ref = str(cmd.get("runbook_ref", ""))
        evidence_outputs = cmd.get("evidence_outputs", [])

        if entrypoint:
            entry_path = FW_ROOT / entrypoint
            if not entry_path.exists():
                issues.append(f"Entrypoint missing for {cmd_id}: {entrypoint}")

        if runbook_ref:
            # Extract file path (before any anchor "#")
            runbook_file = runbook_ref.split("#")[0]
            runbook_path = FW_ROOT / runbook_file
            if not runbook_path.exists():
                issues.append(f"Runbook missing for {cmd_id}: {runbook_ref}")

        if evidence_outputs:
            for out in evidence_outputs:
                if not (str(out).startswith("artifacts/") or str(out).startswith("logs/")):
                    issues.append(f"Evidence path for {cmd_id} must be under artifacts/ or logs/: {out}")

    result = "PASS" if not issues else "FAIL"
    write_summary(evidence["summary"], result, issues)
    print(f"RESULT={result}")
    return 0 if result == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
