---
layout: default
title: Publier le site
description: Configuration rapide GitHub Pages pour /docs.
---

# Publier le mini-site (GitHub Pages)

1. Sur GitHub : **Settings → Pages**
2. **Build and deployment**
   - Source : **Deploy from a branch**
   - Branch : `main` (ou `master`)
   - Folder : **/docs**
3. GitHub affiche l’URL publique (généralement `https://<user>.github.io/<repo>/`).

Si tu utilises un sous-chemin personnalisé, ajuste `baseurl` dans `docs/_config.yml`.
