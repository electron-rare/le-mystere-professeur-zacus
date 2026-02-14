# Validation Manuelle Clavier (hors smoke)

Ce runbook est volontairement separé des smoke tests automatiques.

## Pré-requis
1. ESP32 + ESP8266 flashés et démarrés.
2. SD insérée avec pistes valides.

## Test MP3 UI
1. Passer en mode MP3.
2. Vérifier UI V3.1:
- `K1`: `OK` (play/pause, select, apply setting)
- `K2`: `UP`
- `K3`: `DOWN`
- `K4`: `LEFT` (prev / decrement setting)
- `K5`: `RIGHT` (next / increment setting)
- `K6` court: page suivante
- `K6` long: page precedente
- `K6` en `LECTURE`: bascule source `SD <-> RADIO`

## Test Story/U_LOCK
1. Revenir en mode signal/U_LOCK.
2. Vérifier la réactivité des touches selon scénario actif.

## Résultat
1. PASS: toutes les actions clavier attendues fonctionnent.
2. FAIL: noter touche, contexte (page/mode), logs série, reproduction.
