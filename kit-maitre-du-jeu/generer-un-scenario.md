# Générer un scénario

Ce kit est livré avec un scénario canon (`game/scenarios/zacus_v1.yaml`). Pour créer une variante ou un nouveau mystère, suivez ces étapes :

1. **Copiez le canevas**
   - Dupliquez `game/scenarios/zacus_v1.yaml` et choisissez un nouvel `id` (ex. `zacus_v2`).
   - Ajustez les champs `title`, `canon.timeline`, `stations`, `puzzles` et `solution` pour correspondre à la nouvelle intrigue.

2. **Respectez la structure canonique**
   - `players` doit refléter la plage 6–14.
   - `duration_minutes` doit indiquer une fourchette 60–90.
   - `solution_unique` doit être `true` et les preuves dans `solution.proof` doivent justifier l’accusation principale.

3. **Validez le fichier**
   - Utilisez `python tools/scenario/validate_scenario.py game/scenarios/zacus_v2.yaml` pour confirmer que le fichier contient tous les champs obligatoires (id, title, solution, preuve, stations, puzzles).
   - Le script vérifie aussi que la solution pointe vers un seul coupable et qu’il existe au moins trois preuves inscrites.

4. **Reliez les prompts**
   - Si votre scénario utilise des éléments audio ou imprimables spécifiques, créez des fichiers dans `game/prompts/audio/` et `printables/src/prompts/`. Ajoutez-les ensuite à `audio/manifests/zacus_v1_audio.yaml` ou créez un manifeste dédié.

5. **Documentez la variante**
   - Ajoutez une entrée dans `CHANGELOG.md` et, si le scénario est stable, mentionnez-le dans `docs/QUICKSTART.md` ou `docs/STYLEGUIDE.md` comme référence.

> Conseils : gardez une chronologie simple (3 à 5 temps forts), des preuves tangibles et un mobile clair. Les enfants de 6 à 14 ans ont besoin d’une progression visuelle simple et d’un seul point de friction majeur.
