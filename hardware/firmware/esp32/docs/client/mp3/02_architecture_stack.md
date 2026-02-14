# Architecture - Stack lecteur MP3 moderne

## Vue d'ensemble

Le lecteur MP3 est construit en couches non bloquantes:

1. `serial_commands_mp3`:
- point d'entree canonique des commandes `MP3_*`

2. `Mp3Controller`:
- expose status/UI/caps/backend pour serie et ecran
- centralise l'etat utile a la presentation

3. `Mp3Player`:
- orchestration playback + scan incremental + persistance
- selection backend et fallback deterministe

4. Backends audio:
- `AudioToolsBackend` (capacites runtime declarees)
- backend legacy ESP8266Audio (garantie multi-formats)

5. Ecran ESP8266:
- affiche l'etat MP3 via trame `STAT`
- parse backward-compatible + robustesse `seq`/CRC

## Contrats publics figes

## Commandes MP3

- `MP3_STATUS`
- `MP3_UI_STATUS`
- `MP3_SCAN START|STATUS|CANCEL|REBUILD`
- `MP3_SCAN_PROGRESS`
- `MP3_BACKEND STATUS|SET <AUTO|AUDIO_TOOLS|LEGACY>`
- `MP3_BACKEND_STATUS`
- `MP3_QUEUE_PREVIEW [n]`
- `MP3_CAPS`

Reponses canoniques:

- `OK`
- `BAD_ARGS`
- `OUT_OF_CONTEXT`
- `NOT_FOUND`
- `BUSY`
- `UNKNOWN`

## Format backend status

`MP3_BACKEND_STATUS` expose:

- mode backend et backend actif
- erreur courante + `last_fallback_reason`
- compteurs globaux (`attempts/success/fail/retries/fallback`)
- compteurs par backend (`tools_*`, `legacy_*`)

## Format caps runtime

`MP3_CAPS` expose les capacites reelles:

- matrice codec/backend pour `tools` et `legacy`
- mode et backend actif

## Donnees ecran MP3

Trame `STAT` (additive, backward-compatible):

- `ui_cursor`
- `ui_offset`
- `ui_count`
- `queue_count`

## Strategie fallback

1. Mode `AUTO_FALLBACK`:
- tentative backend tools
- fallback legacy si codec non supporte ou erreur d'init

2. Mode `AUDIO_TOOLS_ONLY`:
- pas de fallback, retry suivant politique runtime

3. Mode `LEGACY_ONLY`:
- bypass tools, compatibilite maximale

## Points de fiabilite

1. Scan catalogue incremental (budget par tick)
2. Aucune attente active critique dans la boucle MP3
3. Observabilite serie exploitable sans debugger
4. Recovery ecran/lien sans bloquer l'audio

