#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
MANIFEST="${SCRIPT_DIR}/font_manifest.json"
OUT_DIR="${REPO_ROOT}/hardware/firmware/ui_freenove_allinone/src/ui/fonts"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required" >&2
  exit 1
fi

if ! command -v npx >/dev/null 2>&1; then
  echo "npx is required (for lv_font_conv)" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

BPP="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["bpp"])' "${MANIFEST}")"
SYMBOLS="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["symbols"])' "${MANIFEST}")"

RANGES=()
while IFS= read -r value; do
  RANGES+=("${value}")
done < <(python3 - <<'PY' "${MANIFEST}"
import json, sys
manifest = json.load(open(sys.argv[1]))
for value in manifest["ranges"]:
    print(value)
PY
)

range_args=()
for value in "${RANGES[@]}"; do
  range_args+=(--range "$value")
done

while IFS=$'\t' read -r font_file prefix size optional_define; do
  if [[ -n "${optional_define}" ]]; then
    enabled="${!optional_define:-1}"
    if [[ "${enabled}" == "0" ]]; then
      echo "[fonts] skip ${prefix}_${size} (disabled by ${optional_define})"
      continue
    fi
  fi

  src="${REPO_ROOT}/${font_file}"
  out="${OUT_DIR}/${prefix}_${size}.c"
  if [[ ! -f "${src}" ]]; then
    echo "[fonts] missing source ${src}, skip"
    continue
  fi

  echo "[fonts] generate ${out}"
  npx --yes lv_font_conv \
    --font "${src}" \
    --size "${size}" \
    --bpp "${BPP}" \
    --format lvgl \
    --output "${out}" \
    --lv-include lvgl.h \
    "${range_args[@]}" \
    --symbols "${SYMBOLS}"

  # Keep generated comments portable: strip machine-specific absolute paths.
  python3 - <<'PY' "${out}"
import os
import re
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as fh:
    data = fh.read()

data = re.sub(
    r"--font\s+(.+?)\s+--size",
    lambda m: f"--font {os.path.basename(m.group(1))} --size",
    data,
)
data = re.sub(
    r"--output\s+(.+?)\s+--range",
    lambda m: f"--output {os.path.basename(m.group(1))} --range",
    data,
)

with open(path, "w", encoding="utf-8") as fh:
    fh.write(data)
PY
done < <(python3 - <<'PY' "${MANIFEST}"
import json, sys
manifest = json.load(open(sys.argv[1]))
for font in manifest["fonts"]:
    opt = font.get("optional_define", "")
    for size in font["sizes"]:
        print(f"{font['file']}\t{font['prefix']}\t{size}\t{opt}")
PY
)

echo "[fonts] done"
