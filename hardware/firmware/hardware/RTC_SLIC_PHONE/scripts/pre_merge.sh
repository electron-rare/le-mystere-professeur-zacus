#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "[pre_merge] this script is deprecated, use scripts/branch_gate.sh"
exec "${SCRIPT_DIR}/branch_gate.sh" "$@"
