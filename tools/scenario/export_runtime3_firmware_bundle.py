#!/usr/bin/env python3
"""Export the canonical Runtime 3 JSON into the firmware LittleFS tree."""

from __future__ import annotations

import argparse
from pathlib import Path

from runtime3_common import compile_runtime3_document, dump_json, read_yaml, validate_runtime3_document


def main() -> int:
    parser = argparse.ArgumentParser(description="Export Runtime 3 JSON into the firmware data tree.")
    parser.add_argument("scenario", help="Path to the canonical scenario YAML file")
    parser.add_argument(
        "-o",
        "--out",
        default="hardware/firmware/data/story/runtime3/DEFAULT.json",
        help="Output path for the firmware Runtime 3 JSON",
    )
    args = parser.parse_args()

    out_path = Path(args.out)
    document = compile_runtime3_document(read_yaml(Path(args.scenario)))
    validate_runtime3_document(document)
    dump_json(document, out_path)
    print(
        "[runtime3-export] ok "
        f"path={out_path} scenario={document['scenario']['id']} steps={len(document['steps'])}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
