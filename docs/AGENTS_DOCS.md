# Agent Contract – Docs & Knowledge Copilot

## Scope
Docs agents govern `docs/`, `README.md`, `esp32_audio/README.md`, and any onboarding / briefing files (`.github/agents/`, `docs/_generated/`).

## Doit
- Avant toute mise à jour, lire `docs/AGENT_TODO.md` pour ne pas dupliquer les efforts de coordination.
- Refléter tout changement d’organisation ou de procédure dans `docs/TEST_SCRIPT_COORDINATOR.md`, `docs/TEST_COHERENCE_AUDIT_RUNBOOK.md`, et les AGENTS concernés.
- Maintenir `docs/_generated/COCKPIT_COMMANDS.md` en cohérence avec `tools/dev/cockpit_commands.yaml` via `python3 tools/dev/gen_cockpit_docs.py`.
- Documenter les nouveaux artefacts (gates/tests) dans la section adéquate du runbook et mentionner les chemins dans le reporting template.
- Mentionner toute dépendance externe (depuis `docs/RTOS_WIFI_HEALTH.md`, `docs/WIFI_RECOVERY_AND_HEALTH.md`, etc.) pour aider tranche QA.

## Artefacts
- Les docs n’intègrent pas d’artefacts binaires ; citez plutôt les dossiers `artifacts/` lorsque vous décrivez un protocole ou un gate réussi.
- Pour les instructions “TODO”, mettez à jour `docs/AGENT_TODO.md` (bloc 4/5) en parallèle des docs.

## Bonnes pratiques
- Chaque mise à jour de runbook ou guide doit inclure un rappel sur la nécessité de respecter `docs/AGENT_TODO.md` comme tracker unique.
