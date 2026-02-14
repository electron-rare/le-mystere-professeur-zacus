PYTHON ?= python3

.PHONY: scenario-validate audio-validate printables-validate export all-validate images

scenario-validate:
	$(PYTHON) tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml \
		--audio-manifest audio/manifests/zacus_v1_audio.yaml

audio-validate:
	$(PYTHON) tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml

printables-validate:
	$(PYTHON) tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml

export:
	$(PYTHON) tools/scenario/export_md.py game/scenarios/zacus_v1.yaml --output docs/scenario-zacus_v1.md

all-validate: scenario-validate audio-validate printables-validate
	@echo "All validations passed."

images:
	$(PYTHON) tools/images/generate_printables.py --manifest printables/manifests/zacus_v1_printables.yaml
