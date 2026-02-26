---
# Zacus Firmware – Test Wrappers

---

## 📝 Description

Ce dossier fournit des wrappers pour lancer les scripts de test depuis `hardware/firmware` sans jongler avec les chemins.

---

## 🚀 Installation & usage

Scripts principaux (implémentation canonique à la racine) :
- `tools/test/hw_now.sh` (ESP32+ESP8266)
- `tools/test/hw_now_esp32_esp8266.sh` (firmware only)
- `tools/test/run_rc_gate.sh` et `tools/dev/rc_execution_seed.sh` (automation RC)

Exemples d’utilisation :
```sh
tools/test/hw_now.sh --env-esp32 esp32_release --wait-port 40
tools/test/hw_now_esp32_esp8266.sh --skip-build --baud 57600
tools/test/run_rc_gate.sh --help
```

Les wrappers respectent la détection auto-port, le logging d’artifacts et la syntaxe smoke (`tools/dev/serial_smoke.py --role auto`).

---

## 🤝 Contribuer

Merci de lire [../../../CONTRIBUTING.md](../../../CONTRIBUTING.md) avant toute PR.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
