# Quickstart

## Vue canonique
- Source de vérité: `game/scenarios/zacus_v2.yaml`
- Studio auteur: `frontend-scratch-v2/`
- Runtime portable: Zacus Runtime 3
- Cible terrain principale: `hardware/firmware` sur `freenove_esp32s3`
- Shell d'orchestration: `tools/dev/zacus.sh`

## Valider le contenu
```bash
bash tools/setup/install_validators.sh
bash tools/test/run_content_checks.sh --install-missing
```

Checks inclus:
- validation scénario
- compilation Runtime 3
- simulation Runtime 3
- validation runtime bundle
- validation audio
- validation printables
- export Markdown

## Compiler le runtime portable
```bash
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml
python3 tools/scenario/simulate_runtime3.py game/scenarios/zacus_v2.yaml
python3 tools/scenario/verify_runtime3_pivots.py game/scenarios/zacus_v2.yaml
python3 -m unittest discover -s tests/runtime3 -p 'test_*.py'
python3 tools/scenario/export_runtime3_firmware_bundle.py game/scenarios/zacus_v2.yaml
```

Ou via le shell canonique:
```bash
./tools/dev/zacus.sh runtime3-compile
./tools/dev/zacus.sh runtime3-simulate
./tools/dev/zacus.sh runtime3-verify
./tools/dev/zacus.sh runtime3-test
./tools/dev/zacus.sh frontend-test
```

Les artefacts sont écrits sous `artifacts/runtime3/<timestamp>/`.
Le bundle firmware canonique est écrit dans `hardware/firmware/data/story/runtime3/DEFAULT.json`.

## Démarrer le studio React + Blockly
```bash
cd frontend-scratch-v2
npm install
npm test
npm run lint
VITE_STORY_API_BASE=http://<esp_ip>:8080 npm run dev
```

Pour un build local:
```bash
./tools/dev/zacus.sh frontend-build
```

## Générer la documentation
```bash
python3 -m pip install --user --break-system-packages -r tools/requirements/docs.txt
python3 -m mkdocs build --strict
```

Ou:
```bash
./tools/dev/zacus.sh docs-build
```

## Valider le firmware
```bash
cd hardware/firmware
pio run -e freenove_esp32s3
pio run -e esp8266_oled
```

Chemin mono-carte recommandé:
```bash
cd hardware/firmware
ZACUS_ENV="freenove_esp32s3" ./tools/dev/run_matrix_and_smoke.sh
```

## Utiliser le shell Zacus
```bash
./tools/dev/zacus.sh content-checks
./tools/dev/zacus.sh ports
./tools/dev/zacus.sh artifacts-summary
./tools/dev/zacus.sh menu
```

Le menu utilise `gum` si disponible et repasse en mode texte sinon.

## Où regarder ensuite
- `docs/architecture/index.md`
- `plans/master-plan.md`
- `todos/master.md`
- `hardware/firmware/docs/AGENT_TODO.md`
