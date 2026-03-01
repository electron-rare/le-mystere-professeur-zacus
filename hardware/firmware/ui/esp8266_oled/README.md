
---
# Zacus Firmware – UI ESP8266 OLED (UI Link v2)
// TODO NO DEV FINISH (need KILL_LIFE ?)
---

## 📝 Description

Firmware UI OLED léger (U8G2) pour ESP8266, implémentant UI Link v2 en UART full duplex.

---

## 🚀 Installation & usage

Tous les assets LittleFS (scénarios, écrans, scènes, audio, etc.) sont centralisés dans `hardware/firmware/data/` à la racine du projet.
Ce dossier unique sert de source pour le flash LittleFS sur ESP32, ESP8266 et RP2040.

---

## 📦 Architecture & contenu

- `src/core/telemetry_state.h` : modèle de télémétrie
- `src/core/stat_parser.*` : decode `STAT`/`KEYFRAME` v2 (`k=v`, CRC8)
- `src/core/link_monitor.*` : watchdog lien et recovery
- `src/core/render_scheduler.*` : rendu non bloquant
- `src/apps/*` : écrans fonctionnels (boot, lien, mp3, unlock)
- `src/main.cpp` : handshake `HELLO`, réponse `PONG`, boucle UI

---

## 🛠️ Protocole

Transport : trames ASCII ligne par ligne :
`<TYPE>,k=v,k=v*CC\n`
- CRC8 polynomial `0x07`
- UI -> ESP32 : `HELLO`, `PONG`
- ESP32 -> UI : `ACK`, `KEYFRAME`, `STAT`, `PING`
- Capacités OLED annoncées : `caps=btn:0;touch:0;display:oled`

---

## 🔌 Câblage

### UART
- ESP32 `GPIO22 (TX)` -> ESP8266 `D4 (RX)`
- ESP8266 `D5 (TX)` -> ESP32 `GPIO19 (RX)`
- GND commun obligatoire
- Baud par défaut : `57600`

### OLED I2C

---

## 🤝 Contribuer

Merci de lire [../../../../../../../../CONTRIBUTING.md](../../../../../../../../CONTRIBUTING.md) avant toute PR.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
---

- VCC -> `3V3`
- GND -> `GND`
- SDA -> `D1` (fallback `D2`)
- SCL -> `D2` (fallback `D1`)


---

## Migration LittleFS (2026)

- Déplacer tous les fichiers d’écrans/scènes dans `hardware/firmware/data/story/screens/`.
- Adapter les scripts de génération et de flash pour pointer vers ce dossier.
- Supprimer l’ancien dossier `ui/esp8266_oled/data/` après migration (si existant).

---

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
