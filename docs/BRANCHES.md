# Branches

This repository is intentionally split by concerns to reduce merge conflicts and Codex cost.

## main
Stable integration branch. Avoid big feature work directly on main.

## hardware/firmware
Firmware + hardware integration (ESP32 / RP2040, wiring, platformio, QA firmware).
Scope: hardware/firmware/** (+ tools/qa/** optionally)

## generation/story-esp
Story engine running on ESP32 (Story V2 apps, scenario runtime integration).
Scope: story engine files listed in CODEX_RULES.md

## generation/story-ia
Canonical game/story content, prompts, printables, audio manifests.
Scope: game/** audio/** printables/** kit-maitre-du-jeu/** include-humain-IA/**

## scripts/generation
Generators/validators/exporters + CI validate workflow.
Scope: tools/** + Makefile + .github/workflows/validate.yml

## documentation
Docs and repo policies.
Scope: docs/** + root markdown/policy files
