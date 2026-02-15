# Name
firmware

## When To Use
Use for PlatformIO firmware delivery, serial smoke checks, and UI link verdict validation.

## Trigger Phrases
- "firmware gate"
- "PlatformIO matrix"
- "serial smoke"
- "UI_LINK_STATUS"

## Do
- Run the full firmware build matrix before final report.
- Use local scripts for USB wait and serial detection.
- Validate `UI_LINK_STATUS connected==1` from ESP32 logs.

## Don't
- Do not treat ESP8266 serial path as binary transport.
- Do not skip panic/reboot marker checks.

## Quick Commands
- `cd hardware/firmware && ./build_all.sh`
- `python3 hardware/firmware/tools/dev/serial_smoke.py --role auto --wait-port 20`
- `bash hardware/firmware/tools/test/hw_now.sh`
