# Script - Le mystère du Professeur Zacus — Version Réelle (U-SON, piano LEFOU, QR WIN)

## Introduction

Le Professeur Zacus a disparu après avoir déclenché une alerte cryptée. Le prototype U-SON (un module sonore expérimental) a commencé à “dériver” : ses signaux deviennent instables, comme si quelque chose perturbait le Campus. Pour protéger U-SON, l’équipe doit prouver deux choses : (1) stabiliser une référence scientifique (LA 440 Hz), (2) respecter la règle de Zone 4 — l’installation personnelle de l’Électron Fou — avant de récupérer la clé finale (QR “WIN”) cachée aux Archives.

## Chronologie canon

- **Acte 1 — Stabiliser U-SON (LA 440) (≈45 min)** - Valider Étape 1 via la référence LA 440 Hz.
- **Acte 2 — Zone 4 (piano LEFOU) + Archives (QR WIN) (≈60 min)** - Valider Étape 2 via piano-alphabet (LEFOU), puis scanner QR WIN derrière le portrait.

## Stations

- **Atelier des Ondes** - Stabiliser la référence LA 440 Hz pour recaler U-SON.
- **Zone 4 — Studio de Résonance (Piano Alphabet)** - Jouer 5 lettres sur le piano-alphabet pour valider l’accès.
- **Salle des Archives** - Trouver et scanner la clé finale.

## Puzzles

- PUZZLE_LA_440 (audio / stabilisation) - Produire un LA 440 Hz stable.
- PUZZLE_PIANO_ALPHABET_5 (piano / code lettres) - Jouer 5 lettres sur le piano-alphabet.
- PUZZLE_QR_WIN (recherche / scan) - Scanner le QR final.

## Solution canon

- Coupable : Aucun coupable unique, epreuve orchestrée par Zacus.
- Mobile : Si l’équipe échoue, le Campus reste en mode sécurité et U-SON demeure instable. Le vrai test n’est pas la vitesse : c’est la coopération, la précision et le calme.
- Methode : Progression V2: STEP_U_SON_PROTO -> STEP_LA_DETECTOR -> STEP_RTC_ESP_ETAPE1 -> STEP_WIN_ETAPE1 -> STEP_WARNING -> STEP_LEFOU_DETECTOR -> STEP_RTC_ESP_ETAPE2 -> STEP_QR_DETECTOR -> STEP_FINAL_WIN
- Preuves :
  - Le son doit rester stable quelques secondes ; l’écran/LED confirme l’alignement. Ensuite, la transmission Étape 1 peut être validée (ACK_WIN1).
  - Validation “réelle terrain” : le MJ écoute/observe la séquence, puis déclenche la transition (unlock / BTN_NEXT / serial) vers la confirmation Étape 2.
  - Timeout 30s → QR_TIMEOUT → retour étape précédente + backup MJ.

## Notes pratiques

- Solution unique : True
