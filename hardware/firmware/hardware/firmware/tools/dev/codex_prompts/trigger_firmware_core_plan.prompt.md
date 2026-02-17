```
Objectif : exécuter le plan `firmware_core` (build PlatformIO + smoke + WiFi/RTOS health + artefacts) depuis Copilot/VS Code.

Contexte :
- Le plan est défini dans `.github/agents/firmware_core.md` sous “Plan d’action”.
- Les artefacts et logs doivent être consigné dans `docs/AGENT_TODO.md`, `hardware/firmware/logs/` et `hardware/firmware/artifacts/`.

Commande :
```
cd "$(git rev-parse --show-toplevel)"
tools/dev/plan_runner.sh --agent firmware_core
```

Options utiles :
- `--plan-only` pour prévisualiser les étapes sans exécuter.
- `--dry-run` pour vérifier la commande sans side-effect.

Consigner ensuite :
- `docs/AGENT_TODO.md`: date, plan exécuté, artefacts générés.
- `hardware/firmware/logs/` et `hardware/firmware/artifacts/`: copies ou liens relatifs.
```
