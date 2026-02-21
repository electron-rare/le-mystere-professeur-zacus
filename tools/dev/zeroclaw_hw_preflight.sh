#!/usr/bin/env bash
set -euo pipefail

ZEROCLAW_BIN="${ZEROCLAW_BIN:-/Users/cils/Documents/Lelectron_rare/Kill_LIFE/zeroclaw/target/release/zeroclaw}"
REQUIRE_PORT=0
declare -a PORTS=()

usage() {
  cat <<'USAGE'
Usage: zeroclaw_hw_preflight.sh [--zeroclaw-bin <path>] [--port <tty>]... [--require-port]

Run ZeroClaw USB discovery + per-port introspection before hardware actions.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --zeroclaw-bin)
      ZEROCLAW_BIN="${2:-}"
      shift 2
      ;;
    --port)
      PORTS+=("${2:-}")
      shift 2
      ;;
    --require-port)
      REQUIRE_PORT=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ ! -x "$ZEROCLAW_BIN" ]]; then
  if command -v "$ZEROCLAW_BIN" >/dev/null 2>&1; then
    ZEROCLAW_BIN="$(command -v "$ZEROCLAW_BIN")"
  elif command -v zeroclaw >/dev/null 2>&1; then
    ZEROCLAW_BIN="$(command -v zeroclaw)"
  else
    echo "[fail] zeroclaw binary not found (use --zeroclaw-bin)." >&2
    exit 2
  fi
fi

echo "$ $ZEROCLAW_BIN hardware discover"
"$ZEROCLAW_BIN" hardware discover

if [[ ${#PORTS[@]} -eq 0 ]]; then
  while IFS= read -r candidate; do PORTS+=("$candidate"); done < <(
    {
      ls /dev/tty.SLAB_USBtoUART 2>/dev/null || true
      ls /dev/tty.usbserial-* 2>/dev/null || true
      ls /dev/tty.usbmodem* 2>/dev/null || true
    } | awk 'NF' | sort -u
  )
fi

if [[ ${#PORTS[@]} -eq 0 ]]; then
  if [[ "$REQUIRE_PORT" == "1" ]]; then
    echo "[fail] no candidate serial ports detected." >&2
    exit 3
  fi
  echo "[warn] no candidate serial ports detected."
  exit 0
fi

failures=0
for port in "${PORTS[@]}"; do
  echo "$ $ZEROCLAW_BIN hardware introspect $port"
  if ! "$ZEROCLAW_BIN" hardware introspect "$port"; then
    failures=$((failures + 1))
  fi
done

if [[ "$failures" -gt 0 ]]; then
  echo "[fail] introspection failed on $failures port(s)." >&2
  exit 4
fi

echo "[ok] zeroclaw hardware preflight passed."
