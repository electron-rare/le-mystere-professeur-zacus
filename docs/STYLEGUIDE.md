# Style guide

## Tonalité générale
- Français léger, accessible, phrases courtes.
- Préfère des verbes d’action, un vocabulaire spatial (laboratoire, station, énigme) et des termes d’enquête (preuve, mobile, hypothèse).
- Maintiens le respect des consignes (noir & blanc, pas de marques externes, jeux pour 6–14 enfants). Mentionne la plage 60–90 minutes dès que tu décris un déroulé.

## Markdown & structure
- Titres H2 ou H3 pour chaque bloc logique (Objectif, Étapes, Matériel).
- Listes numérotées pour les workflows et checkboxes pour les listes de matériel.
- Utilise des blocs `> Astuce :` ou `> Conseils :` pour souligner les recommandations.
- Évite les tableaux complexes, privilégie des sections dédoublées pour les horaires, stations et rôles.

## Contenus spécifiques
- **Scénarios (`game/scenarios/*.yaml`)** : structure clé/valeur, durée 60–90 min, `solution_unique` vrai. Chaque entrée `stations` ou `puzzles` doit contenir `clue` explicite et `effect` qui aide la narration.
- **Audio (`game/prompts/audio/*.md`)** : texte court (<3 phrases), voix directe, mentionne quand déclencher chaque piste. La feuille `audio/manifests/*.yaml` doit décrire les `tracks` avec `cues` et pointer vers les fichiers de `game/prompts/audio/`.
- **Printables (`printables/src/prompts/` et `WORKFLOW.md`)** : encourage les prompts (ex. « dessine un badge ... »), rappelle les dimensions recommandées (A6/A5/A4) et l’orientation recto/verso.

## Documentation collaborative
- Si tu mentionnes un fichier influent (ex. un scénario ou un prompt), écris son chemin complet (`kit-maitre-du-jeu/...`, `game/scenarios/...`).
- Mets à jour `docs/index.md` si tu ajoutes une nouvelle catégorie ou un nouveau guide.
- Avant de soumettre un PR, vérifie les scripts de validation (`tools/scenario/validate_scenario.py`, `tools/audio/validate_manifest.py`).
