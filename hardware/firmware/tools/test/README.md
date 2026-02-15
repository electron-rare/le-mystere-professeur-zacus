# Test wrappers (firmware workspace)

Canonical implementation lives in the repo root:

- `tools/test/hw_now.sh` (combined ESP32+ESP8266 gate)
- `tools/test/hw_now_esp32_esp8266.sh` (fw-only subset)
- `tools/test/run_rc_gate.sh` and `tools/dev/rc_execution_seed.sh` for RC board automation

This folder only provides thin wrappers so you can run a script while staying
in `hardware/firmware` without path juggling. Call them with the same options as
the root scripts:

```
tools/test/hw_now.sh --env-esp32 esp32_release --wait-port 40
tools/test/hw_now_esp32_esp8266.sh --skip-build --baud 19200
tools/test/run_rc_gate.sh --help
```

The wrappers honor the same auto-port detection, artifact logging, and smoke
syntax (`tools/dev/serial_smoke.py --role auto`) defined at the repo root so that
validation runs stay reproducible locally and match the RC gate requirements.
