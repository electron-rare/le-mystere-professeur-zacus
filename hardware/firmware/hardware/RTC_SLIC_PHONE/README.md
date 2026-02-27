# RTC_BL_PHONE

Téléphone RTC expérimental sur ESP32 A252, orienté terrain: numérotation à impulsion, hotline audio, bridge ESP-NOW, validation série stricte.

## Esprit Zacus

Ce repo se pilote comme une session de terrain:

- `Opérateur` : décroche, compose, déclenche les scénarios.
- `Analyste` : surveille `STATUS` / `HOTLINE_STATUS` / `ESPNOW_STATUS`.
- `Archiviste` : garde les logs et rapports dans `artifacts/`.

Référence rôles: [docs/roles_agents.md](docs/roles_agents.md)

## Cible active

- Board focus: `ESP32_A252`
- Port série usuel: `/dev/cu.usbserial-0001`
- Contrat firmware courant: `A252_AUDIO_CHAIN_V4`

## Comportement hotline (actuel)

- Preset forcé au boot:
  - `1 -> /hotline/menu_dtmf_short.wav` (SD)
  - `2 -> /hotline/menu_dtmf.wav` (SD)
  - `3 -> /hotline/menu_dtmf_long.wav` (SD)
- Numérotation impulsion active combiné décroché.
- Après sélection d’un numéro valide:
  - lecture WAV,
  - pause 3s,
  - boucle jusqu’au raccroché.
- `WAITING_VALIDATION` déclenche la sonnerie + prompt SD `enter_code_5.wav` au décroché.
- Lecture MP3 SD activée (decodeur Helix) pour les prompts scène hotline.
- Raccroché détecté rapidement (~300 ms).
- Pas de sonnerie automatique au boot (ring déclenché par événement runtime uniquement).

## Audio scène hotline (SD)

- `SCENE <scene_id>` conserve l’état scène et tente une lecture SD mappée (`/hotline/scene_*`).
- Commande dédiée: `HOTLINE_SCENE_PLAY <scene_id>` pour forcer la lecture scène.
- Mapping voix par défaut: suffixe `__fr-fr-deniseneural.mp3`.

## ESP-NOW (actuel)

- Identité device persistante: `HOTLINE_PHONE`
- Commandes dédiées:
  - `ESPNOW_DEVICE_NAME_GET`
  - `ESPNOW_DEVICE_NAME_SET <NAME>`
- Runtime auto-discovery peers:
  - broadcast `ESPNOW_DEVICE_NAME_GET` toutes les 60s,
  - auto-ajout des MAC qui répondent,
  - télémétrie visible dans `STATUS.espnow.peer_discovery_*`.

## Démarrage rapide A252

1. Build:

```bash
pio run -e esp32dev
```

2. Flash:

```bash
pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001
```

3. Validation minimale série (via terminal série 115200):

```text
PING
STATUS
ESPNOW_DEVICE_NAME_GET
DIAL_MEDIA_MAP_GET
HOTLINE_STATUS
FS_LIST
HOTLINE_SCENE_PLAY SCENE_READY
```

## Inventaire fichiers firmware (SD/LittleFS)

Commande générique paginée:

- `FS_LIST` (defaults: `source=SD`, `path=/`, `page=0`, récursif, dossiers+fichiers)
- `FS_LIST sd`
- `FS_LIST littlefs`
- `FS_LIST {"source":"sd","page":1}`

Exemple opérateur pour paginer:

```text
FS_LIST {"source":"sd","page":0,"page_size":50}
FS_LIST {"source":"sd","page":1,"page_size":50}
```

## Script contrôleur ESP-NOW (terrain)

Script: `scripts/espnow_hotline_control.py`

Exemples:

```bash
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target broadcast+discovery --target-name HOTLINE_PHONE --action ring
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --action discover --target-name HOTLINE_PHONE --discover-rounds 3
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target AA:BB:CC:DD:EE:FF --ensure-peer --action hotline1
```

## Monitoring hotline live

```bash
python3 scripts/hotline_live_monitor.py --port /dev/cu.usbserial-0001 --expect 1,2,3
```

## Gate de validation

- Contrats/tests Python:

```bash
python3 -m pytest -q scripts/test_hw_validation_contracts.py scripts/test_runtime_contracts.py
```

- Validation hardware A252:

```bash
python3 scripts/hw_validation.py \
  --port-a252 /dev/cu.usbserial-0001 \
  --no-require-hook-toggle \
  --strict-serial-smoke \
  --allow-capture-fail-when-disabled \
  --audio-probe-path /welcome.wav \
  --require-contract-version A252_AUDIO_CHAIN_V4
```

## Docs clés

- Contrat ESP-NOW: [docs/espnow_contract.md](docs/espnow_contract.md)
- API ESP-NOW: [docs/espnow_api_v1.md](docs/espnow_api_v1.md)
- Plan tonal/audio: [docs/audio_tone_plan.md](docs/audio_tone_plan.md)
- Gate qualité: [docs/branch_quality_gate.md](docs/branch_quality_gate.md)
- Orchestration dual-repo RTC/Zacus: [docs/CROSS_REPO_INTELLIGENCE.md](docs/CROSS_REPO_INTELLIGENCE.md)
