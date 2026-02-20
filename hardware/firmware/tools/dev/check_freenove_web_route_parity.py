#!/usr/bin/env python3
"""Static route parity check for Freenove embedded WebUI.

Checks that every frontend route fetched from inline HTML/JS exists in
backend route registrations (`g_web_server.on(...)`) in main.cpp.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


BACKEND_RE = re.compile(r'g_web_server\.on\(\s*"(?P<path>/api/[^"]+)"\s*,\s*HTTP_(?P<method>[A-Z]+)')
FRONTEND_FETCH_RE = re.compile(r'fetch\(\s*"(?P<path>/api/[^"]+)"')
FRONTEND_POST_CALL_RE = re.compile(r'post\(\s*"(?P<path>/api/[^"]+)"')
FRONTEND_METHOD_RE = re.compile(r'method:\s*"(?P<method>[A-Z]+)"')


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check Freenove WebUI route parity.")
    parser.add_argument(
        "--source",
        default="hardware/firmware/hardware/firmware/ui_freenove_allinone/src/main.cpp",
        help="Source file containing backend routes and embedded frontend HTML.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source_path = Path(args.source)
    text = source_path.read_text(encoding="utf-8", errors="ignore")

    backend = {(m.group("method"), m.group("path")) for m in BACKEND_RE.finditer(text)}
    frontend = set()

    for match in FRONTEND_FETCH_RE.finditer(text):
        path = match.group("path")
        method = "GET"
        start = match.start()
        end = min(len(text), start + 220)
        near = text[start:end]
        method_match = FRONTEND_METHOD_RE.search(near)
        if method_match:
            method = method_match.group("method")
        frontend.add((method, path))
    for match in FRONTEND_POST_CALL_RE.finditer(text):
        frontend.add(("POST", match.group("path")))

    if not backend:
        print("[freenove-route-parity] no backend routes found", file=sys.stderr)
        return 2
    if not frontend:
        print("[freenove-route-parity] no frontend routes found", file=sys.stderr)
        return 2

    missing = sorted(frontend - backend, key=lambda item: (item[1], item[0]))
    unused = sorted(backend - frontend, key=lambda item: (item[1], item[0]))

    print(f"[freenove-route-parity] backend routes: {len(backend)}")
    print(f"[freenove-route-parity] frontend routes: {len(frontend)}")
    if unused:
        print(f"[freenove-route-parity] backend unused by frontend: {len(unused)}")

    if missing:
        print("[freenove-route-parity] FAIL missing routes:")
        for method, path in missing:
            print(f"  - {method} {path}")
        return 1

    print("[freenove-route-parity] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
