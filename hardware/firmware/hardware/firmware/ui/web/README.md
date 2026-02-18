
# Story V2 WebUI

Interface web pour piloter et designer les scénarios Story V2 (ESP32).

## Démarrage rapide

```sh
npm install
npm run dev
```

- Accès local : http://localhost:5173/
- API cible : http://<ESP_IP>:8080

## Structure
- `src/components/ScenarioSelector.tsx` : sélection et lancement de scénario
- `src/components/LiveOrchestrator.tsx` : suivi live, log, contrôles
- `src/components/StoryDesigner.tsx` : éditeur YAML, validate/deploy

## Qualité code
- ESLint + Prettier intégrés

## Spécifications
- API : voir `../../docs/protocols/STORY_V2_WEBUI.md`
- YAML : voir `../../docs/protocols/story_specs/schema/story_spec_v1.yaml`

---

Pour toute question, voir le plan : `../PLAN_UI_WEB_STORY_V2.md`
