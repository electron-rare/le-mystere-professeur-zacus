---
# Zacus Firmware – UI Fonts

---

## 📝 Description

Ce dossier contient les polices personnalisées pour l’UI Zacus (LVGL, bitmap, etc).

---

## 📦 Contenu du dossier

- Fichiers générés LVGL (.c) via `tools/fonts/scripts/generate_lvgl_fonts.sh`
- TTF/OTF pour génération LVGL
- PNG/BMP pour polices bitmap

Le runtime utilise Montserrat intégré sauf si `UI_FONT_EXTERNAL_SET=1` et que les fichiers générés sont présents/compilés.

---

## 🚀 Installation & usage

Placer les polices ici puis utiliser le script :
```sh
tools/fonts/scripts/generate_lvgl_fonts.sh
```

---

## 🤝 Contribuer

Merci de ne pas ajouter de polices propriétaires sans licence libre.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
---
