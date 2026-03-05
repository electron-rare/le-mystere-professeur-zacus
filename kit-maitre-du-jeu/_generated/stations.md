# Stations detaillees

## Atelier des Ondes

- Focus : Stabiliser la référence LA 440 Hz pour recaler U-SON.
- Clue : La force ne sert à rien. Seule la stabilité ouvre le premier verrou.

## Zone 4 — Studio de Résonance (Piano Alphabet)

- Focus : Jouer 5 lettres sur le piano-alphabet pour valider l’accès.
- Clue : Le Fou ne valide pas la force. Il valide son nom.

## Salle des Archives

- Focus : Trouver et scanner la clé finale.
- Clue : Regarde là où Zacus surveille toujours.

## Puzzles

- PUZZLE_LA_440 (audio / stabilisation) : Produire un LA 440 Hz stable.
  Effet : Le son doit rester stable quelques secondes ; l’écran/LED confirme l’alignement. Ensuite, la transmission Étape 1 peut être validée (ACK_WIN1).

- PUZZLE_PIANO_ALPHABET_5 (piano / code lettres) : Jouer 5 lettres sur le piano-alphabet.
  Effet : Validation “réelle terrain” : le MJ écoute/observe la séquence, puis déclenche la transition (unlock / BTN_NEXT / serial) vers la confirmation Étape 2.

- PUZZLE_QR_WIN (recherche / scan) : Scanner le QR final.
  Effet : Timeout 30s → QR_TIMEOUT → retour étape précédente + backup MJ.
