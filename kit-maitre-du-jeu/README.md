---
---


# 🎩 Kit Maître du Jeu — Le Mystère du Professeur Zacus (v2)

![Chapeau](https://media.giphy.com/media/3o6Zt8zb1Pp2v3A2l6/giphy.gif)

---




Ce dossier rassemble tout le nécessaire pour animer une session du scénario canon `game/scenarios/zacus_v2.yaml` : guides, conducteur, solution, anti-chaos, checklist, modules de stations, et objets spécifiques (piano-alphabet, portrait QR). Il s’appuie sur la structure YAML, les printables, l’audio et les guides MJ du dépôt.

### ⚡ Solution ESP32 Media Kit
Le scénario v2 est conçu pour fonctionner avec le Media Kit ESP32 : un module électronique qui automatise la gestion des étapes, des feedbacks audio/visuels, du QR WIN et du Media Hub final. Ce kit permet au MJ de se concentrer sur l’animation et la coopération, tout en garantissant la robustesse des validations (LA 440, piano LEFOU, QR WIN).

- Boot, transitions, indices audio, confirmations et finale sont gérés par le firmware (voir hardware/firmware/ et la section firmware du YAML canon).
- Le Media Hub (photo, audio, jingle) s’active automatiquement après la réussite.
- Un backup MJ est toujours possible (validation manuelle, QR de secours).

> *"Si tu arrives à la fin sans perdre une seule fiche, tu gagnes le badge Maître du Funk !"*

---



## 📦 Contenu du dossier

- `script-minute-par-minute.md` : conducteur détaillé, minute par minute, aligné sur les 2 actes (LA 440, Zone 4, QR WIN)
- `solution-complete.md` : solution complète du scénario canon (voir aussi `game/scenarios/zacus_v2.yaml`)
- `checklist-materiel.md` : checklist matériel, compatible printables/audio, objets spécifiques (piano-alphabet, portrait QR)
- `plan-stations-et-mise-en-place.md` : plan des stations, instructions de mise en place (voir objets et clues YAML)
- `distribution-des-roles.md` : répartition des rôles MJ/enfants (voir aussi `_generated/roles.md`)
- `guide-anti-chaos.md` : gestion de groupe, règles anti-chaos (voir aussi `_generated/anti-chaos.md`)
- `generer-un-scenario.md` : procédure pour créer/valider un scénario compatible repo
- `stations/` : stations modulaires, à compléter selon le scénario v2
- `_generated/` : versions synchronisées avec le YAML canon et les manifests
- `export/pdf/` : exports prêts à imprimer

---


## 🚀 Mode d’emploi (MJ, version canon v2)

1. Lis ce README et le script-minute-par-minute
2. Prépare le matériel avec la checklist (voir aussi `_generated/checklist.md`)
3. Place le scénario YAML canon (`game/scenarios/zacus_v2.yaml`) sur la table MJ
4. Distribue les rôles (voir `distribution-des-roles.md` et `_generated/roles.md`)
5. Mets en place les stations selon le plan (voir `plan-stations-et-mise-en-place.md` et `_generated/stations.md`)
6. Prépare les objets spécifiques : piano-alphabet (stickers A–Z), portrait QR WIN
7. Lance la session en suivant le conducteur, utilise les guides anti-chaos si besoin
8. (Option) Ajoute ou adapte des stations bonus pour varier les parties

> *"Si tu perds le contrôle, relis le guide anti-chaos. Si tu le perds aussi, improvise : c’est ça, l’esprit Zacus !"*

---



## 🤝 Contribuer

Les contributions sont bienvenues ! Merci de lire [../CONTRIBUTING.md](../CONTRIBUTING.md) avant toute PR. Priorité aux ajouts cohérents avec le YAML canon (`game/scenarios/zacus_v2.yaml`), les manifests audio/printables et la structure du repo.

---


## 🧑‍🎓 Licence

- **Contenu créatif** : CC BY-NC 4.0 ([../LICENSE-CONTENT.md](../LICENSE-CONTENT.md))

---

## 👤 Contact

Pour toute question, suggestion, ou anecdote de MJ, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)

> *"Ce kit a été testé sur MJ stressé, enfants surexcités, et robots en panne d’inspiration. Résultat : 100% de fun garanti."*
---
