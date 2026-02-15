# Repo Status Report

## Tree summary
- `kit-maitre-du-jeu/`: full GM script, solutions, checklists, and export folders for PDF/PNG deliverables.
- `printables/`: `src/` plus `export/{pdf,png}/` for invitations, cards, badges, and the one-page rule set.
- `hardware/`: BOMs, wiring docs, and the ESP32/Arduino firmware flow with `.pio` dependencies (libraries such as Adafruit and ESP8266Audio naturally expand here).
- `docs/`: maintenance plan, repo audit, and asset folder (`docs/assets/` contains the repo map SVG already referenced in `README.md`).
- `examples/` and `scenario-ai-coherence/` (renamed for portability): auxiliary content and tools, now safe for Windows/CI.

## Empty / TODO files
- `printables/invitations/export/pdf/.gitkeep`
- `printables/invitations/export/png/.gitkeep`
- `printables/invitations/src/.gitkeep`
- assorted `.nojekyll`, `.uno.test.skip`, and upstream library placeholders under `hardware/firmware/esp32/.pio/` (e.g., Adafruit BusIO examples); none appear to contain TODO hints, but they are empty files that may surprise contributors or scripts.

## Broken links
- `hardware/firmware/esp32/.pio/libdeps/esp32dev/ESP8266Audio/README.md` line 250 → `examples/StreamMP3FromHTTP_SPIRAM/Schema_Spiram.png` (missing in that vendor snapshot).
- `hardware/firmware/esp32/.pio/libdeps/esp32_release/ESP8266Audio/README.md` line 250 → same missing `Schema_Spiram.png`.
- `hardware/firmware/esp32/.pio/libdeps/esp32dev/Mozzi/README.md` line 116 → `extras/NEWS.txt` (not included in the copied tree).
- `hardware/firmware/esp32/.pio/libdeps/esp32_release/Mozzi/README.md` line 116 → same missing `extras/NEWS.txt`.

## Naming / portability issues
- `scenario-ai-coherence/` and `scenario-ai-coherence/version-finale/` now avoid spaces/colon, keeping the tree stable sur Windows/CI tooling. Continue to prefer hyphenated, lowercase identifiers pour tout nouveau sous-dossier.

## Licences
- `README.md`, `CONTRIBUTING.md`, and `LICENSE.md` sont alignés autour du modèle CC BY-NC 4.0 pour les contenus créatifs et MIT pour le code (`LICENSES/CC-BY-NC-4.0.txt`, `LICENSES/MIT.txt`). Garder la même nomenclature accélère la revue des contributions.

## Patch plan
- Continuer à surveiller les fichiers vides (`.gitkeep`, `.nojekyll`, `.uno.test.skip`) pour décider s’ils restent ou disparaissent lors d’un nettoyage futur.
- Corriger ou supprimer les liens cassés des bibliothèques `ESP8266Audio`/`Mozzi` si les assets nécessaires ne sont plus plastifiés.
- Valider chaque nouvelle version de scénario et de manifeste audio avec `tools/scenario/validate_scenario.py` et `tools/audio/validate_manifest.py`.
- Mettre à jour Quickstart, Styleguide et le workflow printables si la structure du kit ou les fichiers audio changent.
