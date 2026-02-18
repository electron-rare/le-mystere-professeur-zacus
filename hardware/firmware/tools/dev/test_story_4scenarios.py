#!/usr/bin/env python3
"""Compatibility wrapper for the Story 4-scenarios smoke script."""

from __future__ import annotations

import runpy
import sys
from pathlib import Path


def main() -> int:
    fw_root = Path(__file__).resolve().parents[2]
    target = fw_root / "lib" / "zacus_story_portable" / "test_story_4scenarios.py"
    if not target.exists():
        print(f"missing script: {target}", file=sys.stderr)
        return 2
    runpy.run_path(str(target), run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
