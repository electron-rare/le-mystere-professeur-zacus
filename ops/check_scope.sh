#!/usr/bin/env bash
set -euo pipefail

branch="$(git branch --show-current)"
changed="$(git diff --name-only --cached || true)"

if [[ -z "$changed" ]]; then
  echo "No staged changes."
  exit 0
fi

echo "Branch: $branch"
echo "Staged files:"
echo "$changed" | sed 's/^/  - /'

case "$branch" in
  hardware/firmware)
    bad=$(echo "$changed" | egrep -v '^(hardware/firmware/|tools/qa/|\.github/workflows/firmware-).*' || true)
    ;;
  generation/story-esp)
    bad=$(echo "$changed" | egrep -v '^(hardware/firmware/esp32/src/story/|hardware/firmware/esp32/src/controllers/story/|hardware/firmware/esp32/src/services/serial/serial_commands_story\.(cpp|h)$|hardware/firmware/esp32/src/la_detector\.(cpp|h)$|hardware/firmware/esp32/GENERER_UN_SCENARIO_STORY_V2\.md$|hardware/firmware/esp32/RELEASE_STORY_V2\.md$|hardware/firmware/esp32/docs/workstreams/).*' || true)
    ;;
  generation/story-ia)
    bad=$(echo "$changed" | egrep -v '^(game/|audio/|printables/|kit-maitre-du-jeu/|include-humain-IA/).*' || true)
    ;;
  scripts/generation)
    bad=$(echo "$changed" | egrep -v '^(tools/|Makefile$|\.github/workflows/validate\.yml$).*' || true)
    ;;
  documentation)
    bad=$(echo "$changed" | egrep -v '^(docs/|README\.md$|CONTRIBUTING\.md$|LICENSE\.md$|CHANGELOG\.md$|AGENTS\.md$|DISCLAIMER\.md$|SECURITY\.md$|CODE_OF_CONDUCT\.md$|CODEX_RULES\.md$|ops/).*' || true)
    ;;
  *)
    echo "No scope rules for branch '$branch'."
    exit 0
    ;;
esac

if [[ -n "${bad:-}" ]]; then
  echo ""
  echo "ERROR: files outside scope detected:"
  echo "$bad" | sed 's/^/  - /'
  echo ""
  echo "Fix: unstage/revert them or move work to the correct branch."
  exit 1
fi

echo "OK: scope respected."
