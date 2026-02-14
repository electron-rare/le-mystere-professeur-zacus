#!/usr/bin/env bash
set -euo pipefail
git switch scripts/generation
git pull --ff-only
echo "Scope: tools/** + Makefile + .github/workflows/validate.yml"
echo "Tip: In Codex, say: 'ONLY edit tools/** and validate workflow. Do not touch game/** or hardware/**.'"
