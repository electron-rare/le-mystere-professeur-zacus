#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT/hardware/firmware"

exec bash "./tools/dev/run_matrix_and_smoke.sh" "$@"
