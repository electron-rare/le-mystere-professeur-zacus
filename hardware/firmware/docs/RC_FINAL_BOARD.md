# RC Execution Board - hardware/firmware

This file is the source of truth for the RC execution cycle focused on
`hardware/firmware`.

## Scope

- Target branch: `hardware/firmware`
- Cadence: 5 sprints, 2 days each
- Validation model: cheap gates first, then live hardware gate at end of each sprint
- Firmware-only release scope

## Board model (mandatory)

### Columns

`Backlog -> Sprint Ready -> In Progress -> PR Review -> Live Validation -> Done`

### Labels

- `rc:test`
- `gate:cheap`
- `gate:live`
- `risk:blocker`
- `sprint:s1`
- `sprint:s2`
- `sprint:s3`
- `sprint:s4`
- `sprint:s5`

### Card fields

- `Owner`
- `Sprint`
- `PR #`
- `Base branch`
- `Commands run`
- `Evidence link`
- `Result (OK/KO)`
- `Rollback note`

Rule: one card = one PR.

## Card backlog and sprint ownership

| Card | Sprint | PR branch | Goal |
| --- | --- | --- | --- |
| `RCT-01` | S1 | `pr/rct-01-baseline-freeze` | Baseline freeze and test tooling inventory |
| `RCT-02` | S1 | `pr/rct-02-content-gate` | Content gate in CI/local |
| `RCT-03` | S2 | `pr/rct-03-mp3-suites-hardening` | Harden `mp3_basic` and `mp3_fx` suites |
| `RCT-04` | S2 | `pr/rct-04-mp3-live-evidence` | MP3 live evidence and runbook |
| `RCT-05` | S3 | `pr/rct-05-story-v2-basic` | Stabilize `story_v2_basic` |
| `RCT-06` | S3 | `pr/rct-06-story-v2-metrics-live` | Story V2 metrics live evidence |
| `RCT-07` | S4 | `pr/rct-07-ui-link-sim-live` | UI Link simulator live E2E |
| `RCT-08` | S4 | `pr/rct-08-menu-live` | TUI fallback/menu and mini-REPL live validation |
| `RCT-09` | S5 | `pr/rct-09-final-gates` | Final cheap/build/live gates |
| `RCT-10` | S5 | `pr/rct-10-closeout-report` | Final report and board closeout |

## Sprint gates (exact commands)

### Sprint 1

Cheap:

```bash
python3 -m compileall tools/test
bash -n tools/test/run_content_checks.sh
bash tools/test/run_content_checks.sh
bash tools/test/run_content_checks.sh --check-clean-git
```

Live:

```bash
python3 tools/test/run_serial_suite.py --suite smoke_plus --role esp32 --port <PORT_ESP32>
python3 tools/test/zacus_menu.py --action smoke --role esp32 --port <PORT_ESP32>
```

### Sprint 2

Cheap:

```bash
python3 tools/test/run_serial_suite.py --list-suites
python3 tools/test/run_serial_suite.py --suite mp3_basic --allow-no-hardware
python3 tools/test/run_serial_suite.py --suite mp3_fx --allow-no-hardware
```

Live:

```bash
python3 tools/test/run_serial_suite.py --suite mp3_basic --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite mp3_fx --role esp32 --port <PORT_ESP32>
```

### Sprint 3

Cheap:

```bash
python3 tools/test/run_serial_suite.py --suite story_v2_basic --allow-no-hardware
python3 tools/test/run_serial_suite.py --suite story_v2_metrics --allow-no-hardware
```

Live:

```bash
python3 tools/test/run_serial_suite.py --suite story_v2_basic --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite story_v2_metrics --role esp32 --port <PORT_ESP32>
```

### Sprint 4

Cheap:

```bash
python3 tools/test/zacus_menu.py --help
python3 tools/test/zacus_menu.py --action content
python3 tools/test/zacus_menu.py --action suite --suite smoke_plus --allow-no-hardware
python3 tools/test/ui_link_sim.py --allow-no-hardware --script "NEXT:click,OK:long"
```

Live:

```bash
python3 tools/test/ui_link_sim.py --port <PORT_UI> --script "NEXT:click,OK:long,MODE:click"
python3 tools/test/zacus_menu.py --action ui_link --port <PORT_UI> --script "NEXT:click,OK:long"
```

### Sprint 5

Cheap:

```bash
python3 -m compileall tools/test
bash tools/test/run_content_checks.sh --check-clean-git
python3 tools/test/run_serial_suite.py --list-suites
python3 tools/test/zacus_menu.py --help
```

Build and smoke:

```bash
cd hardware/firmware && ZACUS_SKIP_IF_BUILT=1 ./tools/dev/run_matrix_and_smoke.sh
```

If one env fails:

```bash
cd hardware/firmware && pio run -e <env>
```

Live final:

```bash
python3 tools/test/run_serial_suite.py --suite smoke_plus --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite mp3_basic --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite mp3_fx --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite story_v2_basic --role esp32 --port <PORT_ESP32>
python3 tools/test/run_serial_suite.py --suite story_v2_metrics --role esp32 --port <PORT_ESP32>
python3 tools/test/ui_link_sim.py --port <PORT_UI> --script "NEXT:click,OK:long,MODE:click"
python3 tools/test/zacus_menu.py --action console --port <PORT_ESP32> --timeout 1.5
```

## Mandatory live scenarios each sprint

1. ports already connected
2. hotplug during wait window
3. board reset during serial session
4. degraded context (no SD, tolerant `OUT_OF_CONTEXT`/`BUSY`)
5. UI link handshake + button script + PONG (sprints 4 and 5)

## Reporting and closeout

Release is blocked until sprint 5 final gate is green.

Final report table must include:

- `build env -> OK/KO`
- `suite -> OK/KO`
- `ui_link -> OK/KO`
- exact command
- root cause for KO + corrective action

## Automation helper

Use:

- `hardware/firmware/tools/dev/ci/rc_execution_seed.sh` to seed labels/cards
- `tools/test/run_rc_gate.sh` to replay sprint gates with exact commands
