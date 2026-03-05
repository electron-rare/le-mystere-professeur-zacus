---
# Zacus Tools – Printable Image Generator

---

## 📝 Description

Génère des ébauches PNG pour chaque asset imprimable à partir du manifeste, via l’API OpenAI Images.

---

## 🚀 Installation & usage

Pré-requis :
- `OPENAI_API_KEY` dans l’environnement
- Dépendances dans `.venv` (`PyYAML`, `openai`)

Exécution :
```sh
PYTHON=.venv/bin/python tools/images/generate_printables.py \
  --manifest printables/manifests/zacus_v2_printables.yaml
```
Ajouter `--force` pour régénérer même si les fichiers existent déjà.

Les sorties sont dans `printables/export/png/zacus_v2/` et reprennent les IDs du manifeste.

---

## 🤝 Contribuer

Les contributions sont bienvenues !
Merci de lire [../../CONTRIBUTING.md](../../CONTRIBUTING.md) avant toute PR.

---

## 🧑‍🎓 Licence

- **Code** : MIT (`../../LICENSE`)

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
