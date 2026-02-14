#!/usr/bin/env bash
set -euo pipefail
git switch generation/story-esp
git pull --ff-only
echo "Scope: hardware/firmware/esp32/src/story/** + controllers/story/** + serial_commands_story.* + la_detector.* + docs story v2"
echo "Tip: In Codex, say: 'ONLY edit the story engine files listed in CODEX_RULES.md. No wifi/web/mp3 stack changes.'"
