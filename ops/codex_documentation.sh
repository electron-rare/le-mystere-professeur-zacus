#!/usr/bin/env bash
set -euo pipefail
git switch documentation
git pull --ff-only
echo "Scope: docs/** + root docs files"
echo "Tip: In Codex, say: 'ONLY edit docs/** and root markdown/policy files.'"
