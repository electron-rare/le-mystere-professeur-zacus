#!/usr/bin/env python3
"""Generate docs/_generated/COCKPIT_COMMANDS.md from cockpit_commands.yaml."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, Dict, List

from cockpit_registry import load_registry


def format_list(items: List[str]) -> str:
    if not items:
        return ""
    return "<br>".join(items)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate cockpit command docs")
    parser.add_argument(
        "--input",
        default="tools/dev/cockpit_commands.yaml",
        help="Registry YAML path",
    )
    parser.add_argument(
        "--output",
        default="docs/_generated/COCKPIT_COMMANDS.md",
        help="Output markdown path",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    registry = load_registry(input_path)
    commands = registry.get("commands", [])

    lines = [
        "# Cockpit Commands",
        "",
        "Generated from tools/dev/cockpit_commands.yaml. Do not edit manually.",
        "",
        "| ID | Description | Entrypoint | Args | Runbook | Evidence |",
        "| --- | --- | --- | --- | --- | --- |",
    ]

    for entry in commands:
        cmd_id = str(entry.get("id", ""))
        desc = str(entry.get("description", ""))
        entrypoint = str(entry.get("entrypoint", ""))
        args_list = entry.get("args", []) or []
        runbook = str(entry.get("runbook_ref", ""))
        evidence = entry.get("evidence_outputs", []) or []
        lines.append(
            "| {id} | {desc} | {entry} | {args} | {runbook} | {evidence} |".format(
                id=cmd_id,
                desc=desc,
                entry=entrypoint,
                args=format_list([str(a) for a in args_list]),
                runbook=runbook,
                evidence=format_list([str(e) for e in evidence]),
            )
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
