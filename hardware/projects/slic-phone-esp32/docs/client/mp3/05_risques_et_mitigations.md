# Risques et mitigations - Demo client MP3

## R1 - Instabilite USB/ports

Impact:

- perte de temps de demo, reset incomplet, moniteur non connecte

Mitigation:

- preflight `pio device list`
- checklist ports `tools/qa/mp3_client_live_checklist.md`
- plan backup logs/video

## R2 - Promesse codec non alignee runtime

Impact:

- incomprehension client sur la couverture reelle des backends

Mitigation:

- ne pas annoncer en dur
- montrer `MP3_CAPS` en live comme source de verite
- expliquer fallback legacy pour garantie multi-formats

## R3 - Freeze percu pendant scan

Impact:

- perception de manque de robustesse

Mitigation:

- lancer `MP3_SCAN START`
- prouver reactivite via navigation/page change + `MP3_SCAN_PROGRESS`
- rappeler budget tick non bloquant

## R4 - Reconnexion ecran lente ou erratique

Impact:

- demo interrompue, doute sur robustesse systeme

Mitigation:

- test reset croise en repetition
- conserver commande `MP3_UI_STATUS` pour prouver continuite ESP32
- fallback narration sur runbook + evidences

## R5 - Divergence branches/PR

Impact:

- demo sur un code non traceable

Mitigation:

- sequence PR imposee (`#44` mergee, branche demo dediee)
- commit/tag de presentation explicites

