# Test Toolbox

Lightweight test entrypoints for Zacus.

## Prerequisites

- `python3`
- Optional: `pyyaml` for content checks (`pip install pyyaml`)
- Optional: `pyserial` for USB serial tests (`pip install pyserial`)

## Quick commands

```bash
bash tools/test/run_content_checks.sh
python3 tools/test/run_serial_suite.py --list-suites
python3 tools/test/run_serial_suite.py --suite smoke_plus --role auto --allow-no-hardware
python3 tools/test/zacus_menu.py
```

## Hardware modes

- No hardware / CI laptop:
  - use `--allow-no-hardware` on serial tools to return `SKIP` with exit code `0`.
- With hardware:
  - connect USB-UART adapters before running serial suites.

## Minimal wiring for UI Link simulation

UART 3.3V only:

- ESP32 TX (GPIO22) -> adapter RX
- ESP32 RX (GPIO19) -> adapter TX
- GND -> GND
- Baud: `19200`

Then run:

```bash
python3 tools/test/ui_link_sim.py --port /dev/ttyUSB0 --script "NEXT:click,OK:long"
```

## Wrapper path inside firmware workspace

For teams working from `hardware/firmware`, wrappers are provided under:

- `hardware/firmware/tools/test/`

They forward to this canonical `tools/test` implementation.
