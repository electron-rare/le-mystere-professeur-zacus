#!/usr/bin/env bash
# Dump voice-bridge /usage/stats as Prometheus textfile metrics.
# Atomic write via mv. Run via crontab once per minute.
#
# Reads:  $STATS_URL (default http://localhost:8200/usage/stats)
# Writes: $OUT       (default /Users/clems/textfile_collector/voice_bridge_usage.prom)
#
# Side effects: emits voice_bridge_up=0 with no other metrics if the
# voice-bridge is unreachable, so Prometheus can alert on "scrape OK,
# bridge down" without going stale.
set -eu

STATS_URL="${STATS_URL:-http://localhost:8200/usage/stats}"
OUT="${OUT:-/Users/clems/textfile_collector/voice_bridge_usage.prom}"
TMP="${OUT}.tmp"

mkdir -p "$(dirname "$OUT")"

if ! curl -fsS -m 3 "$STATS_URL" -o "$TMP.json"; then
  # Voice-bridge unreachable: emit a single up=0 metric so Prometheus
  # knows we tried but failed (vs stale data without alert).
  printf '# HELP voice_bridge_up 1 if /usage/stats responded\n# TYPE voice_bridge_up gauge\nvoice_bridge_up 0\n' > "$TMP"
  mv "$TMP" "$OUT"
  exit 0
fi

jq -r '
  "# HELP voice_bridge_up 1 if /usage/stats responded\n# TYPE voice_bridge_up gauge\nvoice_bridge_up 1",
  "# HELP voice_bridge_uptime_seconds Process uptime\n# TYPE voice_bridge_uptime_seconds counter\nvoice_bridge_uptime_seconds \(.uptime_s)",
  "# HELP voice_bridge_llm_tokens_total Total LLM tokens by bucket\n# TYPE voice_bridge_llm_tokens_total counter",
  "voice_bridge_llm_tokens_total{bucket=\"npc_fast\",kind=\"prompt\"} \(.npc_fast.prompt_tokens)",
  "voice_bridge_llm_tokens_total{bucket=\"npc_fast\",kind=\"completion\"} \(.npc_fast.completion_tokens)",
  "voice_bridge_llm_tokens_total{bucket=\"hints_deep\",kind=\"prompt\"} \(.hints_deep.prompt_tokens)",
  "voice_bridge_llm_tokens_total{bucket=\"hints_deep\",kind=\"completion\"} \(.hints_deep.completion_tokens)",
  "# HELP voice_bridge_llm_calls_total Total LLM calls by bucket\n# TYPE voice_bridge_llm_calls_total counter",
  "voice_bridge_llm_calls_total{bucket=\"npc_fast\"} \(.npc_fast.calls)",
  "voice_bridge_llm_calls_total{bucket=\"hints_deep\"} \(.hints_deep.calls)",
  "# HELP voice_bridge_tts_f5_calls_total F5 synthesis calls\n# TYPE voice_bridge_tts_f5_calls_total counter\nvoice_bridge_tts_f5_calls_total \(.tts.f5_calls)",
  "# HELP voice_bridge_tts_f5_seconds_total F5 audio seconds synthesized\n# TYPE voice_bridge_tts_f5_seconds_total counter\nvoice_bridge_tts_f5_seconds_total \(.tts.f5_seconds)",
  "# HELP voice_bridge_tts_cache_hits_total TTS cache hits\n# TYPE voice_bridge_tts_cache_hits_total counter\nvoice_bridge_tts_cache_hits_total \(.tts.cache_hits)",
  "# HELP voice_bridge_stt_calls_total STT inference calls\n# TYPE voice_bridge_stt_calls_total counter\nvoice_bridge_stt_calls_total \(.stt.calls)",
  "# HELP voice_bridge_stt_audio_seconds_total STT audio seconds transcribed\n# TYPE voice_bridge_stt_audio_seconds_total counter\nvoice_bridge_stt_audio_seconds_total \(.stt.audio_seconds)"
' "$TMP.json" > "$TMP"

mv "$TMP" "$OUT"
rm -f "$TMP.json"
