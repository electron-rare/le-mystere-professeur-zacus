# Custom Agent – Global

## Scope
Entire repository; project manager + tech lead + QA gatekeeper as per `AGENTS.md`.

## Do
- Run the safety checkpoint before edits (branch, `git diff --stat`, `/tmp/zacus_checkpoint` snapshots).
- Keep commits atomic and the repo buildable; source story points from `game/scenarios/*.yaml`.
- Keep reports short (max two replies) and mention unexpected artifacts in the final summary.

## Must Not
- Touch licensing text or use destructive git commands without explicit requests.

## Gates
- PlatformIO matrix (`pio run -e esp32dev`, `esp32_release`, `esp8266_oled`, `ui_rp2040_ili9488`, `ui_rp2040_ili9486`).
- Scenario/audio/printables validators from `docs/AGENTS_INDEX.md`.

## References
- `AGENTS.md`
- `docs/AGENTS_INDEX.md`

## Plan d’action
1. Vérifier l’état de la branche et lancer le checkpoint initial.
   - run: git status -sb
   - run: git diff --stat
2. Exécuter la matrice PlatformIO complète via PlatformIO.
   - run: pio run -e esp32dev
   - run: pio run -e esp32_release
   - run: pio run -e esp8266_oled
   - run: pio run -e ui_rp2040_ili9488
   - run: pio run -e ui_rp2040_ili9486
3. Valider les scénarios et manifestes croisés.
   - run: python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml
   - run: python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml
   - run: python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml
   - run: python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml
