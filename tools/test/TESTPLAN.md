# Test Plan

## Scope

- Content integrity (scenario/audio/printables/export)
- Firmware build/smoke entrypoints reuse
- USB serial command suites
- UI Link v2 simulation handshake

## A. Content checks (cheap, default)

- `bash tools/test/run_content_checks.sh`
- Expected:
  - scenario/audio/printables validators return OK
  - markdown export succeeds
- Optional CI strictness:
  - `bash tools/test/run_content_checks.sh --check-clean-git`

## B. Build checks (targeted)

Run only when needed:

- single env: `cd hardware/firmware && pio run -e esp32dev`
- full matrix + smoke: `cd hardware/firmware && ./tools/dev/run_matrix_and_smoke.sh`

## C. USB serial suites

- list: `python3 tools/test/run_serial_suite.py --list-suites`
- no hardware: `python3 tools/test/run_serial_suite.py --suite smoke_plus --allow-no-hardware`
- with hardware:
  - `smoke_plus`: baseline status and link commands
  - `mp3_basic`: MP3 status/list/scan probes
  - `mp3_fx`: FX mode/gain/trigger probes
  - `story_v2_basic`: V2 status/health/validate probes
  - `story_v2_metrics`: metrics + reset probes

Pass rule:
- each test gets `[pass]` when regex expectations match within timeout.
- suite passes only if all tests pass.

## D. UI Link simulator

- command:
  - `python3 tools/test/ui_link_sim.py --port /dev/ttyUSB0 --script "NEXT:click,OK:long"`
- expected:
  - HELLO sent
  - ACK + KEYFRAME received
  - PING frames answered by automatic PONG
  - BTN script events sent and acknowledged by logs

## Failure diagnostics

- Missing PyYAML: install with `pip install pyyaml`
- Missing pyserial: install with `pip install pyserial`
- No hardware attached:
  - use `--allow-no-hardware` for skip mode
- MP3 commands returning `OUT_OF_CONTEXT`:
  - attach SD card / ensure MP3 context is active

## RC execution support

- Full sprint command replay:
  - `bash tools/test/run_rc_gate.sh --sprint s1 --allow-no-hardware`
  - `bash tools/test/run_rc_gate.sh --sprint s5 --esp32-port <PORT_ESP32> --ui-port <PORT_UI>`
- Final report template:
  - `hardware/firmware/docs/RC_FINAL_REPORT_TEMPLATE.md`
