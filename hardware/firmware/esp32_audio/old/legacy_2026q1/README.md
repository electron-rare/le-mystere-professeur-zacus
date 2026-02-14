## Legacy Archive 2026Q1

Ce dossier archive les elements retires du runtime actif pendant la refonte V2.

### Retires du build actif

- anciens wrappers controleurs `src/controllers/boot_protocol_controller.*`
- anciens wrappers controleurs `src/controllers/story_controller.*`
- chemins audio bloquants historiques (helpers FS/I2S/diag) retires de `src/app/app_orchestrator.cpp`

### Remplacement

- boot: `src/controllers/boot/boot_protocol_runtime.*`
- story: `src/controllers/story/story_controller.*`
- audio runtime: `src/services/audio/audio_service.*` + appels async depuis `src/app/app_orchestrator.cpp`
- scan catalogue SD non bloquant: `src/services/storage/catalog_scan_service.*` + `src/audio/mp3_player.*`
