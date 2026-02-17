#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FW_RUNNER="$REPO_ROOT/hardware/firmware/tools/dev/plan_runner.sh"

if [[ ! -x "$FW_RUNNER" ]]; then
  echo "Error: firmware plan runner not found or not executable: $FW_RUNNER" >&2
  exit 2
fi

exec "$FW_RUNNER" "$@"
