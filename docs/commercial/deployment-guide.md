# Guide de Déploiement — Le Mystère du Professeur Zacus

**Objectif :** Installation complète en 15 minutes sur n'importe quel lieu.
**Prérequis :** Kit V3 complet (3 valises), batteries chargées à 100%.

---

## Avant de partir (30 min, à faire chez vous / au bureau)

### Vérification matérielle

- [ ] Charger toutes les batteries à 100 % (chargeur GaN 65W 4 ports, ~3h)
  - Anker #1 (hub) — 20 000 mAh
  - Anker #2 (spare) — 20 000 mAh
  - Pack puzzle A — 10 000 mAh
  - Pack puzzle B — 10 000 mAh
- [ ] Allumer BOX-3 → vérifier firmware version (Settings → About → Version ≥ v3.1)
- [ ] Allumer chaque puzzle ESP32 (LED verte = OK, LED rouge = recharger)
- [ ] Compter les 12 tuiles NFC symboles alchimiques (P6)
- [ ] Vérifier les 6 QR codes A5 plastifiés (aucun ne doit être plié ou illisible)

<!-- PHOTO: [batteries_charged.jpg] — 4 batteries posées côte à côte, toutes LED vertes -->

### Vérification logicielle

- [ ] BOX-3 → Settings → Scenario → `zacus_v3_complete` chargé
- [ ] Pool TTS MP3 à jour (si nouvelles phrases ajoutées depuis la dernière session)
  ```bash
  python3 tools/tts/generate_npc_pool.py --dry-run
  ```
- [ ] Test ESP-NOW mesh rapide (à la maison, avant transport) :
  ```bash
  ./tools/dev/zacus.sh espnow-test
  ```
  Résultat attendu : `MESH: 6/6 OK`

---

## Sur place : Installation en 15 minutes

### Minutes 1–2 : Hub central (Valise 1)

<!-- PHOTO: [valise1_layout.jpg] — contenu de la valise 1 vue du dessus -->

- [ ] Poser RTC_PHONE sur la table d'accueil, brancher USB-C → Batterie #1
- [ ] Poser BOX-3 sur mini-trépied, orienter caméra vers le centre de la pièce
- [ ] Brancher BOX-3 USB-C → Batterie #1 (même batterie, hub USB-C 2 ports)
- [ ] Brancher le routeur GL.iNet → Batterie #2 USB-C
- [ ] Attendre les LED vertes sur les 3 appareils (~30 secondes)

**Contrôle visuel :** BOX-3 affiche « WAITING FOR MESH… »

<!-- PHOTO: [hub_setup.jpg] — RTC_PHONE + BOX-3 + routeur sur table, tous allumés -->

---

### Minutes 3–7 : Puzzles (Valise 2)

Placer chaque puzzle à sa position définie dans la salle (voir plan de salle ci-dessous).

<!-- PHOTO: [floor_plan.jpg] — schéma de placement des 7 puzzles dans une salle générique -->

- [ ] **P1 — Séquence Sonore** → table latérale, brancher USB-C → Pack A
- [ ] **P2 — Breadboard Magnétique** → sol ou grande table, déplier, USB-C → Pack A
- [ ] **P4 — Poste Radio** → rebord de fenêtre ou étagère (hauteur yeux)
- [ ] **P5 — Télégraphe** → bureau avec la carte de référence morse posée à côté
- [ ] **P6 — Tablette Symboles** → grande table, disposer les 12 tuiles NFC autour
- [ ] **P7 — Coffre Final** → position centrale et bien visible, USB-C → Pack B, VERROU FERMÉ
- [ ] Cacher les 6 QR codes A5 aux positions définies (voir carte de placement dans valise 1)

<!-- PHOTO: [p1_setup.jpg] — P1 boîte sonore sur table -->
<!-- PHOTO: [p6_setup.jpg] — tablette symboles avec tuiles disposées autour -->
<!-- PHOTO: [p7_setup.jpg] — coffre final position centrale, verrou visible -->

**Contrôle :** Chaque puzzle allumé = LED verte fixe. Si LED orange = calibration en cours (attendre 10s).

---

### Minutes 8–10 : Ambiance (Valise 3)

<!-- PHOTO: [valise3_layout.jpg] — enceinte BT + LED strip + signalétique -->

- [ ] Poser l'enceinte Bluetooth, maintenir le bouton pairing 3 secondes
- [ ] Sur BOX-3 → Settings → Bluetooth → connecter l'enceinte (nom : `ZACUS-SPK`)
- [ ] Dérouler le ruban LED USB (3m), brancher → Port USB de la batterie #2
  - Couleur : bleu/vert pour ambiance laboratoire
- [ ] Installer les 4 panneaux de signalétique :
  - Entrée : « Laboratoire du Professeur Zacus — Accès Restreint »
  - Règles du jeu (format A3, plastifié)
  - Zones puzzle : numérotées P1 à P7
  - Sortie de secours : toujours visible, non obstrué

<!-- PHOTO: [ambiance_setup.jpg] — salle vue de l'entrée avec éclairage ambiance -->

---

### Minutes 11–13 : Vérification du mesh ESP-NOW

- [ ] BOX-3 écran principal → attendre « MESH: 6/6 CONNECTED »
  - Si un nœud manquant après 90 secondes : appuyer RESET sur ce puzzle ESP32
  - Si toujours absent : vérifier LED — éteint = pas alimenté, rouge = erreur firmware
- [ ] Test fonctionnel rapide :
  - Appuyer sur un bouton de P1 → BOX-3 doit afficher l'événement reçu
  - Taper le clé P5 → BOX-3 doit afficher le code morse reçu

<!-- PHOTO: [box3_mesh_ok.jpg] — écran BOX-3 montrant "MESH: 6/6 CONNECTED" -->

---

### Minutes 14–15 : Configuration game master

- [ ] BOX-3 touchscreen → **Settings**
  - **Target Duration** → sélectionner [30 / 45 / 60 / 75 / 90 min] selon le groupe
  - **WiFi** → entrer le WiFi du lieu (pour XTTS live) **OU** → **Offline Mode** (MP3 SD)
  - **Volume NPC** → ajuster selon la taille de la salle (défaut : 70%)
- [ ] Appuyer **READY** → autotest 30 secondes → tous les puzzles en vert
- [ ] Briefing game master : mémoriser les 3 niveaux d'indices par puzzle

<!-- PHOTO: [box3_settings.jpg] — écran BOX-3 menu Settings ouvert -->
<!-- PHOTO: [box3_ready.jpg] — écran BOX-3 "ALL SYSTEMS GO" -->

**La session peut démarrer. Appuyer sur START GAME.**

---

## Démarrage du jeu

1. Faire entrer les joueurs dans la pièce (lumières éteintes ou tamisées)
2. La sonnerie du RTC_PHONE retentit automatiquement après 10 secondes
3. Les joueurs décrochent → Professeur Zacus parle → jeu commence

---

## Pendant le jeu — Rôle du game master

| Situation | Action |
|-----------|--------|
| Joueurs bloqués > 5 min | BOX-3 → Hint → sélectionner puzzle → niveau 1 |
| Puzzle ESP32 déconnecté | Appuyer RESET sur le puzzle, reconnecter sous 30s |
| Enceinte BT déconnectée | BOX-3 → Bluetooth → Reconnect |
| Panne réseau WiFi | BOX-3 bascule automatiquement en mode offline |
| Urgence (évacuation) | BOX-3 → PAUSE GAME → sortie sécurisée |

---

## Rangement (10 minutes)

- [ ] BOX-3 → **End Game** → **Export Stats** (PDF ou QR code)
- [ ] Partager le score/diplôme avec les joueurs (QR ou lien)
- [ ] Éteindre tous les puzzles (débrancher USB-C)
- [ ] **Compter 12 tuiles NFC** avant de les ranger (ne jamais en oublier)
- [ ] Récupérer les 6 QR codes A5
- [ ] Emballer dans l'ordre inverse (puzzles → hub → ambiance)
- [ ] Recharger toutes les batteries dès retour (ne pas stocker à plat)

---

## Dépannage rapide

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| Nœud ESP32 absent du mesh | Firmware crashé | RESET sur le puzzle |
| Voix NPC muette | WiFi hors portée | Basculer Offline Mode |
| Enceinte BT grésillements | Distance > 10m | Rapprocher l'enceinte |
| BOX-3 ne démarre pas | Batterie < 5% | Brancher USB-C direct (pas batterie) |
| QR code non scanné | Angle ou distance | Ajuster la caméra BOX-3, recoller le QR |
| Score non calculé | P7 non résolu | Vérifier que le coffre est bien verrouillé au départ |

---

## Contact support

- Email : clement@lelectronrare.fr
- Urgence session : +33 6 XX XX XX XX (disponible pendant sessions avec préavis)
- Documentation complète : docs/DEPLOYMENT_RUNBOOK.md
- Issues firmware : github.com/electron-rare/le-mystere-professeur-zacus/issues
