#!/usr/bin/env python3
from __future__ import annotations
import json
import subprocess
import sys
from pathlib import Path


SCENARIO = Path("game/scenarios/zacus_v1.yaml")


def load_yaml(path: Path):
    ruby = "require 'yaml'; require 'json'; puts JSON.generate(YAML.load_file(ARGV[0]))"
    p = subprocess.run(["ruby", "-e", ruby, str(path)], capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError(p.stderr.strip() or "YAML parse error")
    return json.loads(p.stdout)


def dup_ids(items, key="id"):
    seen, dups = set(), set()
    for i in items:
        v = i.get(key)
        if v in seen:
            dups.add(v)
        seen.add(v)
    return sorted(dups)


def main() -> int:
    if not SCENARIO.exists():
        print(f"ERROR: missing {SCENARIO}")
        return 1
    doc = load_yaml(SCENARIO)
    errors = []

    zones = doc.get("zones", [])
    suspects = doc.get("suspects", [])
    props = doc.get("props", [])
    steps = doc.get("steps", [])

    for label, arr in [("zones", zones), ("suspects", suspects), ("props", props), ("steps", steps)]:
        d = dup_ids(arr)
        if d:
            errors.append(f"duplicate {label} ids: {d}")

    suspect_ids = {s.get("id") for s in suspects}
    zone_ids = {z.get("id") for z in zones}
    prop_ids = {p.get("id") for p in props}

    for z in zones:
        sid = z.get("responsible_suspect_id")
        if sid not in suspect_ids and sid != "S_NONE":
            errors.append(f"zone {z.get('id')} references unknown suspect {sid}")

    for st in steps:
        for station in st.get("stations", []):
            zid = station.get("zone_id")
            if zid not in zone_ids:
                errors.append(f"step {st.get('id')} references unknown zone {zid}")

    sol = doc.get("solution", {})
    if not sol.get("unique"):
        errors.append("solution.unique must be true")
    culprit = sol.get("culprit_suspect_id")
    if culprit not in suspect_ids:
        errors.append("solution culprit invalid")

    if any(st.get("id") == "ETAPE_3" and not st.get("explicit_false_lead") for st in steps):
        errors.append("ETAPE_3 must be flagged explicit_false_lead")

    required_props = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"}
    if not required_props.issubset(prop_ids):
        errors.append(f"missing required props: {sorted(required_props - prop_ids)}")

    if errors:
        print("SCENARIO VALIDATION: FAIL")
        for e in errors:
            print(" -", e)
        return 2

    print("SCENARIO VALIDATION: PASS")
    print(f" - zones={len(zones)} suspects={len(suspects)} props={len(props)} steps={len(steps)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
