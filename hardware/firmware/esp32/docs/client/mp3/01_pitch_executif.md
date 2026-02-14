# Pitch executif - Stack lecteur MP3 U-SON

## Objectif client

Le lecteur MP3 U-SON apporte une experience audio robuste sur ESP32/ESP8266:

- lecture locale SD avec scan non bloquant
- UX pilotable au clavier, en serie et a l'OLED
- observabilite runtime pour diagnostiquer vite
- strategie backend dual pour garantir la lecture multi-formats

## Valeur metier

1. Fiabilite terrain:
- le systeme reste reactif pendant scan et transitions UI
- le lien ecran est resilient (trame `STAT` avec `seq` + CRC)

2. Productivite exploitation:
- commandes serie canoniques pour debug rapide
- scripts QA/runbook pour reproduire les tests en atelier ou sur site

3. Evolutivite:
- capacites backend exposees en runtime (`MP3_CAPS`)
- etat backend detaille (`MP3_BACKEND_STATUS`) pour piloter les decisions futures

## Ce que la demo prouve

1. UX complete:
- pages `NOW`, `BROWSE`, `QUEUE`, `SET`
- feedback action en moins de 250 ms (cible)

2. Robustesse runtime:
- scan actif sans gel de la boucle principale
- reprise ecran apres reset croise ESP32/ESP8266

3. Transparence technique:
- fallback backend explicite via `last_fallback_reason`
- compteurs d'essais/succes/echecs/retry par backend

## Positionnement livraison

Ce lot est pret pour presentation client:

- documents operateur et Q&A fournis
- scripts smoke et checklist live fournis
- matrice build + QA statique rejouable

