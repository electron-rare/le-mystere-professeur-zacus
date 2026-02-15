#!/usr/bin/env bash
set -euo pipefail

CHECK_CLEAN_GIT=0

usage() {
  cat <<'USAGE'
Usage: bash tools/test/run_content_checks.sh [--check-clean-git]

Runs canonical content validations and markdown export.

Options:
  --check-clean-git  Fail if export updates tracked/untracked files under
                     kit-maitre-du-jeu/_generated or docs/_generated.
  -h, --help         Show this help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --check-clean-git)
      CHECK_CLEAN_GIT=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[fail] unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

if ! command -v python3 >/dev/null 2>&1; then
  echo "[fail] missing command: python3" >&2
  exit 127
fi

if ! python3 -c "import yaml" >/dev/null 2>&1; then
  echo "[fail] missing dependency: pip install pyyaml" >&2
  exit 3
fi

run_step() {
  echo "[step] $*"
  "$@"
}

run_step python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml
run_step python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml
run_step python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml
run_step python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml

if [[ "$CHECK_CLEAN_GIT" == "1" ]]; then
  if ! command -v git >/dev/null 2>&1; then
    echo "[fail] git is required for --check-clean-git" >&2
    exit 8
  fi
  if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[fail] current directory is not a git worktree" >&2
    exit 8
  fi

  STATUS_OUTPUT="$(git status --porcelain -- kit-maitre-du-jeu/_generated docs/_generated)"
  if [[ -n "$STATUS_OUTPUT" ]]; then
    echo "[fail] generated markdown is not clean. Commit or revert generated output." >&2
    echo "$STATUS_OUTPUT" >&2
    git --no-pager diff -- kit-maitre-du-jeu/_generated docs/_generated || true
    exit 9
  fi
  echo "[ok] generated markdown is clean in git"
fi

echo "[ok] content checks passed"
