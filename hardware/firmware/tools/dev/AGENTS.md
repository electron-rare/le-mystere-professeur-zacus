# Agent Contract (Tooling-Specific Rules)

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

Les gates sont recommandees, mais obligatoires uniquement si demande explicite.

## Reporting
Afficher la commande exacte de relance en cas d'echec.

## Stop Conditions
Utiliser les conditions d'arret racine.
