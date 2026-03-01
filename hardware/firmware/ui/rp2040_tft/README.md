
---
# Zacus Firmware – UI RP2040 TFT (LVGL + UI Link v2)
// TODO NO DEV FINISH (need KILL_LIFE ?)
---

## 📝 Description

Firmware UI tactile pour RP2040 avec écran TFT 3.5" + XPT2046, basé sur LVGL et UI Link v2.

---

## 🚀 Installation & usage

Tous les assets LittleFS (scénarios, écrans, scènes, audio, etc.) sont centralisés dans `hardware/firmware/data/` à la racine du projet.
Ce dossier unique sert de source pour le flash LittleFS sur ESP32, ESP8266 et RP2040.

Build depuis `hardware/firmware` :
```sh
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486
```
Flash :
```sh
pio run -e ui_rp2040_ili9488 -t upload --upload-port <PORT_RP2040>
```
Boucle rapide :
```sh
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
```

---

## 📦 Points clefs & contenu

- UI framework : LVGL
- Drivers : TFT_eSPI + XPT2046_Touchscreen
- Lien série : UI Link v2 (`HELLO/ACK/KEYFRAME/STAT/PING/PONG/BTN`)
- Touch -> boutons logiques (`PREV/NEXT/OK/BACK/VOL-/VOL+`)
- Ecran dégradé `LINK DOWN` si timeout

---

## 🤝 Contribuer

Merci de lire [../../../../../../../../CONTRIBUTING.md](../../../../../../../../CONTRIBUTING.md) avant toute PR.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
---


---

## Migration LittleFS (2026)

- Déplacer tous les fichiers d’écrans/scènes dans `hardware/firmware/data/story/screens/`.
- Adapter les scripts de génération et de flash pour pointer vers ce dossier.
- Supprimer l’ancien dossier `ui/rp2040_tft/data/` après migration.

---

## Configuration

- Pins TFT/touch/UART dans `include/ui_config.h`
- Contrat protocole dans `../../protocol/ui_link_v2.md`
- Cablage detaille dans `WIRING.md`
