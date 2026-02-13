# Générer un scénario (méthode simple)

Ce guide te donne une méthode rapide pour construire un scénario jouable et cohérent pour **Le Mystère du Professeur Zacus**.

## 1) Poser la solution avant les indices

Commence par remplir `solution-complete.md` avec une solution unique :
- **Coupable** (qui ?)
- **Mobile** (pourquoi ?)
- **Méthode** (comment ?)
- **Chronologie** (dans quel ordre ?)

Règle d’or : chaque indice doit confirmer la même vérité finale, sans contradiction.

## 2) Transformer la solution en parcours de jeu

Utilise `script-minute-par-minute.md` pour découper la partie en 5 blocs :
1. Accueil + mise en ambiance
2. Découverte du mystère
3. Enquête en équipes
4. Mise en commun des hypothèses
5. Révélation finale

Pour des enfants de 9–11 ans, vise des séquences courtes et rythmées.

## 3) Répartir les informations par rôle et par station

- Dans `distribution-des-roles.md`, donne à chaque équipe un angle d’enquête différent.
- Dans `plan-stations-et-mise-en-place.md`, associe chaque station à 1 information clé.

Bon équilibre :
- des indices faciles (confiance)
- des indices moyens (déduction)
- 1 ou 2 indices-pivot (twist)

## 4) Vérifier la logique avec un test « à rebours »

Teste ton scénario à rebours :
- En partant de la révélation, peut-on justifier chaque affirmation par un indice concret ?
- Un indice peut-il être interprété dans deux sens contradictoires ?
- Sans un indice donné, l’enquête reste-t-elle possible ?

Si blocage : simplifie la chronologie et retire les faux fils trop complexes.

## 5) Préparer la version animateur

Avant de jouer :
- checklist dans `checklist-materiel.md`
- rythme et transitions dans `guide-anti-chaos.md`
- variantes avec/sans stations bonus (`stations/`)

Objectif : un scénario robuste même si le timing dérive de 10–15 minutes.

## Trame prête à copier

Tu peux partir de ce squelette :

- **Mystère de départ** : disparition / sabotage / message codé.
- **Coupable** : personnage avec motivation compréhensible.
- **Mobile** : jalousie / peur / malentendu / secret à protéger.
- **Méthode** : action réalisable avec les éléments du décor.
- **Twist final** : indice réinterprété au moment de la révélation.

Puis décline en 6 à 10 indices, répartis entre équipes et stations.
## Et côté firmware ESP32 ?

Si tu veux synchroniser ce scénario avec la logique embarquée, utilise le guide firmware: `hardware/firmware/esp32/GENERER_UN_SCENARIO_STORY_V2.md`.

