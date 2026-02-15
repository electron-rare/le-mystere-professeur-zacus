# Documentation Firmware - Index

Bienvenue dans la documentation du firmware multi-MCU du projet **Le Mystère du Professeur Zacus**.

## 📋 Table des matières

### 🚀 Getting Started

- **[README principal](../README.md)** - Vue d'ensemble du projet firmware
- **[Quickstart](QUICKSTART.md)** - Guide de démarrage rapide (build, flash, test)
- **[Agents Contract](../AGENTS.md)** - Règles de développement assisté par IA

### 🏗️ Architecture

- **[Architecture UML](ARCHITECTURE_UML.md)** - Diagrammes de classes, séquence, composants
  - Vue d'ensemble multi-MCU (ESP32 + ESP8266 + RP2040)
  - Couches Controllers / Services / Story Engine
  - Protocole UI Link v2
  - Subsystème audio dual-canal

### 📊 État des lieux

- **[State Analysis](STATE_ANALYSIS.md)** - Analyse complète de l'état actuel
  - Structure du projet
  - État des builds et tests
  - Métriques de santé
  - Problèmes identifiés
  - Port mapping hardware

### 🎯 Sprints & Roadmap

- **[Sprint Recommendations](SPRINT_RECOMMENDATIONS.md)** - Actions prioritaires
  - Sprint immédiat (commit cleanup, merge PR #86)
  - Court terme (tests unitaires, docs)
  - Moyen terme (config runtime, CI/CD)

### 📝 Protocoles

- **[Protocols Index](protocols/INDEX.md)** - Tous les protocoles du système
  - Story Engine V2 (format scénarios YAML)
  - UI Link v2 (UART frames)
  - Audio pipeline

### 🔧 Hardware & Testing

- **[HW NOW](HW_NOW.md)** - Status hardware rapide
- **[RC Final Board](RC_FINAL_BOARD.md)** - Tableau de bord tests RC
- **[RC Report Template](RC_FINAL_REPORT_TEMPLATE.md)** - Template rapports

---

## 🗂️ Organisation des documents

```
hardware/firmware/docs/
├── INDEX.md                        # ← Ce fichier (navigation)
├── ARCHITECTURE_UML.md             # Architecture complète (diagrammes)
├── STATE_ANALYSIS.md               # État des lieux détaillé
├── SPRINT_RECOMMENDATIONS.md       # Roadmap & actions
├── QUICKSTART.md                   # Getting started dev
├── HW_NOW.md                       # Hardware status
├── RC_FINAL_BOARD.md               # Tests RC dashboard
├── RC_FINAL_REPORT_TEMPLATE.md    # Template rapports
└── protocols/
    ├── INDEX.md                    # Index protocoles
    ├── PROTOCOL.md                 # Story Engine V2
    ├── GENERER_UN_SCENARIO_STORY_V2.md
    └── scenario.template.yaml
```

---

## 📚 Parcours de lecture recommandés

### Pour un nouveau développeur

1. **[README principal](../README.md)** - Comprendre le contexte projet
2. **[Quickstart](QUICKSTART.md)** - Setup environnement, premier build
3. **[Architecture UML](ARCHITECTURE_UML.md)** - Comprendre l'architecture
4. **[State Analysis](STATE_ANALYSIS.md)** - État actuel du firmware

**Durée estimée :** 1-2h

### Pour un contributeur stories

1. **[Architecture UML](ARCHITECTURE_UML.md)** - Section "Story Engine V2"
2. **[Protocols: Story Engine](protocols/PROTOCOL.md)** - Format YAML détaillé
3. **[Générer un scénario](protocols/GENERER_UN_SCENARIO_STORY_V2.md)**

**Durée estimée :** 30 min

### Pour un review de code

1. **[State Analysis](STATE_ANALYSIS.md)** - Section "État du code"
2. **[Architecture UML](ARCHITECTURE_UML.md)** - Diagrammes classes
3. **[Sprint Recommendations](SPRINT_RECOMMENDATIONS.md)** - Checklist code review

**Durée estimée :** 20 min

### Pour planifier un sprint

1. **[State Analysis](STATE_ANALYSIS.md)** - Métriques santé, problèmes
2. **[Sprint Recommendations](SPRINT_RECOMMENDATIONS.md)** - Actions prioritaires
3. **[RC Final Board](RC_FINAL_BOARD.md)** - Status tests hardware

**Durée estimée :** 15 min

---

## 🔍 Recherche rapide

### Je veux comprendre...

| Sujet | Document | Section |
|-------|----------|---------|
| L'architecture globale | [Architecture UML](ARCHITECTURE_UML.md) | Vue d'ensemble système |
| Les controllers | [Architecture UML](ARCHITECTURE_UML.md) | Couche Controllers |
| Les services | [Architecture UML](ARCHITECTURE_UML.md) | Couche Services |
| Le Story Engine | [Architecture UML](ARCHITECTURE_UML.md) | Story Engine V2 |
| L'audio dual-canal | [Architecture UML](ARCHITECTURE_UML.md) | Audio Subsystem |
| Le protocole UI Link | [Architecture UML](ARCHITECTURE_UML.md) | UI Link Protocol |
| Les UI ESP8266/RP2040 | [Architecture UML](ARCHITECTURE_UML.md) | Architecture UI |
| L'état actuel | [State Analysis](STATE_ANALYSIS.md) | Résumé exécutif |
| Les builds | [State Analysis](STATE_ANALYSIS.md) | État des builds |
| Les tests | [State Analysis](STATE_ANALYSIS.md) | État des tests |
| Le hardware | [State Analysis](STATE_ANALYSIS.md) | Port mapping & Hardware |
| Les prochaines actions | [Sprint Recommendations](SPRINT_RECOMMENDATIONS.md) | Sprint immédiat |
| Les scénarios YAML | [Protocols: Story](protocols/PROTOCOL.md) | Format scénarios |

### Je veux faire...

| Action | Document | Section |
|--------|----------|---------|
| Builder le firmware | [Quickstart](QUICKSTART.md) | Build gates |
| Flasher les devices | [Quickstart](QUICKSTART.md) | Flash procedure |
| Tester hardware | [State Analysis](STATE_ANALYSIS.md) | Smoke tests |
| Créer un scénario | [Générer scénario](protocols/GENERER_UN_SCENARIO_STORY_V2.md) | - |
| Ajouter un service | [Architecture UML](ARCHITECTURE_UML.md) | Évolutivité |
| Ajouter une UI | [Architecture UML](ARCHITECTURE_UML.md) | Évolutivité |
| Review un PR | [Sprint Recommendations](SPRINT_RECOMMENDATIONS.md) | Code review checklist |
| Planifier un sprint | [Sprint Recommendations](SPRINT_RECOMMENDATIONS.md) | Sprint court terme |

---

## 🔗 Liens externes utiles

### Tools & Frameworks

- **[PlatformIO](https://platformio.org/)** - Build system
- **[ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)** - MP3 decoder
- **[arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)** - I2S streaming
- **[LVGL](https://lvgl.io/)** - GUI framework RP2040

### Hardware

- **[ESP32 Audio Kit](https://docs.ai-thinker.com/en/esp32-audio-kit)** - Main board
- **[ES8388 Codec](https://www.esmt.com.tw/en/products/codec)** - Audio codec
- **[ESP8266 NodeMCU](https://www.nodemcu.com/)** - OLED UI board
- **[RP2040 Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/)** - TFT UI board

### Repo & Issues

- **[GitHub Repo](https://github.com/electron-rare/le-mystere-professeur-zacus)**
- **[PR #86 (hardware/firmware)](https://github.com/electron-rare/le-mystere-professeur-zacus/pull/86)**
- **[Issues](https://github.com/electron-rare/le-mystere-professeur-zacus/issues)**

---

## 📝 Conventions de documentation

### Statut des documents

| Badge | Signification |
|-------|---------------|
| ✅ STABLE | Document à jour, validé, prêt pour référence |
| 🔄 DRAFT | Document en cours de rédaction |
| ⚠️ OUTDATED | Document obsolète, nécessite mise à jour |
| 🗑️ DEPRECATED | Document remplacé par nouvelle version |

### Mise à jour

- **Fréquence** : Les docs doivent être mises à jour à chaque merge main
- **Responsable** : L'auteur du PR doit mettre à jour les docs concernées
- **Review** : Les docs font partie de la code review

### Contribution

Pour contribuer à la documentation :

1. Fork le repo
2. Créer une branche `docs/<sujet>`
3. Éditer les fichiers Markdown
4. Commit avec message clair : `docs(firmware): update <sujet>`
5. PR vers `main`

---

## 📞 Support

**Questions techniques :** Ouvrir une [issue GitHub](https://github.com/electron-rare/le-mystere-professeur-zacus/issues)  
**Discussions :** [GitHub Discussions](https://github.com/electron-rare/le-mystere-professeur-zacus/discussions)  
**Lead firmware :** @electron-rare

---

**Dernière mise à jour :** 15 février 2026  
**Version docs :** v1.0.0  
**Status :** ✅ STABLE
