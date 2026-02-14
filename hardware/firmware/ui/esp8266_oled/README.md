# Firmware UI ESP8266 OLED (UI Link v2)

Ce firmware garde une UI OLED legere (U8G2) et implemente UI Link v2 en UART full duplex.

## Architecture

- `src/core/telemetry_state.h`: modele de telemetrie
- `src/core/stat_parser.*`: decode `STAT`/`KEYFRAME` v2 (`k=v`, CRC8)
- `src/core/link_monitor.*`: watchdog lien et recovery
- `src/core/render_scheduler.*`: rendu non bloquant
- `src/apps/*`: ecrans fonctionnels (boot, lien, mp3, ulock)
- `src/main.cpp`: handshake `HELLO`, reponse `PONG`, boucle UI

## Protocole

Transport: trames ASCII ligne par ligne:

`<TYPE>,k=v,k=v*CC\n`

- CRC8 polynomial `0x07`
- UI -> ESP32: `HELLO`, `PONG`
- ESP32 -> UI: `ACK`, `KEYFRAME`, `STAT`, `PING`
- Capacites OLED annoncees: `caps=btn:0;touch:0;display:oled`

## Cablage UART

- ESP32 `GPIO22 (TX)` -> ESP8266 `D6 (RX)`
- ESP8266 `D5 (TX)` -> ESP32 `GPIO19 (RX)`
- GND commun obligatoire
- Baud par defaut: `19200`

## Cablage OLED I2C

- VCC -> `3V3`
- GND -> `GND`
- SDA -> `D1` (fallback `D2`)
- SCL -> `D2` (fallback `D1`)

## Build / flash

Depuis `hardware/firmware`:

```sh
pio run -e esp8266_oled
pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>
pio device monitor -e esp8266_oled --port <PORT_ESP8266>
```

Boucle rapide:

```sh
make fast-ui-oled UI_OLED_PORT=<PORT_ESP8266>
```
