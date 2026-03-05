# Orchestrateur de flash universel (PlatformIO, Arduino, RP2040, STM32...)

## Prérequis
- Python 3.8+
- PlatformIO Core (`pipx install platformio` ou `pip install platformio`)
- (optionnel) Arduino CLI (`brew install arduino-cli` ou équivalent)

## Fichiers clés
- `tools/dev/flash_config.json` : mapping des rôles, méthodes, regex, options
- `tools/dev/autoflash.py` : script principal (auto-détection, build, upload, logs)

## Usage

### 1. Lister les devices et rôles connus
```sh
./tools/dev/autoflash.py list
```

### 2. Flasher un rôle (exemples)
```sh
./tools/dev/autoflash.py flash --role esp32
./tools/dev/autoflash.py flash --role esp8266
./tools/dev/autoflash.py flash --role esp32s3
./tools/dev/autoflash.py flash --role rp2040
./tools/dev/autoflash.py flash --role stm32
./tools/dev/autoflash.py flash --role arduino
```

### 3. Dry-run (voir ce qu’il ferait sans rien flasher)
```sh
./tools/dev/autoflash.py flash --role esp32 --dry-run
```

### 4. Logs
- Tous les logs (upload, compile, meta) sont dans :
  `artifacts/flash/<timestamp>_<role>/`

## Ajouter/modifier un rôle
- Édite `tools/dev/flash_config.json` :
  - Ajoute une entrée dans `roles` avec :
    - `role` : nom logique
    - `method` : `platformio`, `arduino_cli` ou `uf2_copy`
    - `match` : regex sur port/description/hwid (pour auto-détection)
    - autres options selon la méthode (voir exemples)

## Astuces robustesse
- Si plusieurs devices matchent, renforce la regex `match` (ex: ajoute un bout de serial, description, etc).
- Sur macOS, le script préfère automatiquement `/dev/cu.*` pour l’upload série.
- Pour RP2040, mets la carte en mode BOOTSEL (disque RPI-RP2 visible).

## Exemples de config (extrait)
```json
{
  "role": "esp32",
  "method": "platformio",
  "pio_env": "esp32_audio",
  "match": "10c4:ea60|CP210|SLAB_USBtoUART|esp32",
  "need_serial_port": true
}
```

## Dépannage
- Si aucun port n’est trouvé :
  - Vérifie le câblage, les permissions, et que la carte est bien branchée.
  - Utilise `list` pour voir tous les devices détectés.
- Si plusieurs ports sont trouvés :
  - Débranche les autres cartes ou précise la regex `match`.
- Pour RP2040 :
  - Si le disque n’apparaît pas, vérifie le mode BOOTSEL.

---

**Ce script permet de flasher n’importe quelle carte supportée par PlatformIO, Arduino CLI ou UF2, de façon fiable et automatisée, même en atelier multi-cartes.**
