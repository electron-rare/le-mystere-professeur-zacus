# Deployment Runbook

## Status
- State: draft
- Date: 2026-03-21
- Audience: Game master, field technician
- Related: `docs/QUICKSTART.md`, `docs/SECURITY.md`

---

## 1) Pre-Deployment Checklist

### Hardware
- [ ] ESP32-S3 Freenove board powered and accessible via USB
- [ ] Speaker connected to I2S output (verified with test tone)
- [ ] LED strips connected and addressed (WS2812B data pin)
- [ ] Camera OV2640 connected (if vision features enabled)
- [ ] Microphone INMP441 connected (if voice features enabled)
- [ ] All puzzle actuators (servos, relays) wired and tested
- [ ] Backup ESP32 board available on-site

### Network
- [ ] WiFi access point configured and powered
- [ ] SSID and password documented (not the default)
- [ ] ESP32 IP address reserved (DHCP reservation or static)
- [ ] mascarade server reachable from game WiFi (if AI features)
- [ ] Game master device (laptop/tablet) on same network

### Content
- [ ] Scenario YAML validated: `bash tools/test/run_content_checks.sh`
- [ ] Runtime 3 IR compiled: `python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml`
- [ ] Runtime 3 simulation passed: `python3 tools/scenario/simulate_runtime3.py game/scenarios/zacus_v2.yaml`
- [ ] Audio files present in `hardware/firmware/data/audio/`
- [ ] All audio files referenced in scenario exist on filesystem
- [ ] Printable materials generated and printed

### Security
- [ ] API bearer token generated and stored in NVS
- [ ] WiFi credentials stored in NVS (not hardcoded)
- [ ] `.env` file configured on server with matching token
- [ ] Serial debug disabled in production firmware
- [ ] Review `docs/SECURITY.md` checklist

---

## 2) Firmware Flash Procedure

### 2.1 Prerequisites

```bash
# Install PlatformIO CLI (if not installed)
pip install platformio

# Verify toolchain
pio platform update espressif32
```

### 2.2 Build Firmware

```bash
cd hardware/firmware

# Build for Freenove ESP32-S3
pio run -e freenove_esp32s3

# Build for ESP8266 OLED (secondary display)
pio run -e esp8266_oled
```

### 2.3 Flash Firmware

```bash
# Connect ESP32-S3 via USB, identify port
ls /dev/tty.usb*        # macOS
ls /dev/ttyUSB*          # Linux

# Flash firmware + bootloader + partition table
pio run -e freenove_esp32s3 -t upload --upload-port /dev/tty.usbmodem*

# Monitor serial output to verify boot
pio device monitor -e freenove_esp32s3 --port /dev/tty.usbmodem* -b 115200
```

### 2.4 Expected Boot Output

```
[INFO] Zacus Runtime 3 v3.1
[INFO] Free heap: 245000 bytes
[INFO] PSRAM: 8388608 bytes
[INFO] WiFi connecting to: EscapeRoom-Net
[INFO] WiFi connected, IP: 192.168.0.50
[INFO] HTTP server started on port 8080
[INFO] Scenario loaded: ZACUS_V2 (11 steps)
[INFO] Initial step: RTC_ESP_ETAPE1
[INFO] Audio manager ready
[INFO] LED manager ready (60 LEDs)
[INFO] Ready.
```

### 2.5 Provisioning Credentials

On first boot (or after NVS erase):

```bash
# Erase NVS to force setup mode
pio run -e freenove_esp32s3 -t erase

# Flash and boot — device enters AP mode "Zacus-Setup"
pio run -e freenove_esp32s3 -t upload

# Connect to "Zacus-Setup" WiFi from your phone/laptop
# Open http://192.168.4.1 in browser
# Enter WiFi SSID, password, and API token
# Device reboots in STA mode
```

---

## 3) Content Deployment (LittleFS)

### 3.1 Prepare Filesystem Image

Content files are stored in `hardware/firmware/data/`:

```
data/
  story/
    runtime3/
      DEFAULT.json          # Runtime 3 IR (compiled)
    scenarios/
      DEFAULT.json          # Legacy scenario
  audio/
    ambient_01.mp3
    hint_01.mp3
    sfx_unlock.mp3
    ...
  config/
    device.json             # Device-specific config
```

### 3.2 Upload LittleFS Image

```bash
cd hardware/firmware

# Build and upload filesystem image
pio run -e freenove_esp32s3 -t uploadfs --upload-port /dev/tty.usbmodem*
```

### 3.3 Verify Content Upload

```bash
# Via serial
> ls /littlefs/story/runtime3/
DEFAULT.json (4523 bytes)

# Via API (requires auth)
curl -H "Authorization: Bearer <token>" http://192.168.0.50:8080/api/status
# Should show: "scenario": "ZACUS_V2", "steps": 11
```

### 3.4 Update Content Without Reflashing Firmware

For content-only updates (new scenario, audio files):

```bash
# Recompile scenario
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml

# Copy to firmware data directory
cp artifacts/runtime3/latest/DEFAULT.json hardware/firmware/data/story/runtime3/

# Upload only filesystem (firmware unchanged)
pio run -e freenove_esp32s3 -t uploadfs
```

---

## 4) Frontend Deployment

### 4.1 Build Frontend

```bash
cd frontend-scratch-v2

# Install dependencies
npm install

# Run tests
npm test

# Run lint
npm run lint

# Build for production
VITE_STORY_API_BASE=http://192.168.0.50:8080 npm run build
```

Build output is in `frontend-scratch-v2/dist/`.

### 4.2 Serve Frontend

**Option A: From ESP32 (embedded)**
```bash
# Copy built files to firmware data directory
cp -r frontend-scratch-v2/dist/* hardware/firmware/data/www/

# Upload filesystem
cd hardware/firmware
pio run -e freenove_esp32s3 -t uploadfs
```

The ESP32 serves the frontend at `http://<esp32-ip>:8080/`.

**Option B: From separate server**
```bash
# Serve with any static file server
npx serve frontend-scratch-v2/dist -l 3000

# Or via zacus.sh
./tools/dev/zacus.sh frontend-serve
```

### 4.3 Using the Shell Shortcut

```bash
# Build + lint + test in one command
./tools/dev/zacus.sh frontend-build
```

---

## 5) Network Setup

### 5.1 WiFi Access Point Configuration

| Parameter | Recommended Value |
|-----------|------------------|
| SSID | `EscapeRoom-Net` (hidden optional) |
| Security | WPA2-PSK |
| Channel | 1, 6, or 11 (least congested) |
| Band | 2.4 GHz (ESP32 does not support 5 GHz) |
| DHCP range | 192.168.0.50 - 192.168.0.99 |
| DNS | Not required (local network only) |

### 5.2 IP Address Assignments

| Device | IP | Port | Purpose |
|--------|-----|------|---------|
| WiFi AP / Router | 192.168.0.1 | — | Gateway |
| ESP32 main | 192.168.0.50 | 8080 | Game device |
| ESP32 salle 2 | 192.168.0.51 | 8080 | Secondary device |
| Game master laptop | 192.168.0.10 | 3000 | Dashboard |
| mascarade server | 192.168.0.119 | 4001 | AI backend |

### 5.3 ESP-NOW Pairing

For multi-device setups using ESP-NOW mesh:

```bash
# On primary device (serial console)
> espnow pair
Pairing mode active. Press button on secondary device...

# On secondary device (serial console)
> espnow join
Scanning for primary...
Paired with zacus-main (MAC: AA:BB:CC:DD:EE:FF)
```

### 5.4 Verify Network Connectivity

```bash
# Ping ESP32
ping 192.168.0.50

# Check API health
curl http://192.168.0.50:8080/health
# Expected: {"status":"ok"}

# Check authenticated endpoint
curl -H "Authorization: Bearer <token>" http://192.168.0.50:8080/api/status
# Expected: full status JSON

# Check mascarade (if AI features)
curl http://192.168.0.119:4001/health
```

---

## 6) Post-Deployment Validation

### 6.1 Functional Checks

| # | Check | Command / Action | Expected |
|---|-------|-----------------|----------|
| 1 | Device boots | Power on, watch serial | "Ready." message |
| 2 | WiFi connects | Serial log | IP address assigned |
| 3 | API responds | `curl /health` | `{"status":"ok"}` |
| 4 | Auth works | `curl /api/status` with token | 200 OK |
| 5 | Auth rejects | `curl /api/status` without token | 401 |
| 6 | Scenario loaded | Check `/api/status` response | Correct scenario ID |
| 7 | Audio plays | Trigger audio via API or button | Sound from speaker |
| 8 | LEDs respond | Trigger LED via API or scenario | Correct colors |
| 9 | Frontend loads | Open browser to device IP | UI renders |
| 10 | Full walkthrough | Play through scenario start to finish | All transitions work |

### 6.2 Automated Validation Script

```bash
#!/bin/bash
# tools/deploy/validate.sh
set -euo pipefail

ESP_IP="${1:-192.168.0.50}"
TOKEN="${2:-$(cat .env | grep API_TOKEN | cut -d= -f2)}"

echo "Validating deployment at $ESP_IP..."

# Health check
curl -sf "http://$ESP_IP:8080/health" | jq .status
echo "Health: OK"

# Auth check
HTTP_CODE=$(curl -sf -o /dev/null -w "%{http_code}" \
  -H "Authorization: Bearer $TOKEN" \
  "http://$ESP_IP:8080/api/status")
[ "$HTTP_CODE" = "200" ] && echo "Auth: OK" || echo "Auth: FAIL ($HTTP_CODE)"

# Scenario check
SCENARIO=$(curl -sf -H "Authorization: Bearer $TOKEN" \
  "http://$ESP_IP:8080/api/status" | jq -r .scenario)
echo "Scenario: $SCENARIO"

# Step count
STEPS=$(curl -sf -H "Authorization: Bearer $TOKEN" \
  "http://$ESP_IP:8080/api/status" | jq .steps)
echo "Steps: $STEPS"

echo "Validation complete."
```

---

## 7) Rollback Procedure

### 7.1 Firmware Rollback

```bash
# Keep previous firmware binary
cp .pio/build/freenove_esp32s3/firmware.bin backups/firmware_$(date +%Y%m%d).bin

# To rollback:
pio run -e freenove_esp32s3 -t upload --upload-port /dev/tty.usbmodem* \
  --firmware backups/firmware_20260320.bin
```

### 7.2 Content Rollback

```bash
# Content is versioned in git
git log --oneline hardware/firmware/data/story/

# Restore previous version
git checkout HEAD~1 -- hardware/firmware/data/story/runtime3/DEFAULT.json

# Re-upload filesystem
cd hardware/firmware
pio run -e freenove_esp32s3 -t uploadfs
```

### 7.3 Frontend Rollback

```bash
# Previous builds in git
git log --oneline frontend-scratch-v2/

# Restore and rebuild
git checkout HEAD~1 -- frontend-scratch-v2/
cd frontend-scratch-v2 && npm run build
```

### 7.4 Emergency: Factory Reset

```bash
# Full erase (firmware + NVS + filesystem)
pio run -e freenove_esp32s3 -t erase

# Reflash everything from scratch
pio run -e freenove_esp32s3 -t upload
pio run -e freenove_esp32s3 -t uploadfs

# Re-provision credentials via AP mode
```

---

## 8) Troubleshooting Quick Reference

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No serial output | Wrong USB port or baud rate | Try `115200` baud, check cable (data-capable) |
| Boot loop | Stack overflow or panic | Increase stack in `platformio.ini` (16 -> 24 KB) |
| WiFi won't connect | Wrong credentials or channel | Erase NVS, re-provision. Check 2.4 GHz only |
| WiFi keeps disconnecting | Signal too weak or interference | Move AP closer, change channel |
| API returns 401 | Token mismatch | Re-provision token via AP setup portal |
| API returns 429 | Rate limited | Wait 1 s, reduce request frequency |
| No audio | I2S pin mismatch or volume 0 | Check `platformio.ini` pin defines, set volume > 0 |
| Audio distorted | DMA buffer underrun | Reduce concurrent tasks, increase DMA buffer count |
| LEDs wrong color | GRB/RGB order mismatch | Check LED type in code (WS2812B = GRB) |
| LEDs flicker | Insufficient power supply | Use dedicated 5V supply, add 100uF cap |
| LVGL crash | Pool too small | Increase to 96 KB in `platformio.ini` |
| Scenario stuck | Missing transition for current state | Check `DEFAULT.json` transitions, use serial `> status` |
| ESP-NOW not pairing | Different WiFi channels | Both devices must be on same channel |
| Camera timeout | I2C conflict or power issue | Check camera ribbon cable, power cycle |
| OOM / heap exhaustion | Memory leak or too many features | Monitor with `> heap`, disable unused features |
| Frontend won't connect | Wrong `VITE_STORY_API_BASE` | Rebuild with correct ESP32 IP |
| mascarade unreachable | Network or Docker issue | Check `ping`, `docker ps`, firewall rules |

### Serial Debug Commands

```
> status              # Device state, heap, uptime
> heap                # Detailed memory breakdown
> wifi                # WiFi RSSI, IP, channel
> scenario            # Current step, available transitions
> transition <event>  # Manually trigger a transition
> audio list          # List available audio files
> audio play <file>   # Play an audio file
> led test            # Cycle through LED test patterns
> reboot              # Software restart
> factory-reset       # Erase NVS + reboot into AP mode
```

### Useful Monitoring Commands

```bash
# Watch serial output continuously
pio device monitor -e freenove_esp32s3 -b 115200

# Stream API status every 5 seconds
watch -n 5 'curl -s -H "Authorization: Bearer <token>" http://192.168.0.50:8080/api/status | jq .'

# Check WiFi signal from ESP32 perspective
curl -s -H "Authorization: Bearer <token>" http://192.168.0.50:8080/api/status | jq .wifi_rssi
```
