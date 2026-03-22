PYTHON ?= python3
SCENARIO ?= game/scenarios/zacus_v2.yaml
FRONTEND_DIR ?= frontend-scratch-v2

.PHONY: bootstrap-validators bootstrap-docs scenario-validate audio-validate printables-validate export validate-runtime-bundle content-checks runtime3-compile runtime3-simulate runtime3-verify runtime3-test runtime3-firmware-bundle frontend-lint frontend-test frontend-build docs-build docs-serve all-validate images

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

frontend-lint:
	cd $(FRONTEND_DIR) && npm run lint

frontend-test:
	cd $(FRONTEND_DIR) && npm run test

frontend-build:
	cd $(FRONTEND_DIR) && npm run build

docs-build:
	$(PYTHON) -m mkdocs build --strict

docs-serve:
	$(PYTHON) -m mkdocs serve

all-validate: scenario-validate audio-validate printables-validate validate-runtime-bundle runtime3-compile runtime3-simulate runtime3-verify runtime3-test
	@echo "All validations passed."

images:
	$(PYTHON) tools/images/generate_printables.py --manifest printables/manifests/zacus_v2_printables.yaml
