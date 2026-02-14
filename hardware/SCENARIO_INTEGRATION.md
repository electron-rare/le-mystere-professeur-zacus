# Intégration scénario Zacus v1 <-> hardware

## Mapping gameplay -> devices
- **U-LOCK**: verrou d’entrée mission; déverrouillage vocal/ton LA.
- **U-SON**: boot corrompu, progression, jingle victoire.
- **Oscilloscope**: sinus ETAPE 1, morse partiel ETAPE 4.
- **Téléphones zones**: indices tokens L / AFO / EFO / LE / U.
- **Téléphone hotline (salon)**: validation/indice/relance Brigade Z.

## Préparer la SD
Exemple structure:
- `/story/` specs story v2 compilées
- `/audio/uson/` fichiers U-SON
- `/audio/hotline/` répliques courtes
- `/audio/zones/` radios brouillées
- `/audio/ambiances/` longues boucles

Nommage conseillé: `zacus_v1_<device>_<cue>_v01.mp3`

## ETAPE 2 (delay story)
- Départ: état verrouillé.
- Après déverrouillage + délai paramétré: step `ETAPE2_HINT`.
- Action: jouer hint audio salon + garder gate MP3 fermé.
- Puis passage `STEP_DONE` pour ouvrir gate MP3 (selon scénario choisi).
