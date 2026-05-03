# PLIP firmware

Bringup target: **AI-Thinker ESP32-A1S Audio Kit V2.2** (ES8388 codec).
End target: the custom **PLIP-Téléphone** PCB with Si3210 SLIC + RJ9 retro
handset (see `../hardware/projects/plip-telephone/` and
`../docs/superpowers/specs/2026-04-08-plip-telephone-design.md`).

We start on the dev kit so the audio + network state machine can be
debugged without waiting for the PCB. The codec is swapped (ES8388 →
Si3210) on the PCB; everything else stays.

## Architecture

Three FreeRTOS tasks, message-passing through a queue:

| Task | Core | Stack | Role |
|------|------|-------|------|
| `phone` | 1 | 4096 | Off-hook GPIO interrupt (dev kit: BOOT button on GPIO4; PCB: Si3210 INT). Drives ring on the codec. |
| `audio` | 0 | 8192 | Drains `audio_command_queue`. Routes `Stop` / `PlaySdMp3` / `PlayHttpStream` through `ESP32-audioI2S`. |
| `network` | 0 | 8192 | WiFi station + REST endpoint. Translates HTTP requests into audio queue messages. |

## REST contract

The Zacus master ESP32 talks to PLIP over the LAN:

| Method | Path | Body | Effect |
|--------|------|------|--------|
| POST | `/ring` | `{"duration_ms": 4000}` | Trigger ring (SLIC -72V; dev kit: short audio beep) |
| POST | `/play` | `{"source": "sd:/intro.mp3"}` or `{"source": "http://tower:8001/zacus/welcome.wav"}` | Start playback |
| POST | `/stop` | — | Stop current playback |
| GET | `/status` | — | `{"off_hook": bool, "playing": bool}` |

The Piper TTS server on Tower:8001 streams WAV; the audio task consumes
it directly via the I2S library's `connecttohost`.

## Build

```bash
cd PLIP_FIRMWARE
pio run                               # compile
pio run -t upload                     # flash over USB
pio device monitor                    # serial monitor (115200 8N1)
```

PlatformIO will pull `espressif32@6.5.0` + Arduino framework + the audio
library on first build (~3 min).

## Pin map (dev kit)

Defined in `platformio.ini` build flags:

```
ES8388_ADDR  = 0x10   (I2C)
IIS_BCLK     = 27     (I2S bit clock)
IIS_LCLK     = 26     (I2S word select)
IIS_DSIN     = 25     (DAC in)
IIS_DOUT     = 35     (ADC out)
SD_CS        = 13
SD_MOSI      = 15
SD_MISO      = 2
SD_SCK       = 14
OFF_HOOK_GPIO = 4     (BOOT/KEY1 stand-in)
```

When the PCB lands: rewire to Si3210 SPI + INT, swap the codec init in
`audio_task`, ring goes from "ES8388 beep" to "Si3210 ring control
register". The build_flags file is the only place pins live; source
code reads them via macros.

## MVP scope (Phase 3 = b in the brainstorm)

- Sonnerie (ring) on demand
- Off-hook GPIO interrupt → notify master
- MP3 playback from SD
- WiFi REST endpoint (4 routes above)

**Out of scope for v1**:

- Bluetooth Classic (A2DP sink + HFP) — the design doc specs it but the
  bringup focus is REST + SD first.
- Si3210 driver — comes when the PCB arrives.
- OTA via the desktop FirmwareManager — comes after the bringup loop is
  stable.

## Roadmap

| Step | Goal | Status |
|------|------|--------|
| 0 | Skeleton (this commit) — tasks fire `Serial.println` only | ✅ |
| 1 | I2C + ES8388 init in audio task; play a 1 kHz tone on demand | TODO |
| 2 | SD card mount + MP3 file playback | TODO |
| 3 | WiFi station + REST `/ring` `/play` `/stop` `/status` | TODO |
| 4 | Off-hook button → REST status update | TODO |
| 5 | HTTP stream from Tower:8001 Piper TTS | TODO |
| 6 | NVS provisioning via desktop NvsConfigurator | TODO |
| 7 | OTA via desktop FirmwareManager | TODO |
| 8 | Port to Si3210 SLIC on the custom PCB | TODO |
| 9 | Bluetooth A2DP sink (game-master streaming) | TODO |

## Convert to a git submodule

The directory is currently inlined in the parent repo for convenience.
Once you have a remote URL ready:

```bash
# 1. Create a separate repo for PLIP_FIRMWARE on your Git host
# 2. From this directory:
git init
git add .
git commit -m "Initial commit"
git remote add origin <url>
git push -u origin main

# 3. From the parent repo (le-mystere-professeur-zacus):
git rm -r PLIP_FIRMWARE
git submodule add <url> PLIP_FIRMWARE
git commit -m "chore: PLIP_FIRMWARE -> submodule"
```

This mirrors the existing `ESP32_ZACUS/` submodule layout.
