# Changelog Firmware

Tous les changements notables du firmware sont documentés ici.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.0.0/),
et ce projet adhère au [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

### En cours (branche `hardware/firmware`)

#### Added
- Documentation complète architecture UML (diagrammes classes/séquence)
- Documentation état des lieux firmware (métriques, tests, structure)
- Documentation recommandations sprint (roadmap actions)
- Index navigation documentation (docs/INDEX.md)

#### Changed
- Structure nettoyée : suppression symlink `esp32 → esp32_audio`
- Structure nettoyée : suppression duplication `hardware/firmware/hardware/`
- Paths corrigés dans AGENTS.md, READMEs, tools/qa docs
- Port mapping ESP8266 OLED : marker ready timeout étendu
- Port mapping ESP32/ESP8266 : swap 20-6.4.1/20-6.4.2

#### Fixed
- Documentation cohérente (esp32/ vs esp32_audio/ résolu)

---

## [0.9.0-beta] - 2026-02-15 (à venir après merge PR #86)

### Summary

**Refonte complète architecture firmware** avec introduction multi-MCU (ESP32 + ESP8266 OLED + RP2040 TFT), Story Engine V2, protocole UI Link v2, dual-channel audio.

### Added

#### Architecture multi-MCU
- ESP32 Audio Kit : Firmware principal C++ (18.6K lignes)
- ESP8266 NodeMCU : UI OLED SSD1306 (1.2K lignes)
- RP2040 Pico : UI TFT tactile LVGL (285 lignes)
- Protocol UI Link v2 : UART 19200 baud, frame-based ASCII

#### Story Engine V2
- Machine à états déclarative (ScenarioDef YAML)
- Event queue asynchrone (BTN, TOUCH, timers, conditions)
- Transitions implicites (timeout, LA detection, MP3 gate)
- Actions modulaires via ActionRegistry
- Story Apps pluggables : LaDetectorApp, Mp3GateApp, ScreenSceneApp, AudioPackApp
- StoryAppHost : Lifecycle apps (activate/deactivate par step)

#### Audio subsystem
- Dual-channel audio : Base (main) + Overlay (effects)
- AsyncAudioService : MP3 decoder ESP8266Audio
- FmRadioScanFx : Effets I2S (sweep, noise, beep) via arduino-audio-tools
- Mp3Player : Lecteur SD synchrone, TrackCatalog
- I2sJinglePlayer : RTTTL ringtones
- CodecES8388Driver : I2C/I2S hardware config
- Timeouts explicites : Base 30s, Overlay 5s
- Fallback FX si MP3 manquant/corrupt

#### Services layer
- AudioService : Dual-channel orchestration
- RadioService : Web streaming, station presets
- WifiService : AP mode, network management
- WebUiService : AsyncWebServer REST API
- InputService : Analog keypad, debounce, long-press
- ScreenSyncService : UI Link coordination, ScreenFrame serialization
- SerialRouter : USB terminal, debug commands
- CatalogScanService : SD track indexing
- LaDetectorRuntimeService : 440Hz detection

#### Controllers layer
- StoryControllerV2 : Story mode orchestration (142 lignes)
- Mp3Controller : SD player mode
- InputController : Keyboard/touch routing
- BootProtocolRuntime : Boot sequence (Serial, WiFi, Codec, I2C, SD)

#### UI Link Protocol v2
- Frame types : HELLO, ACK, KEYFRAME, STAT, BTN, TOUCH, PING, PONG
- Fields : mode, track, track_tot, vol, tune_ofs, tune_conf, uptime_ms, seq, scene
- CRC8 validation, 320 byte max line, 40 fields max
- Timeouts : HEARTBEAT_MS=1000, TIMEOUT_MS=1500
- Reconnection automatique : session counter, KEYFRAME resync

#### PlatformIO workspace
- 5 environments : esp32dev, esp32_release, esp8266_oled, ui_rp2040_ili9488, ui_rp2040_ili9486
- Shared protocol/ directory (ui_link_v2.h/md)
- Fast build Makefile targets : `make fast-esp32`, `make fast-ui-oled`, `make fast-ui-tft`
- `build_all.sh` : Build 5/5 environments

#### Testing & Automation
- `tools/dev/run_matrix_and_smoke.sh` : Build matrix + smoke tests hardware
- Port mapping by LOCATION (macOS CP2102 stable)
- Smoke gates stricts : panic/reboot markers → FAIL
- UI Link status gate : `connected==1` requis
- RC Live sessions : `./tools/dev/zacus.sh rc`
- Artifacts : `artifacts/rc_live/{timestamp}/` (logs + rapport _rc.md)

#### Documentation
- docs/QUICKSTART.md : Getting started dev
- docs/HW_NOW.md : Hardware status rapide
- docs/RC_FINAL_BOARD.md : Tests RC dashboard
- docs/protocols/PROTOCOL.md : Story Engine V2 format YAML
- protocol/ui_link_v2.md : Spéc protocole UART
- esp32_audio/WIRING.md : Câblage ESP32/UI
- ui/rp2040_tft/WIRING.md : Câblage TFT

### Changed

#### Runtime modes flexibles
- Mode STORY_V2 (défaut) : Quête ETAPE1 → unlock → ETAPE2 → MP3 libre
- Mode MP3_PLAYER : Lecteur SD pur (skip story)
- Mode RADIO : Streaming web (skip story + MP3)
- Sélection boot via config.h flags

#### Architecture refactor
- Séparation claire Controllers / Services / Drivers
- Event-driven architecture (non-blocking)
- Dependency injection via constructeurs
- Error handling via Result enum (Success/Timeout/Failed)
- Timeouts explicites partout (audio, UI link, network)

#### Build improvements
- PlatformIO cache : `$HOME/.platformio` (pas dans repo)
- Artifacts deterministes : `artifacts/rc_live/<timestamp>/`
- Environment overrides : ZACUS_REQUIRE_HW, ZACUS_WAIT_PORT, ZACUS_NO_COUNTDOWN, ZACUS_SKIP_SMOKE, ZACUS_ENV, ZACUS_FORCE_BUILD

### Deprecated

- Ancien Story Engine V1 (remplacé par V2)
- Protocole UI Link v1 (remplacé par v2 frames)
- Single-channel audio (remplacé par dual-channel)
- Hardcoded scénarios (remplacé par YAML loader)

### Removed

- Code legacy Story V1 (src/story_old/)
- Anciens tests manuels (remplacés par automation)
- Symlink `esp32 → esp32_audio` (confusion docs)
- Duplicate `hardware/firmware/hardware/` (artifacts)

### Fixed

- UI Link reconnection robuste (session counter, timeout recovery)
- Audio timeout fallback (MP3 bloqué → FX I2S)
- ESP8266 OLED heap stability (pooled buffers)
- Port mapping macOS CP2102 (by LOCATION stable)
- Panic detection smoke tests (strict gates)

### Security

- Aucun secret committé (tokens exclus .gitignore)
- WiFi credentials via config.h (local only, .gitignore)
- Web UI authentication stub (TODO prod)

---

## [0.1.0] - 2024-XX-XX

### Initial Release

- Proof-of-concept ESP32 Audio Kit
- Simple MP3 player SD
- Basic web UI control
- Single MCU architecture

---

## Conventions de versioning

- **MAJOR** : Breaking changes architecture/API
- **MINOR** : Ajout features backward compatible
- **PATCH** : Bug fixes backward compatible
- **Suffix** :
  - `-alpha` : Fonctionnalités incomplètes, instable
  - `-beta` : Fonctionnalités complètes, tests en cours
  - `-rc.N` : Release candidate (pre-production)
  - Rien : Release stable production

**Exemples :**
- `0.9.0-beta` : Refonte v2 beta (pre-production)
- `1.0.0-rc.1` : Release candidate 1 (pre-prod final)
- `1.0.0` : First stable production release
- `1.1.0` : Ajout feature (ex: nouveau runtime mode)
- `1.1.1` : Bug fix
- `2.0.0` : Breaking change (ex: nouveau protocole incompatible)

---

**Dernière mise à jour :** 15 février 2026  
**Maintenu par :** Firmware team (@electron-rare)
