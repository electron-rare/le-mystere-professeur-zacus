# ESP32 Story V2 — Spécifications WiFi & WebUI

## Objectif
Décrire la connectivité réseau de l’ESP32 (mode station et portail captif), détailler la WebUI exposée, et compiler les commandes / scripts de vérification qui permettent de valider la santé WiFi, les WebSocket et la reprise après perte de connectivité.

## Modes de connectivité
- **Mode station (client)** : l’ESP32 se connecte à un point d’accès classique (`WiFi.begin`) et expose les API HTTP/WS (`/api/status`, `/api/wifi`, `/api/rtos`, `/api/story/stream`) pour piloter la Story et rapporter l’état RTOS/WiFi. Les reconnects sont surveillés via `disconnect_reason/count/label` et `connected=true` doit être atteint <30s après une perte.
- **Mode point d’accès (captive portal)** : lorsque aucun réseau n’est configuré ou en phase de setup, l’ESP32 bascule en AP pour afficher la WebUI (portail captif) permettant de saisir les SSID/credentials, consulter le diagnostic réseau et fournir la télémetrie WiFi. Ce mode reste disponible pour déboguer ou reconfigurer l’appareil hors réseau.
- **Dualité WebUI/API** : l’interface frontale (Smartphone/PC) se connecte au portail captif ou au réseau existant et dispose des mêmes endpoints pour orchestrer les scénarios et suivre l’état réseau (voir la spécification `protocols/STORY_V2_WEBUI.md`).

## WebUI & endpoints critiques
- `/api/status` : bloc `wifi` + `rtos`, expose `connected`, `disconnect_count`, `heap_*`, `stack_min_*`, `tasks`, `runtime_enabled`.
- `/api/wifi` : liste des réseaux, intents de reconnexion, historique (SSID, RSSI, connexion actuelle).
- `/api/rtos` : snapshot détaillé de chaque tâche (stack watermark, ticks, core, last tick).
- `/api/story/stream` (WebSocket) : flux de mise à jour UI Link ; doit recouvrer automatiquement après perte de socket ou reboot.
- `/api/ui/frame` et `/api/ui/config` (selon implémentation) : statut de l’ordre d’affichage et permet la coordination avec l’UI locale (ESP8266 + RP2040).
- Sérial `SYS_RTOS_STATUS` et logs UI Link (GPIO22/TX → ESP8266) sont les derniers recours pour tracer les chutes de stack ou panic.

## Télémetrie & points de santé
- Messages périodiques remontés : `heap_free`, `heap_min`, `stack_min_words`, `stack_min_bytes`, `tasks`.
- Watermarks de stack (per-task) servent à détecter les débordements (stack overflow). Les logs `hardware/firmware/docs/RTOS_WIFI_HEALTH.md` renseignent sur `stack_min_words/bytes`.
- Watchdogs : `kEnableRadioRuntimeWdt` et `kRadioRuntimeWdtTimeoutSec` doivent rester activés pour chaque tâche critique (`StreamNet`, `UiOrchestrator`, etc.).
- WebSocket health : `UI_LINK_STATUS connected==1` (see `hardware/firmware/docs/UI_LINK_DEBUG_REPORT.md`), perte de socket doit générer log + échec de gate.

## Tests et scripts associés
- `tools/dev/rtos_wifi_health.sh` : lance un scan HTTP + RTOS, capture les artefacts `artifacts/rtos_wifi_health_<timestamp>.log`, peut couper/monitorer l’AP (`--serial-debug`). Commandes à automatiser :
  - `ESP_URL=http://<ip>:8080 ./tools/dev/rtos_wifi_health.sh`
  - `./tools/dev/rtos_wifi_health.sh --serial-debug --serial-debug-seconds 600`
- `tools/dev/run_matrix_and_smoke.sh` + `run_smoke_tests.sh` : doivent vérifier reconnect + WebSocket après une coupure réseau (simulate via AP toggle).
- `tools/dev/healthcheck_wifi.sh` : énergie la vérification HTTP, WebSocket et pings (voir `hardware/firmware/docs/AGENT_TODO.md` evidence).
- `podio` : CLI plan runner (option `plan`) doit inclure ces scripts dans les plans `firmware_tests` et `firmware_core`.

## Recovery WiFi & debugging
- Étapes de recovery (`hardware/firmware/docs/WIFI_RECOVERY_AND_HEALTH.md`):
  1. Scanner l’AP et vérifier l’émission du SSID (le module doit apparaître).
  2. Couper le WiFi de l’AP pendant 10-15s, relancer l’ESP32 et confirmer le reconnect (disconnect_label + connected).
  3. Valider que le portail captif reste accessible (mode AP) même sans connexion externe.
  4. Lire `artifacts/rtos_wifi_health_<timestamp>/wifi_serial_debug.log` pour détecter les erreurs `wifi|disconnect|reconnect`.
  5. Si panic/stack overflow, consulter `logs/` et `SYS_RTOS_STATUS`, puis relancer `./tools/dev/run_matrix_and_smoke.sh`.

-## Documentation & artefacts
- Toujours consigner les artefacts (logs HTTP, WS, RTOS, AP scan) dans `docs/AGENT_TODO.md`.
- Mentionner les étapes de la procédure WiFi/WebUI dans `docs/TEST_SCRIPT_COORDINATOR.md` sous la section “WiFi resilience data”.
- Utiliser `tools/dev/plan_runner.sh --agent firmware_tests --dry-run` pour prévisualiser l’enchaînement complet (build, smoke, wifi_health).
-Pour lancer rapidement ce plan depuis Copilot VS Code, utiliser le prompt `trigger_firmware_core_plan.prompt.md` situé dans `hardware/firmware/tools/dev/codex_prompts/` : il exécute `tools/dev/plan_runner.sh --agent firmware_core` et consigne les artefacts dans les mêmes dossiers `docs/AGENT_TODO.md`, `hardware/firmware/logs/` et `artifacts/`.
