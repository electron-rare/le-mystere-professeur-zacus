# Gestion de l'Alimentation — Kit V3

> Dernière mise à jour : 2026-04-03
> Concerne : 3 valises, 7 puzzles ESP32, RTC_PHONE, BOX-3, routeur GL.iNet

---

## Assignation des Batteries

| Batterie | Capacité | Appareils alimentés | Puissance totale | Autonomie estimée |
|----------|----------|---------------------|-----------------|-------------------|
| **Anker #1** (valise Hub) | 20 000 mAh / 5V | RTC_PHONE (0,5 W) + BOX-3 (2 W) + Routeur GL.iNet (3 W) | 5,5 W | ~3,5 h |
| **Anker #2** (valise Hub, réserve) | 20 000 mAh / 5V | Chargeur USB multi-port puzzles (en session) ou secours Hub | — | — |
| **Pack Puzzle A** | 10 000 mAh / 5V | P1 Boîte Sonore (1 W) + P2 Breadboard (2 W) + P4 Radio (interne 18650) | 3 W | ~8 h |
| **Pack Puzzle B** | 10 000 mAh / 5V | P5 Télégraphe (0,5 W) + P6 Tablette Symboles (0,5 W) + P7 Coffre Final (1 W) | 2 W | ~15 h |

> **Note :** P3 QR Treasure utilise la caméra intégrée de BOX-3 (déjà compté dans Anker #1).
> P4 intègre un 18650 rechargeable en interne — chargé via USB-C indépendamment.

---

## Autonomie par Appareil

| Appareil | Puissance moy. | Batterie | Autonomie calculée | Marge pour 2 sessions |
|----------|---------------|----------|--------------------|-----------------------|
| RTC_PHONE | 0,5 W | Anker #1 | >10 h | Oui |
| BOX-3 (ESP32-S3-BOX-3) | 2 W | Anker #1 | ~5 h | Non (recharge entre sessions) |
| Routeur GL.iNet GL-MT3000 | 3 W | Anker #1 | ~3,5 h | Non (1 session) |
| P1 Boîte Sonore | 1 W | Pack A | ~12 h | Oui |
| P2 Breadboard Magnétique | 2 W (pics) | Pack A | ~6 h | Oui |
| P4 Poste Radio | 18650 interne | — | ~4 h | Vérifier avant chaque session |
| P5 Télégraphe | 0,5 W | Pack B | >20 h | Oui |
| P6 Tablette Symboles | 0,5 W | Pack B | >20 h | Oui |
| P7 Coffre Final | 1 W (+ servo) | Pack B | ~10 h | Oui |

---

## Procédure de Recharge entre Sessions

### Séquence optimale (chargeur GaN 65 W, 4 ports)

```
Port 1 (PD 45W) → Anker #1 (20 000 mAh) — Temps : ~2h00
Port 2 (PD 18W) → Anker #2 (20 000 mAh) — Temps : ~3h30
Port 3 (PD 18W) → Pack Puzzle A (10 000 mAh) — Temps : ~1h30
Port 4 (5W)     → P4 Radio via USB-C interne — Temps : ~1h30
```

> Commencer immédiatement après la session pour avoir tout chargé avant la session suivante.

### Règles de gestion

1. **Ne jamais stocker une batterie déchargée** — endommageur des cellules Li-Ion.
2. **Recharger à 100% avant chaque déploiement**, pas seulement à "suffisant".
3. **Vérifier le niveau avant d'emballer** — toutes les LEDs de niveau doivent montrer 4/4.
4. **Pack B peut alimenter 2 sessions consécutives** sans recharge intermédiaire (autonomie 15 h+).
5. **Anker #1 nécessite une recharge entre sessions** si deux sessions le même jour.

### Matériel de recharge (à emporter dans la valise 1)

- [ ] Chargeur GaN 65 W, 4 ports USB-C (Anker 737 ou équivalent)
- [ ] 5× câbles USB-C courts (0,5 m) pour la recharge simultanée
- [ ] Étiquettes indiquant le port assigné à chaque batterie

---

## Alimentation d'Urgence (batterie morte en cours de partie)

### Procédure selon l'appareil défaillant

#### Hub mort (Anker #1 vide) — impact critique

**Symptômes :** BOX-3 éteint, RTC_PHONE silencieux, routeur éteint.

**Action immédiate :**
1. Brancher Anker #2 (réserve) sur le splitter Hub.
2. Temps de redémarrage : ~30 secondes.
3. Annoncer aux joueurs (en voix de maître du jeu) : *"Le Professeur a eu un petit problème technique…"*
4. Sur BOX-3 : `Settings → Restore Session` — reprend là où le jeu s'est arrêté.

#### Puzzle mort (Pack A ou B vide) — impact partiel

**Symptômes :** Un ou plusieurs puzzles ESP32 ne répondent plus sur le mesh.

**Action :**
1. Connecter ce puzzle sur le chargeur GaN (Port 3 ou 4) — le puzzle redémarre en quelques secondes.
2. Sur BOX-3 : le nœud manquant réapparaît dans `MESH: X/6 nodes`.
3. Si un nœud ne revient pas après 2 min : `Reset Hardware` sur BOX-3 → envoie une commande de reset ESP-NOW à tous les nœuds.

#### P4 Radio (18650 interne) mort

**Action :** Brancher USB-C sur n'importe quel port disponible pendant le jeu — charge et fonctionne simultanément.

#### Cas extrême : panne totale irréparable

1. Passer en mode "maître du jeu manuel" : lire les énigmes depuis le kit papier (valise 3).
2. Simuler les réponses du Professeur avec le kit audio de secours (téléphone + playlist MP3).
3. Utiliser le code de secours maître (imprimé sur la fiche lamifiée valise 1) pour ouvrir P7 manuellement.

---

## Mode Hors-Ligne (sans WiFi/internet)

Tous les puzzles fonctionnent **entièrement sans WiFi** via le mesh ESP-NOW local :

| Fonction | Online (WiFi dispo) | Offline (ESP-NOW seul) |
|----------|---------------------|------------------------|
| NPC voix (XTTS-v2) | Live depuis KXKM-AI | Fallback MP3 SD card |
| NPC voix (Piper TTS) | Tower:8001 | Fallback MP3 SD card |
| Détection NFC P6 | Locale (pas de réseau) | Identique |
| QR scanner P3 | Locale (BOX-3 camera) | Identique |
| Analytics | Cache local, upload post-jeu | Cache local |
| Scoring + diplôme PDF | En mémoire BOX-3 | En mémoire BOX-3 |

> **Recommandation :** générer le pool MP3 XTTS avant chaque déploiement si de nouvelles phrases ont été ajoutées (`python3 tools/tts/generate_xtts_pool.py`).
