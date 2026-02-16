#!/usr/bin/env bash
set -euo pipefail

PORT="${MOCK_PORT:-18080}"
LOG_PATH="${MOCK_LOG_PATH:-/tmp/story_http_mock_${PORT}.log}"

python3 - "$PORT" "$LOG_PATH" <<'PY' &
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

port = int(sys.argv[1])
log_path = sys.argv[2]

SCENARIOS = ["DEFAULT", "EXPRESS", "EXPRESS_DONE", "SPECTRE"]

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        with open(log_path, "a", encoding="utf-8") as fp:
            fp.write(fmt % args)
            fp.write("\n")

    def _send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/api/story/list":
            payload = {"scenarios": [{"id": s, "version": 2} for s in SCENARIOS]}
            return self._send_json(200, payload)
        if self.path.startswith("/api/audit/log"):
            return self._send_json(200, {"events": []})
        if self.path == "/api/story/fs-info":
            return self._send_json(200, {"ok": True})
        if self.path == "/api/story/status":
            return self._send_json(200, {"status": "running", "step": "STEP_A"})
        return self._send_json(404, {"error": "not_found"})

    def do_POST(self):
        if self.path.startswith("/api/story/select/"):
            return self._send_json(200, {"ok": True})
        if self.path == "/api/story/start":
            return self._send_json(200, {"status": "running"})
        if self.path == "/api/story/pause":
            return self._send_json(200, {"status": "paused"})
        if self.path == "/api/story/resume":
            return self._send_json(200, {"status": "running"})
        if self.path == "/api/story/skip":
            return self._send_json(200, {"status": "running", "step": "STEP_B"})
        if self.path == "/api/story/validate":
            return self._send_json(200, {"ok": True})
        if self.path == "/api/story/deploy":
            return self._send_json(200, {"ok": True})
        if self.path == "/api/story/serial-command":
            return self._send_json(200, {"ok": True, "output": "STORY_V2_STATUS"})
        return self._send_json(404, {"error": "not_found"})

HTTPServer(("127.0.0.1", port), Handler).serve_forever()
PY

SERVER_PID=$!
cleanup() {
  kill "$SERVER_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

BASE_URL="http://127.0.0.1:${PORT}"

sleep 0.4

curl -s "${BASE_URL}/api/story/list" | grep -q "scenarios" || { echo "FAIL list"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/select/DEFAULT" | grep -q "ok" || { echo "FAIL select"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/start" | grep -q "running" || { echo "FAIL start"; exit 1; }
curl -s "${BASE_URL}/api/story/status" | grep -q "status" || { echo "FAIL status"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/pause" | grep -q "paused" || { echo "FAIL pause"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/resume" | grep -q "running" || { echo "FAIL resume"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/skip" | grep -q "STEP_B" || { echo "FAIL skip"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/validate" | grep -q "ok" || { echo "FAIL validate"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/deploy" | grep -q "ok" || { echo "FAIL deploy"; exit 1; }
curl -s "${BASE_URL}/api/audit/log?limit=50" | grep -q "events" || { echo "FAIL audit"; exit 1; }
curl -s "${BASE_URL}/api/story/fs-info" | grep -q "ok" || { echo "FAIL fs-info"; exit 1; }
curl -s -X POST "${BASE_URL}/api/story/serial-command" | grep -q "STORY_V2_STATUS" || { echo "FAIL serial-command"; exit 1; }

echo "OK mock API tests"
