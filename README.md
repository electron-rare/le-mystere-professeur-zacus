# Le Mystere du Professeur Zacus

Zacus est en refonte vers un produit hybride unique:
- un jeu terrain fiable sur carte Freenove ESP32-S3,
- un studio auteur moderne en React + Blockly,
- un runtime portable "Zacus Runtime 3" compile depuis le YAML canonique.

## Canon actuel
- Source narrative: `game/scenarios/zacus_v2.yaml`
- Studio auteur: `frontend-scratch-v2/`
- Runtime portable: `tools/scenario/compile_runtime3.py` + `tools/scenario/simulate_runtime3.py`
- Cible hardware principale: `hardware/firmware` avec `freenove_esp32s3`
- Plans et memoire: `memory/`, `plans/`, `todos/`
- Architecture et cartes Mermaid: `docs/architecture/`

## Démarrage rapide

### 1. Bootstrap validation
```bash
bash tools/setup/install_validators.sh
bash tools/test/run_content_checks.sh
```

### 2. Compiler et simuler Runtime 3
```bash
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml
python3 tools/scenario/simulate_runtime3.py game/scenarios/zacus_v2.yaml
python3 tools/scenario/export_runtime3_firmware_bundle.py game/scenarios/zacus_v2.yaml
```

### 3. Démarrer le studio React + Blockly
```bash
cd frontend-scratch-v2
npm install
npm test
VITE_STORY_API_BASE=http://<esp_ip>:8080 npm run dev
```

### 4. Utiliser le shell canonique
```bash
./tools/dev/zacus.sh content-checks
./tools/dev/zacus.sh runtime3-compile
make runtime3-verify
make runtime3-test
./tools/dev/zacus.sh frontend-test
./tools/dev/zacus.sh frontend-build
./tools/dev/zacus.sh menu
```

## Cartographie du dépôt
- `game/`: scénarios YAML canoniques.
- `audio/`: manifestes audio et assets associés.
- `printables/`: manifestes et exports imprimables.
- `kit-maitre-du-jeu/`: matériel MJ et déroulé terrain.
- `frontend-scratch-v2/`: studio auteur React + Blockly.
- `hardware/firmware/`: firmware, APIs device, scripts terrain.
- `tools/`: validateurs, compilateur/simulateur Runtime 3, shells d'automatisation.
- `docs/`: quickstart, architecture, benchmark OSS et runbooks.
- `memory/`, `plans/`, `todos/`: pilotage de la refonte.

## AI Integration

Le projet intègre une couche IA pour enrichir l'expérience terrain :
- **Voice pipeline** : wake word (ESP-SR) → ASR → LLM → TTS (Piper / XTTS-v2) → speaker. Scaffold prêt, voir [`docs/voice/VOICE_PIPELINE_GUIDE.md`](docs/voice/VOICE_PIPELINE_GUIDE.md).
- **Vision** : détection d'objets via ESP-DL pour indices contextuels.
- **LLM hints** : le Professeur Zacus répond aux joueurs via mascarade.
- **TUI dev** : script interactif d'orchestration → `python3 tools/dev/zacus_tui.py`

Analyse complète : [`docs/AI_INTEGRATION_ANALYSIS.md`](docs/AI_INTEGRATION_ANALYSIS.md)

## Sécurité & Déploiement

- [`docs/SECURITY.md`](docs/SECURITY.md) — audit firmware, HMAC auth, rate limiting
- [`docs/DEPLOYMENT_RUNBOOK.md`](docs/DEPLOYMENT_RUNBOOK.md) — procédures de déploiement terrain

## Statut du projet

- Runtime 3 : compilateur + simulateur + export firmware bundle OK
- Studio auteur : React 19 + Blockly, 18 tests passing
- Firmware : sécurité P0 intégrée (HMAC, rate limit, safe OTA)
- Voice pipeline : scaffold ESP-SR prêt, TTS Docker validé
- Specs : `ZACUS_RUNTIME_3_SPEC.md`, `STORY_DESIGNER_SCRATCH_LIKE_SPEC.md`

## Documentation à lire
- `docs/QUICKSTART.md`
- `docs/architecture/index.md`
- `docs/AI_INTEGRATION_ANALYSIS.md`
- `specs/ZACUS_RUNTIME_3_SPEC.md`
- `specs/STORY_DESIGNER_SCRATCH_LIKE_SPEC.md`
- `docs/benchmark-oss.md`

## Notes de refonte
- Le YAML reste la source de vérité pendant la migration.
- Le Runtime 3 devient le contrat portable entre studio, simulateur et firmware.
- `hardware/firmware/esp32/` reste en lecture seule.
- Les chemins legacy ne doivent être supprimés qu'après preuve de remplacement.

## Licences
- Code: MIT (`LICENSE`)
- Contenu créatif: CC BY-NC 4.0 (`LICENSE-CONTENT.md`)
