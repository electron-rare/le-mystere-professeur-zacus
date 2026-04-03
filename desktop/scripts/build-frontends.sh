#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FRONTEND_V3="$REPO_ROOT/frontend-v3"
DESKTOP_DIST="$REPO_ROOT/desktop/dist/renderer/apps"

echo "Building frontend-v3 apps..."
mkdir -p "$DESKTOP_DIST"

for app in editor dashboard simulation; do
  echo "  -> Building $app..."
  (cd "$FRONTEND_V3/apps/$app" && npm run build -- --outDir "$DESKTOP_DIST/$app" --base "./apps/$app/")
done

echo "Done: frontend-v3 apps built to $DESKTOP_DIST"
