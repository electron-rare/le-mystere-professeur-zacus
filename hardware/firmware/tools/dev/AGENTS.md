
# Agent Contract (Tooling-Specific Rules)

**Note : Le Test & Script Coordinator doit mettre à jour ce fichier à chaque évolution des scripts, conventions ou structure outillage, afin d’assurer la cohérence avec la documentation et le contrat global d’agent.**

**📌 For global agent contract and expectations**, see [Firmware Agent Contract](../../AGENTS.md).

---

## Role
Conventions pour les scripts outillage firmware.

## Scope
S'applique aux scripts et helpers dans `hardware/firmware/tools/dev/**`.

## Doit
- Fournir `--help` et des valeurs par defaut non interactives.
- Utiliser des codes de sortie clairs (`0` succes, non-zero echec exploitable).
- Garder la resolution des ports et timeouts locales et configurables via flags/env.
- Ecrire les logs dans `hardware/firmware/logs/` avec des noms horodates.
- Garder une sortie CLI courte et grep-friendly (`[step]`, `[ok]`, `[fail]`).
- Documenter et maintenir `tools/dev/plan_runner.sh` pour permettre d’exécuter automatiquement les sections `## Plan d’action` des briefs.
- En mode carte combinee (`ZACUS_ENV=freenove_esp32s3`), les gates UI link/story screen doivent etre sorties en `SKIP` avec justification explicite `not needed for combined board`.
- `run_smoke_tests.sh` doit supporter les deux modes: dual-board (ESP32+ESP8266) et combined-board Freenove (`--combined-board`).
- `run_matrix_and_smoke.sh` doit toujours produire l’evidence minimale: `meta.json`, `commands.txt`, `summary.md`, `summary.json`, logs par etape.
- `run_stress_tests.py` doit rester compatible protocoles story JSON + serial legacy (`SC_*`) et permettre la cible `DEFAULT` en single-board.
- Les changements structurels d'outillage sont autorises, mais doivent etre reflites dans la doc/onboarding.

## Interdit
- Ne pas exiger d'interaction chat/operator quand l'automatisation est possible.
- Ne pas hardcoder des chemins series machine-specifiques dans les scripts commits.

## Flow d'execution
1. Detecter les dependances.
2. Resoudre les ports avec timeout.
3. Executer les etapes avec logs par etape.
4. Emettre un resume clair du statut.

## Gates
- `python3 hardware/firmware/tools/dev/serial_smoke.py --help`
- `bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh` (quand le contexte hardware est disponible)
- `bash hardware/firmware/tools/dev/run_smoke_tests.sh --help`

Les gates sont recommandees, mais obligatoires uniquement si demande explicite.

## Reporting
Afficher la commande exacte de relance en cas d'echec.

## TODO governance
- `docs/AGENT_TODO.md` est la liste d'actions canonique : avant de lancer un script/outillage, vérifiez les items concernés, notez l'avancement ou les blocages (p. ex. “hardware requis”), et mentionnez les artefacts créés sans les committer.
- Les logs/artéfacts générés via les scripts `tools/dev` doivent rester hors git ; décrivez leur présence (chemin, timestamp) dans le TODO ou le rapport final pour que les autres agents sachent quoi relancer.

## Stop Conditions
Utiliser les conditions d'arret racine.
