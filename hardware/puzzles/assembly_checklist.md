# Guide d'Assemblage et Checklist Qualité — Kit V3

> Dernière mise à jour : 2026-04-03
> Temps estimé (premier assemblage) : 8-12 heures
> Temps estimé (assemblage répété, kit connu) : 3-4 heures

---

## Prérequis

### Outillage
- Fer à souder 50 W + étain 0,8 mm
- Multimètre (continuité + tension)
- Pistolet à colle thermique
- Tournevis Torx T8 + cruciforme PH1
- Pince coupante, pince à dénuder
- Imprimante 3D (PLA/ABS pour boîtiers)
- Découpeuse laser (optionnel — tablette P6 peut être achetée)

### Logiciels requis sur le poste d'assemblage
- PlatformIO CLI (`pip install platformio`)
- Python 3.11+
- `esptool.py` (`pip install esptool`)
- `idf.py` (ESP-IDF v5.x pour ESP32-S3-BOX-3)

---

## Étape 1 : Impression 3D des Boîtiers (2-4 h)

| Boîtier | Fichier STL | Matière | Paramètres |
|---------|------------|---------|-----------|
| P1 Boîte Sonore | `hardware/enclosure/p1_son/p1_son_box.stl` | PLA noir | 0,2 mm, 20% infill |
| P2 Support Breadboard | `hardware/enclosure/p2_circuit/p2_frame.stl` | PLA gris | 0,2 mm, 15% infill |
| P4 Poste Radio | `hardware/enclosure/p4_radio/p4_radio_retro.stl` | PLA marron | 0,2 mm, 30% infill |
| P5 Télégraphe base | `hardware/enclosure/p5_morse/p5_base.stl` | PLA bois (PHA) | 0,15 mm, 25% infill |
| P7 Coffre | `hardware/enclosure/p7_coffre/p7_coffre_body.stl` | ABS noir | 0,2 mm, 40% infill |

- [ ] Tous les boîtiers imprimés sans warping
- [ ] Trous de fixation propres (percer si nécessaire : M3 pour PCB, M2 pour servo)
- [ ] Post-traitement : ponçage 120→240 grains si besoin esthétique

---

## Étape 2 : Assemblage Électronique par Puzzle

### P1 — Boîte Sonore (Séquence musicale)

**Composants :**
- ESP32-S3 DevKit × 1
- Haut-parleur 8 Ω 1 W × 1
- MAX98357A (I2S amplificateur) × 1
- Boutons arcade 30 mm (rouge, bleu, jaune, vert) × 4
- LED 5 mm (couleurs correspondantes) × 4
- Résistances 220 Ω × 4

**Câblage :**
```
ESP32-S3 GPIO4  → BCLK (MAX98357A)
ESP32-S3 GPIO5  → LRCLK
ESP32-S3 GPIO6  → DIN
GPIO 12/13/14/15 → Boutons (INPUT_PULLUP)
GPIO 16/17/18/19 → LEDs via 220 Ω
```

- [ ] Souder MAX98357A sur mini-PCB stripboard
- [ ] Tester amplificateur seul (multimètre sur sortie : ~5V idle)
- [ ] Coller haut-parleur avec colle thermique
- [ ] Insérer boutons arcade dans les 4 trous du boîtier
- [ ] Câbler selon schéma `hardware/wiring/wiring.md`
- [ ] Test continuité toutes les connexions

### P2 — Breadboard Magnétique (Circuit LED)

**Composants :**
- ESP32 DevKit v1 × 1
- Reed switches × 8 (détection positionnement composants)
- Magnets néodyme Ø5mm × 8 (intégrés aux composants pédagogiques)
- LED 5 mm rouge × 1
- Panneau support MDF 60×40 cm (découpé laser)

**Câblage :**
```
GPIO 12-19 → Reed switches (INPUT_PULLUP)
GPIO 21    → LED témoin résolution
```

- [ ] Coller reed switches sous le panneau MDF aux positions définies
- [ ] Coller magnets dans les embases composants pédagogiques
- [ ] Câbler ESP32 et tester chaque reed switch individuellement
- [ ] Vérifier que la combinaison correcte allume LED GPIO21

### P4 — Poste Radio (Fréquence)

**Composants :**
- ESP32 DevKit v1 × 1
- Encodeur rotatif KY-040 × 1
- Écran OLED 128×64 SSD1306 I2C × 1
- 18650 Li-Ion + support + circuit de charge TP4056 × 1

**Câblage :**
```
Encodeur CLK → GPIO 32
Encodeur DT  → GPIO 33
Encodeur SW  → GPIO 25
SSD1306 SDA  → GPIO 21
SSD1306 SCL  → GPIO 22
TP4056 OUT   → VIN ESP32
```

- [ ] Souder support 18650 + TP4056 sur veroboard
- [ ] Monter encodeur sur face avant boîtier rétro
- [ ] Coller OLED sur fenêtre boîtier (cadre époxy)
- [ ] Charger 18650 à 100% via USB-C avant test

### P5 — Télégraphe Morse

**Composants :**
- ESP32 DevKit v1 × 1
- Bouton télégraphe bois + laiton (artisanal ou impression 3D) × 1
- Buzzer actif 5V × 1
- LED ambre 5 mm × 1 (signal visuel mode non-tech)

**Câblage :**
```
Bouton télégraphe → GPIO 12 (INPUT_PULLUP, ACTIVE LOW)
Buzzer            → GPIO 13 (via transistor NPN 2N2222)
LED ambre         → GPIO 14 (via 220 Ω)
```

- [ ] Assembler mécanisme télégraphe (ressort de rappel, laiton contact)
- [ ] Tester durée de vie contact (appuyer 100× : doit être fiable)
- [ ] Régler seuil point/tiret dans firmware (défaut : 150 ms)

### P6 — Tablette Symboles Alchimiques (NFC)

**Composants :**
- ESP32 DevKit v1 × 1
- Module NFC PN532 I2C × 1
- NTAG213 (autocollant NFC) × 12
- Tablette bois découpée laser 30×20 cm × 1
- Emplacements gravés laser × 12

**Câblage :**
```
PN532 SDA → GPIO 21
PN532 SCL → GPIO 22
PN532 IRQ → GPIO 19 (interruption lecture NFC)
```

- [ ] Programmer les 12 UIDs NTAG213 (voir section NVS ci-dessous)
- [ ] Coller symboles alchimiques sur les tuiles NTAG213
- [ ] Tester lecture NFC de chaque tuile (distance max : 3 cm)
- [ ] Graver les 12 emplacements sur la tablette (laser ou Dremel)

### P7 — Coffre Final (Verrou électronique)

**Composants :**
- ESP32 DevKit v1 × 1
- Clavier matriciel 4×4 × 1
- Servo SG90 × 1 (verrou mécanique)
- OLED 128×32 SSD1306 × 1
- Buzzer passif × 1
- Coffre en bois 25×20×15 cm × 1

**Câblage :**
```
Clavier lignes  → GPIO 12/13/14/15
Clavier colonnes→ GPIO 16/17/18/19
Servo signal    → GPIO 23 (PWM)
OLED SDA        → GPIO 21
OLED SCL        → GPIO 22
Buzzer          → GPIO 25
```

- [ ] Tester servo (0°=fermé, 90°=ouvert)
- [ ] Ajuster mécanisme de verrou (doit tenir sans alimentation — ressort)
- [ ] Tester clavier matriciel (toutes les 16 touches)

---

## Étape 3 : Flash Firmware par Puzzle (PlatformIO)

### Prérequis

```bash
# Cloner le repo et les submodules
git clone --recurse-submodules https://github.com/electron-rare/le-mystere-professeur-zacus.git
cd le-mystere-professeur-zacus/ESP32_ZACUS

# Vérifier que PlatformIO reconnaît les environnements
pio project config
```

### Flash par environnement

```bash
# P1 — Boîte Sonore
pio run -e p1_son --target upload --upload-port /dev/ttyUSB0

# P2 — Breadboard Magnétique
pio run -e p2_circuit --target upload --upload-port /dev/ttyUSB0

# P4 — Poste Radio
pio run -e p4_radio --target upload --upload-port /dev/ttyUSB0

# P5 — Télégraphe Morse
pio run -e p5_morse --target upload --upload-port /dev/ttyUSB0

# P6 — Tablette Symboles
pio run -e p6_symboles --target upload --upload-port /dev/ttyUSB0

# P7 — Coffre Final
pio run -e p7_coffre --target upload --upload-port /dev/ttyUSB0

# BOX-3 (ESP32-S3 — utiliser idf.py)
cd ui_freenove_allinone
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Note :** sur macOS, remplacer `/dev/ttyUSB0` par `/dev/cu.usbserial-XXXX` (vérifier avec `ls /dev/cu.*`).

### Vérification post-flash

```bash
# Monitor série pour vérifier le boot
pio device monitor -e p1_son --baud 115200

# Attendu dans les logs :
# [BOOT] P1_SON v3.1 — ESP-NOW MAC: AA:BB:CC:DD:EE:FF
# [ESPNOW] Waiting for BOX-3 master...
```

- [ ] P1 : boot OK, MAC affiché, LED clignotante (attente mesh)
- [ ] P2 : boot OK, reed switches testés (log: `REED[0..7]: 00000000`)
- [ ] P4 : boot OK, OLED affiche `FREQ: 800 Hz`, encodeur réactif
- [ ] P5 : boot OK, mode détecté (`TECH` ou `NON_TECH` selon profil)
- [ ] P6 : boot OK, NFC prêt (`PN532: READY, 12 UIDs loaded`)
- [ ] P7 : boot OK, OLED `LOCKED`, clavier actif, servo en position 0°

---

## Étape 4 : Configuration NVS (Non-Volatile Storage)

Le NVS stocke les paramètres persistants de chaque puzzle : UIDs NFC, credentials WiFi, paramètres maître du jeu.

### Configuration NFC (P6)

```bash
# Programmer les UIDs des 12 tuiles NTAG213
# Créer le fichier nvs_p6.csv :
cat > /tmp/nvs_p6.csv << 'EOF'
key,type,encoding,value
nfc_uid_01,data,hex,04A1B2C3D4E500
nfc_uid_02,data,hex,04B2C3D4E5F600
# ... (12 UIDs au total)
nfc_correct_order,data,string,"7,2,11,4,9,1,8,3,12,6,10,5"
EOF

# Flash NVS partition
python3 $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  generate /tmp/nvs_p6.csv /tmp/nvs_p6.bin 0x6000

esptool.py -p /dev/ttyUSB0 write_flash 0x9000 /tmp/nvs_p6.bin
```

### Configuration WiFi (tous les ESP32)

```bash
# Méthode simple : via serial monitor au premier démarrage
# Sur le terminal PlatformIO monitor, envoyer :
WIFI_SSID=ZacusVenue2026
WIFI_PASS=motdepasse_lieu

# Ou via nvs_partition_gen pour le déploiement en masse :
cat > /tmp/nvs_wifi.csv << 'EOF'
key,type,encoding,value
wifi_ssid,data,string,ZacusVenue2026
wifi_pass,data,string,motdepasse_lieu
EOF
```

### Paramètres Maître du Jeu (BOX-3)

Sur l'interface tactile BOX-3 au premier démarrage :
- `Settings → Game Master PIN` : définir un PIN à 4 chiffres (accès réglages en jeu)
- `Settings → Default Duration` : 60 min (modifiable à chaque session)
- `Settings → XTTS Host` : `kxkm-ai:5002` (ou laisser vide pour Piper/SD fallback)
- `Settings → Piper Host` : `192.168.0.120:8001`

---

## Étape 5 : Checklist Qualité Avant Première Utilisation

### Test fonctionnel complet (durée : ~30 min)

- [ ] **Allumer tout le kit** — toutes batteries connectées, attendre 60 secondes
- [ ] **Mesh ESP-NOW** — BOX-3 affiche `MESH: 6/6 nodes connected`
- [ ] **P1 Séquence Sonore** — Mélodie jouée, appuyer séquence correcte → signal OK envoyé au BOX-3
- [ ] **P2 Circuit** — Placer composants en position correcte → LED GPIO21 allumée → BOX-3 reçoit `P2_SOLVED`
- [ ] **P3 QR Treasure** — Scanner les 6 QR dans l'ordre → BOX-3 valide `P3_SOLVED`
- [ ] **P4 Radio** — Tourner à 1337 Hz → BOX-3 reçoit `P4_SOLVED`
- [ ] **P5 Morse** — Taper ZACUS en morse → BOX-3 reçoit `P5_SOLVED`
- [ ] **P6 Symboles** — Placer 12 tuiles dans l'ordre correct → BOX-3 reçoit `P6_SOLVED`
- [ ] **P7 Coffre** — Entrer le code 8 chiffres assemblé → servo tourne à 90° → coffre ouvert
- [ ] **NPC Audio** — Le Professeur Zacus parle à chaque étape clé (tester avec `./tools/dev/zacus.sh voice-bridge test`)
- [ ] **Scoring** — Fin de partie → BOX-3 affiche score et diplôme PDF généré

### Checklist esthétique et robustesse

- [ ] Aucun câble visible ou dangereux (tout cheminé + fermé)
- [ ] Boîtiers vissés ou clipsés — aucune pièce qui s'ouvre involontairement
- [ ] Toutes les LEDs et écrans visibles depuis la position joueur normale
- [ ] Texte imprimé lisible (taille min 14 pt à distance de bras)
- [ ] Logo et habillage thématique ("Laboratoire du Pr. Zacus") appliqués
- [ ] Kit de secours validé : code maître P7 fonctionne, fiche lamifiée présente
