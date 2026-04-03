# Guide de Maintenance — Kit V3

> Dernière mise à jour : 2026-04-03
> Fréquence : après chaque session (vérifications rapides) + annuelle (maintenance complète)

---

## Dépannage des Problèmes Courants

### Mesh ESP-NOW — Nœuds manquants

**Symptôme :** BOX-3 affiche `MESH: X/6 nodes connected` avec X < 6.

| Cause probable | Diagnostic | Solution |
|----------------|-----------|---------|
| ESP32 non alimenté | LED d'alimentation éteinte sur le puzzle | Vérifier câble USB-C + batterie |
| ESP32 bloqué au boot | LED clignote rapidement sans s'arrêter | Appuyer bouton Reset (EN) sur le DevKit |
| Hors de portée ESP-NOW (~200 m max) | Puzzle à plus de 20 m de BOX-3 | Rapprocher le puzzle ou repositionner |
| Firmware corrompu | Log série : `Guru Meditation Error` | Re-flasher le firmware (voir section Update) |
| Conflit MAC address | Log série : `ESPNOW: peer already exists` | Effacer NVS et redémarrer : `esptool.py erase_flash` |

**Commande de diagnostic rapide :**
```bash
# Afficher l'état du mesh depuis BOX-3 via serial
pio device monitor -e box3 --baud 115200
# Puis dans le menu tactile BOX-3 : Debug → ESP-NOW Status
```

---

### P6 Tablette Symboles NFC — Tuile non détectée

**Symptôme :** Une tuile NFC posée sur son emplacement n'est pas reconnue.

| Cause | Diagnostic | Solution |
|-------|-----------|---------|
| Autocollant NFC endommagé | Tester la tuile avec un smartphone NFC | Remplacer l'autocollant NTAG213 (stock de rechange) |
| Distance trop grande | Positionner la tuile directement sur l'encoche | Vérifier que le PN532 est bien collé sous la surface |
| PN532 déconnecté | Log série : `PN532: INIT FAILED` | Vérifier câbles I2C SDA/SCL, re-souder si nécessaire |
| UID non enregistré en NVS | Log série : `UNKNOWN UID: XXXXXXXX` | Re-programmer le NVS avec les UIDs corrects |

**Programmer un autocollant NTAG213 de remplacement :**
```bash
# Avec un smartphone Android + app NFC Tools
# OU avec le reader PN532 + script Python :
python3 tools/dev/nfc_program.py --uid-slot 7 --new-uid 04A1B2C3D4E500
```

---

### P5 Télégraphe — Morse non détecté

**Symptôme :** Appuyer sur le télégraphe ne produit aucun effet.

| Cause | Solution |
|-------|---------|
| Contact oxydé (laiton) | Nettoyer les contacts avec papier abrasif 400 grain |
| Ressort de rappel trop faible | Remplacer le ressort (diamètre 5 mm, force 0,3 N) |
| Seuil point/tiret mal calibré | Ajuster `MORSE_DOT_MAX_MS` dans `firmware/p5_morse/config.h` |
| GPIO en pull-up cassé | Mesurer tension GPIO au repos (doit être 3,3 V) |

---

### P7 Coffre — Servo bloqué ou ne tourne pas

**Symptôme :** Code correct entré mais le servo ne bouge pas (coffre reste fermé).

| Cause | Solution |
|-------|---------|
| Servo desserré mécaniquement | Vérifier vis de fixation bras de servo, re-coller si nécessaire |
| Surcharge mécanique | Vérifier que le verrou n'est pas physiquement bloqué (dégager manuellement) |
| Signal PWM incorrect | Mesurer GPIO 23 avec oscilloscope : 50 Hz, pulse 1-2 ms |
| Servo défaillant | Remplacer le SG90 (prévoir 2 de rechange dans la valise) |

**Ouverture d'urgence (sans électronique) :**
Utiliser la clé Allen M2 fournie dans la poche valise 3 pour retirer le bras de servo manuellement.

---

### BOX-3 — Écran noir au démarrage

**Diagnostic :**
```bash
# Vérifier le firmware via serial
idf.py -p /dev/cu.usbserial-XXXX monitor
# Log attendu : [BOOT] BOX3 v3.1 — Display init...
```

| Cause | Solution |
|-------|---------|
| Firmware absent | Re-flasher : `idf.py -p /dev/cu.usbserial-XXXX flash` |
| Batterie trop faible | Connecter au chargeur 5V/2A minimum avant démarrage |
| Écran LCD déconnecté | Ouvrir le BOX-3 (4 vis Torx T8), vérifier nappe LCD |

---

### Voix NPC silencieuse

**Diagnostic par ordre de priorité :**

1. **Vérifier la connectivité WiFi** — BOX-3 : `Settings → WiFi Status`
2. **Tester Piper TTS directement :**
   ```bash
   curl -X POST http://192.168.0.120:8001/api/tts \
     -H "Content-Type: application/json" \
     -d '{"text": "Test professeur Zacus", "voice": "tom-medium"}' \
     --output /tmp/test.wav && aplay /tmp/test.wav
   ```
3. **Mode offline** — Vérifier que la SD card est présente dans BOX-3 et contient le pool MP3 :
   ```bash
   ls /sdcard/hotline_tts/ | wc -l  # attendu : 50+ fichiers
   ```
4. **Régénérer le pool MP3 si absent :**
   ```bash
   python3 tools/tts/generate_npc_pool.py --dry-run  # vérifier sans générer
   python3 tools/tts/generate_npc_pool.py             # générer
   ```

---

## Procédure de Mise à Jour Firmware

### Option 1 : Mise à jour OTA (Over-The-Air) — recommandée

```bash
# Prérequis : BOX-3 et tous les puzzles sur le même réseau WiFi

# Build le firmware mis à jour
cd ESP32_ZACUS
pio run -e p1_son

# Upload OTA (l'ESP32 doit être en mode OTA_READY)
# Depuis BOX-3 : Maintenance → OTA Update → Enable OTA on all nodes
pio run -e p1_son --target upload --upload-protocol espota \
  --upload-port 192.168.1.XXX  # IP de P1 affichée sur BOX-3
```

### Option 2 : Mise à jour USB-C (physique)

```bash
# Connecter le puzzle en USB-C au poste de développement
pio run -e p1_son --target upload --upload-port /dev/cu.usbserial-XXXX

# BOX-3 (ESP32-S3)
idf.py -p /dev/cu.usbserial-XXXX flash
```

### Vérification post-mise à jour

```bash
# Vérifier la version firmware
pio device monitor -e p1_son --baud 115200
# Attendu : [BOOT] P1_SON v3.X — ...
```

---

## Guide de Remplacement des Composants

### Composants consommables (à stocker en réserve)

| Composant | Quantité recommandée | Fournisseur | Prix unitaire |
|-----------|---------------------|-------------|--------------|
| NTAG213 autocollant NFC | 5 | AliExpress / Reichelt | ~0,30 € |
| Servo SG90 | 2 | AliExpress | ~2 € |
| ESP32 DevKit v1 | 1 | AliExpress / Mouser | ~5 € |
| 18650 Li-Ion (P4) | 1 | NKON / Reichelt | ~4 € |
| Bouton arcade 30 mm | 4 (jeu complet P1) | Aliexpress | ~1 € / pièce |
| Câble USB-C 1 m | 3 | n'importe | ~3 € |
| Ressort télégraphe (P5) | 3 | Bricomarché / quincaillerie | ~0,50 € |

### Remplacement d'un ESP32 DevKit

1. Déconnecter l'alimentation et tous les câbles.
2. Dessouder ou déconnecter le connecteur JST si présent.
3. Flasher le nouveau DevKit **avant** de l'installer (voir Étape 3 de l'assemblage).
4. Re-programmer le NVS avec les UIDs et credentials du puzzle remplacé.
5. Reconnecter et tester le mesh ESP-NOW.

### Remplacement d'une tuile NFC (P6)

1. Retirer l'autocollant NTAG213 de la tuile symbole (avec lame de rasoir, chauffer légèrement).
2. Programmer le nouvel autocollant avec le même UID (script `tools/dev/nfc_program.py`).
3. Coller le nouvel autocollant et vérifier la lecture (log : `NFC READ: UID match slot X`).

---

## Calendrier de Maintenance Annuelle

### Avant la saison (1× par an, avant les premières sessions)

| Tâche | Durée | Priorité |
|-------|-------|---------|
| Test complet de tous les puzzles (protocole étape 5) | 30 min | P0 |
| Mise à jour firmware tous les ESP32 et BOX-3 | 45 min | P0 |
| Vérification et remplacement des batteries dégradées | 15 min | P0 |
| Nettoyage contacts électroniques (spray contact) | 15 min | P1 |
| Vérification intégrité boîtiers (fissures, vis desserrées) | 10 min | P1 |
| Régénération pool MP3 NPC (nouvelles phrases si besoin) | 20 min | P1 |
| Vérification QR codes A5 (décoloration, plastification intacte) | 5 min | P2 |
| Test servo P7 (1000 cycles ouvrir/fermer) | 10 min | P2 |
| Mise à jour documentation si modifications hardware | — | P2 |

### Après 50 sessions (compteur dans BOX-3)

- Remplacer les 4 boutons arcade de P1 (usure mécanique prévisible).
- Remplacer le servo SG90 de P7 (durée de vie : ~3000-5000 cycles).
- Vérifier les contacts du télégraphe P5 (re-polir si nécessaire).
- Inspecter le câblage interne de P6 (vibrations de transport).

### Log de maintenance

Tenir à jour le fichier `hardware/maintenance_log.csv` avec :
```
date,session_count,technician,actions_performed,components_replaced,notes
```
