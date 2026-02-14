# Validation Manuelle Clavier (hors smoke)

Ce runbook est volontairement separé des smoke tests automatiques.

## Pré-requis
1. ESP32 + ESP8266 flashés et démarrés.
2. SD insérée avec pistes valides.

## Test MP3 UI
1. Passer en mode MP3.
2. Vérifier:
- `K1`: play/pause (ou action setting)
- `K2/K3`: navigation prev/next ou curseur selon page
- `K4/K5`: volume -/+
- `K6`: changement de page

## Test Story/U_LOCK
1. Revenir en mode signal/U_LOCK.
2. Vérifier la réactivité des touches selon scénario actif.

## Résultat
1. PASS: toutes les actions clavier attendues fonctionnent.
2. FAIL: noter touche, contexte (page/mode), logs série, reproduction.
