---
# Zacus Tools – Test Toolbox

---

## 📝 Description

Entrées de test légères pour le projet Zacus (validation, hardware, CI, simulation UART).

---

## 🚀 Installation & usage

Pré-requis :
- `python3`
- (Optionnel) `pyyaml` pour les checks de contenu (`pip install pyyaml`)
- (Optionnel) `pyserial` pour les tests USB série (`pip install pyserial`)

Commandes rapides :
```bash
bash tools/test/run_content_checks.sh
python3 tools/test/run_serial_suite.py --list-suites
python3 tools/test/run_serial_suite.py --suite smoke_plus --role auto --allow-no-hardware
python3 tools/test/zacus_menu.py
bash tools/test/run_rc_gate.sh --sprint s1 --port-esp32 /dev/cu.SLAB_USBtoUART
bash tools/test/hw_now.sh
```

Modes hardware :
- Sans hardware : utiliser `--allow-no-hardware` pour retour `SKIP` (exit 0)
- Avec hardware : connecter les adaptateurs USB-UART avant les suites

Simulation UI Link (UART 3.3V) :
- ESP32 TX (GPIO22) -> RX adaptateur
- ESP32 RX (GPIO19) -> TX adaptateur
- GND -> GND
- Baud : `19200`
```bash
python3 tools/test/ui_link_sim.py --port /dev/ttyUSB0 --script "NEXT:click,OK:long"
```

Wrappers pour firmware :
- `hardware/firmware/tools/test/` (forward vers ce dossier)

Helpers RC cycle :
- Sprint gate : `bash tools/test/run_rc_gate.sh --sprint s1..s5 ...`
- Live one-shot : `bash tools/test/hw_now.sh`
- Board seed : `bash hardware/firmware/tools/dev/rc_execution_seed.sh`
- Board source of truth : `hardware/firmware/docs/RC_FINAL_BOARD.md`

---

## 🤝 Contribuer

Les contributions sont bienvenues !
Merci de lire [../../CONTRIBUTING.md](../../CONTRIBUTING.md) avant toute PR.

---

## 🧑‍🎓 Licence

- **Code** : MIT (`../../LICENSE`)

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
