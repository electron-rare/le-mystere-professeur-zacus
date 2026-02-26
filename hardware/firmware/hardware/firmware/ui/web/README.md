
---
# Zacus Firmware – Story V2 WebUI

---

## 📝 Description

Interface web pour piloter et designer les scénarios Story V2 (ESP32).

---

## 🚀 Installation & usage

```sh
npm install
npm run dev
```

- Accès local : http://localhost:5173/
- API cible : http://<ESP_IP>:8080

---

## 📦 Contenu du dossier

- `src/components/ScenarioSelector.tsx` : sélection et lancement de scénario
- `src/components/LiveOrchestrator.tsx` : suivi live, log, contrôles
- `src/components/StoryDesigner.tsx` : éditeur YAML, validate/deploy

---

## 🛠️ Qualité code

- ESLint + Prettier intégrés

---

## 📄 Spécifications

- API : voir `../../docs/protocols/STORY_V2_WEBUI.md`
- YAML : voir `../../docs/protocols/story_specs/schema/story_spec_v1.yaml`

---

## 🤝 Contribuer

Merci de lire [../../../../../../../../CONTRIBUTING.md](../../../../../../../../CONTRIBUTING.md) avant toute PR.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
---
