#!/usr/bin/env bash
set -euo pipefail
git switch generation/story-ia
git pull --ff-only
echo "Scope: game/** audio/** printables/** kit-maitre-du-jeu/** scenario-ai-coherence/**"
echo "Tip: In Codex, say: 'ONLY edit those folders. Do not touch hardware/** or tools/**.'"
