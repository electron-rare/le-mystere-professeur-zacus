#!/usr/bin/env python3
"""Wrapper to run the canonical root script: tools/test/ui_link_sim.py."""

from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[4]
TARGET = ROOT / "tools" / "test" / "ui_link_sim.py"

sys.argv[0] = str(TARGET)
runpy.run_path(str(TARGET), run_name="__main__")
