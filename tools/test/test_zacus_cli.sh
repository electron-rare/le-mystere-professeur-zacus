#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIXTURE="$ROOT/tools/test/fixtures/ports_list_macos.txt"
ZACUS_CMD="$ROOT/tools/dev/zacus.sh"

run_help() {
  printf '== running help ==\n'
  ZACUS_MOCK_PORTS=1 ZACUS_PORTS_FIXTURE="$FIXTURE" "$ZACUS_CMD" help >/dev/null
}

run_ports() {
  printf '== running ports (mock) ==\n'
  ZACUS_MOCK_PORTS=1 ZACUS_PORTS_FIXTURE="$FIXTURE" "$ZACUS_CMD" ports >/dev/null
  local json="$ROOT/artifacts/ports/latest_ports_resolve.json"
  if [[ ! -f "$json" ]]; then
    printf 'ports json missing: %s\n' "$json"
    exit 1
  fi
  python3 - "$json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text(encoding='utf-8'))
if 'status' not in data:
    raise SystemExit('status missing')
print('ports status:', data['status'])
PY
}

run_codex_expect_fail() {
  printf '== running codex (expect exit 2) ==\n'
  set +e
  PATH="/bin:/usr/bin" ZACUS_MOCK_PORTS=1 ZACUS_PORTS_FIXTURE="$FIXTURE" "$ZACUS_CMD" codex --prompt "$ROOT/tools/dev/codex_prompts/zacus_hw_now.md"
  local code=$?
  set -e
  if [[ $code -ne 2 ]]; then
    printf 'expected codex exit 2, got %d\n' "$code"
    exit 1
  fi
  printf 'codex exit as expected\n'
}

run_menu_non_tui() {
  printf '== running menu --no-tui ==\n'
  ZACUS_MOCK_PORTS=1 ZACUS_PORTS_FIXTURE="$FIXTURE" "$ZACUS_CMD" menu --no-tui >/dev/null
}

run_help
run_ports
run_codex_expect_fail
run_menu_non_tui
