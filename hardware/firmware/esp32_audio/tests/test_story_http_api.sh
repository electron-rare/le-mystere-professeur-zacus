#!/bin/bash
set -euo pipefail

ESP_URL="${ESP_URL:-http://192.168.1.100:8080}"

echo "TEST 1: GET /api/story/list"
curl -s "$ESP_URL/api/story/list" | jq . || echo "FAIL"

echo "TEST 2: POST /api/story/select/DEFAULT"
curl -s -X POST "$ESP_URL/api/story/select/DEFAULT" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

echo "TEST 3: POST /api/story/start"
curl -s -X POST "$ESP_URL/api/story/start" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

echo "TEST 4: GET /api/story/status"
curl -s "$ESP_URL/api/story/status" | jq . || echo "FAIL"

echo "TEST 5: POST /api/story/pause"
curl -s -X POST "$ESP_URL/api/story/pause" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

echo "TEST 6: POST /api/story/resume"
curl -s -X POST "$ESP_URL/api/story/resume" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

echo "TEST 7: POST /api/story/skip"
curl -s -X POST "$ESP_URL/api/story/skip" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

echo "TEST 8: POST /api/story/validate"
curl -s -X POST "$ESP_URL/api/story/validate" -H "Content-Type: application/json" -d '{"yaml":"---\nversion: 1\n"}' | jq . || echo "FAIL"

echo "TEST 9: POST /api/story/deploy"
curl -s -X POST "$ESP_URL/api/story/deploy" -H "Content-Type: application/octet-stream" --data-binary @/dev/null | jq . || echo "FAIL"

echo "TEST 10: GET /api/audit/log"
curl -s "$ESP_URL/api/audit/log?limit=50" | jq . || echo "FAIL"

echo "TEST 11: GET /api/story/fs-info"
curl -s "$ESP_URL/api/story/fs-info" | jq . || echo "FAIL"

echo "TEST 12: POST /api/story/serial-command"
curl -s -X POST "$ESP_URL/api/story/serial-command" -H "Content-Type: application/json" -d '{"command":"STORY_V2_STATUS"}' | jq . || echo "FAIL"

echo "TEST 13: WebSocket /api/story/stream (30 sec)"
if command -v wscat >/dev/null 2>&1; then
  timeout 30 wscat -c "${ESP_URL/http/ws}/api/story/stream" --execute 'ping' || echo "FAIL"
else
  echo "SKIP: wscat not installed"
fi
