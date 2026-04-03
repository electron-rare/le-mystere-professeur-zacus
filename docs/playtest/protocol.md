# Protocole de Bêta-Test — Le Mystère du Professeur Zacus V3

> Version : 1.0 — 2026-04-03
> Objectif : Valider l'expérience complète avec 2 groupes minimum (profil TECH + profil NON_TECH)

---

## Sélection des Groupes

| Critère | Groupe 1 (TECH) | Groupe 2 (NON_TECH) |
|---------|-----------------|---------------------|
| Taille | 4–6 personnes | 4–6 personnes |
| Profil | Développeurs, ingénieurs, makers | Artistes, enseignants, étudiants hors tech |
| Âge recommandé | 18–45 ans | 10–50 ans |
| Expérience escape room | Pas d'expérience (ou peu) | Pas d'expérience (ou peu) |
| Langue | Français | Français |

---

## Brief Pré-Session (30 min avant chaque groupe)

### Checklist déploiement complet

- [ ] Checklist 15 minutes (`docs/deployment/15min_setup_checklist.md`) complétée
- [ ] Durée cible réglée à **60 minutes** pour le bêta-test
- [ ] Caméra d'observation positionnée (consentement joueurs signé)
- [ ] Grille d'observation imprimée pour chaque observateur (cette page, section "Points d'observation")
- [ ] Questionnaire NPS préparé (papier ou Typeform sur téléphone)
- [ ] Chronomètre prêt (ou timer BOX-3 activé)
- [ ] Maître du jeu en position d'observation silencieuse

### Consignes aux observateurs

L'observateur **ne participe pas** et **ne donne aucune aide** sauf urgence de sécurité. Il note en silence :
- Les temps de résolution
- Les comportements de groupe
- Les verbatims spontanés (noter mot pour mot)
- Les moments de blocage, frustration, découverte

---

## Points d'Observation par Puzzle

Pour chaque puzzle, l'observateur complète la grille suivante :

### Grille d'observation (par puzzle)

| Puzzle | Heure début | Heure fin | Durée | Indices demandés | Tentatives ratées | Verbatims clés | Observation |
|--------|------------|-----------|-------|-----------------|-------------------|----------------|-------------|
| P1 — Séquence Sonore | | | | | | | |
| P2 — Circuit LED | | | | | | | |
| P3 — QR Treasure | | | | | | | |
| P4 — Fréquence Radio | | | | | | | |
| P5 — Code Morse | | | | | | | |
| P6 — Symboles Alchimiques | | | | | | | |
| P7 — Coffre Final | | | | | | | |

### Comportements à noter impérativement

- **Confusion initiale :** "C'est quoi ça ?", "On fait quoi ?" sans lire les instructions
- **Blocage prolongé** : plus de 5 min sans progression visible
- **Moment "Eurêka"** : réaction physique ou verbale de découverte
- **Frustration** : abandon, verbalisation négative, regard vers l'observateur
- **Collaboration** : qui prend le leadership ? comment le groupe se coordonne-t-il ?
- **Réaction au Professeur Zacus** : rire, sourire, question sur la voix (est-ce un humain ?)

---

## Vérification de la Classification NPC (Phase de Profilage)

À noter après la résolution de P1 et P2 :

| Donnée | Valeur mesurée |
|--------|---------------|
| Temps P1 (P1 solve time) | _______ secondes |
| Temps P2 (P2 solve time) | _______ secondes |
| Classification NPC affichée sur BOX-3 | TECH / NON_TECH / MIXED |
| Votre évaluation subjective du groupe | TECH / NON_TECH / MIXED |
| Classification correcte ? | OUI / NON |

**Objectif bêta :** > 80 % de classifications correctes sur l'ensemble des sessions.

---

## Suivi de la Précision Temporelle

| Donnée | Valeur |
|--------|--------|
| Durée cible configurée | 60 minutes |
| Heure de début effective | ____:____ |
| Heure de fin effective | ____:____ |
| Durée réelle | _______ minutes |
| Écart (réel - cible) | _______ minutes (_______ %) |

**Objectif bêta :** écart ≤ ± 10 % de la durée cible (soit ≤ 6 minutes sur 60 min).

---

## Débrief Post-Session (30 min après la fin du jeu)

### Phase 1 : Discussion libre (10 min)

Laisser le groupe s'exprimer librement. Questions ouvertes :
- "Quelle a été votre première impression en arrivant ?"
- "Quel puzzle vous a le plus surpris ?"
- "Avez-vous cru que le Professeur était une vraie personne ?"
- "Qu'est-ce qui vous a le plus bloqués ?"
- "Si vous pouviez changer une chose, ce serait quoi ?"

### Phase 2 : Enquête NPS (10 min)

Distribuer le questionnaire NPS (voir section suivante). Anonyme, individuel, sans discussion.

### Phase 3 : Questions techniques (10 min, optionnel)

Pour les groupes curieux (souvent TECH) :
- Montrer l'architecture ESP-NOW sur BOX-3
- Expliquer le clonage vocal XTTS-v2
- Montrer le scoring adaptatif

---

## Questionnaire NPS — 10 Questions (Français)

> Instructions : Notez chacune des affirmations suivantes de 1 (pas du tout) à 10 (absolument).

**Q1.** Dans quelle mesure recommanderiez-vous cette expérience à un ami ou collègue ?
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q2.** Le Professeur Zacus vous a semblé crédible et engageant comme personnage.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q3.** La difficulté des puzzles était adaptée à votre groupe.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q4.** Les instructions de chaque puzzle étaient claires et compréhensibles.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q5.** La qualité de la voix du Professeur Zacus était convaincante.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q6.** L'ambiance générale (décor, son, lumières) vous a immergé dans l'univers.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q7.** Vous avez ressenti une progression narrative cohérente du début à la fin.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q8.** Le matériel physique (boîtiers, tablettes, télégraphe) vous a semblé solide et bien fait.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q9.** L'expérience a favorisé la cohésion et la collaboration au sein de votre groupe.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Q10.** Vous seriez prêt(e) à participer à une prochaine version ou à un scénario différent.
`[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ][ 8 ][ 9 ][ 10 ]`

**Questions ouvertes :**
- Votre puzzle préféré et pourquoi : ___________________________________
- Le moment le plus confus ou frustrant : ___________________________________
- Suggestions libres : ___________________________________

---

## Critères de Succès du Bêta-Test

| Critère | Objectif | Mesure |
|---------|----------|--------|
| Taux de complétion | Les 2 groupes atteignent GAME_OUTRO | BOX-3 session log |
| NPS moyen (Q1) | ≥ 8/10 | Questionnaire |
| Temps de setup | ≤ 15 minutes | Chronomètre |
| Précision durée | Écart ≤ ±10 % | Mesure directe |
| Classification NPC | Correcte pour les 2 groupes | Grille observation |
| Pannes matériel | Zéro pendant la session | Grille observation |
| Voix perçue comme humaine | ≥ 3/5 joueurs ne font pas la différence | Q5 + verbatims |

---

## Checklist Observateur (pendant le jeu)

- [ ] Timer démarré à `[GAME_START]` (synchro avec BOX-3)
- [ ] Grille observation imprimée, stylo en main
- [ ] Téléphone en mode silencieux
- [ ] Caméra en marche (si consentement obtenu)
- [ ] Ne pas répondre aux questions des joueurs (sourire et pointer BOX-3)
- [ ] Marquer toute panne matérielle immédiatement (heure + description)
- [ ] Marquer toute intervention NPC (heure + phrase + réaction groupe)
- [ ] À la fin : noter heure exacte de GAME_OUTRO et score affiché
