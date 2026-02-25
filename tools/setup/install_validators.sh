#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<'USAGE'
Usage: bash tools/setup/install_validators.sh

Install Python dependencies required by repository validators.
Environment override:
  PYTHON_BIN   Python executable to use (default: python3)
USAGE
  exit 0
fi

PYTHON_BIN="${PYTHON_BIN:-python3}"
REQ_FILE="tools/requirements/validators.txt"

if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  echo "[install-validators] error: python interpreter not found: $PYTHON_BIN" >&2
  exit 1
fi

if [[ ! -f "$REQ_FILE" ]]; then
  echo "[install-validators] error: missing requirements file: $REQ_FILE" >&2
  exit 1
fi

"$PYTHON_BIN" -m pip install --user -r "$REQ_FILE"
echo "[install-validators] ok installed requirements from $REQ_FILE"
