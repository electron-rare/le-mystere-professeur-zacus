#!/usr/bin/env python3
"""Compile canonical Zacus scenario YAML into Zacus Runtime 3 IR JSON."""

from __future__ import annotations

import argparse
from pathlib import Path

from runtime3_common import compile_runtime3_document, dump_json, read_yaml, validate_runtime3_document


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile a Zacus scenario YAML into Runtime 3 IR JSON.")
    parser.add_argument("scenario", help="Path to the canonical scenario YAML file")
    parser.add_argument("-o", "--out", help="Write JSON to this file instead of stdout")
    args = parser.parse_args()

    scenario_path = Path(args.scenario)
    document = compile_runtime3_document(read_yaml(scenario_path))
    validate_runtime3_document(document)
    dump_json(document, Path(args.out) if args.out else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
