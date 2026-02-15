#!/usr/bin/env bash
set -euo pipefail

FW_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$FW_ROOT"

exec bash "./tools/dev/run_matrix_and_smoke.sh" "$@"
