# Agent Contract – Tests Copilot

## Scope
Tests agents focus on files under `tools/dev/`, `tools/test/`, `esp32_audio/tests/`, and the artifacts/log directories (`artifacts/`, `logs/`).

## Doit
- Toujours lire/mettre à jour `docs/AGENT_TODO.md` pour annoncer les gates en cours ou bloqués (ex. smoke, stress, audit).
- Exécuter les calibrations/gates via les scripts officiels (`tools/dev/run_matrix_and_smoke.sh`, `tools/dev/run_smoke_tests.sh`, `tools/dev/run_stress_tests.py`, `tools/test/audit_coherence.py`), puis consigner les artefacts sous `artifacts/`.
- Tenir `docs/TEST_SCRIPT_COORDINATOR.md` à jour avec les résultats et les chemins d’évidence (section “Exécutions récentes” + reporting template).  
- Valider les commandes cockpit via `tools/dev/cockpit_commands.yaml` et `docs/_generated/COCKPIT_COMMANDS.md` (regénération avec `python3 tools/dev/gen_cockpit_docs.py` si besoin).
- Mentionner les échecs réseau (HTTP/WebSocket/WiFi) dans `docs/AGENT_TODO.md` et `docs/TEST_SCRIPT_COORDINATOR.md` pour la coordination.

## Artefacts
- Les logs exigent `meta.json`, `git.txt`, `commands.txt`, `summary.md` (au minimum) ; vérifiez les répertoires demandés dans `docs/TEST_SCRIPT_COORDINATOR.md`.
- Pas de commit d’artefacts ; les chemins doivent être mentionnés dans le TODO.

## Bonnes pratiques
- En cas de panne persistante (UI link, stress panic, scénario manquant), capturez les traces dans `artifacts/rc_live/<timestamp>` et notez la prochaine action dans `docs/AGENT_TODO.md`.
