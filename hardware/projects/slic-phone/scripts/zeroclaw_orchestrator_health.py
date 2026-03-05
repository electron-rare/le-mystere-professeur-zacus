#!/usr/bin/env python3
"""ZeroClaw Docker orchestrator health check for A252 strict gate."""

from __future__ import annotations

import argparse
import json
import os
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Optional
from urllib import error, request


@dataclass
class StepResult:
    name: str
    ok: bool
    details: Dict[str, Any]


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def request_json(method: str, url: str, payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    body = None
    headers: Dict[str, str] = {}
    if payload is not None:
        body = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = request.Request(url, method=method, data=body, headers=headers)
    with request.urlopen(req, timeout=15) as resp:
        raw = resp.read().decode("utf-8")
        if not raw:
            return {}
        return json.loads(raw)


def run_step(name: str, method: str, url: str, payload: Optional[Dict[str, Any]] = None) -> StepResult:
    try:
        data = request_json(method, url, payload)
        return StepResult(name=name, ok=True, details={"url": url, "response": data})
    except error.HTTPError as exc:
        details: Dict[str, Any] = {"url": url, "error": f"HTTP {exc.code}"}
        try:
            details["body"] = exc.read().decode("utf-8")
        except Exception:
            details["body"] = ""
        return StepResult(name=name, ok=False, details=details)
    except Exception as exc:
        return StepResult(name=name, ok=False, details={"url": url, "error": str(exc)})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ZeroClaw orchestrator health check")
    parser.add_argument(
        "--base-url",
        default=os.environ.get("ZEROCLAW_ORCH", "http://127.0.0.1:8788"),
        help="ZeroClaw orchestrator base URL (default: $ZEROCLAW_ORCH or http://127.0.0.1:8788)",
    )
    parser.add_argument(
        "--report-json",
        default="artifacts/zeroclaw_orchestrator_health.json",
        help="JSON report output path",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    base_url = args.base_url.rstrip("/")

    steps = [
        run_step("status", "GET", f"{base_url}/api/status"),
        run_step("agents", "GET", f"{base_url}/api/agents"),
        run_step("workflows", "GET", f"{base_url}/api/workflows"),
        run_step("provider_scan", "POST", f"{base_url}/api/run", {"action": "provider_scan"}),
    ]

    overall_ok = all(step.ok for step in steps)
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "base_url": base_url,
        "overall_passed": overall_ok,
        "results": [
            {"name": step.name, "state": "PASS" if step.ok else "FAIL", "details": step.details} for step in steps
        ],
    }

    report_path = Path(args.report_json)
    ensure_parent(report_path)
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
