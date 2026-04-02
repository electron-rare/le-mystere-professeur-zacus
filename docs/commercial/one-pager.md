# One-Pager : Le Mystère du Professeur Zacus

<!-- To print: open in browser, use print-to-PDF, A4 recto-verso -->

---

## RECTO

**[LOGO] L'Electron Rare**
*Solutions technologiques créatives*

---

# Le Mystère du Professeur Zacus

### *L'escape room portable, adaptative, et alimentée par l'IA*

---

**7 énigmes physiques** — Séquence sonore, circuit LED, fréquence radio,
code morse, symboles alchimiques, QR Treasure, coffre final verrouillé.

**IA adaptative** — Le Professeur profile votre groupe (tech / non-tech) en 10 minutes
et ajuste la difficulté en temps réel. Chaque groupe vit une expérience calibrée.

**Voix clonée** — Professeur Zacus parle en direct via IA vocale (XTTS-v2 sur GPU)
avec fallback Piper TTS — toujours disponible, même sans connexion internet.

**3 valises** — Déployable en 15 minutes, partout, sans technicien. Offline-first.

---

| | **Animation** | **Location** | **Kit** |
|---|---|---|---|
| **Prix** | 800 – 1 500 €/séance | 300 – 500 €/week-end | 3 000 – 5 000 € |
| **Joueurs** | 4 – 15 | 4 – 15 | 4 – 15 |
| **Durée** | 2 – 3h sur place | Vendredi → Lundi | Achat définitif |
| **Inclus** | Animateur + débrief | Guide + support tél. | Firmware + 3 scénarios + 1h formation |
| **Idéal pour** | Teambuilding, événements | Associations, anniversaires | Escape rooms, musées |
| **CTA** | Demander un devis | Réserver | Acheter |

---

> *"Le Professeur nous a bluffés. On ne savait pas que c'était une IA."*
> — Groupe bêta, session de test 2026

<!-- Add second testimonial after beta playtest Group 2 -->
<!-- > *"Parfaitement adapté à notre groupe. Ni trop facile, ni trop difficile."* -->
<!-- > — Groupe non-tech, bêta 2026 -->

---

**Contact :** clement@lelectronrare.fr | lelectronrare.fr/zacus | +33 6 XX XX XX XX

---

## VERSO — Fiche technique (Kit buyers)

### Architecture technique

| Composant | Détail |
|-----------|--------|
| Hub | BOX-3 (ESP32-S3) orchestrateur + RTC_PHONE custom (voix) |
| Communication | ESP-NOW mesh offline-first, 7 nœuds |
| TTS | XTTS-v2 (GPU) → Piper TTS → MP3 SD card (3 niveaux de fallback) |
| Audio ambiance | 6 pistes pré-générées AudioCraft MusicGen (RTX 4090) |
| Firmware | ESP-IDF, C, FreeRTOS, PlatformIO — open source MIT |
| Scénarios | YAML → Runtime 3 IR — compilateur Python inclus |

### BOM indicatif

| Catégorie | Coût estimé |
|-----------|-------------|
| Électronique (ESP32 × 9, capteurs, relais) | ~280 € |
| Boîtiers 3D imprimés ABS + découpe laser | ~120 € |
| 3 valises Peli ou équivalent | ~180 € |
| Câbles, batteries, accessoires | ~62 € |
| **Total BOM** | **~642 €** |
| **Marge Kit Standard (3 500 €)** | **~83 %** |

### Robustesse & support

- Conçu pour **>100 déploiements** (boîtiers industriels, contacts dorés)
- Composants standards : Reed switches, NTAG213, SG90, ESP32-S3
- Mises à jour firmware OTA via WiFi ou USB-C — **1 an inclus** dans le kit
- Documentation complète + guide de déploiement 15 minutes
- Support premium disponible : 500 €/an (téléphone + 2 scénarios supplémentaires)

### Scénarios disponibles

| Scénario | Thème | Durée cible |
|----------|-------|-------------|
| zacus_v3_complete | Laboratoire du Professeur (7 énigmes) | 30 – 90 min |
| *(en développement)* | Bibliothèque mystérieuse | Q3 2026 |
| *(en développement)* | Vaisseau spatial | Q4 2026 |

---

**Contact commercial :** clement@lelectronrare.fr
**Site :** lelectronrare.fr/zacus
**GitHub :** github.com/electron-rare/le-mystere-professeur-zacus
