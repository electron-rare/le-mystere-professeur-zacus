---
# Zacus WebUI (Story V2)
// TODO NO DEV FINISH (need KILL_LIFE ?)

Frontend Mission Control pour les devices Zacus.

---

## 📝 Description

Interface web de pilotage, design et diagnostic pour les firmwares Zacus (Story V2 et legacy).

---

## 📦 Fonctionnalités principales

- Sélecteur de scénario avancé (recherche, tri, mode legacy, CTA adaptés)
- Orchestrateur live (statut runtime, recovery, filtres, contrôles réseau)
- Story Designer nodal (React Flow, import/export YAML, édition guidée, auto-layout)
- UI harmonisée (glass modern, labels FR, accessibilité)

---

## 🚀 Installation & démarrage rapide

```bash
npm install
npm run dev
```

Accès local : http://localhost:5173
Accès LAN : http://<ip-machine>:5173

Preset ESP :
```bash
npm run dev:esp
```

---

## 🛠️ Usage

Détection automatique du firmware connecté :
- `story_v2` : endpoints `/api/story/*` + WebSocket
- `freenove_legacy` : endpoints `/api/status`, `/api/scenario/*`, `/api/stream` (SSE)

Diagnostics firmware :
- Version, OTA, reboot détectés automatiquement
- État affiché dans le panneau "Firmware"

Variables d'environnement :
- `VITE_API_BASE` (ex: `http://192.168.0.91`)
- `VITE_API_PROBE_PORTS` (défaut: `80,8080`)
- `VITE_API_FLAVOR` (`auto|story_v2|freenove_legacy`, défaut `auto`)

---

## 🤝 Contribuer

Les contributions sont bienvenues !
Merci de lire [../../CONTRIBUTING.md](../../CONTRIBUTING.md) avant toute PR.

---

## 🧑‍🎓 Licence

- **Code** : MIT (`../../LICENSE`)

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)

- Preview local: `http://localhost:4173`
- Preview LAN: `http://<ip-machine>:4173`

Build + preview:

```bash
npm run preview:esp:build
```

## Tests / gates frontend

```bash
npm run lint
npm run build
npm run test:unit
npx playwright test
npx playwright test --grep @live
```

- `test:unit`: validation parser/generation YAML Story Designer.
- `playwright @mock`: detection Story V2/legacy, UX et regressions principales.
- `playwright @live`: tests live sur `192.168.0.91` (actions mutantes autorisees).

## Notes live

- La suite `@live` execute des actions de controle (`unlock`, `next`, `wifi reconnect`, `espnow off/on`).
- Un finalizer force `espnow on` en fin de run.
- A lancer sur une fenetre de test dediee (pas pendant une partie active).
