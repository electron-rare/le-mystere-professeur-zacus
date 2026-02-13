# Workflow printables

1. **Collecte**
   - Ouvre les prompts dans `src/prompts/` pour guider la création graphique (forme, hiérarchie, éléments narratifs).
   - Rassemble les textes essentiels : titres des stations, listes de preuves, dialogues courts.

2. **Construction**
   - Crée les fichiers dans `src/` en respectant les recommandations de `printables/README.md` (formats A6/A5/A4, `recto`/`verso`, versions multiples).
   - Place les ressources graphiques nécessaires (icônes, glyphes) dans `src/assets/` si besoin.

3. **Validation et export**
   - Vérifie les polices (noir et blanc, lisibilité) et imprime une maquette PDF / PNG dans `export/{pdf,png}/`.
   - Pour chaque modèle, crée un aperçu dans le dossier approprié (`general`, `fiche-enquete`, `personnages`, `zones`).

4. **Distribution**
   - Prépare les enveloppes pour chaque station en incluant les fichiers PDF version imprimée.
   - Ajoute un lien vers les fichiers audio ou QR codes dans la version finale (voir `game/prompts/audio/`).

> Astuce : les prompts sont aussi des fiches de briefing pour les illustrateurs novices. Ils permettent de rester dans le style N&B, d’imprimer rapidement et de respecter la durée (60–90 min) du jeu. Si tu veux ajouter une déclinaison, copie les prompts et adapte la version (ex. `indices-v2`).
