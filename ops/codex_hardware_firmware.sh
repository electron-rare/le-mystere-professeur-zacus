#!/usr/bin/env bash
set -euo pipefail
git switch hardware/firmware
git pull --ff-only
echo "Scope: hardware/firmware/** (+ tools/qa/** optional)"
echo "Tip: In Codex, say: 'ONLY edit hardware/firmware/** and tools/qa/**. Do not touch anything else.'"
