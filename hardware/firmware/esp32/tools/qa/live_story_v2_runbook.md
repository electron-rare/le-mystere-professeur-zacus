# Live Story V2 Runbook (semi-auto)

Objectif: rejouer rapidement la validation terrain Story V2 (USB, flash, runtime, recovery).

## 0) Prerequis

- Deux cartes connectees en USB (ESP32 + ESP8266)
- Depuis `hardware/firmware/esp32`
- Python PlatformIO env disponible: `~/.platformio/penv/bin/python`

## 1) Preflight statique

```bash
pio device list
make qa-story-v2
```

Attendu: `[qa-story-v2] OK`.

## 2) Mapping ports automatique

```bash
for p in /dev/cu.SLAB_USBtoUART /dev/cu.SLAB_USBtoUART7 /dev/cu.usbserial-0001 /dev/cu.usbserial-5; do
  echo "=== $p (esp32 probe) ==="
  ~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port "$p" --before default_reset --after no_reset chip_id || true
  echo "=== $p (esp8266 probe) ==="
  ~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp8266 --port "$p" --before default_reset --after no_reset flash_id || true
done
```

Attendu:

- 1 port ESP32 detecte
- 1 port ESP8266 detecte

## 3) Flash sequentiel

Remplacer `<PORT_ESP32>` et `<PORT_ESP8266>`:

```bash
make upload-esp32 ESP32_PORT=<PORT_ESP32>
make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>
make upload-screen SCREEN_PORT=<PORT_ESP8266>
```

## 4) Runtime Story V2 (script serie)

Script copiable:

```bash
~/.platformio/penv/bin/python - <<'PY'
import time
import serial

ESP32='/dev/cu.SLAB_USBtoUART'  # adapter
BAUD=115200

commands = [
    'BOOT_STATUS','BOOT_REPLAY','BOOT_TEST_TONE','BOOT_TEST_DIAG',
    'STORY_V2_ENABLE ON','STORY_V2_TRACE ON','STORY_V2_STATUS','STORY_V2_LIST',
    'STORY_V2_VALIDATE','STORY_V2_HEALTH','STORY_TEST_ON','STORY_TEST_DELAY 2500',
    'STORY_ARM','STORY_V2_EVENT ETAPE2_DUE','STORY_V2_STATUS','STORY_V2_HEALTH',
]
commands += ['STORY_V2_EVENT ETAPE2_DUE'] * 20
commands += ['STORY_V2_HEALTH','STORY_V2_TRACE OFF','STORY_TEST_OFF','STORY_STATUS']

with serial.Serial(ESP32, BAUD, timeout=0.2) as ser:
    time.sleep(1)
    end=time.time()+4
    while time.time()<end:
        b=ser.readline()
        if b:
            print(b.decode('utf-8',errors='replace').rstrip())

    for cmd in commands:
        print('>>>', cmd)
        ser.write((cmd+'\n').encode())
        ser.flush()
        time.sleep(0.2)
        end=time.time()+1.8
        got=False
        while time.time()<end:
            b=ser.readline()
            if b:
                got=True
                print(b.decode('utf-8',errors='replace').rstrip())
        if not got:
            print('(no immediate response)')
PY
```

Attendu:

- `STORY_V2 OK valid`
- transitions jusqu'a `STEP_DONE`
- `gate=1` en fin

## 5) Recovery test (reset croise)

- Reset ESP8266: bouton reset carte
- Observer logs ESP32: `SCREEN_SYNC` continue
- Reset ESP32: bouton reset carte
- Observer reprise ecran < 2s

## 6) Troubleshooting rapide

Si flash impossible:

1. Refaire le mapping ports (section 2)
2. ESP32: maintenir `BOOT`, appuyer `EN`, relacher `EN`, relacher `BOOT`, relancer upload
3. ESP8266: maintenir `FLASH`, appuyer `RST`, relacher `RST`, relacher `FLASH`, relancer upload
4. Reessayer avec alias de port (`/dev/cu.SLAB_*` vs `/dev/cu.usbserial-*`)

Si commandes Story ne repondent pas:

1. Verifier baud monitor: `115200`
2. Envoyer `STORY_V2_ENABLE STATUS`
3. Envoyer `STORY_V2_ENABLE ON`
4. Relancer `STORY_V2_STATUS` puis `STORY_V2_VALIDATE`
