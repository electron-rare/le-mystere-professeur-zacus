PYTHON ?= python3
SCENARIO ?= game/scenarios/zacus_v2.yaml
FRONTEND_DIR ?= frontend-v3

.PHONY: bootstrap-validators bootstrap-docs scenario-validate audio-validate printables-validate export validate-runtime-bundle content-checks runtime3-compile runtime3-simulate runtime3-verify runtime3-test runtime3-firmware-bundle frontend-typecheck frontend-test frontend-build docs-build docs-serve all-validate images playtest hints-serve hints-test

bootstrap-validators:
	bash tools/setup/install_validators.sh

bootstrap-docs:
	$(PYTHON) -m pip install --user --break-system-packages -r tools/requirements/docs.txt

scenario-validate:
	$(PYTHON) tools/scenario/validate_scenario.py $(SCENARIO)

audio-validate:
	$(PYTHON) tools/audio/validate_manifest.py audio/manifests/zacus_v2_audio.yaml

printables-validate:
	$(PYTHON) tools/printables/validate_manifest.py printables/manifests/zacus_v2_printables.yaml

export:
	$(PYTHON) tools/scenario/export_md.py $(SCENARIO)

validate-runtime-bundle:
	$(PYTHON) tools/scenario/validate_runtime_bundle.py

content-checks:
	bash tools/test/run_content_checks.sh

runtime3-compile:
	$(PYTHON) tools/scenario/compile_runtime3.py $(SCENARIO)

runtime3-simulate:
	$(PYTHON) tools/scenario/simulate_runtime3.py $(SCENARIO)

runtime3-verify:
	$(PYTHON) tools/scenario/verify_runtime3_pivots.py $(SCENARIO)

runtime3-test:
	$(PYTHON) -m unittest discover -s tests/runtime3 -p 'test_*.py'

runtime3-firmware-bundle:
	$(PYTHON) tools/scenario/export_runtime3_firmware_bundle.py $(SCENARIO)

frontend-typecheck:
	cd $(FRONTEND_DIR) && pnpm typecheck

frontend-test:
	cd $(FRONTEND_DIR) && pnpm test

frontend-build:
	cd $(FRONTEND_DIR) && pnpm build

docs-build:
	$(PYTHON) -m mkdocs build --strict

docs-serve:
	$(PYTHON) -m mkdocs serve

all-validate: scenario-validate audio-validate printables-validate validate-runtime-bundle runtime3-compile runtime3-simulate runtime3-verify runtime3-test
	@echo "All validations passed."

images:
	$(PYTHON) tools/images/generate_printables.py --manifest printables/manifests/zacus_v2_printables.yaml

playtest:
	uv run --no-project --with pyyaml python tools/playtest/run_playtest.py \
		--scenario game/scenarios/zacus_v2.yaml \
		--playtest game/scenarios/playtests/zacus_v3_60min_tech.playtest.yaml \
		--snapshot game/scenarios/playtests/snapshots/zacus_v3_60min_tech.snapshot.json

hints-serve:
	uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
		uvicorn tools.hints.server:app --reload --port 8300

hints-test:
	uv run --with fastapi --with uvicorn --with pyyaml --with pydantic --with pytest --with httpx \
		pytest tests/hints/ -v
