# Script demo live client (12 minutes)

## Preparation (avant audience)

1. Lancer `tools/qa/mp3_client_demo_smoke.sh`
2. Flasher ESP32 + ESP8266:
- `make upload-esp32 ESP32_PORT=<PORT_ESP32>`
- `make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>`
- `make upload-screen SCREEN_PORT=<PORT_ESP8266>`
3. Ouvrir moniteur ESP32:
- `make monitor-esp32 ESP32_PORT=<PORT_ESP32>`

## Deroule minute par minute

## 0:00 - 2:00 Contexte

Message:
- "On montre une stack MP3 moderne, pilotable en live et robuste en runtime."
- "Le systeme reste reactif pendant scan et recovery ecran."

## 2:00 - 5:00 Etat runtime

Commandes:

1. `MP3_STATUS`
2. `MP3_UI_STATUS`
3. `MP3_CAPS`
4. `MP3_BACKEND_STATUS`

Attendus:

- reponses canoniques
- capacites runtime explicites
- compteurs backend visibles

## 5:00 - 9:00 UX complete 4 pages

Commandes:

1. `MP3_UI PAGE NOW`
2. `MP3_UI PAGE BROWSE`
3. `MP3_UI PAGE QUEUE`
4. `MP3_UI PAGE SET`
5. `MP3_UI_STATUS`
6. `MP3_QUEUE_PREVIEW 5`

Attendus:

- OLED synchronise avec la page active
- `MP3_UI_STATUS` coherent avec ce qui est affiche

## 9:00 - 11:00 Scan non bloquant + fallback

Commandes:

1. `MP3_SCAN START`
2. `MP3_SCAN_PROGRESS` (repetee 2-3 fois)
3. `MP3_BACKEND STATUS`
4. `MP3_BACKEND_STATUS`

Attendus:

- progression scan visible
- aucune perte de reactivite serie/clavier/ecran
- fallback reason exploitable si bascule backend

## 11:00 - 12:00 Robustesse ecran

Action:

1. reset ESP8266 seul
2. verifier reprise affichage (< 2 s cible)
3. `MP3_UI_STATUS` pour prouver continuite cote ESP32

## Plan de secours (backup)

Si incident live:

1. basculer sur logs pre-captures locaux (`reports/`, non versionnes)
2. montrer les sections:
- `docs/client/mp3/04_evidence_qa.md`
- `tools/qa/mp3_client_live_checklist.md`
- `docs/client/mp3/05_risques_et_mitigations.md`

