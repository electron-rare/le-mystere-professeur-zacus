# Quickstart

## En un coup d’œil
- Source de vérité : `game/scenarios/zacus_v1.yaml` (dérive tout le reste : kit MJ, audio, printables).
- Public : 6 à 14 enfants, 60–90 minutes.
- Matériel : `kit-maitre-du-jeu/`, `printables/`, `game/scenarios/zacus_v1.yaml`, `audio/manifests/zacus_v1_audio.yaml`, `hardware/firmware/esp32` (ESP32 + écran tactile).
- Licences : contenus créatifs CC BY-NC 4.0 (`LICENSES/CC-BY-NC-4.0.txt`), code/script MIT (`LICENSES/MIT.txt`).

## Installer
1. Copie les imprimables depuis `printables/export/pdf/` ou généré via les prompts listés dans `printables/manifests/zacus_v1_printables.yaml` + `printables/src/prompts/`.
2. Prépare un lecteur audio avec `audio/manifests/zacus_v1_audio.yaml` et les fichiers de `game/prompts/audio/`.
3. Positionne les stations selon `kit-maitre-du-jeu/plan-stations-et-mise-en-place.md`, prépare les rôles (`distribution-des-roles.md`) et l’accueil rapide (`script-minute-par-minute.md`).
4. Flash et configure l’ESP32 en suivant les instructions de `hardware/firmware/esp32/README.md` pour que l’interface réagisse aux stations imprimées.

## Préparer l’électronique

1. Installe PlatformIO (`pip install -U platformio`) et branche l’ESP32 décrit dans `hardware/firmware/esp32`, écran + alim.
2. Compile/flash avec un profil supporté (`pio run -e esp32dev -t upload`), puis charge les assets `pio run -e esp32dev -t uploadfs` si nécessaire.
3. Vérifie que l’écran affiche le scénario (`python3 tools/scenario/validate_scenario.py ...`), la connectique audio/led est prête, et que l’ESP32 reste branché pendant toute la partie.

## Déroulé express
1. Accueil + immersion (0-10 min) : boucle audio d’`intro.md`, attribution des rôles, mise au courant sur les règles anti-chaos.
2. Support d’enquête (10-65 min) : station par station, fiche d’enquête, audio `incident.md`, indices prints.
3. Synthèse & final (65-90 min) : audio `accusation.md`, accusation finale, audio `solution.md`, lecture `solution-complete.md`.

## Outils de validation
- Scénario : `python tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml`
- Audio : `python tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- Printables : `python tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`
- Export Markdown : `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`

## Outils test firmware

Entrée rapide:

```bash
python3 tools/test/zacus_menu.py
```

Codex CLI intégré : `./tools/dev/zacus.sh codex --prompt tools/dev/codex_prompts/zacus_overhaul_one_shot.md`
Préflight USB ZeroClaw : `./tools/dev/zacus.sh zeroclaw-preflight --require-port`

Checks contenu (sans hardware):

```bash
bash tools/test/run_content_checks.sh
```

Suites série disponibles:

```bash
python3 tools/test/run_serial_suite.py --list-suites
```

Mode laptop/CI (sans carte branchée):

```bash
python3 tools/test/run_serial_suite.py --suite smoke_plus --allow-no-hardware
python3 tools/test/zacus_menu.py --action smoke --allow-no-hardware
```

Pour résoudre les ports sans matériel, la commande `./tools/dev/zacus.sh ports` écrit le JSON contractuel dans `artifacts/ports/<timestamp>/ports_resolve.json` et met à jour `artifacts/ports/latest_ports_resolve.json`. Pour mocker les CP2102 utilisez `ZACUS_MOCK_PORTS=1 ZACUS_PORTS_FIXTURE=tools/test/fixtures/ports_list_macos.txt ./tools/dev/zacus.sh ports`.

Codex CLI intégré : `./tools/dev/zacus.sh codex --prompt tools/dev/codex_prompts/zacus_overhaul_one_shot.md`. Le script note aussi l’état des ports résolus et place les logs dans `artifacts/codex/<timestamp>/`.

Guide orchestration ZeroClaw: `docs/zeroclaw_orchestration.md`

Dépendances optionnelles:
- `pip install pyyaml` pour `run_content_checks.sh`
- `pip install pyserial` pour suites USB, UI Link sim et console série

## Pour aller plus loin
- Crée une variante : duplique le YAML, modifie `canon` et `solution` et revalide avec le script.
- Ajoute des cartes imprimables en t’inspirant des prompts dans `printables/src/prompts/`.
- Mets à jour ce quickstart via un PR si le montage change (p. ex. nouvelle station ou nouvelle plage horaire).
