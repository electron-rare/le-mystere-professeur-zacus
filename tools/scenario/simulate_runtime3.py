#!/usr/bin/env python3
"""Simulate the linear Runtime 3 route of a Zacus scenario."""

from __future__ import annotations

import argparse
from pathlib import Path

from runtime3_common import compile_runtime3_document, read_yaml, simulate_runtime3_document


def main() -> int:
    parser = argparse.ArgumentParser(description="Simulate a Zacus Runtime 3 document from a canonical YAML file.")
    parser.add_argument("scenario", help="Path to the canonical scenario YAML file")
    parser.add_argument("--max-steps", type=int, default=16, help="Maximum transitions to follow")
    args = parser.parse_args()

    scenario_path = Path(args.scenario)
    document = compile_runtime3_document(read_yaml(scenario_path))
    result = simulate_runtime3_document(document, max_steps=args.max_steps)
    print("[runtime3-sim] " + " -> ".join(result.history))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
