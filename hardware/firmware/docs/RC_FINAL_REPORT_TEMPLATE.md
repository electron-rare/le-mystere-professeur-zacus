# RC Final Report Template

## Metadata

- Branch:
- Date:
- Owner:
- Build baseline commit:
- Hardware setup:

## Build env status

| Env | Result (OK/KO) | Command | Short log | Root cause if KO | Corrective action |
| --- | --- | --- | --- | --- | --- |
| esp32dev |  | `pio run -e esp32dev` |  |  |  |
| esp32_release |  | `pio run -e esp32_release` |  |  |  |
| esp8266_oled |  | `pio run -e esp8266_oled` |  |  |  |
| ui_rp2040_ili9488 |  | `pio run -e ui_rp2040_ili9488` |  |  |  |
| ui_rp2040_ili9486 |  | `pio run -e ui_rp2040_ili9486` |  |  |  |

## Suite status

| Suite | Result (OK/KO/SKIP) | Command | Short log | Root cause if KO | Corrective action |
| --- | --- | --- | --- | --- | --- |
| smoke_plus |  | `python3 tools/test/run_serial_suite.py --suite smoke_plus ...` |  |  |  |
| mp3_basic |  | `python3 tools/test/run_serial_suite.py --suite mp3_basic ...` |  |  |  |
| mp3_fx |  | `python3 tools/test/run_serial_suite.py --suite mp3_fx ...` |  |  |  |
| story_v2_basic |  | `python3 tools/test/run_serial_suite.py --suite story_v2_basic ...` |  |  |  |
| story_v2_metrics |  | `python3 tools/test/run_serial_suite.py --suite story_v2_metrics ...` |  |  |  |

## UI Link status

| Scenario | Result (OK/KO/SKIP) | Command | Short log | Root cause if KO | Corrective action |
| --- | --- | --- | --- | --- | --- |
| HELLO/ACK/KEYFRAME |  | `python3 tools/test/ui_link_sim.py ...` |  |  |  |
| PING/PONG auto |  | `python3 tools/test/ui_link_sim.py ...` |  |  |  |
| BTN scripted sequence |  | `python3 tools/test/ui_link_sim.py --script ...` |  |  |  |

## Live mandatory scenarios checklist

- [ ] Ports already connected
- [ ] Hotplug during wait window
- [ ] Reset board during serial session
- [ ] Degraded context without SD
- [ ] UI link handshake and scripted buttons (S4/S5)

## Rollback note

- Last known good commit/PR:
- Rollback command/path:
- Operator confirmation:
