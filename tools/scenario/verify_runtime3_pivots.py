#!/usr/bin/env python3
"""Verify that Runtime 3 compilation preserves Zacus pivot steps and route order."""

from __future__ import annotations

import argparse
from pathlib import Path

from runtime3_common import compile_runtime3_document, read_yaml, simulate_runtime3_document, validate_runtime3_document


REQUIRED_STEPS = (
    "STEP_U_SON_PROTO",
    "STEP_LA_DETECTOR",
    "STEP_LEFOU_DETECTOR",
    "STEP_QR_DETECTOR",
    "STEP_FINAL_WIN",
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the canonical Runtime 3 pivots and route order.")
    parser.add_argument("scenario", help="Path to the canonical scenario YAML file")
    args = parser.parse_args()

    document = compile_runtime3_document(read_yaml(Path(args.scenario)))
    validate_runtime3_document(document)
    if document.get("metadata", {}).get("migration_mode") != "firmware_import":
        raise SystemExit("[runtime3-pivots] canonical scenario must compile via firmware_import")
    steps_by_id = {step["id"]: step for step in document["steps"]}

    for step_id in REQUIRED_STEPS:
        if step_id not in steps_by_id:
            raise SystemExit(f"[runtime3-pivots] missing required step: {step_id}")

    history = simulate_runtime3_document(document, max_steps=16).history
    expected_order = list(REQUIRED_STEPS)
    cursor = 0
    for step_id in history:
        if cursor < len(expected_order) and step_id == expected_order[cursor]:
            cursor += 1
    if cursor != len(expected_order):
        raise SystemExit(
            "[runtime3-pivots] route order mismatch: "
            + " -> ".join(history)
        )

    la_targets = {transition["target_step_id"] for transition in steps_by_id["STEP_LA_DETECTOR"]["transitions"]}
    lefou_targets = {transition["target_step_id"] for transition in steps_by_id["STEP_LEFOU_DETECTOR"]["transitions"]}
    qr_targets = {transition["target_step_id"] for transition in steps_by_id["STEP_QR_DETECTOR"]["transitions"]}

    if "STEP_RTC_ESP_ETAPE1" not in la_targets:
        raise SystemExit("[runtime3-pivots] LA detector does not lead to STEP_RTC_ESP_ETAPE1")
    if "STEP_RTC_ESP_ETAPE2" not in lefou_targets:
        raise SystemExit("[runtime3-pivots] LEFOU detector does not lead to STEP_RTC_ESP_ETAPE2")
    if "STEP_FINAL_WIN" not in qr_targets:
        raise SystemExit("[runtime3-pivots] QR detector does not lead to STEP_FINAL_WIN")

    print(
        "[runtime3-pivots] ok "
        f"entry={document['scenario']['entry_step_id']} "
        f"steps={len(document['steps'])} "
        f"history={' -> '.join(history)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
