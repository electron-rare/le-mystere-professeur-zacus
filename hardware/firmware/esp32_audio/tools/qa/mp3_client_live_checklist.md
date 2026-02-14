# Checklist live client MP3 (operateur)

## A. Preflight materiel

- [ ] `pio device list` execute
- [ ] port ESP32 identifie
- [ ] port ESP8266 identifie
- [ ] carte SD inseree (pistes de test chargees)
- [ ] liaison UART ESP32->ESP8266 verifiee (`GPIO22 -> D6`, GND commun)

## B. Flash

- [ ] `make upload-esp32 ESP32_PORT=<PORT_ESP32>`
- [ ] `make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>`
- [ ] `make upload-screen SCREEN_PORT=<PORT_ESP8266>`
- [ ] moniteur ESP32 ouvert

## C. Script commandes live (ordre fige)

- [ ] `MP3_STATUS`
- [ ] `MP3_UI_STATUS`
- [ ] `MP3_SCAN START`
- [ ] `MP3_SCAN_PROGRESS`
- [ ] `MP3_UI PAGE NOW`
- [ ] `MP3_UI PAGE BROWSE`
- [ ] `MP3_UI PAGE QUEUE`
- [ ] `MP3_UI PAGE SET`
- [ ] `MP3_QUEUE_PREVIEW 5`
- [ ] `MP3_BACKEND STATUS`
- [ ] `MP3_BACKEND_STATUS`
- [ ] `MP3_CAPS`

## D. Validation immediate

- [ ] reponses canoniques visibles
- [ ] OLED affiche la page active
- [ ] scan non bloquant perceptible
- [ ] backend status lisible (fallback/counters)

## E. Recovery rapide

- [ ] reset ESP8266 seul -> reprise ecran < 2 s
- [ ] reset ESP32 seul -> reprise sequence normale

## F. Conditions fallback backup

Declencher backup si au moins un critere est vrai:

- [ ] port USB instable (> 1 deconnexion)
- [ ] erreur critique non recuperable pendant la fenetre demo
- [ ] temps restant demo < 3 minutes

Actions fallback:

1. Basculer sur `docs/client/mp3/04_evidence_qa.md`
2. Montrer `docs/client/mp3/05_risques_et_mitigations.md`
3. Poursuivre Q&A avec `docs/client/mp3/06_qna_client.md`

