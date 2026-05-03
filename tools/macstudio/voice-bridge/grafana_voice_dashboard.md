# Grafana panel — voice-bridge usage

Target dashboard: `Zacus / Voice bridge`. Datasource: Prometheus scraping
`studio:9100/metrics` (node_exporter textfile collector).

The metrics below are emitted by
`tools/macstudio/voice-bridge/usage_to_prom.sh` (cron `* * * * *`) into
`/Users/clems/textfile_collector/voice_bridge_usage.prom`. They are picked
up by node_exporter when launched with
`--collector.textfile.directory=/Users/clems/textfile_collector/`.

## Exposed metrics

| Metric | Type | Labels | Meaning |
|--------|------|--------|---------|
| `voice_bridge_up` | gauge | – | 1 if `/usage/stats` responded on the last cron tick, 0 otherwise. Use for liveness alerts. |
| `voice_bridge_uptime_seconds` | counter (process) | – | Seconds since the voice-bridge process counted from `since` in `/usage/stats`. Resets on restart. |
| `voice_bridge_llm_tokens_total` | counter | `bucket={npc_fast,hints_deep}`, `kind={prompt,completion}` | Cumulative LLM tokens routed via the bridge. |
| `voice_bridge_llm_calls_total` | counter | `bucket={npc_fast,hints_deep}` | Cumulative LLM call count per routing bucket. |
| `voice_bridge_tts_f5_calls_total` | counter | – | F5 synthesis calls (cache misses that hit the model). |
| `voice_bridge_tts_f5_seconds_total` | counter | – | Cumulative seconds of audio synthesized by F5. |
| `voice_bridge_tts_cache_hits_total` | counter | – | TTS cache hits (text+voice_ref+steps already on disk). |
| `voice_bridge_stt_calls_total` | counter | – | Whisper STT inference calls. |
| `voice_bridge_stt_audio_seconds_total` | counter | – | Cumulative seconds of audio transcribed. |

All counters reset to 0 when the bridge restarts (in-memory accumulators
in `main.py`). Use `rate()` / `increase()` with reset-tolerant windows.

## PromQL queries

### LLM tokens per second, broken down by bucket and kind

```promql
sum by (bucket, kind) (rate(voice_bridge_llm_tokens_total[5m]))
```

### TTS cache hit ratio (over the last 1 h)

```promql
increase(voice_bridge_tts_cache_hits_total[1h])
/ clamp_min(
    increase(voice_bridge_tts_cache_hits_total[1h])
    + increase(voice_bridge_tts_f5_calls_total[1h]),
    1
  )
```

`clamp_min(..., 1)` avoids div-by-zero before any traffic.

### Inverse RTF — audio seconds synthesized per wall-clock second

```promql
rate(voice_bridge_tts_f5_seconds_total[5m])
```

A value of `1.0` means F5 is keeping up with real-time. The MLX backend
typically sits at `0.2`–`0.4` (cold) and bursts higher on warm cache
streaks (because cache hits inflate the numerator implicitly only if you
add `voice_bridge_tts_cache_hits_total`).

### STT minutes transcribed per day

```promql
increase(voice_bridge_stt_audio_seconds_total[1d]) / 60
```

### Liveness (alerting)

```promql
voice_bridge_up == 0
```

Alert with `for: 3m` to tolerate one missed cron tick.

## Suggested Grafana layout

| Row | Panel | Type | Query |
|-----|-------|------|-------|
| 1 — LLM throughput | Tokens/s by bucket+kind | Time series, stacked | `sum by (bucket, kind) (rate(voice_bridge_llm_tokens_total[5m]))` |
| 1 — LLM throughput | Calls/min by bucket | Bar chart | `sum by (bucket) (rate(voice_bridge_llm_calls_total[1m])) * 60` |
| 2 — TTS pipeline | F5 calls/min vs cache hits/min | Time series | `rate(voice_bridge_tts_f5_calls_total[1m])`, `rate(voice_bridge_tts_cache_hits_total[1m])` |
| 2 — TTS pipeline | Cache hit ratio (1 h) | Stat (gauge, 0–1) | see formula above |
| 2 — TTS pipeline | Inverse RTF | Stat | `rate(voice_bridge_tts_f5_seconds_total[5m])` |
| 3 — STT volume | Audio seconds/min | Time series | `rate(voice_bridge_stt_audio_seconds_total[1m]) * 60` |
| 3 — STT volume | Audio minutes today | Stat | `increase(voice_bridge_stt_audio_seconds_total[1d]) / 60` |
| 4 — Liveness | `voice_bridge_up` | State timeline | `voice_bridge_up` |
| 4 — Liveness | Uptime | Stat | `voice_bridge_uptime_seconds` |

## Operational notes

- The script writes atomically (`mv tmp prom`) so node_exporter never
  sees a half-written file.
- If the bridge is unreachable, the file is rewritten with only
  `voice_bridge_up 0`. Counters disappear — use `last_over_time(...[10m])`
  in panels if you want to keep displaying the last known value during
  short outages.
- `node_exporter` on studio currently runs **without**
  `--collector.textfile.directory` (verified 2026-05-03). Until that flag
  is added to the `@reboot` crontab line, the metrics file is generated
  but **not exposed** on `:9100/metrics`. See
  `tools/macstudio/README.md` § Prometheus / Grafana for the exact
  change to apply.
