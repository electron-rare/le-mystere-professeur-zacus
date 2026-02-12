# LittleFS assets (ESP32)

Mettre ici les sons internes a flasher dans la partition LittleFS.

## Fichier boot recommande

- Nom conseille: `boot.<ext>`
- Extensions supportees: `.mp3`, `.wav`, `.aac`, `.flac`, `.opus`, `.ogg`
- Resolution runtime:
  - chemin configure `kBootFxLittleFsPath` (par defaut `/boot.mp3`)
  - puis auto-detection `/boot.*`
  - puis premier fichier audio supporte trouve a la racine LittleFS
- Duree conseillee (profil actuel): ~20 s (intro radio + voix)
- Debit conseille (fichiers compressees): 64 a 128 kbps mono/stereo

## Flash LittleFS

```bash
pio run -e esp32dev -t uploadfs
```

ou

```bash
make uploadfs-esp32 ESP32_PORT=/dev/cu.usbserial-xxxx
```

## Debug serie

- `FS_INFO` : etat / capacite LittleFS
- `FS_LIST` : liste des fichiers
- `FSTEST` : joue le FX boot detecte via I2S si present
