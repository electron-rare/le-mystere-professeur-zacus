#!/usr/bin/env bash
# Run WhisperX STT server on KXKM-AI
# Usage: ssh kxkm@kxkm-ai 'bash -s' < tools/stt/run_whisperx.sh
set -euo pipefail

cd "$(dirname "$0")"

# Install deps if needed
pip install -q whisperx fastapi uvicorn python-multipart 2>/dev/null || true

echo "Starting WhisperX STT on port 8901..."
python3 whisperx_server.py --port 8901 --model large-v3
